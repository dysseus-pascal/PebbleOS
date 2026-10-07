/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2: der Smart Alarm ueber einen Neustart. Warum so: wecklauf.h.

#include "pbl/services/activity/boulder/wecklauf.h"

#include "pbl/kernel/compiler.h"
#include "pbl/kernel/mutex.h"
#include "pbl/services/activity/boulder/wecker.h"
#include "pbl/services/settings/settings_file.h"
#include "pbl/services/time.h"
#include "pbl/util/units.h"
#include <pbl/drivers/rtc.h>
#include <pbl/logging/logging.h>

#include <inttypes.h>
#include <string.h>

PBL_LOG_MODULE_DECLARE(service_activity, CONFIG_SERVICE_ACTIVITY_LOG_LEVEL);

// Eine eigene kleine Datei: "boulder_v2" oeffnet speicher.c auf KernelBG, das Klingeln nach
// einem Uhrzeitsprung kommt auf KernelMain - zweimal offen hiesse PBL_CROAK
// (settings_file.c:44-48). Je Schreibvorgang rund 30 B, also etwa alle zwei Wochen eine
// Verdichtung von hoechstens 1 KiB.
#define DATEI "boulder_wecker"
#define DATEI_GROESSE PBL_KIB(1)
#define SCHLUESSEL "lauf"
#define FASSUNG 1

// Ein Lauf gilt vom Fensterbeginn bis zum Ende des Nachholfensters. Seine Weckzeit ist aus der
// Ortszeit nur eindeutig, solange beides zusammen keinen halben Tag fuellt.
_Static_assert(WECKER_NACHHOLEN_S >= PBL_SEC_PER_MIN, "Nachholfenster unter einer Minute");
_Static_assert(WECKER_NACHHOLEN_S + SMART_ALARM_RANGE_S <= 12 * PBL_SEC_PER_HOUR,
               "Nachholfenster zu lang fuer eine eindeutige Weckzeit");

typedef struct PBL_PACKED {
  uint8_t fassung;
  int32_t id;         //< AlarmId, ALARM_INVALID_ID = kein Lauf
  uint8_t stunde;     //< die Weckzeit des Weckers beim Lauf (Ortszeit), zum Vergleich nach
  uint8_t minute;     //  dem Boot: anders gestellt heisst nicht nachholen
  uint32_t weckzeit;  //< dieselbe Weckzeit als UTC-Zeitpunkt
  uint8_t geklungen;
} Lauf;

static PBL_MUTEX_DEFINE(s_sperre);
static Lauf s_lauf = {.id = ALARM_INVALID_ID};
// Pebbles scharfer Wecker von vor dem Boot und ob die Frage nach dem Boot noch aussteht.
static AlarmId s_scharf_id = ALARM_INVALID_ID;
static time_t s_scharf_utc;
static bool s_boot_offen;

// Aufrufer haelt s_sperre.
static void prv_schreiben(void) {
  SettingsFile datei;
  if (settings_file_open(&datei, DATEI, DATEI_GROESSE) != S_SUCCESS) {
    PBL_LOG_WRN("Boulder: %s nicht geoeffnet, Wecklauf nur im RAM", DATEI);
    return;
  }
  const status_t rv =
      settings_file_set(&datei, SCHLUESSEL, strlen(SCHLUESSEL), &s_lauf, sizeof(s_lauf));
  settings_file_close(&datei);
  if (rv != S_SUCCESS) {
    PBL_LOG_WRN("Boulder: Wecklauf nicht gespeichert (%" PRId32 ")", rv);
  }
}

// Vom Fensterbeginn bis zum Ende des Nachholfensters.
static bool prv_im_fenster(time_t weckzeit, time_t jetzt) {
  return jetzt >= weckzeit - SMART_ALARM_RANGE_S && jetzt < weckzeit + WECKER_NACHHOLEN_S;
}

// Ein Lauf, den der Neustart unterbrach.
static bool prv_lauf_nachholen(const Lauf *lauf, time_t jetzt) {
  if (lauf->id == ALARM_INVALID_ID || lauf->geklungen || !prv_im_fenster(lauf->weckzeit, jetzt)) {
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

// Ein Fensterbeginn, den die Uhr aus war: Pebbles scharfer Wecker ist faellig gewesen.
static bool prv_scharf_nachholen(AlarmId id, time_t cron_utc, const Lauf *lauf, time_t jetzt) {
  // Der Normalfall nach einem Boot: Der scharfe Wecker liegt noch vor uns - ohne Dateizugriff.
  if (id == ALARM_INVALID_ID || cron_utc == 0 || jetzt < cron_utc) {
    return false;
  }
  AlarmInfo info;
  if (!alarm_get_info(id, &info) || !info.is_smart || !info.enabled) {
    return false;  // ein gewoehnlicher Wecker bleibt bei Pebbles 5 min
  }
  const time_t weckzeit = cron_utc + SMART_ALARM_RANGE_S;  // Pebbles Cron startet bei T-30
  // Schon geklungen fuer diese Weckzeit ginge nur nach einem Uhrzeitsprung zurueck.
  return prv_im_fenster(weckzeit, jetzt) &&
         !(lauf->id == id && lauf->weckzeit == (uint32_t)weckzeit && lauf->geklungen);
}

void boulder_wecklauf_boot(AlarmId scharf_id, time_t scharf_utc) {
  pbl_mutex_lock(&s_sperre, PBL_FOREVER);
  s_scharf_id = scharf_id;
  s_scharf_utc = scharf_utc;
  s_boot_offen = true;
  s_lauf = (Lauf){.id = ALARM_INVALID_ID};
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
  // alarm_get_info nimmt die Sperre von alarm.c: erst nach der eigenen, nie darunter.
  if (!offen || *id != ALARM_INVALID_ID) {
    return;
  }
  const time_t jetzt = rtc_get_time();
  AlarmId nachholen = ALARM_INVALID_ID;
  if (prv_lauf_nachholen(&lauf, jetzt)) {
    nachholen = lauf.id;
  } else if (prv_scharf_nachholen(scharf_id, scharf_utc, &lauf, jetzt)) {
    nachholen = scharf_id;
  }
  if (nachholen == ALARM_INVALID_ID) {
    return;
  }
  PBL_LOG_INFO("Boulder: Smart Alarm %d nach dem Neustart weiter (Lauf %d, scharf %d)",
               nachholen, (int)lauf.id, scharf_id);
  *id = nachholen;
  // Pebble prueft beim Einschalten, ob der verpasste Wecker hoechstens 5 min her ist: ab jetzt.
  *zeit = jetzt;
}

bool boulder_wecklauf_fragen(AlarmId id, int zaehler, int stunde, int minute) {
  const time_t jetzt = rtc_get_time();
  pbl_mutex_lock(&s_sperre, PBL_FOREVER);
  // Die Weckzeit steht ab der ersten Frage eines Laufs (Zaehler 0) fest, in UTC: Springt die
  // Ortszeit im Lauf, endet er wie Pebbles Zaehler. Ein nachgeholter Lauf behaelt die Weckzeit
  // von vor dem Neustart.
  const bool neu = s_lauf.id != id ||
                   (zaehler == 0 && !(s_lauf.stunde == stunde && s_lauf.minute == minute &&
                                      prv_im_fenster(s_lauf.weckzeit, jetzt)));
  if (neu) {
    s_lauf = (Lauf){
      .fassung = FASSUNG, .id = id, .stunde = stunde, .minute = minute,
      .weckzeit = boulder_wecker_weckzeit(jetzt, stunde, minute),
    };
    prv_schreiben();
    PBL_LOG_INFO("Boulder: Smart Alarm %d laeuft, Weckzeit %" PRIu32, id, s_lauf.weckzeit);
  }
  const time_t weckzeit = s_lauf.weckzeit;
  pbl_mutex_unlock(&s_sperre);
  return boulder_wecker_erlaubt(weckzeit);
}

void boulder_wecklauf_geklungen(AlarmId id) {
  pbl_mutex_lock(&s_sperre, PBL_FOREVER);
  if (s_lauf.id == id && !s_lauf.geklungen) {
    s_lauf.geklungen = 1;
    prv_schreiben();
    PBL_LOG_INFO("Boulder: Smart Alarm %d geklungen, Weckzeit %" PRIu32, id, s_lauf.weckzeit);
  }
  pbl_mutex_unlock(&s_sperre);
}
