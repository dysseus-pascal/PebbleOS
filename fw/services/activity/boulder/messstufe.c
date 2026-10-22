/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2: die Messstufe (messstufe.h, Spezifikation 5.1/5.5, F3).
//
// WOHER DIE STUFE KOMMT: Boulder schreibt "boulderMessstufe" per BlobDB in shellpref
// (Whitelist E3). Pebbles Pref-Tabelle kennt den Schluessel nicht und uebergeht ihn still
// (prefs.c, prefs_private_handle_blob_db_event) - die Variante liest ihn selbst. Lesen
// oeffnet eine Datei und holt Heap, deshalb nur im eigenen Callback "Boulder Messstufe":
// nach dem ersten Minutenlauf, nach jedem passenden BlobDB-Ereignis und zur Sicherheit in
// jeder Ablage (AP4). Den Schalter "Schlaf" fuer SpO2 schreibt Boulder auf demselben Weg; er
// wird mit der Stufe gelesen.
//
// WARUM DAS ABO AUF KERNELMAIN: blob_db_insert meldet PEBBLE_BLOBDB_EVENT auf KernelMain
// (blob_db/api.c, blob_db_event_put); event_service_client_subscribe endet auf KernelBG in
// WTF (event_service_client.c, prv_get_state). Der Rueckruf setzt nur einen Merker und
// reiht verwerfbar ein.

#include "pbl/services/activity/boulder/messstufe.h"

#include "applib/event_service_client.h"
#include "kernel/event_loop.h"
#include "kernel/events.h"
#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/boulder/puls.h"
#include "pbl/services/activity/boulder/takt.h"
#include "pbl/services/blob_db/api.h"
#include "pbl/services/settings/settings_file.h"
#include "shell/prefs_private.h"
#include "system/status_codes.h"
#include <pbl/logging/logging.h>

#include <string.h>

PBL_LOG_MODULE_DECLARE(service_activity, CONFIG_SERVICE_ACTIVITY_LOG_LEVEL);

// Stufe 1 zuerst. Unterschied zu b25 (M: messstufe.c:19): Stufe 1 tags 10 statt 30 min -
// sie ist jetzt Pebbles Original (Kriterium 6).
// STUFE 4 NACHTS ALLE DREI MINUTEN (F38): So oft kam der Wert unter b26 tatsaechlich. b26 gab den
// Takt an Pebbles eigene Messung, die ihn erst ab dem Ausschalten zaehlt; das eigene Abo haelt ihn
// genau. Die REM-Regel (schlafanalyse.c, REM_NAH) ist an Naechten mit diesem Takt eingestellt: Mit
// einem Wert alle zwei Minuten sah sie am 16. und 17.10. 168 und 141 min REM, Fitbit 42 und 76;
// auf drei Minuten ausgeduennt 93 und 81.
static const BoulderStufe s_stufen[MESSSTUFE_MAX] = {
  { .puls_nacht_s = 10 * 60, .puls_tag_min = 10, .hrv_takt_min = 0, .spo2_min = 0 },
  { .puls_nacht_s = 10 * 60, .puls_tag_min = 10, .hrv_takt_min = 60, .spo2_min = 60 },
  { .puls_nacht_s = 5 * 60, .puls_tag_min = 10, .hrv_takt_min = 30, .spo2_min = 30 },
  { .puls_nacht_s = 3 * 60, .puls_tag_min = 10, .hrv_takt_min = 15, .spo2_min = 10 },
  { .puls_nacht_s = 1 * 60, .puls_tag_min = 5, .hrv_takt_min = 10, .spo2_min = 10 },
};

static uint8_t s_stufe;       //< geltende Stufe, 0 = noch nicht gelesen
static bool s_gestartet;      //< Abo auf KernelMain angestossen
static bool s_lesen;          //< neu lesen (Ereignis, Menue, Ablage)
static bool s_pebble_offen;   //< Pebbles Werte noch nicht ganz gesetzt (kein Platz)

// Der Schalter "Schlaf" in einem Byte: Das Menue (App-Task) setzt ihn vorab, KernelBG und der
// HRM-Manager lesen ihn, und keiner sieht ihn halb geschrieben.
#define SCHLAF_UNGELESEN 0
#define SCHLAF_AUS       1
#define SCHLAF_AN        2
static uint8_t s_schlaf;

static void prv_messstufe_cb(void *data);
static BoulderAufgabe s_aufgabe = BOULDER_AUFGABE("Boulder Messstufe", prv_messstufe_cb);

static bool prv_gueltig(uint8_t stufe) {
  return stufe >= MESSSTUFE_MIN && stufe <= MESSSTUFE_MAX;
}

const BoulderStufe *boulder_messstufe_werte(uint8_t stufe) {
  return &s_stufen[(prv_gueltig(stufe) ? stufe : MESSSTUFE_VORGABE) - MESSSTUFE_MIN];
}

// EINE LISTE fuer beide Orte, an denen die Stufe steht (Settings -> Health und die Health-App):
// Die Namen liegen im Flash (messstufe.h). Pebbles Auswahlmenue verlangt ein Feld ohne const und
// liest es nur (option_menu.c, i18n_get).
const char **boulder_messstufe_namen(void) {
  return (const char **)BOULDER_MESSSTUFE_NAMEN;
}

// Fehlt der Wert oder ist er ungueltig, gilt die Vorgabe 3 (b25, M: messstufe.h:39).
static uint8_t prv_lesen(void) {
  uint8_t wert = 0;
  const status_t st = blob_db_read(BlobDBIdSettings, (const uint8_t *)BOULDER_MESSSTUFE_SCHLUESSEL,
                                   strlen(BOULDER_MESSSTUFE_SCHLUESSEL), &wert, sizeof(wert));
  if (st != S_SUCCESS) {
    if (st != E_DOES_NOT_EXIST) {
      PBL_LOG_WRN("Boulder: Messstufe nicht lesbar (%d), gilt %d", (int)st, MESSSTUFE_VORGABE);
    }
    return MESSSTUFE_VORGABE;
  }
  if (!prv_gueltig(wert)) {
    PBL_LOG_WRN("Boulder: Messstufe %d ungueltig, gilt %d", wert, MESSSTUFE_VORGABE);
    return MESSSTUFE_VORGABE;
  }
  return wert;
}

// Aus, solange Boulder nichts geschrieben hat: Wer den Schalter nicht kennt, misst wie bisher.
static bool prv_schlaf_lesen(void) {
  uint8_t wert = 0;
  const status_t st =
      blob_db_read(BlobDBIdSettings, (const uint8_t *)BOULDER_SPO2_SCHLAF_SCHLUESSEL,
                   strlen(BOULDER_SPO2_SCHLAF_SCHLUESSEL), &wert, sizeof(wert));
  if (st != S_SUCCESS) {
    if (st != E_DOES_NOT_EXIST) {
      PBL_LOG_WRN("Boulder: SpO2 im Schlaf nicht lesbar (%d), gilt aus", (int)st);
    }
    return false;
  }
  if (wert > 1) {
    PBL_LOG_WRN("Boulder: SpO2 im Schlaf %d ungueltig, gilt aus", wert);
    return false;
  }
  return wert == 1;
}

#ifdef CONFIG_HRM
static bool prv_platz(void) {
  return system_task_get_available_space() >= BOULDER_SCHLANGE_FREI;
}

static HRMonitoringInterval prv_intervall(uint8_t minuten) {
  switch (minuten) {
    case 5:
      return HRMonitoringInterval_5Min;
    case 30:
      return HRMonitoringInterval_30Min;
    case 60:
      return HRMonitoringInterval_1Hour;
    default:
      return HRMonitoringInterval_10Min;
  }
}

// F3: DIE UHR SETZT PEBBLES EIGENE WERTE nach der Stufe - Puls-Takt, SpO2 an/aus und
// SpO2-Takt. Pebble misst dann selbst (activity.c); die Werte kommen ueber den Minutensatz.
// Jeder Setter schreibt shellpref (geht ans Telefon) und reiht nicht verwerfbar ein
// (hrm_manager_handle_prefs_changed): nur bei Platz, nur wenn anders. Eine eigene Wahl
// des Nutzers ueberschreibt der Regler beim naechsten Lesen. Rueckgabe: alles gesetzt.
static bool prv_pebble_werte_setzen(void) {
  const BoulderStufe *w = boulder_messstufe_werte(s_stufe);
  const HRMonitoringInterval puls = prv_intervall(w->puls_tag_min);
  if (activity_prefs_get_hrm_measurement_interval() != puls) {
    if (!prv_platz()) {
      return false;
    }
    activity_prefs_set_hrm_measurement_interval(puls);
  }
  const bool spo2_an = (w->spo2_min != 0);
  // Erst der Takt, dann an: so misst Pebble nie mit dem alten Takt.
  if (spo2_an && activity_prefs_get_spo2_measurement_interval() != prv_intervall(w->spo2_min)) {
    if (!prv_platz()) {
      return false;
    }
    activity_prefs_set_spo2_measurement_interval(prv_intervall(w->spo2_min));
  }
  if (activity_prefs_blood_oxygen_is_enabled() != spo2_an) {
    if (!prv_platz()) {
      return false;
    }
    activity_prefs_set_blood_oxygen_enabled(spo2_an);
  }
  return true;
}
#else
static bool prv_pebble_werte_setzen(void) {
  return true;
}
#endif

static bool prv_schluessel(const PebbleBlobDBEvent *b, const char *name) {
  const int n = (int)strlen(name);
  return (b->key_len == n || b->key_len == n + 1) && memcmp(b->key, name, n) == 0;
}

// KernelMain. Auch Pebbles eigene Puls-/SpO2-Einstellungen: schreibt das Telefon sie
// (0.50 legt sie ab und kann sie zurueckschreiben), zieht der Regler sie wieder auf die
// Stufe. Die Schluessel stehen in prefs.c (PREF_KEY_ACTIVITY_HRM_PREFERENCES usw.).
static void prv_blobdb_ereignis(PebbleEvent *e, void *context) {
  const PebbleBlobDBEvent *b = &e->blob_db;
  if (b->db_id != BlobDBIdSettings || b->type != BlobDBEventTypeInsert || !b->key) {
    return;
  }
  if (prv_schluessel(b, BOULDER_MESSSTUFE_SCHLUESSEL) ||
      prv_schluessel(b, BOULDER_SPO2_SCHLAF_SCHLUESSEL) || prv_schluessel(b, "hrmPreferences") ||
      prv_schluessel(b, "spo2Preferences") || prv_schluessel(b, "bloodOxygenPreferences")) {
    s_lesen = true;
    boulder_aufgabe_einreihen(&s_aufgabe);
  }
}

static void prv_abo_anmelden(void *data) {
  static EventServiceInfo s_info = {
    .type = PEBBLE_BLOBDB_EVENT,
    .handler = prv_blobdb_ereignis,
  };
  event_service_client_subscribe(&s_info);
}

static void prv_messstufe_cb(void *data) {
  BOULDER_AUFGABE_LAEUFT(&s_aufgabe);
  if (!s_gestartet) {
    s_gestartet = true;
    s_lesen = true;
    launcher_task_add_callback(prv_abo_anmelden, NULL);
  }
  if (s_lesen) {
    s_lesen = false;
    const uint8_t neu = prv_lesen();
    if (neu != s_stufe) {
      PBL_LOG_INFO("Boulder: Messstufe %d", neu);
    }
    // Immer: das Menue setzt s_stufe schon vorab fuer die Anzeige.
    s_stufe = neu;
    const uint8_t schlaf = prv_schlaf_lesen() ? SCHLAF_AN : SCHLAF_AUS;
    if (schlaf != s_schlaf) {
      PBL_LOG_INFO("Boulder: SpO2 im Schlaf %s", (schlaf == SCHLAF_AN) ? "an" : "aus");
    }
    s_schlaf = schlaf;
    boulder_puls_stufe_neu();
    s_pebble_offen = true;
  }
  if (s_pebble_offen) {
    s_pebble_offen = !prv_pebble_werte_setzen();
  }
}

uint8_t boulder_messstufe_geladen(void) {
  return s_stufe;
}

void boulder_messstufe_minute(void) {
  if (!s_gestartet || s_lesen || s_pebble_offen) {
    boulder_aufgabe_einreihen(&s_aufgabe);
  }
}

void boulder_messstufe_nachlesen(void) {
  s_lesen = true;
  boulder_aufgabe_einreihen(&s_aufgabe);
}

uint8_t boulder_messstufe(void) {
  const uint8_t stufe = s_stufe;
  return prv_gueltig(stufe) ? stufe : prv_lesen();
}

bool boulder_spo2_schlaf_geladen(void) {
  return s_schlaf == SCHLAF_AN;
}

bool boulder_spo2_schlaf(void) {
  const uint8_t schlaf = s_schlaf;
  return (schlaf == SCHLAF_UNGELESEN) ? prv_schlaf_lesen() : (schlaf == SCHLAF_AN);
}

// App-Task (Menue). Geschrieben wie Pebbles eigene Prefs: unter der Prefs-Sperre direkt in
// shellpref, mit Null am Schluessel (settings_blob_db.c, prv_canonical_shell_key). So
// meldet settings_file die Aenderung, und settings_blob_db gibt sie ans Telefon - ein
// blob_db_insert dagegen gaelte als vom Telefon und ginge nicht zurueck.
static status_t prv_schreiben(const char *schluessel, uint8_t wert) {
  prefs_private_lock();
  SettingsFile datei;
  status_t st = settings_file_open(&datei, SHELL_PREFS_FILE_NAME, SHELL_PREFS_FILE_LEN);
  if (st == S_SUCCESS) {
    st = settings_file_set(&datei, schluessel, strlen(schluessel) + 1, &wert, sizeof(wert));
    settings_file_close(&datei);
  }
  prefs_private_unlock();
  return st;
}

void boulder_messstufe_setzen(uint8_t stufe) {
  if (!prv_gueltig(stufe)) {
    PBL_LOG_WRN("Boulder: Messstufe %d abgelehnt", stufe);
    return;
  }
  const status_t st = prv_schreiben(BOULDER_MESSSTUFE_SCHLUESSEL, stufe);
  if (st != S_SUCCESS) {
    PBL_LOG_ERR("Boulder: Messstufe %d nicht gespeichert (%d)", stufe, (int)st);
    return;
  }
  s_stufe = stufe;  // gleich anzeigen; gelesen und angewandt wird im Callback
  s_lesen = true;
  boulder_aufgabe_einreihen(&s_aufgabe);
}

void boulder_spo2_schlaf_setzen(bool an) {
  const status_t st = prv_schreiben(BOULDER_SPO2_SCHLAF_SCHLUESSEL, an ? 1 : 0);
  if (st != S_SUCCESS) {
    PBL_LOG_ERR("Boulder: SpO2 im Schlaf nicht gespeichert (%d)", (int)st);
    return;
  }
  s_schlaf = an ? SCHLAF_AN : SCHLAF_AUS; // gleich anzeigen; gelesen wird im Callback
  s_lesen = true;
  boulder_aufgabe_einreihen(&s_aufgabe);
}

#ifdef UNITTEST
void boulder_messstufe_test_zuruecksetzen(void) {
  s_stufe = 0;
  s_schlaf = SCHLAF_UNGELESEN;
  s_gestartet = false;
  s_lesen = false;
  s_pebble_offen = false;
  s_aufgabe.steht_an = false;
}
#endif
