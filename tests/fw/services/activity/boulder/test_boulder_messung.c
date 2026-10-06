/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2 AP5: Messstufe, Puls-Abo und HRV-Fenster (Spezifikation 9: T6.1-T6.3, F2, F3).
//
// ECHT: hrm_manager.c (mit dem HRV-Fix F2), settings_file.c auf der Flash-Attrappe (die
// Datei shellpref), und die Variante (messstufe.c, puls.c, hrv.c, takt.c). ATTRAPPEN nur
// fuer die Grenzen: der Sensor-Treiber (liefert erst nach 15 s Anlauf gute Werte, wie das
// echte Geraet nach dem Einschwingen), der Zeitgeber des Managers, Pebbles Prefs-Setter
// (zaehlen und reihen ein wie die echten), KernelMain (die BlobDB-Ereignisse gehen an den
// Rueckruf, den die Variante dort anmeldet), und BlobDB vor shellpref: settings_blob_db.c
// braucht verschachtelte Funktionen (gcc), die Host-Tests bauen mit clang. Seine Whitelist
// (E3) prueft tools/boulder/vertraege.py; hier schreibt das "Telefon" den Schluessel so in
// shellpref, wie settings_blob_db_insert es tut (mit Nullbyte, prv_canonical_shell_key).

#include "clar.h"

#include "applib/event_service_client.h"
#include "kernel/events.h"
#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/activity_private.h"
#include "pbl/services/activity/boulder/hrv.h"
#include "pbl/services/activity/boulder/messstufe.h"
#include "pbl/services/activity/boulder/puls.h"
#include "pbl/services/activity/boulder/takt.h"
#include "pbl/services/blob_db/api.h"
#include "pbl/services/filesystem/pfs.h"
#include "pbl/services/hrm/hrm_manager.h"
#include "pbl/services/hrm/hrm_manager_private.h"
#include "pbl/services/new_timer/new_timer.h"
#include "pbl/services/settings/settings_file.h"
#include "pbl/util/size.h"
#include "pbl/util/units.h"
#include "shell/prefs_private.h"
#include <pbl/drivers/hrm.h>
#include <pbl/drivers/rtc.h>

#include "fake_app_manager.h"
#include "fake_events.h"
#include "fake_pbl_malloc.h"
#include "fake_rtc.h"
#include "fake_spi_flash.h"
#include "fake_system_task.h"

#include "stubs_accel_manager.h"
#include "stubs_hexdump.h"
#include "stubs_sleep.h"
#include "stubs_task_wdt.h"
#include "stubs_analytics.h"
#include "stubs_bluetooth_persistent_storage.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_worker_manager.h"

#include <string.h>

// =============================================================================================
// Grenzen

// Sensor-Treiber
static struct {
  bool an;
  HRMFeature merkmale;
  int einschalten;     //< aus -> an
  RtcTicks an_seit;
} s_sensor;

bool hrm_enable(HRMDevice *dev, HRMFeature features, bool low_latency) {
  if (!s_sensor.an) {
    s_sensor.einschalten++;
    s_sensor.an_seit = rtc_get_ticks();
  }
  s_sensor.an = true;
  s_sensor.merkmale = features;
  return true;
}
void hrm_disable(HRMDevice *dev) {
  s_sensor.an = false;
}
bool hrm_is_enabled(HRMDevice *dev) {
  return s_sensor.an;
}
void hrm_set_activity_scene(HRMDevice *dev, HRMActivityScene scene) {
}
bool battery_is_usb_connected(void) {
  return false;
}
int pbl_msgq_put(struct pbl_msgq *q, const void *msg, pbl_timeout_t timeout) {
  return 0;  // Apps bekommen hier nichts; gezaehlt werden die Health-Ereignisse (F2)
}
struct pbl_msgq *pebble_task_get_to_queue(PebbleTask task) {
  return (task == PebbleTask_App) ? (struct pbl_msgq *)0x1 : NULL;
}

// Der Zeitgeber des Managers: feuert, wenn die simulierte Zeit ihn erreicht.
static struct {
  bool an;
  uint64_t faellig_ms;
  NewTimerCallback cb;
  void *daten;
} s_wecker;
static uint64_t prv_ms(void) {
  return rtc_get_ticks() * 1000 / RTC_TICKS_HZ;
}
TimerID new_timer_create(void) {
  return 1;
}
bool new_timer_start(TimerID t, uint32_t ms, NewTimerCallback cb, void *daten, uint32_t flags) {
  s_wecker.an = true;
  s_wecker.faellig_ms = prv_ms() + ms;
  s_wecker.cb = cb;
  s_wecker.daten = daten;
  return true;
}
bool new_timer_stop(TimerID t) {
  s_wecker.an = false;
  return true;
}
void new_timer_delete(TimerID t) {
}
bool new_timer_scheduled(TimerID t, uint32_t *ms) {
  return s_wecker.an;
}

// Pebbles Prefs: die Setter zaehlen und reihen wie die echten den Manager ein (prefs.c).
static bool s_puls_an;
static bool s_spo2_an;
static HRMonitoringInterval s_puls_takt;
static HRMonitoringInterval s_spo2_takt;
static int s_setter;
bool activity_prefs_heart_rate_is_enabled(void) {
  return s_puls_an;
}
bool activity_prefs_blood_oxygen_is_enabled(void) {
  return s_spo2_an;
}
bool activity_prefs_blood_oxygen_activity_tracking_is_enabled(void) {
  return false;
}
// AP6: Pebbles Pref "Puls bei Aktivitaet" (prefs.c:2110), Bedingung des Velo-Abos.
static bool s_puls_bei_aktivitaet;
bool activity_prefs_hrm_activity_tracking_is_enabled(void) {
  return s_puls_bei_aktivitaet;
}
HRMonitoringInterval activity_prefs_get_hrm_measurement_interval(void) {
  return s_puls_takt;
}
HRMonitoringInterval activity_prefs_get_spo2_measurement_interval(void) {
  return s_spo2_takt;
}
void activity_prefs_set_hrm_measurement_interval(HRMonitoringInterval i) {
  s_puls_takt = i;
  s_setter++;
  hrm_manager_handle_prefs_changed();
}
void activity_prefs_set_spo2_measurement_interval(HRMonitoringInterval i) {
  s_spo2_takt = i;
  s_setter++;
  hrm_manager_handle_prefs_changed();
}
void activity_prefs_set_blood_oxygen_enabled(bool an) {
  s_spo2_an = an;
  s_setter++;
  hrm_manager_handle_prefs_changed();
}

static ActivityState s_activity;
ActivityState *activity_private_state(void) {
  return &s_activity;
}

// BlobDB vor shellpref, wie settings_blob_db.c: Schluessel mit Nullbyte (kanonisch).
static int s_prefs_gesperrt;
void prefs_private_lock(void) {
  s_prefs_gesperrt++;
}
void prefs_private_unlock(void) {
  s_prefs_gesperrt--;
}
static status_t prv_shellpref(bool schreiben, const char *schluessel, uint8_t *wert) {
  SettingsFile f;
  status_t st = settings_file_open(&f, SHELL_PREFS_FILE_NAME, SHELL_PREFS_FILE_LEN);
  if (st == S_SUCCESS) {
    st = schreiben ? settings_file_set(&f, schluessel, strlen(schluessel) + 1, wert, 1)
                   : settings_file_get(&f, schluessel, strlen(schluessel) + 1, wert, 1);
    if (schreiben && st == S_SUCCESS) {
      settings_file_mark_synced(&f, schluessel, strlen(schluessel) + 1);  // vom Telefon
    }
    settings_file_close(&f);
  }
  return st;
}
status_t blob_db_read(BlobDBId db_id, const uint8_t *key, int key_len, uint8_t *val_out,
                      int val_len) {
  cl_assert_equal_i(db_id, BlobDBIdSettings);
  cl_assert_equal_i(key_len, strlen(BOULDER_MESSSTUFE_SCHLUESSEL));
  cl_assert_equal_i(val_len, 1);
  return prv_shellpref(false, BOULDER_MESSSTUFE_SCHLUESSEL, val_out);
}
// Was settings_file als geaendert meldet, gibt settings_blob_db ans Telefon (Whitelist).
static int s_zum_telefon;
static void prv_geaendert(SettingsFile *file, const void *key, int key_len, time_t zeit) {
  if (key_len == (int)strlen(BOULDER_MESSSTUFE_SCHLUESSEL) + 1 &&
      memcmp(key, BOULDER_MESSSTUFE_SCHLUESSEL, key_len) == 0) {
    s_zum_telefon++;
  }
}

// KernelMain: Abos merken, Callbacks gleich ausfuehren.
static EventServiceInfo *s_abos[4];
static int s_abo_anzahl;
void event_service_client_subscribe(EventServiceInfo *info) {
  cl_assert(s_abo_anzahl < 4);
  s_abos[s_abo_anzahl++] = info;
}
void event_service_client_unsubscribe(EventServiceInfo *info) {
}
static int s_kernel_main_callbacks;
void launcher_task_add_callback(CallbackEventCallback cb, void *daten) {
  s_kernel_main_callbacks++;
  cb(daten);
}

// Fertige HRV-Fenster an den Dauerteil.
static struct {
  int anzahl;
  time_t beginn;
  uint16_t rmssd;
  uint8_t puls;
} s_fenster;
void boulder_dauerteil_hrv_fenster(time_t beginn_utc, uint16_t rmssd, uint8_t puls) {
  s_fenster.anzahl++;
  s_fenster.beginn = beginn_utc;
  s_fenster.rmssd = rmssd;
  s_fenster.puls = puls;
}

extern HRMSubscriberState *prv_get_subscriber_state_from_ref(HRMSessionRef session);

// =============================================================================================
// Ablauf

#define BEGINN 1790445600  // 26.9.2026 18:00 UTC, durch 60 Minuten teilbar

static bool s_nachtruhe;

// Das Telefon schreibt die Stufe (BlobDB), KernelMain meldet es.
static status_t prv_telefon_schreibt(const char *schluessel, uint8_t wert) {
  s_zum_telefon = 0;
  const status_t st = prv_shellpref(true, schluessel, &wert);
  if (st == S_SUCCESS) {
    PebbleEvent e = {
      .type = PEBBLE_BLOBDB_EVENT,
      .blob_db = { .db_id = BlobDBIdSettings, .type = BlobDBEventTypeInsert,
                   .key = (uint8_t *)schluessel, .key_len = (uint8_t)strlen(schluessel) },
    };
    for (int i = 0; i < s_abo_anzahl; i++) {
      if (s_abos[i]->type == PEBBLE_BLOBDB_EVENT) {
        s_abos[i]->handler(&e, s_abos[i]->context);
      }
    }
  }
  return st;
}

static void prv_kernel_bg(void) {
  fake_system_task_callbacks_invoke_pending();
}

// Der Minutenteil der Variante, wie dauerteil.c ihn ruft.
static void prv_minute(void) {
  boulder_messstufe_minute();
  boulder_puls_minute(rtc_get_time(), s_nachtruhe);
  prv_kernel_bg();
}

// Eine Sekunde: Zeit, Zeitgeber, Sensordaten (gut erst nach 15 s Anlauf), KernelBG.
static void prv_sekunde(uint16_t ppi_ms) {
  fake_rtc_increment_ticks(RTC_TICKS_HZ);
  fake_rtc_increment_time(1);
  if (s_wecker.an && prv_ms() >= s_wecker.faellig_ms) {
    s_wecker.an = false;
    s_wecker.cb(s_wecker.daten);
  }
  if (s_sensor.an) {
    const bool eingeschwungen = (rtc_get_ticks() - s_sensor.an_seit) >= 15 * RTC_TICKS_HZ;
    HRMData d = {
      .features = s_sensor.merkmale & (HRMFeature_BPM | HRMFeature_HRV),
      .hrm_bpm = 55,
      .hrm_quality = eingeschwungen ? HRMQuality_Good : HRMQuality_Poor,
      .hrv_ppi_ms = ppi_ms,
      .hrv_quality = HRMQuality_Good,
    };
    hrm_manager_new_data_cb(&d);
  }
  prv_kernel_bg();
  if ((rtc_get_time() % PBL_SEC_PER_MIN) == 0) {
    prv_minute();
  }
}

// Minuten ohne verwertbare HRV-Abstaende (die Abstaende geben die Tests selbst vor).
static void prv_minuten(int n) {
  for (int i = 0; i < n * PBL_SEC_PER_MIN; i++) {
    prv_sekunde(0);
  }
}

static HRMFeature prv_abo_merkmale(uint32_t *takt_s) {
  HRMFeature f = 0;
  uint32_t t = 0;
  cl_assert(sys_hrm_manager_get_subscription_info(boulder_puls_abo(), NULL, &t, NULL, &f));
  if (takt_s) {
    *takt_s = t;
  }
  return f;
}

// Ein HRV-Abstand vom Sensor; er kommt nach seiner eigenen Dauer (b25-Testweise).
static void prv_abstand(uint16_t ppi, HRMQuality q) {
  fake_rtc_increment_ticks((RtcTicks)ppi * RTC_TICKS_HZ / 1000);
  HRMData d = { .features = HRMFeature_HRV, .hrv_ppi_ms = ppi, .hrv_quality = q };
  hrm_manager_new_data_cb(&d);
  prv_kernel_bg();
}

// Stufe setzen und bis zur vollen Stunde laufen (dort beginnt jedes HRV-Takt-Fenster).
static void prv_start(uint8_t stufe) {
  cl_assert_equal_i(prv_telefon_schreibt(BOULDER_MESSSTUFE_SCHLUESSEL, stufe), S_SUCCESS);
  prv_minute();
}

void test_boulder_messung__initialize(void) {
  fake_rtc_init(100, BEGINN - 30);
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  pfs_format(false);
  stub_pebble_tasks_set_current(PebbleTask_KernelBackground);
  fake_system_task_callbacks_cleanup();
  system_task_set_available_space(100);
  fake_event_init();
  memset(&s_sensor, 0, sizeof(s_sensor));
  memset(&s_wecker, 0, sizeof(s_wecker));
  memset(&s_activity, 0, sizeof(s_activity));
  memset(&s_fenster, 0, sizeof(s_fenster));
  s_puls_an = true;
  s_puls_bei_aktivitaet = true;
  s_spo2_an = false;
  s_puls_takt = HRMonitoringInterval_10Min;
  s_spo2_takt = HRMonitoringInterval_10Min;
  s_setter = 0;
  s_abo_anzahl = 0;
  s_kernel_main_callbacks = 0;
  s_zum_telefon = 0;
  s_nachtruhe = true;
  settings_file_set_change_callback(prv_geaendert);
  s_prefs_gesperrt = 0;
  hrm_manager_init();
  hrm_manager_enable(true);
  boulder_messstufe_test_zuruecksetzen();
  boulder_puls_test_zuruecksetzen();
  prv_kernel_bg();
}

void test_boulder_messung__cleanup(void) {
  fake_system_task_callbacks_cleanup();
}

// =============================================================================================
// F3 und E3: eine Einstellung, Pebbles Werte folgen

static const struct {
  HRMonitoringInterval puls;
  bool spo2_an;
  HRMonitoringInterval spo2;
} s_pebble_soll[MESSSTUFE_MAX] = {
  { HRMonitoringInterval_10Min, false, HRMonitoringInterval_10Min },
  { HRMonitoringInterval_10Min, true, HRMonitoringInterval_1Hour },
  { HRMonitoringInterval_10Min, true, HRMonitoringInterval_30Min },
  { HRMonitoringInterval_10Min, true, HRMonitoringInterval_10Min },
  { HRMonitoringInterval_5Min, true, HRMonitoringInterval_10Min },
};

void test_boulder_messung__stufe_vom_telefon_setzt_pebbles_werte(void) {
  // Ohne gespeicherte Stufe gilt 3; das KernelMain-Abo entsteht einmal.
  prv_minute();
  cl_assert_equal_i(boulder_messstufe_geladen(), MESSSTUFE_VORGABE);
  cl_assert_equal_i(s_kernel_main_callbacks, 1);
  for (uint8_t stufe = MESSSTUFE_MIN; stufe <= MESSSTUFE_MAX; stufe++) {
    cl_assert_equal_i(prv_telefon_schreibt(BOULDER_MESSSTUFE_SCHLUESSEL, stufe), S_SUCCESS);
    prv_kernel_bg();
    cl_assert_equal_i(boulder_messstufe_geladen(), stufe);
    cl_assert_equal_i(s_puls_takt, s_pebble_soll[stufe - 1].puls);
    cl_assert_equal_b(s_spo2_an, s_pebble_soll[stufe - 1].spo2_an);
    if (s_spo2_an) {
      cl_assert_equal_i(s_spo2_takt, s_pebble_soll[stufe - 1].spo2);
    }
  }
  cl_assert_equal_i(s_kernel_main_callbacks, 1);
  // Ungueltig gilt als Vorgabe.
  prv_telefon_schreibt(BOULDER_MESSSTUFE_SCHLUESSEL, 9);
  prv_kernel_bg();
  cl_assert_equal_i(boulder_messstufe_geladen(), MESSSTUFE_VORGABE);
}

void test_boulder_messung__regler_zieht_pebbles_wahl_zurueck(void) {
  prv_start(4);
  const int setter = s_setter;
  // Eine Pebble-Wahl vom Telefon (Core-App) - der Regler stellt die Stufe wieder her.
  s_puls_takt = HRMonitoringInterval_1Hour;
  s_spo2_an = false;
  prv_telefon_schreibt("hrmPreferences", 0);
  prv_kernel_bg();
  cl_assert_equal_i(s_puls_takt, HRMonitoringInterval_10Min);
  cl_assert_equal_b(s_spo2_an, true);
  // Nichts anders: kein Setter, nichts geht ans Telefon.
  const int danach = s_setter;
  prv_telefon_schreibt("spo2Preferences", 0);
  prv_kernel_bg();
  cl_assert_equal_i(s_setter, danach);
  cl_assert(danach > setter);
}

void test_boulder_messung__menue_speichert_und_geht_ans_telefon(void) {
  prv_start(3);
  s_zum_telefon = 0;
  stub_pebble_tasks_set_current(PebbleTask_App);
  boulder_messstufe_setzen(5);
  cl_assert_equal_i(boulder_messstufe(), 5);  // gleich sichtbar im Menue
  stub_pebble_tasks_set_current(PebbleTask_KernelBackground);
  prv_kernel_bg();
  // Gespeichert unter dem kanonischen Schluessel, unter der Prefs-Sperre ...
  uint8_t wert = 0;
  cl_assert_equal_i(prv_shellpref(false, BOULDER_MESSSTUFE_SCHLUESSEL, &wert), S_SUCCESS);
  cl_assert_equal_i(wert, 5);
  cl_assert_equal_i(s_prefs_gesperrt, 0);
  // ... als Aenderung gemeldet, also auf dem Weg zum Telefon ...
  cl_assert_equal_i(s_zum_telefon, 1);
  // ... und angewandt.
  cl_assert_equal_i(boulder_messstufe_geladen(), 5);
  cl_assert_equal_i(s_puls_takt, HRMonitoringInterval_5Min);
  // Ungueltiges aus dem Menue: nichts.
  boulder_messstufe_setzen(0);
  prv_kernel_bg();
  cl_assert_equal_i(boulder_messstufe_geladen(), 5);
}

// =============================================================================================
// T6.1: Puls je Stufe gegen den echten Manager

void test_boulder_messung__puls_je_stufe(void) {
  // Fenster (Sensor aus -> an) in einer Stunde Nachtruhe; Stufe 1 und 2 fragen nichts an
  // (Pebbles eigener 10-min-Takt reicht, activity.c ist hier nicht dabei).
  static const int erwartet[MESSSTUFE_MAX] = { 0, 0, 12, 30, 60 };
  for (uint8_t stufe = MESSSTUFE_MIN; stufe <= MESSSTUFE_MAX; stufe++) {
    test_boulder_messung__initialize();
    // Keine HRV-Fenster hier: SpO2 "misst" (dann beginnt keins), das Puls-Abo laeuft weiter.
    s_activity.spo2.currently_sampling = true;
    prv_start(stufe);
    prv_minuten(5);
    const int vorher = s_sensor.einschalten;
    prv_minuten(60);
    const int fenster = s_sensor.einschalten - vorher;
    const int soll = erwartet[stufe - 1];
    printf("Stufe %d: %d Fenster je Stunde\n", stufe, fenster);
    cl_assert(fenster >= soll * 8 / 10 && fenster <= soll * 12 / 10);
    uint32_t takt = 0;
    const HRMFeature f = prv_abo_merkmale(&takt);
    if (soll) {
      cl_assert_equal_i(f, HRMFeature_BPM);
      cl_assert_equal_i(takt, boulder_messstufe_werte(stufe)->puls_nacht_s);
    } else {
      cl_assert_equal_i(f, 0);
    }
    // Tags (oder in Bewegung) misst nur Pebble: das Abo ruht.
    s_nachtruhe = false;
    prv_minuten(2);
    const int tags = s_sensor.einschalten;
    prv_minuten(30);
    cl_assert_equal_i(prv_abo_merkmale(NULL), 0);
    cl_assert_equal_i(s_sensor.einschalten, tags);
  }
}

// T6.3: Ein Abo fuer immer; Setter und Anmelden nur bei Platz.
void test_boulder_messung__ein_abo_setter_nur_bei_platz(void) {
  s_activity.spo2.currently_sampling = true;  // ohne HRV-Fenster: Takt und Merkmale fest
  system_task_set_available_space(BOULDER_SCHLANGE_FREI - 1);
  prv_telefon_schreibt(BOULDER_MESSSTUFE_SCHLUESSEL, 5);
  prv_minuten(3);
  cl_assert_equal_i(boulder_puls_abo(), HRM_INVALID_SESSION_REF);
  cl_assert_equal_i(boulder_messstufe_geladen(), 0);
  cl_assert_equal_i(s_setter, 0);
  system_task_set_available_space(100);
  prv_minuten(1);
  const uint32_t abo = boulder_puls_abo();
  cl_assert(abo != HRM_INVALID_SESSION_REF);
  cl_assert_equal_i(boulder_messstufe_geladen(), 5);
  // Eine ganze Nacht mit Stufenwechseln, Tag und Fenstern: immer dasselbe Abo.
  for (int i = 0; i < 6; i++) {
    prv_telefon_schreibt(BOULDER_MESSSTUFE_SCHLUESSEL, (uint8_t)(1 + (i % 5)));
    s_nachtruhe = (i % 3) != 0;
    prv_minuten(20);
    cl_assert_equal_i(boulder_puls_abo(), abo);
  }
  cl_assert(prv_get_subscriber_state_from_ref(abo) != NULL);
  // Ohne Platz bleibt das Abo, wie es ist, und holt nach.
  prv_telefon_schreibt(BOULDER_MESSSTUFE_SCHLUESSEL, 3);
  s_nachtruhe = true;
  prv_minuten(2);
  uint32_t takt = 0;
  cl_assert_equal_i(prv_abo_merkmale(&takt), HRMFeature_BPM);
  cl_assert_equal_i(takt, 5 * 60);
  system_task_set_available_space(BOULDER_SCHLANGE_FREI - 1);
  prv_telefon_schreibt(BOULDER_MESSSTUFE_SCHLUESSEL, 5);
  prv_minuten(2);
  cl_assert_equal_i(boulder_messstufe_geladen(), 3);
  system_task_set_available_space(100);
  prv_minuten(2);
  prv_abo_merkmale(&takt);
  cl_assert_equal_i(takt, 60);
  // Eingereiht mit Platz, beim Lauf knapp: gelesen wird, gesetzt nicht - erst mit Platz.
  prv_telefon_schreibt(BOULDER_MESSSTUFE_SCHLUESSEL, 2);
  const int setter = s_setter;
  system_task_set_available_space(BOULDER_SCHLANGE_FREI - 1);
  prv_kernel_bg();
  cl_assert_equal_i(boulder_messstufe_geladen(), 2);
  cl_assert_equal_i(s_setter, setter);
  prv_abo_merkmale(&takt);
  cl_assert_equal_i(takt, 60);
  system_task_set_available_space(100);
  prv_minuten(1);
  cl_assert(s_setter > setter);
  cl_assert_equal_i(s_spo2_takt, HRMonitoringInterval_1Hour);
  cl_assert_equal_i(prv_abo_merkmale(NULL), 0);
  // Dasselbe fuer jeden Setter einzeln: 2 -> 3 nur der SpO2-Takt, 3 -> 1 nur SpO2 aus.
  static const uint8_t stufen[] = { 3, 1 };
  for (size_t i = 0; i < ARRAY_LENGTH(stufen); i++) {
    prv_telefon_schreibt(BOULDER_MESSSTUFE_SCHLUESSEL, stufen[i]);
    const int vorher = s_setter;
    system_task_set_available_space(BOULDER_SCHLANGE_FREI - 1);
    prv_kernel_bg();
    cl_assert_equal_i(s_setter, vorher);
    system_task_set_available_space(100);
    prv_minuten(1);
    cl_assert_equal_i(s_setter, vorher + 1);
  }
  cl_assert(!s_spo2_an);
}

// Tracking aus (Ladekabel, Schalter): das Abo ruht sofort, ein laufendes Fenster zaehlt nicht.
void test_boulder_messung__tracking_aus_laesst_das_abo_ruhen(void) {
  prv_start(5);
  prv_minuten(2);
  cl_assert(prv_abo_merkmale(NULL) & HRMFeature_BPM);
  boulder_puls_tracking_aus();
  prv_kernel_bg();
  cl_assert_equal_i(prv_abo_merkmale(NULL), 0);
}

// Gegenpruefung B2: Ist die Schlange beim "Tracking aus" knapp, scheitert das Abschalten des
// Velo-Abos (1 Hz). Ohne Tracking kommt kein Minutenteil; der Takt holt es nach, sobald Platz
// ist - und reiht danach nichts mehr ein.
void test_boulder_messung__tracking_aus_bei_knapper_schlange_holt_der_takt_nach(void) {
  prv_start(1);
  s_nachtruhe = false;
  prv_minuten(2);
  boulder_puls_aktiv(true);
  prv_kernel_bg();
  cl_assert_equal_i(prv_abo_merkmale(NULL), HRMFeature_BPM);
  system_task_set_available_space(BOULDER_SCHLANGE_FREI - 1);
  boulder_puls_tracking_aus();
  prv_kernel_bg();
  cl_assert_equal_i(prv_abo_merkmale(NULL), HRMFeature_BPM);  // der Sensor misst weiter
  boulder_puls_takt();
  prv_kernel_bg();
  cl_assert_equal_i(prv_abo_merkmale(NULL), HRMFeature_BPM);  // noch knapp: wartet
  system_task_set_available_space(100);
  boulder_puls_takt();
  prv_kernel_bg();
  cl_assert_equal_i(prv_abo_merkmale(NULL), 0);
  boulder_puls_takt();
  cl_assert_equal_i(fake_system_task_count_callbacks(), 0);
}

// T5.4: das Velo-Abo "Aktiv" (3.7) ueber dasselbe eine Abo. Ab der Vorstufe BPM jede Sekunde -
// auch auf Stufe 1 und tags, wie Kraepelins Walk/Run-Abo; SpO2 bei Aktivitaet pausiert es;
// danach ruht es. Ohne Pebbles Pref "Puls bei Aktivitaet" bleibt es aus.
void test_boulder_messung__aktiv_abo_velo(void) {
  prv_start(1);
  s_nachtruhe = false;
  prv_minuten(2);
  const uint32_t abo = boulder_puls_abo();
  cl_assert_equal_i(prv_abo_merkmale(NULL), 0);
  const int vorher = s_sensor.einschalten;
  boulder_puls_aktiv(true);
  prv_kernel_bg();
  uint32_t takt = 0;
  cl_assert_equal_i(prv_abo_merkmale(&takt), HRMFeature_BPM);
  cl_assert_equal_i(takt, 1);
  prv_minuten(10);
  // Einmal an und durchgehend an: Werte jede Sekunde fuer den Minutenmedian.
  cl_assert(s_sensor.an);
  cl_assert_equal_i(s_sensor.einschalten, vorher + 1);
  boulder_puls_aktiv_pausieren(true);
  prv_kernel_bg();
  cl_assert_equal_i(prv_abo_merkmale(NULL), 0);
  boulder_puls_aktiv_pausieren(false);
  prv_kernel_bg();
  cl_assert_equal_i(prv_abo_merkmale(NULL), HRMFeature_BPM);
  boulder_puls_aktiv(false);
  prv_kernel_bg();
  cl_assert_equal_i(prv_abo_merkmale(NULL), 0);
  s_puls_bei_aktivitaet = false;
  boulder_puls_aktiv(true);
  prv_kernel_bg();
  prv_minuten(2);
  cl_assert_equal_i(prv_abo_merkmale(NULL), 0);
  cl_assert_equal_i(boulder_puls_abo(), abo);
}

// =============================================================================================
// HRV-Fenster (T6.2, aus b25 test_boulder_hrv portiert) gegen den echten Manager

// Bis zum naechsten Beginn eines Fensters (Stufe 3: alle 30 min) laufen.
static void prv_bis_fenster(void) {
  for (int i = 0; i < 31 * 60 && !boulder_puls_hrv_laeuft(); i++) {
    prv_sekunde(0);
  }
  cl_assert(boulder_puls_hrv_laeuft());
}

void test_boulder_messung__rmssd_wie_kieselsport(void) {
  prv_start(3);
  prv_bis_fenster();
  const time_t beginn = rtc_get_time();
  cl_assert_equal_i(beginn % (30 * 60), 0);
  // Dauer-Gruen: der Sensor bleibt an, Pebbles SpO2 weicht aus (hrm_manager_has_...).
  cl_assert_equal_i(prv_abo_merkmale(NULL), HRMFeature_BPM | HRMFeature_HRV);
  cl_assert(hrm_manager_has_continuous_green_subscriber());
  // Im Wechsel 1000 und 1040 ms: jede Differenz 40, RMSSD 40; ein 1900er faellt weg.
  for (int i = 0; i < 40; i++) {
    prv_abstand((i % 2) ? 1040 : 1000, HRMQuality_Good);
    if (i == 20) {
      prv_abstand(1900, HRMQuality_Good);
    }
  }
  const int an = s_sensor.einschalten;
  prv_minuten(6);
  cl_assert_equal_i(s_sensor.einschalten, an);  // fuenf Minuten am Stueck
  cl_assert_equal_i(s_fenster.anzahl, 1);
  cl_assert_equal_i(s_fenster.beginn, beginn);
  cl_assert_equal_i(s_fenster.rmssd, 40);
  cl_assert_equal_i(s_fenster.puls, 59);  // im Mittel 1020 ms
  // Danach wieder nur Nachtpuls, kein Dauer-Gruen.
  cl_assert_equal_i(prv_abo_merkmale(NULL), HRMFeature_BPM);
  cl_assert(!hrm_manager_has_continuous_green_subscriber());
}

void test_boulder_messung__zu_wenige_intervalle_zaehlen_nicht(void) {
  prv_start(3);
  prv_bis_fenster();
  for (int i = 0; i < 20; i++) {
    prv_abstand((i % 2) ? 950 : 1000, HRMQuality_Good);
  }
  prv_minuten(6);
  cl_assert_equal_i(s_fenster.anzahl, 0);
}

void test_boulder_messung__kein_fenster_bei_puls_aus_in_bewegung_oder_stufe_1(void) {
  s_puls_an = false;
  prv_start(5);
  prv_minuten(40);
  cl_assert(!boulder_puls_hrv_laeuft());
  s_puls_an = true;
  s_nachtruhe = false;
  prv_minuten(40);
  cl_assert(!boulder_puls_hrv_laeuft());
  s_nachtruhe = true;
  prv_telefon_schreibt(BOULDER_MESSSTUFE_SCHLUESSEL, 1);
  prv_minuten(70);
  cl_assert(!boulder_puls_hrv_laeuft());
  cl_assert_equal_i(s_sensor.einschalten, 0);
}

// SOLANGE SPO2 MISST, BEGINNT KEIN FENSTER (b25 M8); das naechste im Takt schon.
void test_boulder_messung__kein_fenster_solange_spo2_misst(void) {
  prv_start(3);
  s_activity.spo2.currently_sampling = true;
  prv_minuten(35);
  cl_assert(!boulder_puls_hrv_laeuft());
  s_activity.spo2.currently_sampling = false;
  prv_bis_fenster();
  cl_assert_equal_i(rtc_get_time() % (30 * 60), 0);
}

// NUR SICHERE ABSTAENDE, KEINE DIFFERENZ UEBER EINE LUECKE (b25 M8): 1000/1010 (RMSSD 10),
// dazwischen ein 700er "Worst", dann zwei Minuten nichts und 1200/1210.
void test_boulder_messung__schlechte_abstaende_und_luecken_zaehlen_nicht(void) {
  prv_start(3);
  prv_bis_fenster();
  for (int i = 0; i < 30; i++) {
    prv_abstand((i % 2) ? 1010 : 1000, HRMQuality_Good);
    if (i == 15) {
      prv_abstand(700, HRMQuality_Worst);
    }
  }
  fake_rtc_increment_ticks(2 * 60 * RTC_TICKS_HZ);
  for (int i = 0; i < 30; i++) {
    prv_abstand((i % 2) ? 1210 : 1200, HRMQuality_Acceptable);
  }
  prv_minuten(6);
  cl_assert_equal_i(s_fenster.anzahl, 1);
  cl_assert_equal_i(s_fenster.rmssd, 10);
}

// WIRD DIE UHR ZURUECKGESTELLT, HAENGT DAS FENSTER NICHT (b25 N4).
void test_boulder_messung__zurueckgestellte_uhr_haengt_das_fenster_nicht(void) {
  prv_start(3);
  prv_bis_fenster();
  for (int i = 0; i < 40; i++) {
    prv_abstand((i % 2) ? 1040 : 1000, HRMQuality_Good);
  }
  fake_rtc_increment_time(-3600);
  prv_minuten(1);
  cl_assert(!boulder_puls_hrv_laeuft());
  cl_assert_equal_i(s_fenster.anzahl, 0);
  prv_bis_fenster();
  cl_assert_equal_i(rtc_get_time() % (30 * 60), 0);
}

// F2: Die Fenster der Variante gehen nicht als Health-Ereignis an alle Apps; bestellt eine
// App HRV, bekommt sie es.
void test_boulder_messung__hrv_rundruf_nur_fuer_apps_mit_hrv(void) {
  prv_start(3);
  prv_bis_fenster();
  fake_event_reset_count();
  prv_abstand(1000, HRMQuality_Good);
  cl_assert_equal_i(fake_event_get_count(), 0);
  stub_pebble_tasks_set_current(PebbleTask_App);
  sys_hrm_manager_app_subscribe(7, 1, 0, HRMFeature_HRV);
  stub_pebble_tasks_set_current(PebbleTask_KernelBackground);
  prv_abstand(1000, HRMQuality_Good);
  cl_assert_equal_i(fake_event_get_count(), 1);
  cl_assert_equal_i(fake_event_get_last().health_event.type, HealthEventHRVUpdate);
}
