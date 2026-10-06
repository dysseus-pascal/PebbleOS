/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2: die Nacht nach aussen (ausgabe.h). Die Saetze baut sie wie b26 (M: nacht.c
// prv_schicke, prv_schicke_hrv, prv_bild_merken), Byte fuer Byte gegen die Referenz geprueft
// (test_boulder_zyklen, ausgabe_wie_b26).

#include "pbl/services/activity/boulder/ausgabe.h"

#include "applib/data_logging.h"
#include <pbl/drivers/rtc.h>
#include "pbl/kernel/mutex.h"
#include "pbl/services/activity/boulder/messstufe.h"
#include "pbl/services/activity/boulder/nachlauf.h"
#include "pbl/services/activity/boulder/speicher.h"
#include "pbl/services/activity/boulder/takt.h"
#include "pbl/services/alarms/alarm.h"
#include "pbl/services/battery/battery_state.h"
#include "pbl/services/data_logging/data_logging_service.h"
#include "pbl/services/settings/settings_file.h"
#include "pbl/services/time.h"
#include "pbl/util/units.h"
#include "pbl/util/uuid.h"
#include <pbl/logging/logging.h>

#include <string.h>

PBL_LOG_MODULE_DECLARE(service_activity, CONFIG_SERVICE_ACTIVITY_LOG_LEVEL);

// Scheitert dls_create (Pool voll: 20 Sitzungen, 640 KiB, dls_private.h:49, :52), erst nach
// so langer Pause wieder - nicht jeden Takt ein Versuch, der bis 4 s warten kann.
#define ANLAGE_PAUSE_S (30 * PBL_SEC_PER_MIN)
// Ruhezeit um die naechste Weckzeit T: [T - 30 min, T + 5 min] keine Anlage (2.4, Regel 6).
// T wird hoechstens stuendlich gelesen, nicht jede Minute.
#define RUHE_VOR_S (30 * PBL_SEC_PER_MIN)
#define RUHE_NACH_S (5 * PBL_SEC_PER_MIN)
#define WECKZEIT_LESEN_S PBL_SEC_PER_HOUR

// Die letzte Nacht in der Datei von b25/b26 (M: nacht.c:163-164): mit dieser Groesse
// geoeffnet, sonst schriebe settings_file sie um oder scheiterte beim Schreiben.
#define BILD_DATEI "boulder_nacht"
#define BILD_DATEI_GROESSE 2048
static const char BILD_SCHLUESSEL[] = "letzte";

// EIN SATZPUFFER fuer alles, was gebaut und sofort weggegeben wird (Spezifikation 7.2): ein
// Teil 0x4001 oder 0x4002 oder das NachtBild, je innerhalb eines Callbacks.
static union {
  NachtDlsRecord nacht;
  NachtHrvDlsRecord hrv;
  NachtBild bild;
} s_puffer;

static BoulderProtokoll s_protokoll;
static BoulderGesendet s_gesendet;

// Der laufende Auftrag: eine Nacht in Teilen, erst 0x4001, dann 0x4002.
typedef struct {
  bool offen;               //< angefangen, noch nicht alle Teile im Datalogging
  uint32_t stand;           //< boulder_nachlauf_stand der Endfassung
  const SchlafErgebnis *e;
  time_t beginn;
  uint8_t teil;             //< naechster Teil, ueber beide Tags gezaehlt
  uint8_t teile_nacht;
  uint8_t teile_hrv;
  uint8_t akku_beginn;
  uint8_t akku_ende;
  uint8_t flags;
  uint8_t messstufe;
} Auftrag;
static Auftrag s_auftrag;

static DataLoggingSession *s_dls_nacht;
static DataLoggingSession *s_dls_hrv;
static time_t s_anlage_frueheste;
static bool s_schub_faellig;
static time_t s_weckzeit;          //< naechste Weckzeit, 0 = keine
static time_t s_weckzeit_gelesen;

// Das NachtBild: geschrieben auf KernelBG, gelesen aus der Health-App.
static PBL_MUTEX_DEFINE(s_bild_sperre);
static NachtBild s_bild;
static bool s_bild_da;
static bool s_bild_gelesen;   //< die Datei wurde seit dem Boot gelesen
static bool s_bild_in_datei;  //< s_bild steht so in der Datei
static bool s_bild_faellig;   //< fertig und noch nicht in der Datei

static void prv_ausgabe_cb(void *data);
static void prv_anlage_cb(void *data);
static BoulderAufgabe s_aufgabe_ausgabe = BOULDER_AUFGABE("Boulder Ausgabe", prv_ausgabe_cb);
static BoulderAufgabe s_aufgabe_anlage = BOULDER_AUFGABE("Boulder Anlage Nacht", prv_anlage_cb);

// ---------------------------------------------------------------------------------------
// Protokoll

void boulder_ausgabe_protokoll(time_t fenster, int takt, uint8_t akku, uint8_t stufe,
                               bool am_kabel, bool schlaf_da) {
  if (s_protokoll.fenster != fenster) {
    s_protokoll.fenster = fenster;
    memset(s_protokoll.akku, AUSGABE_UNBEKANNT, sizeof(s_protokoll.akku));
    memset(s_protokoll.stufe, AUSGABE_UNBEKANNT, sizeof(s_protokoll.stufe));
    s_protokoll.geladen = false;
  }
  if (takt >= 0 && takt < AUSGABE_PROTOKOLL_TAKTE) {
    s_protokoll.akku[takt] = akku;
    s_protokoll.stufe[takt] = stufe;
  }
  s_protokoll.geladen |= am_kabel && schlaf_da;
}

BoulderProtokoll *boulder_ausgabe_protokoll_daten(void) {
  return &s_protokoll;
}

// Der Wert zum ersten Takt ab `utc` (b26 prv_bei); AUSGABE_UNBEKANNT ohne.
static uint8_t prv_bei(const uint8_t *protokoll, time_t utc) {
  const int takt_s = AUSGABE_PROTOKOLL_TAKT * PBL_SEC_PER_MIN;
  for (int i = (int)((utc - s_protokoll.fenster + takt_s - 1) / takt_s);
       i >= 0 && i < AUSGABE_PROTOKOLL_TAKTE; i++) {
    if (protokoll[i] != AUSGABE_UNBEKANNT) {
      return protokoll[i];
    }
  }
  return AUSGABE_UNBEKANNT;
}

void boulder_ausgabe_endgueltig(void) {
  s_protokoll.geladen = false;
}

// ---------------------------------------------------------------------------------------
// Saetze

bool boulder_ausgabe_noetig(time_t beginn, uint16_t aufwachen, uint16_t schlaf) {
  return (uint32_t)beginn != s_gesendet.beginn || aufwachen != s_gesendet.aufwachen ||
         schlaf != s_gesendet.schlaf;
}

bool boulder_ausgabe_offen(void) {
  return s_auftrag.offen;
}

void boulder_ausgabe_verwerfen(void) {
  s_auftrag.offen = false;
}

void boulder_ausgabe_nacht(time_t beginn, const SchlafErgebnis *e) {
  uint8_t hrv_anzahl = 0;
  (void)boulder_nachlauf_hrv(&hrv_anzahl);
  const time_t eingeschlafen = beginn + (time_t)e->einschlafen * PBL_SEC_PER_MIN;
  const uint8_t stufe = prv_bei(s_protokoll.stufe, eingeschlafen);
  s_auftrag = (Auftrag){
    .offen = true,
    .stand = boulder_nachlauf_stand(),
    .e = e,
    .beginn = beginn,
    .teil = 0,
    .teile_nacht = (uint8_t)((e->abschnitt_anzahl + NACHT_DLS_ABSCHNITTE - 1) /
                             NACHT_DLS_ABSCHNITTE),
    .teile_hrv = (uint8_t)((hrv_anzahl + NACHT_HRV_DLS_FENSTER - 1) / NACHT_HRV_DLS_FENSTER),
    .akku_beginn = prv_bei(s_protokoll.akku, eingeschlafen),
    .akku_ende = battery_get_charge_state().charge_percent,
    .flags = s_protokoll.geladen ? NACHT_DLS_FLAG_GELADEN : 0,
    .messstufe = (stufe != AUSGABE_UNBEKANNT) ? stufe : boulder_messstufe(),
  };
  // b26: ohne Abschnitte trotzdem ein Satz (teile 0).
  if (s_auftrag.teile_nacht == 0) {
    s_auftrag.teile_nacht = 1;
  }
  PBL_LOG_INFO("Boulder: Nacht senden, %d min Schlaf, %d+%d Teile", e->schlaf_minuten,
               s_auftrag.teile_nacht, s_auftrag.teile_hrv);
  boulder_aufgabe_einreihen(&s_aufgabe_ausgabe);
}

static void prv_baue_nacht(int teil) {
  const SchlafErgebnis *e = s_auftrag.e;
  NachtDlsRecord *s = &s_puffer.nacht;
  memset(s, 0, sizeof(*s));
  *s = (NachtDlsRecord){
    .version = NACHT_DLS_VERSION,
    .size = sizeof(NachtDlsRecord),
    .beginn_utc = (uint32_t)s_auftrag.beginn,
    .utc_to_local = (int32_t)(time_utc_to_local(s_auftrag.beginn) - s_auftrag.beginn),
    .einschlafen = e->einschlafen,
    .aufwachen = e->aufwachen,
    .schlaf_minuten = e->schlaf_minuten,
    .ruhepuls = e->ruhepuls,
    .teil = (uint8_t)teil,
    // b26 schreibt die rechnerische Zahl (0 ohne Abschnitte), nicht die gesendete.
    .teile = (uint8_t)((e->abschnitt_anzahl + NACHT_DLS_ABSCHNITTE - 1) / NACHT_DLS_ABSCHNITTE),
    .hrv = e->hrv,
    .akku_beginn = s_auftrag.akku_beginn,
    .akku_ende = s_auftrag.akku_ende,
    .flags = s_auftrag.flags,
    .messstufe = s_auftrag.messstufe,
  };
  for (int k = teil * NACHT_DLS_ABSCHNITTE;
       k < e->abschnitt_anzahl && s->anzahl < NACHT_DLS_ABSCHNITTE; k++) {
    s->abschnitte[s->anzahl++] = (NachtDlsAbschnitt){
      .von = e->abschnitte[k].von,
      .bis = e->abschnitte[k].bis,
      .phase = e->abschnitte[k].phase,
    };
  }
}

static void prv_baue_hrv(int teil) {
  uint8_t anzahl = 0;
  const SchlafHrv *hrv = boulder_nachlauf_hrv(&anzahl);
  NachtHrvDlsRecord *s = &s_puffer.hrv;
  memset(s, 0, sizeof(*s));
  *s = (NachtHrvDlsRecord){
    .version = NACHT_HRV_DLS_VERSION,
    .size = sizeof(NachtHrvDlsRecord),
    .beginn_utc = (uint32_t)s_auftrag.beginn,
    .teil = (uint8_t)teil,
    .teile = s_auftrag.teile_hrv,
  };
  for (int k = teil * NACHT_HRV_DLS_FENSTER; k < anzahl && s->anzahl < NACHT_HRV_DLS_FENSTER;
       k++) {
    s->fenster[s->anzahl++] = (NachtHrvDlsFenster){
      .minute = hrv[k].minute,
      .rmssd = hrv[k].rmssd,
      .puls = hrv[k].puls,
    };
  }
}

static bool prv_ruhezeit(time_t jetzt) {
  if (!s_weckzeit_gelesen || jetzt >= s_weckzeit_gelesen + WECKZEIT_LESEN_S ||
      jetzt < s_weckzeit_gelesen) {
    s_weckzeit_gelesen = jetzt;
    if (!alarm_get_next_enabled_alarm(&s_weckzeit)) {
      s_weckzeit = 0;
    }
  }
  return s_weckzeit && jetzt >= s_weckzeit - RUHE_VOR_S && jetzt <= s_weckzeit + RUHE_NACH_S;
}

static void prv_anlage_anstossen(void) {
  const time_t jetzt = rtc_get_time();
  if (jetzt < s_anlage_frueheste || prv_ruhezeit(jetzt)) {
    return;  // der naechste Takt des Lebenslaufs versucht es wieder (Auftrag offen)
  }
  boulder_aufgabe_einreihen(&s_aufgabe_anlage);
}

// "Anlage": NUR dls_create, ohne gehaltene Sperre. resume = false wie b26: eine liegende
// Sitzung dieses Tags von vor dem Boot wird beendet und geht ans Telefon.
static void prv_anlage_cb(void *data) {
  boulder_aufgabe_laeuft(&s_aufgabe_anlage);
  if (!s_auftrag.offen) {
    return;
  }
  Uuid system_uuid = UUID_SYSTEM;
  if (!s_dls_nacht) {
    s_dls_nacht = dls_create(BOULDER_DLS_TAG_NACHT, DATA_LOGGING_BYTE_ARRAY,
                             sizeof(NachtDlsRecord), false /*buffered*/, false /*resume*/,
                             &system_uuid);
  }
  if (s_dls_nacht && !s_dls_hrv && s_auftrag.teile_hrv > 0) {
    s_dls_hrv = dls_create(BOULDER_DLS_TAG_NACHT_HRV, DATA_LOGGING_BYTE_ARRAY,
                           sizeof(NachtHrvDlsRecord), false /*buffered*/, false /*resume*/,
                           &system_uuid);
  }
  if (!s_dls_nacht || (s_auftrag.teile_hrv > 0 && !s_dls_hrv)) {
    s_anlage_frueheste = rtc_get_time() + ANLAGE_PAUSE_S;
    PBL_LOG_WRN("Boulder: keine Datalogging-Sitzung fuer die Nacht, in 30 min wieder");
    return;
  }
  boulder_aufgabe_einreihen(&s_aufgabe_ausgabe);
}

// "Ausgabe": ein Teil je Lauf.
static void prv_ausgabe_cb(void *data) {
  boulder_aufgabe_laeuft(&s_aufgabe_ausgabe);
  if (!s_auftrag.offen) {
    return;
  }
  if (boulder_nachlauf_stand() != s_auftrag.stand) {
    // Die Endfassung ist ueberschrieben; der Lebenslauf schickt die Nacht neu (offen bleibt).
    PBL_LOG_DBG("Boulder: Ausgabe abgebrochen, neu gerechnet");
    return;
  }
  const bool hrv = s_auftrag.teil >= s_auftrag.teile_nacht;
  DataLoggingSession **sitzung = hrv ? &s_dls_hrv : &s_dls_nacht;
  if (!*sitzung) {
    prv_anlage_anstossen();
    return;
  }
  if (hrv) {
    prv_baue_hrv(s_auftrag.teil - s_auftrag.teile_nacht);
  } else {
    prv_baue_nacht(s_auftrag.teil);
  }
  const DataLoggingResult r = dls_log(*sitzung, &s_puffer, 1);
  if (r != DATA_LOGGING_SUCCESS) {
    PBL_LOG_WRN("Boulder: Datalogging-Fehler %d bei Teil %d", (int)r, s_auftrag.teil);
    if (r == DATA_LOGGING_CLOSED || r == DATA_LOGGING_NOT_FOUND) {
      *sitzung = NULL;  // beim naechsten Versuch neu anlegen
    }
    return;
  }
  s_auftrag.teil++;
  if (s_auftrag.teil < s_auftrag.teile_nacht + s_auftrag.teile_hrv) {
    boulder_aufgabe_einreihen(&s_aufgabe_ausgabe);
    return;
  }
  s_auftrag.offen = false;
  s_gesendet = (BoulderGesendet){
    .beginn = (uint32_t)s_auftrag.beginn,
    .aufwachen = s_auftrag.e->aufwachen,
    .schlaf = s_auftrag.e->schlaf_minuten,
  };
  s_schub_faellig = true;
  boulder_speicher_anstossen(true);
}

void boulder_ausgabe_takt(time_t jetzt) {
  (void)jetzt;
  // Der Schub erst im naechsten Takt, nicht im Lauf, der geloggt hat (b26: SCHICKEN_NACH_MS).
  // dls_send_all_sessions reiht einen nicht verwerfbaren Callback ein (dls_main.c:343-350).
  if (!s_schub_faellig || system_task_get_available_space() < BOULDER_SCHLANGE_FREI) {
    return;
  }
  s_schub_faellig = false;
  dls_send_all_sessions();
}

BoulderGesendet *boulder_ausgabe_gesendet(void) {
  return &s_gesendet;
}

// ---------------------------------------------------------------------------------------
// NachtBild

static void prv_bild_laden(void) {
  if (s_bild_da || s_bild_gelesen) {
    return;
  }
  s_bild_gelesen = true;
  SettingsFile datei;
  if (settings_file_open(&datei, BILD_DATEI, BILD_DATEI_GROESSE) != S_SUCCESS) {
    PBL_LOG_DBG("Boulder: keine Datei %s", BILD_DATEI);
    return;
  }
  if (settings_file_get_len(&datei, BILD_SCHLUESSEL, sizeof(BILD_SCHLUESSEL)) ==
          (int)sizeof(s_bild) &&
      settings_file_get(&datei, BILD_SCHLUESSEL, sizeof(BILD_SCHLUESSEL), &s_bild,
                        sizeof(s_bild)) == S_SUCCESS) {
    if (s_bild.version == NACHT_BILD_VERSION) {
      s_bild_da = true;
      s_bild_in_datei = true;
    } else if (s_bild.version == NACHT_BILD_VERSION_B24) {
      // AP8, Wechsel (Spezifikation 8): ein Bild von b24 hat denselben Aufbau, nur die Fassung
      // stand in einem Fuellbyte. Ohne es fehlte der Health-App die letzte Nacht, und der
      // Umzug faende nicht, was schon geschickt ist (wie b25, M: nacht.c:493-520). In der
      // Datei bleibt es b24, bis die naechste fertige Nacht es in Fassung 1 schreibt.
      s_bild.version = NACHT_BILD_VERSION;
      s_bild_da = true;
      s_bild_in_datei = false;
      PBL_LOG_INFO("Boulder: NachtBild von b24 uebernommen");
    } else {
      PBL_LOG_WRN("Boulder: NachtBild Fassung %d nicht gelesen", (int)s_bild.version);
      memset(&s_bild, 0, sizeof(s_bild));
    }
  }
  settings_file_close(&datei);
}

bool boulder_nacht_letzte(NachtBild *aus) {
  pbl_mutex_lock(&s_bild_sperre, PBL_FOREVER);
  prv_bild_laden();
  const bool da = s_bild_da;
  if (da) {
    *aus = s_bild;
  }
  pbl_mutex_unlock(&s_bild_sperre);
  return da;
}

bool boulder_ausgabe_bild(time_t beginn, const SchlafErgebnis *e, bool fertig) {
  if (!e->gefunden) {
    return false;
  }
  // Verdichtet in der freien Arbeitsliste des Nachlaufs (b26: r->arbeit.liste).
  SchlafAbschnitt *a = boulder_nachlauf_ablage();
  memcpy(a, e->abschnitte, e->abschnitt_anzahl * sizeof(*a));
  const int n = schlafanalyse_verdichten(a, e->abschnitt_anzahl, NACHT_BILD_ABSCHNITTE);

  // Ganz genullt, auch die Fuellbytes: verglichen wird Byte fuer Byte.
  NachtBild *b = &s_puffer.bild;
  memset(b, 0, sizeof(*b));
  b->beginn_utc = (uint32_t)beginn;
  b->utc_to_local = (int32_t)(time_utc_to_local(beginn) - beginn);
  b->einschlafen = e->einschlafen;
  b->aufwachen = e->aufwachen;
  b->schlaf_minuten = e->schlaf_minuten;
  b->hrv = e->hrv;
  b->ruhepuls = e->ruhepuls;
  b->fertig = fertig;
  b->anzahl = (uint8_t)n;
  b->version = NACHT_BILD_VERSION;
  // Die Minuten je Phase aus der ungekuerzten Liste - die verdichtete verschoebe sie.
  for (int k = 0; k < e->abschnitt_anzahl; k++) {
    if (e->abschnitte[k].phase < NACHT_PHASEN) {
      b->minuten[e->abschnitte[k].phase] += e->abschnitte[k].bis - e->abschnitte[k].von;
    }
  }
  for (int k = 0; k < n; k++) {
    b->abschnitte[k] = (NachtDlsAbschnitt){ .von = a[k].von, .bis = a[k].bis, .phase = a[k].phase };
  }
  pbl_mutex_lock(&s_bild_sperre, PBL_FOREVER);
  const bool gleich = s_bild_da && memcmp(b, &s_bild, sizeof(*b)) == 0;
  if (!gleich) {
    s_bild = *b;
    s_bild_da = true;
    s_bild_in_datei = false;
  }
  s_bild_faellig = fertig && !s_bild_in_datei;
  pbl_mutex_unlock(&s_bild_sperre);
  return s_bild_faellig;
}

// NUR WAS SICH GEAENDERT HAT, und ausserhalb der Sperre: die Health-App soll nicht auf den
// Flash warten (b26).
void boulder_ausgabe_bild_ablegen(void) {
  if (!s_bild_faellig) {
    return;
  }
  pbl_mutex_lock(&s_bild_sperre, PBL_FOREVER);
  NachtBild *b = &s_puffer.bild;
  *b = s_bild;
  pbl_mutex_unlock(&s_bild_sperre);
  SettingsFile datei;
  bool geschrieben = false;
  if (settings_file_open(&datei, BILD_DATEI, BILD_DATEI_GROESSE) == S_SUCCESS) {
    geschrieben = settings_file_set(&datei, BILD_SCHLUESSEL, sizeof(BILD_SCHLUESSEL), b,
                                    sizeof(*b)) == S_SUCCESS;
    settings_file_close(&datei);
  }
  if (!geschrieben) {
    PBL_LOG_WRN("Boulder: NachtBild nicht gespeichert");
    return;
  }
  pbl_mutex_lock(&s_bild_sperre, PBL_FOREVER);
  s_bild_in_datei = memcmp(b, &s_bild, sizeof(*b)) == 0;
  s_bild_faellig = !s_bild_in_datei;
  pbl_mutex_unlock(&s_bild_sperre);
}

void boulder_ausgabe_test_neustart(void) {
  memset(&s_protokoll, 0, sizeof(s_protokoll));
  memset(&s_gesendet, 0, sizeof(s_gesendet));
  memset(&s_auftrag, 0, sizeof(s_auftrag));
  memset(&s_bild, 0, sizeof(s_bild));
  s_dls_nacht = NULL;
  s_dls_hrv = NULL;
  s_anlage_frueheste = 0;
  s_schub_faellig = false;
  s_weckzeit = 0;
  s_weckzeit_gelesen = 0;
  s_bild_da = false;
  s_bild_gelesen = false;
  s_bild_in_datei = false;
  s_bild_faellig = false;
  s_aufgabe_ausgabe.steht_an = false;
  s_aufgabe_anlage.steht_an = false;
}
