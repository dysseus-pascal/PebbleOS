/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2: SpO2 im Schlaf (Entscheid vom 22.10.2026) - wann Pebbles SpO2-Plan misst.
//
// DIE REGEL (spo2_plan.c) in allen acht Faellen, dazu Pebbles echter Plan: activity.c mit der
// Variante gebaut wie auf obelix, mit Pebbles echten Prefs (prefs.c, "SpO2" und der Takt)
// auf der echten settings_file.c. Der Umbau stammt aus Pebbles test_activity.c (Attrappen,
// Algorithmus); der Algorithmus meldet die Uhr nicht flach, sonst misst Pebble nie.
// ATTRAPPEN an der Naht zur Variante: der geladene Schalter "Schlaf" und die Nachtruhe der
// laufenden Minute - beide liest test_boulder_messung gegen messstufe.c und puls.c.
// GEZAEHLT: wie oft der Plan sein SpO2-Abo zum Messen einschaltet (prv_spo2_set_sampling).

#include "stubs_app_install_manager.h"
#include "stubs_app_manager.h"
#include "stubs_logging.h"

#include <pbl/drivers/rtc.h>
#include <pbl/drivers/vibe.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/types.h>
#include <pbl/logging/logging.h>
#include <pbl/services/activity/activity.h>
#include <pbl/services/activity/activity_algorithm.h>
#include <pbl/services/activity/activity_private.h>
#include <pbl/services/activity/boulder/spo2_plan.h>
#include <pbl/services/data_logging/data_logging_service.h>
#include <pbl/services/filesystem/pfs.h>
#include <pbl/services/hrm/hrm_manager_private.h>
#include <pbl/services/protobuf_log/protobuf_log.h>
#include <pbl/services/time.h>
#include <pbl/util/size.h>
#include <pbl/util/testing.h>
#include <pbl/util/units.h>

#include <applib/accel_service.h>
#include <applib/data_logging.h>
#include <applib/health_service.h>
#include <applib/health_service_private.h>
#include <clar.h>
#include <fake_accel_service.h>
#include <fake_cron.h>
#include <fake_events.h>
#include <fake_new_timer.h>
#include <fake_pbl_std.h>
#include <fake_rtc.h>
#include <fake_spi_flash.h>
#include <fake_system_task.h>
#include <kernel/events.h>
#include <shell/prefs.h>

// =============================================================================================
// Die Naht zur Variante

static bool s_schlaf;
static bool s_nachtruhe;

bool boulder_spo2_schlaf_geladen(void) {
  return s_schlaf;
}

bool boulder_puls_nachtruhe(void) {
  return s_nachtruhe;
}

// =============================================================================================
// HRM-Manager: merkt sich je Abo Rueckruf und Merkmale.

void prv_spo2_subscription_cb(PebbleHRMEvent *hrm_event, void *context); // PBL_T_STATIC

#define ABOS_MAX 8
static struct {
  HRMSubscriberCallback rueckruf;
  HRMFeature merkmale;
} s_abos[ABOS_MAX];
static HRMSessionRef s_naechstes_abo = 1;
static HRMSessionRef s_spo2_abo; //< das Abo von Pebbles SpO2-Plan
static int s_spo2_fenster;       //< so oft schaltete der Plan es zum Messen ein

HRMSessionRef hrm_manager_subscribe_with_callback(AppInstallId app_id, uint32_t update_interval_s,
                                                  uint16_t expire_s, HRMFeature features,
                                                  bool low_latency, HRMSubscriberCallback callback,
                                                  void *context) {
  const HRMSessionRef abo = s_naechstes_abo++;
  cl_assert(abo < ABOS_MAX);
  s_abos[abo].rueckruf = callback;
  s_abos[abo].merkmale = features;
  if (callback == prv_spo2_subscription_cb) {
    s_spo2_abo = abo;
  }
  return abo;
}

bool sys_hrm_manager_unsubscribe(HRMSessionRef session) {
  cl_assert(session < s_naechstes_abo);
  return true;
}

bool sys_hrm_manager_set_features(HRMSessionRef session, HRMFeature features) {
  cl_assert(session < s_naechstes_abo);
  if (session == s_spo2_abo && (features & HRMFeature_SpO2) &&
      !(s_abos[session].merkmale & HRMFeature_SpO2)) {
    s_spo2_fenster++;
  }
  s_abos[session].merkmale = features;
  return true;
}

bool sys_hrm_manager_set_update_interval(HRMSessionRef session, uint32_t update_interval_s,
                                         uint16_t expire_s) {
  cl_assert(session < s_naechstes_abo);
  return true;
}

bool hrm_manager_has_continuous_green_subscriber(void) {
  return false;
}

void hrm_manager_handle_prefs_changed(void) {
}

HRMSessionRef sys_hrm_manager_app_subscribe(AppInstallId app_id, uint32_t update_interval_s,
                                            uint16_t expire_s, HRMFeature features) {
  return HRM_INVALID_SESSION_REF;
}

HRMSessionRef sys_hrm_manager_get_app_subscription(AppInstallId app_id) {
  return HRM_INVALID_SESSION_REF;
}

bool sys_hrm_manager_get_subscription_info(HRMSessionRef session, AppInstallId *app_id,
                                           uint32_t *update_interval_s, uint16_t *expire_s,
                                           HRMFeature *features) {
  return false;
}

static bool prv_spo2_misst(void) {
  return activity_private_state()->spo2.currently_sampling;
}

// Ein SpO2-Wert, den der Algorithmus verwirft: Der Plan sieht ihn wie jeden Rueckruf.
static void prv_spo2_ereignis(void) {
  PebbleHRMEvent e = {
    .event_type = HRMEvent_SpO2,
    .spo2 = {.percent = 0, .quality = HRMQuality_Worst, .invalid = true},
  };
  prv_spo2_subscription_cb(&e, NULL);
}

// =============================================================================================
// Pebbles Umfeld (aus test_activity.c)

void prefs_sync_init(void) {
}

void health_tracking_ui_feature_show_disabled(void) {
}

void prv_minute_system_task_cb(void *data); // PBL_T_STATIC

static HealthServiceState s_health_service;

HealthServiceState *app_state_get_health_service_state(void) {
  return &s_health_service;
}

HealthServiceState *worker_state_get_health_service_state(void) {
  return &s_health_service;
}

void event_service_client_subscribe(EventServiceInfo *service_info) {
}

void event_service_client_unsubscribe(EventServiceInfo *service_info) {
}

void sys_send_pebble_event_to_kernel(PebbleEvent *event) {
}

UnitsDistance sys_shell_prefs_get_units_distance(void) {
  return UnitsDistance_KM;
}

int32_t vibes_get_vibe_strength(void) {
  return VIBE_STRENGTH_OFF;
}

AppInstallId app_get_app_id(void) {
  return 1;
}

void system_resource_language_changed(void) {
}

bool activity_algorithm_activity_hrm_is_active(void) {
  return false;
}

void activity_algorithm_activity_hrm_set_paused(bool paused) {
}

void workout_service_set_hrm_paused(bool paused) {
}

static int s_queue_value;

void pbl_sem_init(struct pbl_sem *s, uint32_t initial, uint32_t limit) {
}

int pbl_sem_take(struct pbl_sem *s, pbl_timeout_t timeout) {
  while (s_queue_value <= 0) {
    fake_system_task_callbacks_invoke_pending();
  }
  s_queue_value--;
  return 0;
}

void pbl_sem_give(struct pbl_sem *s) {
  s_queue_value++;
}

DataLoggingResult dls_log(DataLoggingSession *logging_session, const void *data,
                          uint32_t num_items) {
  return DATA_LOGGING_SUCCESS;
}

DataLoggingSession *dls_create(uint32_t tag, DataLoggingItemType item_type, uint16_t item_size,
                               bool buffered, bool resume, const Uuid *uuid) {
  return (DataLoggingSession *)1;
}

void dls_finish(DataLoggingSession *logging_session) {
}

ProtobufLogRef protobuf_log_hr_create(void) {
  return (ProtobufLogRef)1;
}

bool protobuf_log_session_delete(ProtobufLogRef session) {
  return true;
}

bool protobuf_log_hr_add_sample(ProtobufLogRef ref, time_t now_utc, uint8_t bpm,
                                HRMQuality quality) {
  return true;
}

// Der Algorithmus: ruhig, nicht flach (z-Achse weder 0 noch 8, activity.c), ohne Schlaf.
#define LAGE_NICHT_FLACH 0x22

bool activity_algorithm_init(AccelSamplingRate *sampling_rate) {
  *sampling_rate = ACCEL_SAMPLING_25HZ;
  return true;
}

void activity_algorithm_early_deinit(void) {
}

bool activity_algorithm_deinit(void) {
  return true;
}

void activity_algorithm_handle_accel(AccelRawData *data, uint32_t num_samples, uint64_t timestamp) {
}

bool activity_algorithm_set_user(uint32_t height_mm, uint32_t weight_g, ActivityGender gender,
                                 uint32_t age_years) {
  return true;
}

bool activity_algorithm_get_steps(uint32_t *steps) {
  *steps = 0;
  return true;
}

bool activity_algorithm_get_step_rate(uint16_t *steps, uint32_t *elapsed_ms, time_t *end_sec) {
  *steps = 0;
  *elapsed_ms = 0;
  *end_sec = rtc_get_time();
  return true;
}

bool activity_algorithm_metrics_changed_notification(void) {
  return true;
}

bool activity_algorithm_get_sleep_sessions(time_t sleep_earliest_end_utc,
                                           time_t *last_processed_utc) {
  *last_processed_utc = rtc_get_time();
  return true;
}

void activity_algorithm_post_process_sleep_sessions(uint16_t num_input_sessions,
                                                    ActivitySession *sessions) {
}

void activity_algorithm_minute_handler(time_t utc_sec, AlgMinuteRecord *record_out) {
  record_out->data.base.orientation = LAGE_NICHT_FLACH;
}

bool activity_algorithm_dump_minute_data_to_log(void) {
  return false;
}

bool activity_algorithm_minute_file_info(bool compact_first, uint32_t *num_records,
                                         uint32_t *data_bytes, uint32_t *minutes) {
  *num_records = 0;
  *data_bytes = 0;
  *minutes = 0;
  return true;
}

bool activity_algorithm_test_fill_minute_file(void) {
  return true;
}

bool activity_algorithm_get_minute_history(HealthMinuteData *minute_data, uint32_t *num_records,
                                           time_t *utc_start) {
  *num_records = 0;
  return true;
}

bool activity_algorithm_get_minute_heart_rate(uint8_t *heart_rate_bpm, uint32_t num_minutes,
                                              time_t utc_start) {
  return false;
}

time_t activity_algorithm_get_last_sleep_utc(void) {
  return 0;
}

bool activity_algorithm_test_send_fake_minute_data_dls_record(void) {
  return true;
}

// =============================================================================================
// Ablauf

#define BEGINN_UTC 1792526400 // 20.10.2026 20:00 UTC, durch 60 teilbar

// Minuten mit Pebbles Minutenlauf (der Plan laeuft am Ende davon).
static void prv_minuten(int anzahl) {
  for (int i = 0; i < anzahl; i++) {
    fake_rtc_increment_time(PBL_SEC_PER_MIN);
    fake_rtc_increment_ticks(PBL_TICK_HZ * PBL_SEC_PER_MIN);
    fake_cron_job_fire();
    fake_system_task_callbacks_invoke_pending();
  }
}

// Laeuft bis zum naechsten Fenster des Plans; Rueckgabe: ob eins kam.
static bool prv_bis_zum_fenster(int max_minuten) {
  const int vorher = s_spo2_fenster;
  for (int i = 0; i < max_minuten && s_spo2_fenster == vorher; i++) {
    prv_minuten(1);
  }
  return s_spo2_fenster > vorher;
}

void test_boulder_spo2_plan__initialize(void) {
  fake_rtc_init(100 /*initial_ticks*/, BEGINN_UTC);
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  pfs_format(false);
  memset(s_abos, 0, sizeof(s_abos));
  s_naechstes_abo = 1;
  s_spo2_abo = HRM_INVALID_SESSION_REF;
  s_spo2_fenster = 0;
  s_schlaf = false;
  s_nachtruhe = false;
  activity_init();
  activity_set_enabled(true);
  fake_system_task_callbacks_invoke_pending();
  activity_prefs_set_spo2_enabled(false);
  activity_start_tracking(false /*test_mode*/);
  fake_system_task_callbacks_invoke_pending();
  cl_assert(s_spo2_abo != HRM_INVALID_SESSION_REF);
}

void test_boulder_spo2_plan__cleanup(void) {
  activity_stop_tracking();
  fake_system_task_callbacks_invoke_pending();
}

// =============================================================================================
// Die Regel

void test_boulder_spo2_plan__regel_in_allen_faellen(void) {
  for (int i = 0; i < 8; i++) {
    const bool intervall = i & 1;
    const bool schlaf = i & 2;
    const bool nachtruhe = i & 4;
    const bool soll = intervall || (schlaf && nachtruhe);
    cl_assert_equal_b(boulder_spo2_plan(intervall, schlaf, nachtruhe), soll);
    s_schlaf = schlaf;
    s_nachtruhe = nachtruhe;
    cl_assert_equal_b(boulder_spo2_plan_offen(intervall), soll);
  }
}

// =============================================================================================
// Pebbles Plan

// Nur "Schlaf" an: Tags (keine Nachtruhe) misst der Plan nie, auch nicht in zwei Stunden.
void test_boulder_spo2_plan__schlaf_tags_nie(void) {
  s_schlaf = true;
  prv_minuten(120);
  cl_assert_equal_i(s_spo2_fenster, 0);
  cl_assert(!prv_spo2_misst());
}

// Nur "Schlaf" an und Nachtruhe: Der Plan misst in seinem Takt (Vorgabe 10 min, mit Pebbles
// Vortritt fuer den Puls etwas seltener).
void test_boulder_spo2_plan__schlaf_in_nachtruhe(void) {
  s_schlaf = true;
  s_nachtruhe = true;
  cl_assert(prv_bis_zum_fenster(15));
  cl_assert(prv_spo2_misst());
  prv_minuten(120);
  printf("\nSchlaf, Nachtruhe: %d Fenster in gut zwei Stunden", s_spo2_fenster);
  cl_assert(s_spo2_fenster >= 10 && s_spo2_fenster <= 13);
}

// Der Takt bleibt der der Messstufe: alle 30 min (Stufe 3).
void test_boulder_spo2_plan__takt_der_stufe(void) {
  activity_prefs_set_spo2_measurement_interval(HRMonitoringInterval_30Min);
  s_schlaf = true;
  s_nachtruhe = true;
  prv_minuten(120);
  printf("\nSchlaf, Nachtruhe, 30 min: %d Fenster in zwei Stunden", s_spo2_fenster);
  cl_assert(s_spo2_fenster >= 3 && s_spo2_fenster <= 4);
}

// Nachtruhe ohne "Schlaf" (und ohne "SpO2"): nie.
void test_boulder_spo2_plan__nachtruhe_ohne_schalter_nie(void) {
  s_nachtruhe = true;
  prv_minuten(120);
  cl_assert_equal_i(s_spo2_fenster, 0);
}

// "SpO2" (Intervall) misst wie bei Pebble, tags wie nachts, mit und ohne "Schlaf".
void test_boulder_spo2_plan__intervall_wie_pebble(void) {
  activity_prefs_set_spo2_enabled(true);
  for (int i = 0; i < 4; i++) {
    s_schlaf = i & 1;
    s_nachtruhe = i & 2;
    cl_assert(prv_bis_zum_fenster(15));
  }
}

// Endet die Nachtruhe mitten in einer Messung, endet die Messung beim naechsten Wert des Sensors,
// wie wenn "SpO2" ausgeht - nicht erst nach Pebbles 45 s ohne guten Wert.
void test_boulder_spo2_plan__ende_der_nachtruhe_beendet_die_messung(void) {
  s_schlaf = true;
  s_nachtruhe = true;
  cl_assert(prv_bis_zum_fenster(15));
  fake_rtc_increment_time(5);
  fake_rtc_increment_ticks(5 * PBL_TICK_HZ);
  prv_spo2_ereignis();
  cl_assert(prv_spo2_misst()); // in Nachtruhe misst er weiter
  s_nachtruhe = false;
  fake_rtc_increment_time(1);
  fake_rtc_increment_ticks(PBL_TICK_HZ);
  prv_spo2_ereignis();
  cl_assert(!prv_spo2_misst());
  cl_assert_equal_i(s_abos[s_spo2_abo].merkmale, 0);
  // Danach kein neues Fenster ohne Nachtruhe.
  const int fenster = s_spo2_fenster;
  prv_minuten(60);
  cl_assert_equal_i(s_spo2_fenster, fenster);
}
