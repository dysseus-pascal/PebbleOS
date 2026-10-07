/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2: der Smart Alarm ueber einen Neustart. Warum so: wecklauf.h.

#include "pbl/services/activity/boulder/wecklauf.h"

#include "pbl/kernel/compiler.h"
#include "pbl/kernel/mutex.h"
#include "pbl/services/activity/boulder/wecker.h"
#include "pbl/services/settings/settings_file.h"
#include "pbl/services/system_task.h"
#include "pbl/services/time.h"
#include "pbl/util/math.h"
#include "pbl/util/units.h"
#include <pbl/drivers/rtc.h>
#include <pbl/logging/logging.h>

#include <inttypes.h>
#include <stddef.h>
#include <string.h>

PBL_LOG_MODULE_DECLARE(service_activity, CONFIG_SERVICE_ACTIVITY_LOG_LEVEL);

// Eine eigene kleine Datei: "boulder_v2" oeffnet speicher.c auf KernelBG, das Klingeln nach
// einem Uhrzeitsprung kommt auf KernelMain - zweimal offen hiesse PBL_CROAK
// (settings_file.c:44-48). Je Morgen ein Satz von rund 30 B, also etwa jeden Monat eine
// Verdichtung von hoechstens 1 KiB.
#define DATEI "boulder_wecker"
#define DATEI_GROESSE PBL_KIB(1)
#define SCHLUESSEL "lauf"
// Fassung 2: "offen" (0xFF) statt "geklungen" (1). Das Ende eines Laufs loescht nur Bits und
// geht so an seinen Platz (settings_file_set_byte) - ohne freien Platz und ohne Verdichtung,
// die bei vollem Flash scheitern und den Wecker nach einem Neustart zweimal klingeln liessen.
#define FASSUNG 2
#define OFFEN 0xFF
#define ERLEDIGT 0x00

// Ein Lauf gilt vom Fensterbeginn bis zum Ende des Nachholfensters. Seine Weckzeit ist aus der
// Ortszeit nur eindeutig, solange beides zusammen keinen halben Tag fuellt.
_Static_assert(WECKER_NACHHOLEN_S >= PBL_SEC_PER_MIN, "Nachholfenster unter einer Minute");
_Static_assert(WECKER_NACHHOLEN_S + SMART_ALARM_RANGE_S <= 12 * PBL_SEC_PER_HOUR,
               "Nachholfenster zu lang fuer eine eindeutige Weckzeit");
// Der Start eines nachgeholten Weckers wird so gelegt, dass Pebble ihn noch im Nachholfenster
// annimmt (boulder_wecklauf_nachholen).
_Static_assert(WECKER_NACHHOLEN_S > WECKLAUF_PEBBLE_VERPASST_S, "Nachholfenster zu kurz");

typedef struct PBL_PACKED {
  uint8_t fassung;
  int32_t id;         //< AlarmId, ALARM_INVALID_ID = kein Lauf
  uint8_t stunde;     //< die Weckzeit des Weckers beim Lauf (Ortszeit), zum Vergleich nach
  uint8_t minute;     //  dem Boot: anders gestellt heisst nicht nachholen
  uint32_t weckzeit;  //< dieselbe Weckzeit als UTC-Zeitpunkt
  uint8_t offen;      //< OFFEN, bis er klingelt oder Pebbles Lauf ohne Klingeln endet
} Lauf;

// Wie weit die Datei s_lauf kennt: ganz, bis auf sein Ende (das Byte "offen") oder gar nicht.
typedef enum {
  DateiGleich,
  DateiNochOffen,
  DateiOhne,
} DateiStand;

static PBL_MUTEX_DEFINE(s_sperre);
static Lauf s_lauf = {.id = ALARM_INVALID_ID};
static DateiStand s_datei;
// Pebble fragte in diesem Boot nach s_lauf: Sein Lauf lief seither (fuer die Minute).
static bool s_gefragt;
// Pebbles scharfer Termin von vor dem Boot und ob die Frage nach dem Boot noch aussteht.
static AlarmId s_scharf_id = ALARM_INVALID_ID;
static time_t s_scharf_utc;
static bool s_boot_offen;
// Der Lauf, den nachholen an Pebble gab, bis zu seiner ersten Frage: Seine Weckzeit steht fest.
static AlarmId s_nachgeholt_id = ALARM_INVALID_ID;
static time_t s_nachgeholt_weckzeit;
// Die Minute hat etwas zu tun (Lauf offen und gefragt, oder ungespeichert). Ohne Sperre gelesen:
// Eine Minute zu spaet ist harmlos.
static volatile bool s_minute_noetig;

// Aufrufer haelt s_sperre. Schreibt, was die Datei von s_lauf noch nicht kennt.
static void prv_schreiben(void) {
  if (s_datei == DateiGleich) {
    return;
  }
  SettingsFile datei;
  if (settings_file_open(&datei, DATEI, DATEI_GROESSE) != S_SUCCESS) {
    PBL_LOG_ERR("Boulder: %s nicht geoeffnet, Wecklauf nur im RAM", DATEI);
    return;
  }
  status_t rv = E_DOES_NOT_EXIST;
  if (s_datei == DateiNochOffen) {
    rv = settings_file_set_byte(&datei, SCHLUESSEL, strlen(SCHLUESSEL), offsetof(Lauf, offen),
                                s_lauf.offen);
  }
  if (rv != S_SUCCESS) {
    rv = settings_file_set(&datei, SCHLUESSEL, strlen(SCHLUESSEL), &s_lauf, sizeof(s_lauf));
  }
  settings_file_close(&datei);
  if (rv == S_SUCCESS) {
    s_datei = DateiGleich;
  } else {
    PBL_LOG_ERR("Boulder: Wecklauf nicht gespeichert (%" PRId32 "), naechste Minute wieder", rv);
  }
}

// Vom Fensterbeginn bis zum Ende des Nachholfensters.
static bool prv_im_fenster(time_t weckzeit, time_t jetzt) {
  return jetzt >= weckzeit - SMART_ALARM_RANGE_S && jetzt < weckzeit + WECKER_NACHHOLEN_S;
}

// Aufrufer haelt s_sperre.
static void prv_minute_stellen(time_t jetzt) {
  s_minute_noetig = prv_im_fenster(s_lauf.weckzeit, jetzt) &&
                    ((s_gefragt && s_lauf.offen == OFFEN) || s_datei != DateiGleich);
}

// Aufrufer haelt s_sperre. Der Lauf ist zu Ende: Nach einem Neustart nicht nachholen.
static void prv_erledigen(void) {
  if (s_lauf.offen != OFFEN) {
    return;
  }
  s_lauf.offen = ERLEDIGT;
  if (s_datei == DateiGleich) {
    s_datei = DateiNochOffen;
  }
  prv_schreiben();
}

// Ein Lauf, den der Neustart unterbrach.
static bool prv_lauf_nachholen(const Lauf *lauf, time_t jetzt) {
  if (lauf->id == ALARM_INVALID_ID || lauf->offen != OFFEN ||
      !prv_im_fenster(lauf->weckzeit, jetzt)) {
    return false;
  }
  AlarmInfo info;
  if (!alarm_get_info(lauf->id, &info)) {
    return false;  // geloescht
  }
  // Einen Einmalwecker schaltet alarm.c beim Fensterbeginn aus; ein Wiederholungswecker, der
  // jetzt aus ist, wurde im Lauf ausgeschaltet.
  return info.is_smart && info.hour == lauf->stunde && info.minute == lauf->minute &&
         (info.enabled || info.kind == ALARM_KIND_JUST_ONCE);
}

typedef struct {
  time_t scharf_utc;
  time_t jetzt;
  const Lauf *lauf;
  AlarmId id;       //< Ergebnis: der Smart Alarm mit der fruehesten Weckzeit
  time_t weckzeit;
} Suche;

// alarm_for_each, unter der Sperre von alarm.c: kein Aufruf zurueck in alarm.c, keine eigene
// Sperre. Hat dieser Smart Alarm seinen Fensterbeginn verpasst?
static void prv_verpasst(AlarmId id, const AlarmInfo *info, void *context) {
  Suche *suche = context;
  // Ein verpasster Einmalwecker ist noch an: alarm.c schaltet ihn erst beim Fensterbeginn aus.
  if (!info->is_smart || !info->enabled) {
    return;
  }
  const time_t weckzeit = boulder_wecker_weckzeit(suche->jetzt, info->hour, info->minute);
  const time_t fensterbeginn = weckzeit - SMART_ALARM_RANGE_S;
  // Vor dem scharfen Termin war er nicht geplant (alarm.c plant den fruehesten): Sein Fenster
  // lief schon, oder er war da noch nicht an.
  if (fensterbeginn < suche->scharf_utc || fensterbeginn > suche->jetzt) {
    return;
  }
  // Pebbles Cron wertet den Wochentag der Weckzeit. Einem Einmalwecker hat alarm_init den Tag
  // schon neu gesetzt (prv_refresh_just_once_alarm_days): Er ist an, also war er faellig.
  if (info->kind != ALARM_KIND_JUST_ONCE) {
    struct tm ort;
    localtime_r(&weckzeit, &ort);
    if (!info->scheduled_days || !(*info->scheduled_days)[ort.tm_wday]) {
      return;
    }
  }
  // Fuer diese Weckzeit schon zu Ende ginge nur nach einem Uhrzeitsprung zurueck.
  const Lauf *lauf = suche->lauf;
  if (lauf->id == id && lauf->weckzeit == (uint32_t)weckzeit && lauf->offen != OFFEN) {
    return;
  }
  if (suche->id == ALARM_INVALID_ID || weckzeit < suche->weckzeit) {
    suche->id = id;
    suche->weckzeit = weckzeit;
  }
}

void boulder_wecklauf_boot(AlarmId scharf_id, time_t scharf_utc) {
  pbl_mutex_lock(&s_sperre, PBL_FOREVER);
  s_scharf_id = scharf_id;
  s_scharf_utc = scharf_utc;
  s_boot_offen = true;
  s_lauf = (Lauf){.id = ALARM_INVALID_ID};
  s_datei = DateiGleich;
  s_gefragt = false;
  s_nachgeholt_id = ALARM_INVALID_ID;
  s_minute_noetig = false;
  SettingsFile datei;
  if (settings_file_open(&datei, DATEI, DATEI_GROESSE) == S_SUCCESS) {
    Lauf gelesen;
    if (settings_file_get_len(&datei, SCHLUESSEL, strlen(SCHLUESSEL)) == sizeof(gelesen) &&
        settings_file_get(&datei, SCHLUESSEL, strlen(SCHLUESSEL), &gelesen, sizeof(gelesen)) ==
            S_SUCCESS &&
        gelesen.fassung == FASSUNG) {
      s_lauf = gelesen;
    }
    settings_file_close(&datei);
  } else {
    PBL_LOG_WRN("Boulder: %s nicht geoeffnet, kein Wecklauf", DATEI);
  }
  pbl_mutex_unlock(&s_sperre);
}

void boulder_wecklauf_nachholen(AlarmId *id, time_t *zeit) {
  pbl_mutex_lock(&s_sperre, PBL_FOREVER);
  const bool offen = s_boot_offen;
  s_boot_offen = false;
  const Lauf lauf = s_lauf;
  const AlarmId scharf_id = s_scharf_id;
  const time_t scharf_utc = s_scharf_utc;
  pbl_mutex_unlock(&s_sperre);
  // alarm_get_info und alarm_for_each nehmen die Sperre von alarm.c: erst nach der eigenen,
  // nie darunter.
  if (!offen || *id != ALARM_INVALID_ID) {
    return;
  }
  const time_t jetzt = rtc_get_time();
  Suche suche = {.scharf_utc = scharf_utc, .jetzt = jetzt, .lauf = &lauf,
                 .id = ALARM_INVALID_ID};
  if (prv_lauf_nachholen(&lauf, jetzt)) {
    suche.id = lauf.id;
    suche.weckzeit = lauf.weckzeit;
  } else if (scharf_utc != 0 && jetzt >= scharf_utc) {
    // Der Normalfall nach einem Boot ist der scharfe Termin in der Zukunft - ohne Suche.
    alarm_for_each(prv_verpasst, &suche);
  }
  if (suche.id == ALARM_INVALID_ID) {
    return;
  }
  pbl_mutex_lock(&s_sperre, PBL_FOREVER);
  s_nachgeholt_id = suche.id;
  s_nachgeholt_weckzeit = suche.weckzeit;
  pbl_mutex_unlock(&s_sperre);
  PBL_LOG_INFO("Boulder: Smart Alarm %d nach dem Neustart weiter, Weckzeit %" PRIu32
               " (Lauf %d, scharf %d)", suche.id, (uint32_t)suche.weckzeit, (int)lauf.id,
               scharf_id);
  *id = suche.id;
  // Pebble startet ihn beim Freigeben der Wecker nur, wenn *zeit hoechstens
  // WECKLAUF_PEBBLE_VERPASST_S zurueckliegt: ab jetzt, aber nie nach dem Nachholfenster.
  const time_t letzter_start = suche.weckzeit + WECKER_NACHHOLEN_S - 1;
  *zeit = MIN(jetzt, letzter_start - WECKLAUF_PEBBLE_VERPASST_S);
}

bool boulder_wecklauf_fragen(AlarmId id, int zaehler, int stunde, int minute) {
  const time_t jetzt = rtc_get_time();
  pbl_mutex_lock(&s_sperre, PBL_FOREVER);
  // Die erste Frage des nachgeholten Laufs: Seine Weckzeit steht seit dem Boot fest. KernelBG
  // kann sie knapp nach dem Nachholfenster stellen; sie gilt, solange Pebble einen verpassten
  // Wecker annaehme, nicht laenger (sonst traefe sie den Lauf von morgen).
  const bool nachgeholt =
      zaehler == 0 && s_nachgeholt_id == id &&
      jetzt < s_nachgeholt_weckzeit + WECKER_NACHHOLEN_S + WECKLAUF_PEBBLE_VERPASST_S;
  if (zaehler == 0) {
    s_nachgeholt_id = ALARM_INVALID_ID;
  }
  // Sonst steht die Weckzeit ab der ersten Frage eines Laufs (Zaehler 0) fest, in UTC: Springt
  // die Ortszeit im Lauf, endet er wie Pebbles Zaehler. Ein nachgeholter Lauf behaelt die
  // Weckzeit von vor dem Neustart.
  bool weiter = s_lauf.id == id;
  if (nachgeholt) {
    weiter = weiter && s_lauf.weckzeit == (uint32_t)s_nachgeholt_weckzeit;
  } else if (zaehler == 0) {
    weiter = weiter && s_lauf.stunde == stunde && s_lauf.minute == minute &&
             prv_im_fenster(s_lauf.weckzeit, jetzt);
  }
  if (!weiter) {
    s_lauf = (Lauf){
      .fassung = FASSUNG, .id = id, .stunde = stunde, .minute = minute,
      .weckzeit = nachgeholt ? s_nachgeholt_weckzeit
                             : boulder_wecker_weckzeit(jetzt, stunde, minute),
      .offen = OFFEN,
    };
    s_datei = DateiOhne;
    PBL_LOG_INFO("Boulder: Smart Alarm %d laeuft, Weckzeit %" PRIu32, id, s_lauf.weckzeit);
  }
  s_gefragt = true;
  // Datei nur beim neuen Lauf - oder wenn das letzte Schreiben scheiterte.
  prv_schreiben();
  prv_minute_stellen(jetzt);
  const time_t weckzeit = s_lauf.weckzeit;
  pbl_mutex_unlock(&s_sperre);
  // Nachgeholt nach der Weckzeit: sofort, auch wenn die Frage knapp nach dem Nachholfenster
  // kommt (die Freigabe kam noch darin).
  return (nachgeholt && jetzt >= weckzeit) || boulder_wecker_erlaubt(weckzeit);
}

void boulder_wecklauf_geklungen(AlarmId id) {
  pbl_mutex_lock(&s_sperre, PBL_FOREVER);
  if (s_lauf.id == id && s_lauf.offen == OFFEN) {
    prv_erledigen();
    PBL_LOG_INFO("Boulder: Smart Alarm %d geklungen, Weckzeit %" PRIu32, id, s_lauf.weckzeit);
  }
  prv_minute_stellen(rtc_get_time());
  pbl_mutex_unlock(&s_sperre);
}

// KernelBG, wie alarm.c: Zwischen Pebbles Lauf (Cron, Schlummer) und dieser Pruefung kommt
// nichts dazwischen, was den Lauf neu setzt.
static void prv_minute_aufgabe(void *unused) {
  const time_t jetzt = rtc_get_time();
  pbl_mutex_lock(&s_sperre, PBL_FOREVER);
  if (prv_im_fenster(s_lauf.weckzeit, jetzt)) {
    // Pebbles Lauf ist ohne Klingeln vorbei: ausgeschaltet, geloescht oder von einem anderen
    // Wecker abgeloest (alarm.c setzt dann s_most_recent_alarm_id). Ohne Neustart klingelt er
    // heute nicht mehr - mit Neustart auch nicht, selbst wenn er gleich wieder an ist.
    if (s_gefragt && s_lauf.offen == OFFEN && alarm_get_most_recent_id() != s_lauf.id) {
      PBL_LOG_INFO("Boulder: Smart Alarm %d ohne Klingeln beendet", (int)s_lauf.id);
      prv_erledigen();
    }
    prv_schreiben();
  }
  prv_minute_stellen(jetzt);
  pbl_mutex_unlock(&s_sperre);
}

void boulder_wecklauf_minute(void) {
  if (s_minute_noetig) {
    system_task_add_callback(prv_minute_aufgabe, NULL);
  }
}
