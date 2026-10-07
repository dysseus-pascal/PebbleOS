/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "applib/accel_service_private.h"
#include "applib/compass_service.h"
#include "applib/ui/vibes.h"
#include "flash_region/flash_region.h"
#include "kernel/events.h"
#include "process_management/app_install_manager.h"
#include "process_management/app_manager.h"
#include "process_management/pebble_process_md.h"
#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/activity_private.h"
#include "pbl/services/activity/boulder/sport/kieselsport.h"
#include "pbl/services/activity/workout_service.h"
#include "pbl/services/filesystem/pfs.h"
#include "pbl/services/hrm/hrm_manager.h"
#include "pbl/services/hrm/hrm_manager_private.h"
#include "pbl/services/new_timer/new_timer.h"
#include "pbl/services/persist.h"
#include "pbl/services/regular_timer.h"
#include "pbl/services/settings/settings_file.h"

#include "stubs_analytics.h"
#include "stubs_evented_timer.h"
#include "stubs_hexdump.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_rand_ptr.h"
#include "stubs_sleep.h"
#include "stubs_task_wdt.h"

#include "fake_events.h"
#include "fake_pebble_tasks.h"
#include "fake_rtc.h"
#include "fake_spi_flash.h"
#include "fake_system_task.h"

// Zuletzt die App: ihre Koepfe ziehen pebble_compat.h nach, und dessen Makros (malloc, time, ...)
// sollen die Koepfe davor nicht umschreiben.
#include "apps/system/kieselsport/einstellungen.h"
#include "apps/system/kieselsport/schluessel.h"
#include "apps/system/kieselsport/telefon.h"
#undef malloc
#undef calloc
#undef free
#undef time

#include <stdio.h>
#include <stdlib.h>

// T6.2 und T6.3 (Kriterium 6, AP6): der Wechsel von b25. b25 hinterlaesst Kieselsports Persist
// unter der Kennung der App ("ps" + UUID, persist/service.c in b25 und V2 bis auf Umbenennungen
// gleich). Was darin steht, kommt aus einem Lauf von b25s eigenem Code
// (tests/fixtures/activity/boulder/kieselsport-b25/: abbild_szenario.c, persist.txt) und geht hier
// Schluessel fuer Schluessel ueber den echten Persist-Dienst in die Datei - so, wie b25s
// speicher.c und applib/persist.c es tun (settings_file_set, Schluessel uint32).
//
// Echt sind dann V2s Dienst (dienst.c mit Persist, Wartendem, Kurve, Einstellungen) ueber
// speicher.c, persist/service.c und settings_file.c, und auf der Seite der App telefon.c,
// meldungen.c, einstellungen.c und applib/persist.c. Nachgebaut sind das Umfeld des Dienstes
// (er laeuft hier ohne Training: Zeitgeber, Sensoren, Pebbles Activity), der Postausgang mit
// Wecker und das Telefon, das wie Boulder erst bestaetigt, wenn es gesichert hat.

#define BEGINN_LAUF 1790445600u  // b25s Lauf (abbild_szenario.c)
#define ZEIT_WECHSEL (BEGINN_LAUF + 6 * 3600)

static const Uuid s_kieselsport = KIESELSPORT_UUID_INIT;

extern void kieselsport_test_neustart(void);

// =============================================================================================
// Umfeld des Dienstes (kein Training: was nur ein Training braucht, darf hier nicht laufen)

#define NIE(...) cl_fail("im Wechsel ohne Training gerufen: " __VA_ARGS__)

static RegularTimerInfo *s_minutentakt;
void regular_timer_add_seconds_callback(RegularTimerInfo *cb) { NIE("Sekundentakt"); }
void regular_timer_add_minutes_callback(RegularTimerInfo *cb) { s_minutentakt = cb; }
bool regular_timer_is_scheduled(RegularTimerInfo *cb) { return cb == s_minutentakt; }
bool regular_timer_remove_callback(RegularTimerInfo *cb) {
  const bool da = cb == s_minutentakt;
  if (da) s_minutentakt = NULL;
  return da;
}
TimerID new_timer_create(void) { return 1; }
bool new_timer_start(TimerID t, uint32_t ms, NewTimerCallback cb, void *daten, uint32_t flags) {
  return true;
}
bool new_timer_stop(TimerID t) { return true; }
void new_timer_delete(TimerID t) {}

static ActivityState s_state;
ActivityState *activity_private_state(void) { return &s_state; }
bool activity_get_sessions(uint32_t *session_entries, ActivitySession *sessions) {
  *session_entries = 0;
  return true;
}
bool activity_get_metric(ActivityMetric metric, uint32_t history_len, int32_t *history) {
  *history = 0;
  return true;
}
void activity_algorithm_enable_activity_tracking(bool enable) { NIE("Auto-Erkennung"); }
void activity_insights_push_activity_session_notification(time_t notif_time,
                                                          ActivitySession *session, int32_t avg_hr,
                                                          int32_t *hr_zone_time_s) {
  NIE("Meldung");
}
void workout_utils_send_abandoned_workout_notification(void) {}
uint8_t activity_prefs_get_age_years(void) { return 40; }
ActivityGender activity_prefs_get_gender(void) { return ActivityGenderMale; }
uint16_t activity_prefs_get_weight_dag(void) { return 7000; }
uint16_t activity_prefs_get_height_mm(void) { return 1800; }
uint8_t activity_prefs_heart_get_elevated_hr(void) { return 100; }
uint8_t activity_prefs_heart_get_zone1_threshold(void) { return 130; }
uint8_t activity_prefs_heart_get_zone2_threshold(void) { return 154; }
uint8_t activity_prefs_heart_get_zone3_threshold(void) { return 172; }
HRMonitoringInterval activity_prefs_get_hrm_measurement_interval(void) {
  return HRMonitoringInterval_10Min;
}
AppInstallId app_get_app_id(void) { return 0; }
uint32_t time_get_uptime_seconds(void) { return rtc_get_time(); }
time_t time_utc_to_local(time_t utc_time) { return utc_time; }
time_t time_util_get_midnight_of(time_t ts) { return ts - ts % PBL_SEC_PER_DAY; }
int time_util_get_minute_of_day(time_t utc_sec) { return (utc_sec % PBL_SEC_PER_DAY) / 60; }
time_t activity_algorithm_get_last_sleep_utc(void) { return 0; }
void activity_algorithm_post_process_sleep_sessions(uint16_t num_sessions,
                                                    ActivitySession *sessions) {}
SettingsFile *activity_private_settings_open(void) { return NULL; }
void activity_private_settings_close(SettingsFile *file) {}
DataLoggingSession *dls_create(uint32_t tag, DataLoggingItemType item_type, uint16_t item_size,
                               bool buffered, bool resume, const Uuid *uuid) {
  return NULL;
}
DataLoggingResult dls_log(DataLoggingSession *s, const void *data, uint32_t num_items) {
  return DATA_LOGGING_SUCCESS;
}
void dls_send_all_sessions(void) {}

HRMSessionRef hrm_manager_subscribe_with_callback(AppInstallId app_id, uint32_t update_interval_s,
                                                  uint16_t expire_s, HRMFeature features,
                                                  bool low_latency, HRMSubscriberCallback callback,
                                                  void *context) {
  NIE("Puls-Abo");
  return 0;
}
bool sys_hrm_manager_unsubscribe(HRMSessionRef ref) { return false; }
bool sys_hrm_manager_set_update_interval(HRMSessionRef ref, uint32_t takt, uint16_t ablauf) {
  return false;
}
bool sys_hrm_manager_set_features(HRMSessionRef ref, HRMFeature features) { return false; }
HRMSessionRef sys_hrm_manager_app_subscribe(AppInstallId app_id, uint32_t update_interval_s,
                                            uint16_t expire_s, HRMFeature features) {
  NIE("Pebbles Puls-Abo");
  return 0;
}
void hrm_manager_set_activity_scene(HRMActivityScene scene) {}

static AccelServiceState s_accel;
AccelServiceState *accel_session_create(void) { return &s_accel; }
void accel_session_delete(AccelServiceState *session) {}
void accel_session_data_subscribe(AccelServiceState *session, uint32_t samples_per_update,
                                  AccelDataHandler handler) {
  NIE("Beschleunigung");
}
void accel_session_data_unsubscribe(AccelServiceState *session) {}
void compass_service_subscribe(CompassHeadingHandler handler) { NIE("Kompass"); }
void compass_service_unsubscribe(void) {}
int compass_service_set_heading_filter(CompassHeading filter) { return 0; }
void launcher_task_add_callback(CallbackEventCallback cb, void *daten) { cb(daten); }
void vibes_enqueue_custom_pattern(VibePattern muster) {}
BatteryChargeState battery_get_charge_state(void) {
  return (BatteryChargeState){.charge_percent = 80};
}
bool app_manager_is_watchface_running(void) { return false; }
void app_manager_put_launch_app_event(const AppLaunchEventConfig *config) {}
AppInstallId app_install_get_id_for_uuid(const Uuid *uuid) { return -103; }

// Die App laeuft mit Kieselsports Kennung: ihr Persist (applib/persist.c) ist dieselbe Datei.
static const PebbleProcessMd s_app = {.uuid = KIESELSPORT_UUID_INIT};
const PebbleProcessMd *sys_process_manager_get_current_process_md(void) { return &s_app; }

// =============================================================================================
// Die App: Wecker in Millisekunden, Verbindung, Postausgang (wie test_kieselsport_telefon)

typedef struct {
  uint32_t id;
  uint32_t faellig_ms;
  AppTimerCallback cb;
  void *daten;
} Wecker;
#define WECKER_MAX 16
static Wecker s_wecker[WECKER_MAX];
static uint32_t s_wecker_id;
static uint32_t s_ms;

AppTimer *app_timer_register(uint32_t timeout_ms, AppTimerCallback cb, void *daten) {
  for (int i = 0; i < WECKER_MAX; i++) {
    if (!s_wecker[i].id) {
      s_wecker[i] = (Wecker){.id = ++s_wecker_id, .faellig_ms = s_ms + timeout_ms, .cb = cb,
                             .daten = daten};
      return (AppTimer *)(uintptr_t)s_wecker[i].id;
    }
  }
  cl_fail("zu viele Wecker");
  return NULL;
}
void app_timer_cancel(AppTimer *timer) {
  for (int i = 0; i < WECKER_MAX; i++) {
    if (s_wecker[i].id && s_wecker[i].id == (uint32_t)(uintptr_t)timer) s_wecker[i].id = 0;
  }
}
static bool prv_wecker_offen(void) {
  for (int i = 0; i < WECKER_MAX; i++) {
    if (s_wecker[i].id) return true;
  }
  return false;
}
static void prv_warte(uint32_t ms) {
  const uint32_t bis = s_ms + ms;
  for (;;) {
    int n = -1;
    for (int i = 0; i < WECKER_MAX; i++) {
      if (s_wecker[i].id && s_wecker[i].faellig_ms <= bis &&
          (n < 0 || s_wecker[i].faellig_ms < s_wecker[n].faellig_ms)) {
        n = i;
      }
    }
    if (n < 0) break;
    const Wecker w = s_wecker[n];
    s_wecker[n].id = 0;
    if (w.faellig_ms > s_ms) s_ms = w.faellig_ms;
    w.cb(w.daten);
  }
  s_ms = bis;
}

static bool s_verbunden;
bool connection_service_peek_pebble_app_connection(void) { return s_verbunden; }

#define NACHRICHT_MAX 700
typedef struct {
  uint8_t daten[NACHRICHT_MAX];
  uint16_t laenge;
  bool bestaetigt;  //< Boulder hat sie gesichert und quittiert
} Nachricht;
#define GESENDET_MAX 64
static Nachricht s_gesendet[GESENDET_MAX];
static int s_anzahl;
static uint8_t s_out[NACHRICHT_MAX];
static DictionaryIterator s_out_iter;
static bool s_belegt;
static bool s_schreibt;
static AppMessageInboxReceived s_bei_eingang;
static AppMessageOutboxSent s_bei_angekommen;
static AppMessageOutboxFailed s_bei_abgelehnt;

AppMessageInboxReceived app_message_register_inbox_received(AppMessageInboxReceived cb) {
  s_bei_eingang = cb;
  return NULL;
}
AppMessageOutboxSent app_message_register_outbox_sent(AppMessageOutboxSent cb) {
  s_bei_angekommen = cb;
  return NULL;
}
AppMessageOutboxFailed app_message_register_outbox_failed(AppMessageOutboxFailed cb) {
  s_bei_abgelehnt = cb;
  return NULL;
}
AppMessageResult app_message_open(const uint32_t ein, const uint32_t aus) {
  cl_assert(aus <= NACHRICHT_MAX);
  return APP_MSG_OK;
}
AppMessageResult app_message_outbox_begin(DictionaryIterator **iter) {
  *iter = NULL;
  if (s_belegt) return APP_MSG_BUSY;
  if (s_schreibt) return APP_MSG_INVALID_STATE;
  dict_write_begin(&s_out_iter, s_out, sizeof(s_out));
  *iter = &s_out_iter;
  s_schreibt = true;
  return APP_MSG_OK;
}
AppMessageResult app_message_outbox_send(void) {
  if (s_belegt) return APP_MSG_BUSY;
  if (!s_schreibt) return APP_MSG_INVALID_STATE;
  s_schreibt = false;
  const uint32_t n = dict_write_end(&s_out_iter);
  if (s_anzahl >= GESENDET_MAX) {
    cl_fail("mehr als 64 Nachrichten: die Uhr schickt dasselbe immer wieder");
  }
  memcpy(s_gesendet[s_anzahl].daten, s_out, n);
  s_gesendet[s_anzahl].laenge = (uint16_t)n;
  s_gesendet[s_anzahl].bestaetigt = false;
  s_anzahl++;
  s_belegt = true;
  return APP_MSG_OK;
}

// Boulder: sichert, was ankommt, und quittiert (ACK) - oder die Antwort bleibt aus (Timeout).
static void prv_antwort(bool angekommen) {
  cl_assert(s_belegt);
  s_belegt = false;
  if (angekommen) {
    s_gesendet[s_anzahl - 1].bestaetigt = true;
    s_bei_angekommen(&s_out_iter, NULL);
  } else {
    s_bei_abgelehnt(&s_out_iter, APP_MSG_SEND_TIMEOUT, NULL);
  }
}

static Tuple *prv_tupel(int i, uint32_t key) {
  static DictionaryIterator it;
  dict_read_begin_from_buffer(&it, s_gesendet[i].daten, s_gesendet[i].laenge);
  return dict_find(&it, key);
}
static int32_t prv_feld(int i, uint32_t key) {
  Tuple *t = prv_tupel(i, key);
  return t ? t->value->int32 : -1;
}

typedef enum { ArtZustand, ArtZusammenfassung, ArtKurve, ArtEinst } Nachrichtenart;
static Nachrichtenart prv_art(int i) {
  if (prv_tupel(i, MESSAGE_KEY_DAUER)) return ArtZusammenfassung;
  if (prv_tupel(i, MESSAGE_KEY_KURVE)) return ArtKurve;
  if (prv_tupel(i, MESSAGE_KEY_MAXPULS)) return ArtEinst;
  return ArtZustand;
}

// Die App laeuft, bis nichts mehr ansteht; das Telefon antwortet mit [angekommen].
static void prv_app_bis_ruhe(bool angekommen) {
  for (int runde = 0; runde < 400; runde++) {
    if (s_belegt) {
      prv_antwort(angekommen);
    } else if (!prv_wecker_offen()) {
      return;
    }
    prv_warte(500);
  }
  cl_fail("es hoert nicht auf zu senden");
}

// =============================================================================================
// b25s Persist und der Wechsel

typedef struct {
  uint32_t key;
  uint8_t wert[256];
  int laenge;
} B25Eintrag;
#define B25_MAX 32
static B25Eintrag s_b25[B25_MAX];
static int s_b25_anzahl;

static void prv_b25_lesen(void) {
  char pfad[512];
  snprintf(pfad, sizeof(pfad), "%s/activity/boulder/kieselsport-b25/persist.txt",
           CLAR_FIXTURE_PATH);
  FILE *f = fopen(pfad, "r");
  cl_assert(f);
  char zeile[1024];
  s_b25_anzahl = 0;
  while (fgets(zeile, sizeof(zeile), f)) {
    if (zeile[0] == '#' || zeile[0] == '\n') continue;
    cl_assert(s_b25_anzahl < B25_MAX);
    B25Eintrag *e = &s_b25[s_b25_anzahl++];
    char hex[600] = "";
    cl_assert(sscanf(zeile, "%" SCNu32 " %599s", &e->key, hex) >= 1);
    e->laenge = (int)strlen(hex) / 2;
    for (int i = 0; i < e->laenge; i++) {
      unsigned int b;
      cl_assert(sscanf(&hex[2 * i], "%2x", &b) == 1);
      e->wert[i] = (uint8_t)b;
    }
  }
  fclose(f);
}

static const B25Eintrag *prv_b25(uint32_t key) {
  for (int i = 0; i < s_b25_anzahl; i++) {
    if (s_b25[i].key == key) return &s_b25[i];
  }
  return NULL;
}

// Wer Kieselsports Datei offen haelt (App, Test); cleanup schliesst, was ein gescheiterter Test
// offen liess - sonst stolperte der naechste Test ueber eine offene Datei auf formatiertem Flash.
static int s_offen;
static void prv_auf(void) {
  persist_service_client_open(&s_kieselsport);
  s_offen++;
}
static void prv_zu(void) {
  persist_service_client_close(&s_kieselsport);
  s_offen--;
}

// b25 hat geschrieben: jeder Eintrag in Kieselsports Datei.
static void prv_b25_schreiben(void) {
  prv_auf();
  SettingsFile *datei = persist_service_lock_and_get_store(&s_kieselsport);
  for (int i = 0; i < s_b25_anzahl; i++) {
    cl_assert_equal_i(settings_file_set(datei, &s_b25[i].key, sizeof(uint32_t), s_b25[i].wert,
                                        s_b25[i].laenge), S_SUCCESS);
  }
  persist_service_unlock_store(datei);
  prv_zu();
}

// Was jetzt in der Datei steht; -1, wenn der Schluessel fehlt.
static int prv_datei(uint32_t key, uint8_t *aus, size_t platz) {
  prv_auf();
  SettingsFile *datei = persist_service_lock_and_get_store(&s_kieselsport);
  int n = -1;
  if (settings_file_exists(datei, &key, sizeof(key))) {
    n = settings_file_get_len(datei, &key, sizeof(key));
    cl_assert(n <= (int)platz);
    cl_assert_equal_i(settings_file_get(datei, &key, sizeof(key), aus, n), S_SUCCESS);
  }
  persist_service_unlock_store(datei);
  prv_zu();
  return n;
}

// Ein Start der Uhr mit V2: der Dienst liest den Persist (die Variante oder die App stossen ihn
// auf KernelBG an), dann oeffnet der Nutzer Kieselsport - die App meldet sich beim Telefon.
static void prv_v2_startet(void) {
  kieselsport_test_neustart();
  stub_pebble_tasks_set_current(PebbleTask_KernelBackground);
  kieselsport_init();
  stub_pebble_tasks_set_current(PebbleTask_App);
}
static void prv_app_oeffnet(void) {
  memset(s_wecker, 0, sizeof(s_wecker));
  s_belegt = s_schreibt = false;
  telefon_zuruecksetzen();
  prv_auf();  // wie app_state_init
  telefon_init();
}
static void prv_app_schliesst(void) {
  prv_zu();
}

void test_kieselsport_wechsel__initialize(void) {
  fake_rtc_init(0, ZEIT_WECHSEL);
  fake_system_task_callbacks_cleanup();
  stub_pebble_tasks_set_current(PebbleTask_App);
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  pfs_format(false);
  persist_service_init();
  memset(&s_state, 0, sizeof(s_state));
  s_minutentakt = NULL;
  memset(s_wecker, 0, sizeof(s_wecker));
  s_wecker_id = 0;
  s_ms = 0;
  s_anzahl = 0;
  s_belegt = s_schreibt = false;
  s_verbunden = true;
  prv_b25_lesen();
  prv_b25_schreiben();
}

void test_kieselsport_wechsel__cleanup(void) {
  while (s_offen > 0) {
    prv_zu();
  }
}

// =============================================================================================
// T6.2: die Einstellungen (Schluessel 1 bis 6 und 12)

void test_kieselsport_wechsel__einstellungen_wie_b25(void) {
  // Das Abbild traegt, was b25 von Boulder bekam (abbild_szenario.c).
  cl_assert(prv_b25(1) && prv_b25(2) && prv_b25(3) && prv_b25(4) && prv_b25(5) && prv_b25(6) &&
            prv_b25(12));
  prv_v2_startet();
  // Der Dienst rechnet damit ...
  cl_assert_equal_i(kieselsport_maxpuls(), 183);
  cl_assert(kieselsport_maxpuls_gesetzt());
  cl_assert_equal_i(kieselsport_empfindlichkeit(), 3);
  cl_assert_equal_i(kieselsport_pausenziel(), 120);
  cl_assert_equal_i(kieselsport_becken(), 50);
  // ... und Boulder bekommt beim Oeffnen der App dieselben Werte zurueck, auch die der App.
  prv_app_oeffnet();
  prv_app_bis_ruhe(true);
  int einst = -1;
  for (int i = 0; i < s_anzahl; i++) {
    if (prv_art(i) == ArtEinst) {
      cl_assert_equal_i(einst, -1);
      einst = i;
    }
  }
  cl_assert(einst >= 0);
  cl_assert_equal_i(prv_feld(einst, MESSAGE_KEY_MAXPULS), 183);
  cl_assert_equal_i(prv_feld(einst, MESSAGE_KEY_MAXPULS_GESETZT), 1);
  cl_assert_equal_i(prv_feld(einst, MESSAGE_KEY_EMPFIND), 3);
  cl_assert_equal_i(prv_feld(einst, MESSAGE_KEY_PAUSENZIEL), 120);
  cl_assert_equal_i(prv_feld(einst, MESSAGE_KEY_BECKEN), 50);
  cl_assert_equal_i(prv_feld(einst, MESSAGE_KEY_PIN_ART), 2);
  cl_assert_equal_s(prv_tupel(einst, MESSAGE_KEY_PIN_ZEIT)->value->cstring, "07:30");
  prv_app_schliesst();
  // Nichts davon wurde umgeschrieben: die Datei haelt b25s Bytes.
  const uint32_t keys[] = {1, 2, 3, 4, 5, 6, 12};
  for (size_t k = 0; k < sizeof(keys) / sizeof(keys[0]); k++) {
    uint8_t wert[256];
    const B25Eintrag *e = prv_b25(keys[k]);
    cl_assert_equal_i(prv_datei(keys[k], wert, sizeof(wert)), e->laenge);
    cl_assert(memcmp(wert, e->wert, e->laenge) == 0);
  }
}

// =============================================================================================
// T6.3: Liegengebliebenes (Zusammenfassung 30-34, Stopp 35, Kurve 40-42/50-55) genau einmal

typedef struct {
  int stopp;            //< Zustand 0 des verworfenen Krafttrainings, bestaetigt
  int zusammenfassung;  //< des Laufs, bestaetigt
  int kurvenwerte;      //< bestaetigte Kurvenwerte
  int fremd;            //< etwas anderes als Stopp, Zusammenfassung, Kurve, Einstellungen
  uint8_t kurve[1200];  //< die bestaetigten Werte an ihrer Stelle
  bool gesehen[1200];
  int kurve_doppelt;    //< ein Wert kam zweimal an
} Bei_Boulder;

static uint32_t prv_u32(const uint8_t *b) {
  return b[0] | (b[1] << 8) | (b[2] << 16) | ((uint32_t)b[3] << 24);
}

static void prv_auswerten(Bei_Boulder *b, int ab) {
  const uint32_t stopp_beginn = prv_u32(prv_b25(35)->wert);
  for (int i = ab; i < s_anzahl; i++) {
    if (!s_gesendet[i].bestaetigt) continue;
    switch (prv_art(i)) {
      case ArtZustand:
        if (prv_feld(i, MESSAGE_KEY_ZUSTAND) == 0 &&
            (uint32_t)prv_feld(i, MESSAGE_KEY_BEGINN) == stopp_beginn) {
          b->stopp++;
        } else {
          b->fremd++;
        }
        break;
      case ArtZusammenfassung:
        cl_assert_equal_i((uint32_t)prv_feld(i, MESSAGE_KEY_BEGINN), BEGINN_LAUF);
        b->zusammenfassung++;
        break;
      case ArtKurve: {
        cl_assert_equal_i((uint32_t)prv_feld(i, MESSAGE_KEY_BEGINN), BEGINN_LAUF);
        const int von = prv_feld(i, MESSAGE_KEY_KURVE_AB);
        Tuple *t = prv_tupel(i, MESSAGE_KEY_KURVE);
        for (int k = 0; k < t->length; k++) {
          cl_assert(von + k < 1200);
          b->kurve_doppelt += b->gesehen[von + k];
          b->gesehen[von + k] = true;
          b->kurve[von + k] = t->value->data[k];
        }
        b->kurvenwerte += t->length;
        break;
      }
      case ArtEinst:
        break;
    }
  }
}

// Die Kurve, wie b25 sie abgelegt hat: Anzahl (40), Werte in Stuecken zu 200 (50-55).
static void prv_kurve_wie_b25(const Bei_Boulder *b) {
  const int anzahl = (int)prv_u32(prv_b25(40)->wert);
  cl_assert_equal_i(b->kurvenwerte, anzahl);
  cl_assert_equal_i(b->kurve_doppelt, 0);
  for (int k = 0; k < anzahl; k++) {
    const B25Eintrag *stueck = prv_b25(50 + k / 200);
    cl_assert(stueck);
    cl_assert_equal_i(b->kurve[k], stueck->wert[k % 200]);
  }
}

static void prv_nichts_liegt_mehr(void) {
  uint8_t wert[256];
  const uint32_t weg[] = {30, 31, 32, 33, 34, 35, 40, 41, 42, 50, 51, 52, 53, 54, 55};
  for (size_t k = 0; k < sizeof(weg) / sizeof(weg[0]); k++) {
    cl_assert_equal_i(prv_datei(weg[k], wert, sizeof(wert)), -1);
  }
}

void test_kieselsport_wechsel__liegengebliebenes_genau_einmal(void) {
  cl_assert(prv_b25(30) && prv_b25(35) && prv_b25(40) && prv_b25(50) && prv_b25(51));
  prv_v2_startet();
  prv_app_oeffnet();
  prv_app_bis_ruhe(true);
  prv_app_schliesst();
  Bei_Boulder b = {0};
  prv_auswerten(&b, 0);
  cl_assert_equal_i(b.stopp, 1);
  cl_assert_equal_i(b.zusammenfassung, 1);
  cl_assert_equal_i(b.fremd, 0);
  prv_kurve_wie_b25(&b);
  // Die Zusammenfassung traegt b25s Lauf (abbild_szenario.c: 55 min aktiv, eine Pause von 95 s
  // nach 30 min, Ende am Beginn der letzten Pause).
  for (int i = 0; i < s_anzahl; i++) {
    if (prv_art(i) != ArtZusammenfassung) continue;
    cl_assert_equal_i(prv_feld(i, MESSAGE_KEY_ART), ArtLaufen);
    cl_assert_equal_i(prv_feld(i, MESSAGE_KEY_DAUER), 3300);
    cl_assert_equal_i(prv_feld(i, MESSAGE_KEY_SCHRITTE), 6600);
    cl_assert_equal_i((uint32_t)prv_feld(i, MESSAGE_KEY_ENDE), BEGINN_LAUF + 1800 + 95 + 1500);
    Tuple *p = prv_tupel(i, MESSAGE_KEY_PAUSEN);
    cl_assert(p);
    cl_assert_equal_i(p->length, prv_b25(33)->laenge);
    cl_assert(memcmp(p->value->data, prv_b25(33)->wert, p->length) == 0);
  }
  prv_nichts_liegt_mehr();
  // Das Archiv aus b25 (10, 11, 100-119) bleibt liegen und geht nie hinaus (V7).
  uint8_t wert[256];
  cl_assert_equal_i(prv_datei(100, wert, sizeof(wert)), prv_b25(100)->laenge);

  // Der naechste Start und das naechste Oeffnen: nichts mehr ausser den Einstellungen.
  const int vorher = s_anzahl;
  prv_v2_startet();
  prv_app_oeffnet();
  prv_app_bis_ruhe(true);
  prv_app_schliesst();
  for (int i = vorher; i < s_anzahl; i++) {
    cl_assert_equal_i(prv_art(i), ArtEinst);
  }
}

// Ohne Telefon bleibt alles liegen; ein Neustart mitten im Senden (Zusammenfassung quittiert,
// Kurve zur Haelfte) verliert und verdoppelt nichts.
void test_kieselsport_wechsel__neustart_mitten_im_senden(void) {
  prv_v2_startet();
  s_verbunden = false;
  prv_app_oeffnet();
  prv_warte(30 * 1000);
  cl_assert_equal_i(s_anzahl, 0);
  prv_app_schliesst();

  // Das Telefon kommt; nach der Zusammenfassung und dem ersten Kurvenstueck faellt die Uhr aus.
  s_verbunden = true;
  prv_app_oeffnet();
  bool erstes_stueck = false;
  for (int runde = 0; runde < 400 && !erstes_stueck; runde++) {
    if (s_belegt) {
      erstes_stueck = prv_art(s_anzahl - 1) == ArtKurve;
      prv_antwort(true);
    }
    prv_warte(500);
  }
  cl_assert(erstes_stueck);
  prv_app_schliesst();
  // Neustart: der Dienst liest neu, die App oeffnet wieder.
  prv_v2_startet();
  prv_app_oeffnet();
  prv_app_bis_ruhe(true);
  prv_app_schliesst();

  Bei_Boulder b = {0};
  prv_auswerten(&b, 0);
  cl_assert_equal_i(b.stopp, 1);
  cl_assert_equal_i(b.zusammenfassung, 1);
  cl_assert_equal_i(b.fremd, 0);
  prv_kurve_wie_b25(&b);
  prv_nichts_liegt_mehr();
}
