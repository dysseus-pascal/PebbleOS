/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/activity_private.h"
#include "pbl/services/activity/boulder/sport/kieselsport.h"
#include "pbl/services/activity/workout_service.h"
#include "pbl/services/hrm/hrm_manager.h"
#include "pbl/services/hrm/hrm_manager_private.h"
#include "pbl/services/new_timer/new_timer.h"
#include "pbl/services/regular_timer.h"
#include "applib/accel_service_private.h"
#include "applib/event_service_client.h"
#include "applib/ui/vibes.h"
#include "kernel/events.h"
#include "process_management/app_install_manager.h"
#include "process_management/app_manager.h"
#include <pbl/drivers/hrm.h>
#include <pbl/drivers/rtc.h>

#include "fake_events.h"
#include "fake_pbl_malloc.h"
#include "fake_rtc.h"
#include "fake_system_task.h"

#include "stubs_accel_manager.h"
#include "stubs_analytics.h"
#include "stubs_evented_timer.h"
#include "stubs_hexdump.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"

// T2.2 (Spezifikation Schritt 2, 6.1; F2 = A): die HRM-Abos im Training gegen den ECHTEN
// hrm_manager.c. Kieselsports Dienst mit seiner echten Plattform und Pebbles echter Workout-Dienst
// (mit P1); nachgebaut sind der Sensor-Treiber, die Zeitgeber und das Profil.
//
// Rot, wenn im Training mehr als ein Abo mit 1 s besteht, wenn Pebbles Workout-Dienst ein Abo
// anlegt, wenn die Pause nicht auf 10 s geht oder nach Stopp und Verwerfen ein Abo bleibt (T7.4).
// Pebbles eigene Messfenster (Activity, Messstufe) und das Velo-Abo der Variante sind hier nicht
// gebaut: Ruhen des Velo-Abos im Training prueft Schritt 1 (kontext.c, D15).

extern void prv_workout_timer_cb(void *data);
extern void workout_service_reset(void);
extern void kieselsport_test_neustart(void);
extern void fake_ruhepuls_setzen(uint8_t bpm);

// =============================================================================================
// Grenzen

static struct {
  bool an;
  HRMFeature merkmale;
} s_sensor;
bool hrm_enable(HRMDevice *dev, HRMFeature features, bool low_latency) {
  s_sensor.an = true;
  s_sensor.merkmale = features;
  return true;
}
void hrm_disable(HRMDevice *dev) { s_sensor.an = false; }
bool hrm_is_enabled(HRMDevice *dev) { return s_sensor.an; }
void hrm_set_activity_scene(HRMDevice *dev, HRMActivityScene scene) {}
bool battery_is_usb_connected(void) { return false; }
int pbl_msgq_put(struct pbl_msgq *q, const void *msg, pbl_timeout_t timeout) { return 0; }
struct pbl_msgq *pebble_task_get_to_queue(PebbleTask task) {
  return (task == PebbleTask_App) ? (struct pbl_msgq *)0x1 : NULL;
}
void event_service_client_subscribe(EventServiceInfo *info) {}
void event_service_client_unsubscribe(EventServiceInfo *info) {}

bool activity_prefs_heart_rate_is_enabled(void) { return true; }
bool activity_prefs_blood_oxygen_is_enabled(void) { return false; }
bool activity_prefs_blood_oxygen_activity_tracking_is_enabled(void) { return false; }
bool activity_prefs_hrm_activity_tracking_is_enabled(void) { return true; }
HRMonitoringInterval activity_prefs_get_hrm_measurement_interval(void) {
  return HRMonitoringInterval_10Min;
}
HRMonitoringInterval activity_prefs_get_spo2_measurement_interval(void) {
  return HRMonitoringInterval_10Min;
}
uint8_t activity_prefs_get_age_years(void) { return 41; }
ActivityGender activity_prefs_get_gender(void) { return ActivityGenderMale; }
uint16_t activity_prefs_get_weight_dag(void) { return 7000; }
uint16_t activity_prefs_get_height_mm(void) { return 1800; }
uint8_t activity_prefs_heart_get_elevated_hr(void) { return 100; }
uint8_t activity_prefs_heart_get_zone1_threshold(void) { return 130; }
uint8_t activity_prefs_heart_get_zone2_threshold(void) { return 154; }
uint8_t activity_prefs_heart_get_zone3_threshold(void) { return 172; }

static ActivityState s_activity;
ActivityState *activity_private_state(void) { return &s_activity; }

// Pebbles Workout-Dienst: Sitzungsliste und Meldung zaehlen nur (T2.1 steht in test_kieselsport).
static int s_sitzungen;
void activity_sessions_prv_add_activity_session(ActivitySession *session) {
  s_sitzungen += session->manual;
}
void activity_sessions_prv_delete_activity_session(ActivitySession *session) {}
bool activity_get_sessions(uint32_t *session_entries, ActivitySession *sessions) {
  *session_entries = 0;
  return true;
}
bool activity_get_metric(ActivityMetric metric, uint32_t history_len, int32_t *history) {
  *history = 0;
  return true;
}
void activity_algorithm_enable_activity_tracking(bool enable) {}
void activity_insights_push_activity_session_notification(time_t notif_time,
                                                          ActivitySession *session, int32_t avg_hr,
                                                          int32_t *hr_zone_time_s) {}
void workout_utils_send_abandoned_workout_notification(void) {}
AppInstallId app_get_app_id(void) { return 0; }
uint32_t time_get_uptime_seconds(void) { return PBL_SEC_PER_DAY + rtc_get_time(); }
void dls_send_all_sessions(void) {}

// Plattform: Sensoren ausser Puls, Brummen, App
AccelServiceState *accel_session_create(void) { return NULL; }
void accel_session_delete(AccelServiceState *session) {}
void accel_session_data_subscribe(AccelServiceState *session, uint32_t samples_per_update,
                                  AccelDataHandler handler) {}
void accel_session_data_unsubscribe(AccelServiceState *session) {}
void vibes_enqueue_custom_pattern(VibePattern muster) {}
static uint8_t s_akku;
BatteryChargeState battery_get_charge_state(void) {
  return (BatteryChargeState){.charge_percent = s_akku};
}
bool app_manager_is_watchface_running(void) { return false; }
void app_manager_put_launch_app_event(const AppLaunchEventConfig *config) {}
AppInstallId app_install_get_id_for_uuid(const Uuid *uuid) { return INSTALL_ID_INVALID; }
void app_install_mark_prioritized(AppInstallId install_id, bool can_expire) {}
void launcher_task_add_callback(CallbackEventCallback cb, void *daten) { cb(daten); }

// Persist von Kieselsport: leer, nichts liegt an
bool ks_persist_exists(uint32_t key) { return false; }
int32_t ks_persist_read_int(uint32_t key) { return 0; }
void ks_persist_write_int(uint32_t key, int32_t wert) {}
int ks_persist_read_data(uint32_t key, void *puffer, size_t groesse) { return 0; }
void ks_persist_write_data(uint32_t key, const void *daten, size_t groesse) {}
void ks_persist_delete(uint32_t key) {}
void ks_persist_offen_halten(bool an) {}

// Zeitgeber: Sekunden (Pebbles Workout-Timer, Kieselsports Takt) und einmalige (der Manager,
// Kieselsports Datalogging-Anstoss)
static RegularTimerInfo *s_takte[4];
void regular_timer_add_seconds_callback(RegularTimerInfo *cb) {
  for (int i = 0; i < 4; i++) {
    if (s_takte[i] == cb) return;
  }
  for (int i = 0; i < 4; i++) {
    if (!s_takte[i]) {
      s_takte[i] = cb;
      return;
    }
  }
  cl_fail("zu viele Takte");
}
void regular_timer_add_minutes_callback(RegularTimerInfo *cb) {}
bool regular_timer_is_scheduled(RegularTimerInfo *cb) {
  for (int i = 0; i < 4; i++) {
    if (s_takte[i] == cb) return true;
  }
  return false;
}
bool regular_timer_remove_callback(RegularTimerInfo *cb) {
  for (int i = 0; i < 4; i++) {
    if (s_takte[i] == cb) {
      s_takte[i] = NULL;
      return true;
    }
  }
  return false;
}

typedef struct {
  bool laeuft;
  time_t faellig;
  NewTimerCallback cb;
  void *daten;
} Wecker;
static Wecker s_wecker[32];
static int s_wecker_anzahl;
TimerID new_timer_create(void) {
  cl_assert(s_wecker_anzahl < 32);
  return (TimerID)(s_wecker_anzahl++ + 1);
}
bool new_timer_start(TimerID t, uint32_t ms, NewTimerCallback cb, void *daten, uint32_t flags) {
  s_wecker[t - 1] = (Wecker){.laeuft = true, .faellig = rtc_get_time() + ms / 1000, .cb = cb,
                             .daten = daten};
  return true;
}
bool new_timer_stop(TimerID t) {
  s_wecker[t - 1].laeuft = false;
  return true;
}
void new_timer_delete(TimerID t) {}
bool new_timer_scheduled(TimerID t, uint32_t *ms) { return s_wecker[t - 1].laeuft; }

// =============================================================================================
// Ablauf

#define BEGINN 1790445600

static void prv_kernel_bg(void) {
  fake_system_task_callbacks_invoke_pending();
}

// Eine Sekunde: RTC, die Takte und Wecker auf dem NewTimer, ein Messwert des laufenden Sensors
// durch den echten Manager, dann KernelBG.
static void prv_sekunde(uint8_t bpm) {
  fake_rtc_increment_time(1);
  fake_rtc_increment_ticks(RTC_TICKS_HZ);
  stub_pebble_tasks_set_current(PebbleTask_NewTimers);
  for (int i = 0; i < 4; i++) {
    if (s_takte[i]) s_takte[i]->cb(s_takte[i]->cb_data);
  }
  for (int i = 0; i < s_wecker_anzahl; i++) {
    if (s_wecker[i].laeuft && rtc_get_time() >= s_wecker[i].faellig) {
      s_wecker[i].laeuft = false;
      s_wecker[i].cb(s_wecker[i].daten);
    }
  }
  stub_pebble_tasks_set_current(PebbleTask_App);
  if (s_sensor.an && bpm) {
    HRMData d = {.features = HRMFeature_BPM, .hrm_bpm = bpm, .hrm_quality = HRMQuality_Good};
    hrm_manager_new_data_cb(&d);
  }
  prv_kernel_bg();
}

static void prv_sekunden(int n, uint8_t bpm) {
  for (int i = 0; i < n; i++) prv_sekunde(bpm);
}

static void prv_befehl(void (*befehl)(void)) {
  befehl();
  prv_kernel_bg();
}
static void prv_starte_laufen(void) { kieselsport_starte(ArtLaufen, BEGINN); }

// Die geltenden Abos im echten Manager: wie viele, wie viele davon mit Takt [takt_s]. Ein
// abgelaufenes Abo bleibt in der Liste des Managers, er uebergeht es nur (hrm_manager.c:360);
// hier hat jedes Abo einen Ablauf (Kieselsport: 5 min), 0 s Rest heisst abgelaufen.
static int prv_abos(uint32_t takt_s, int *mit_takt) {
  int alle = 0;
  *mit_takt = 0;
  for (HRMSessionRef ref = 1; ref < 64; ref++) {
    uint32_t takt = 0;
    uint16_t rest = 0;
    HRMFeature merkmale = 0;
    if (sys_hrm_manager_get_subscription_info(ref, NULL, &takt, &rest, &merkmale) && rest > 0) {
      alle++;
      *mit_takt += (takt == takt_s && (merkmale & HRMFeature_BPM));
    }
  }
  return alle;
}

void test_kieselsport_abo__initialize(void) {
  fake_rtc_init(100, BEGINN - 30);
  fake_event_init();
  fake_system_task_callbacks_cleanup();
  system_task_set_available_space(100);
  stub_pebble_tasks_set_current(PebbleTask_KernelBackground);
  memset(&s_sensor, 0, sizeof(s_sensor));
  memset(s_takte, 0, sizeof(s_takte));
  memset(&s_activity, 0, sizeof(s_activity));
  s_sitzungen = 0;
  s_akku = 80;
  fake_ruhepuls_setzen(0);
  hrm_manager_init();
  hrm_manager_enable(true);
  workout_service_reset();
  workout_service_init();
  kieselsport_test_neustart();
  kieselsport_init();
  stub_pebble_tasks_set_current(PebbleTask_App);
  prv_kernel_bg();
}

void test_kieselsport_abo__cleanup(void) {
  if (kieselsport_laeuft()) {
    prv_befehl(kieselsport_verwerfen);
  }
  prv_kernel_bg();
  workout_service_reset();
}

// Im Training genau EIN Abo, Kieselsports mit 1 s: Pebbles Workout-Dienst legt keines an
// (frontend_opened bleibt ungerufen, F2 = A), sonst staenden hier zwei. Der Sensor laeuft, und
// Kieselsport bekommt die Werte durch den echten Manager. In der Pause 10 s, sparsam 5 s.
void test_kieselsport_abo__ein_abo_im_training(void) {
  int eins = 0;
  cl_assert_equal_i(prv_abos(1, &eins), 0);
  prv_befehl(prv_starte_laufen);
  cl_assert_equal_i(prv_abos(1, &eins), 1);
  cl_assert_equal_i(eins, 1);
  prv_sekunden(5, 0);
  cl_assert(s_sensor.an);
  cl_assert(s_sensor.merkmale & HRMFeature_BPM);
  prv_sekunden(5, 142);
  KsStand s;
  kieselsport_stand(&s);
  cl_assert_equal_i(s.puls, 142);
  cl_assert(s.frisch);
  // Ueber Minuten: es bleibt eines (minuetlich erneuert, nicht neu angelegt).
  prv_sekunden(180, 142);
  cl_assert_equal_i(prv_abos(1, &eins), 1);
  cl_assert_equal_i(eins, 1);

  prv_befehl(kieselsport_pause_umschalten);
  int zehn = 0;
  cl_assert_equal_i(prv_abos(10, &zehn), 1);
  cl_assert_equal_i(zehn, 1);
  prv_befehl(kieselsport_pause_umschalten);
  cl_assert_equal_i(prv_abos(1, &eins), 1);
  cl_assert_equal_i(eins, 1);

  s_akku = 15;
  prv_sekunden(60, 142);
  int fuenf = 0;
  cl_assert_equal_i(prv_abos(5, &fuenf), 1);
  cl_assert_equal_i(fuenf, 1);
}

// T7.4: Nach dem Speichern (auch in den 30 s bis Pebbles Stopp) und nach dem Verwerfen gibt es
// kein Abo mehr; der Sensor geht aus.
void test_kieselsport_abo__kein_abo_nach_stopp_und_verwerfen(void) {
  int eins = 0;
  prv_befehl(prv_starte_laufen);
  prv_sekunden(90, 140);
  prv_befehl(kieselsport_speichern);
  cl_assert_equal_i(prv_abos(1, &eins), 0);
  prv_sekunden(31, 140);
  cl_assert_equal_i(s_sitzungen, 1);
  cl_assert_equal_i(prv_abos(1, &eins), 0);
  cl_assert(!s_sensor.an);

  prv_befehl(prv_starte_laufen);
  prv_sekunden(90, 140);
  cl_assert_equal_i(prv_abos(1, &eins), 1);
  prv_befehl(kieselsport_verwerfen);
  cl_assert_equal_i(prv_abos(1, &eins), 0);
  prv_sekunden(5, 0);
  cl_assert(!s_sensor.an);
  cl_assert_equal_i(s_sitzungen, 1);
}

// Haengt der Dienst (kein Takt mehr), laeuft das Abo nach 5 min von selbst ab: Der Sensor misst
// nicht die Nacht durch im Sekundentakt.
void test_kieselsport_abo__abo_laeuft_ohne_takt_ab(void) {
  int eins = 0;
  prv_befehl(prv_starte_laufen);
  prv_sekunden(10, 140);
  for (int i = 0; i < 4; i++) {
    s_takte[i] = NULL;  // weder Pebbles Timer noch Kieselsports Takt
  }
  prv_sekunden(5 * 60 + 5, 140);
  cl_assert_equal_i(prv_abos(1, &eins), 0);
  prv_sekunden(5, 0);
  cl_assert(!s_sensor.an);
}
