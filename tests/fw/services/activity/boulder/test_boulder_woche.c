/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2, Schritt 3, AP4: die Woche der Health-App gegen Pebbles echten Tageswert
// (Spezifikation Schritt 3, 3.8.3, Entscheid F9 = A; 9.1: T4.5).
//
// ECHT: Pebbles activity.c, activity_metrics.c und activity_sessions.c - der Minutenlauf mit der
// Verschiebung der Historie um Mitternacht und dem Zuruecksetzen danach -, Pebbles healthdb
// (health_db.c) mit dem Weg, auf dem das Telefon Tageswerte schreibt, und woche.c. Der Umbau dieser
// Umgebung stammt aus Pebbles test_activity.c (Attrappen, Algorithmus mit den Schritten aus den
// Beschleunigungsdaten).
// ATTRAPPE an der Naht: Der Minutenlauf des Algorithmus gibt die eben fertige Minute an die Woche
// wie die Glue-Kopie an den Dauerteil (D2: der Satz traegt den Beginn der Minute, utc - 60). Die
// Ablage der Variante steht als Ruf auf woche.c: alle 15 min zur Minute 3 und nach dem Anstoss des
// Tageswechsels (speicher.c, dauerteil.c); "Laden" ebenso. Ein Neustart der Variante beginnt woche.c
// neu und laedt aus boulder_v2 - Pebble laeuft dabei weiter.
//
// GEZAEHLT: wie oft "woche" je Tag geschrieben wird (T4.5: hoechstens 25).

#include "pbl/kernel/types.h"
#include "applib/accel_service.h"
#include "applib/data_logging.h"
#include "applib/health_service.h"
#include "applib/health_service_private.h"
#include <pbl/drivers/rtc.h>
#include <pbl/drivers/vibe.h>
#include "kernel/events.h"
#include "pbl/services/hrm/hrm_manager_private.h"
#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/activity_algorithm.h"
#include "pbl/services/activity/activity_private.h"
#include "pbl/services/data_logging/data_logging_service.h"
#include "pbl/services/filesystem/pfs.h"
#include "pbl/services/protobuf_log/protobuf_log.h"
#include "shell/prefs.h"
#include <pbl/logging/logging.h>
#include "system/passert.h"
#include "pbl/util/math.h"
#include "pbl/util/size.h"

#include <sys/stat.h>

#include "clar.h"
#include "pbl/services/time.h"
#include "pbl/util/time.h"
#include "pbl/util/units.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

// Stubs
#include "stubs_activity_insights.h"
#include "stubs_alarm.h"
#include "stubs_app_manager.h"
#include "stubs_analytics.h"
#include "stubs_app_install_manager.h"
#include "stubs_battery.h"
#include "stubs_event_loop.h"
#include "stubs_irq.h"
#include "stubs_hexdump.h"
#include "stubs_i18n.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pebble_process_info.h"
#include "stubs_sleep.h"
#include "stubs_system_theme.h"
#include "stubs_task_wdt.h"
#include "stubs_timeline_peek.h"
#include "stubs_worker_manager.h"
#include "stubs_workout_service.h"
#include "stubs_ambient_light.h"
#include "pbl/util/testing.h"

void prefs_sync_init(void) {
}

// Fakes
#include "fake_accel_service.h"
#include "fake_cron.h"
#include "fake_events.h"
#include "fake_new_timer.h"
#include "fake_pbl_std.h"
#include "fake_rtc.h"
#include "fake_spi_flash.h"
#include "fake_system_task.h"

// We start time out at 5pm on Jan 1, 2015 for all of these tests
static const struct tm s_init_time_tm = {
  // Thursday, Jan 1, 2015, 5:pm
  .tm_hour = 17,
  .tm_mday = 1,
  .tm_mon = 0,
  .tm_year = 115
};

#define ACTIVITY_FIXTURE_PATH "activity"

// The expected resting kcalories is determined empirically from a known good commit and
// is based on the current time of day and the user's weight, age etc.
const int s_exp_5pm_resting_kcalories = 1031;
const int s_exp_full_day_resting_kcalories = 1455;

// Stub for health tracking disabled UI
void health_tracking_ui_feature_show_disabled(void) {
}

// These are declared as PBL_T_STATIC in activity.c
void prv_hrm_subscription_cb(PebbleHRMEvent *hrm_event, void *context);
void prv_minute_system_task_cb(void *data);

void hrm_manager_handle_prefs_changed(void) {
}

#define ASSERT_EQUAL_I(i1, i2, file, line) \
  clar__assert_equal_i((i1), (i2), file, line, #i1 " != " #i2, 1)

// ======================================================================================
// Misc stubs
static HealthServiceState s_health_service;

HealthServiceState *app_state_get_health_service_state(void) {
  return &s_health_service;
}

HealthServiceState *worker_state_get_health_service_state(void) {
  cl_fail("should never be called");
  return NULL;
}

void event_service_client_subscribe(EventServiceInfo *service_info) {
}
void event_service_client_unsubscribe(EventServiceInfo *service_info) {
}

void sys_send_pebble_event_to_kernel(PebbleEvent *event) {
}

static UnitsDistance s_units_distance_result;

UnitsDistance sys_shell_prefs_get_units_distance(void) {
  return s_units_distance_result;
}

int32_t vibes_get_vibe_strength(void) {
  return VIBE_STRENGTH_OFF;
}

HRMSessionRef s_hrm_next_session_ref = 1;
static uint32_t s_hrm_manager_update_interval;
static int s_hrm_manager_num_update_interval_changes;
static uint16_t s_hrm_manager_expire_s;
HRMSessionRef hrm_manager_subscribe_with_callback(AppInstallId app_id, uint32_t update_interval_s,
                                                  uint16_t expire_s, HRMFeature features,
                                                  bool low_latency, HRMSubscriberCallback callback,
                                                  void *context) {
  s_hrm_manager_update_interval = update_interval_s;
  s_hrm_manager_expire_s = expire_s;
  return s_hrm_next_session_ref++;
}

bool sys_hrm_manager_unsubscribe(HRMSessionRef session) {
  cl_assert(session < s_hrm_next_session_ref);
  return true;
}

bool sys_hrm_manager_set_features(HRMSessionRef session, HRMFeature features) {
  return true;
}

bool hrm_manager_has_continuous_green_subscriber(void) {
  return false;
}

// Activity-SpO2 / HR-pause APIs referenced by activity.c. Stubbed to keep the auto-activity HR
// path idle so these tests exercise only the daily HR/SpO2 schedulers.
bool activity_algorithm_activity_hrm_is_active(void) {
  return false;
}

void activity_algorithm_activity_hrm_set_paused(bool paused) {
  (void)paused;
}

bool sys_hrm_manager_set_update_interval(HRMSessionRef session, uint32_t update_interval_s,
                                         uint16_t expire_s) {
  cl_assert(session < s_hrm_next_session_ref);
  s_hrm_manager_update_interval = update_interval_s;
  s_hrm_manager_expire_s = expire_s;
  s_hrm_manager_num_update_interval_changes++;
  return true;
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

AppInstallId app_get_app_id(void) {
  return 1;
}

// ======================================================================================
// Queue stubs, to support the semaphore that activity.c uses to block on a kernel BG callback
int s_queue_value = 0;

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

// =============================================================================================
// Data logging stubs
typedef enum {
  DataLoggingSession_AccelSamples = 1,
  DataLoggingSession_ActivitySessions = 2,
} DataLoggingSession_ID;

// Logged items
static bool s_dls_accel_samples_created;
static int s_num_dls_accel_records;
static ActivityRawSamplesRecord s_dls_accel_records[100];

static bool s_dls_activity_sessions_created;
static int s_num_dls_activity_records;
static ActivitySessionDataLoggingRecord s_dls_activity_records[100];

static void prv_reset_captured_dls_data(void) {
  s_num_dls_accel_records = 0;
  s_num_dls_activity_records = 0;
}

DataLoggingResult dls_log(DataLoggingSession *logging_session, const void *data,
                          uint32_t num_items) {
  if (logging_session == (DataLoggingSession *)DataLoggingSession_AccelSamples) {
    cl_assert(s_dls_accel_samples_created);

    ActivityRawSamplesRecord *records = (ActivityRawSamplesRecord *)data;
    for (int i = 0; i < num_items; i++) {
      cl_assert(s_num_dls_accel_records < ARRAY_LENGTH(s_dls_accel_records));
      s_dls_accel_records[s_num_dls_accel_records++] = records[i];
    }

  } else if (logging_session == (DataLoggingSession *)DataLoggingSession_ActivitySessions) {
    cl_assert(s_dls_activity_sessions_created);

    ActivitySessionDataLoggingRecord *records = (ActivitySessionDataLoggingRecord *)data;
    for (int i = 0; i < num_items; i++) {
      if (s_num_dls_activity_records >= 100) {
        cl_assert(false);
      }
      cl_assert(s_num_dls_activity_records < ARRAY_LENGTH(s_dls_activity_records));
      s_dls_activity_records[s_num_dls_activity_records++] = records[i];
    }

  } else {
    return DATA_LOGGING_INVALID_PARAMS;
  }

  return DATA_LOGGING_SUCCESS;
}

DataLoggingSession *dls_create(uint32_t tag, DataLoggingItemType item_type, uint16_t item_size,
                               bool buffered, bool resume, const Uuid *uuid) {
  if (tag == DlsSystemTagActivityAccelSamples) {
    s_dls_accel_samples_created = true;
    cl_assert_equal_i(item_size, sizeof(ActivityRawSamplesRecord));
    return (DataLoggingSession *)DataLoggingSession_AccelSamples;

  } else if (tag == DlsSystemTagActivitySession) {
    s_dls_activity_sessions_created = true;
    cl_assert_equal_i(item_size, sizeof(ActivitySessionDataLoggingRecord));
    return (DataLoggingSession *)DataLoggingSession_ActivitySessions;

  } else {
    return NULL;
  }
}

void dls_finish(DataLoggingSession *logging_session) {
  if (logging_session == (DataLoggingSession *)DataLoggingSession_AccelSamples) {
    s_dls_accel_samples_created = false;
  } else if (logging_session == (DataLoggingSession *)DataLoggingSession_ActivitySessions) {
    s_dls_activity_sessions_created = false;
  } else {
    cl_assert(false);
  }
}

// =================================================================================
// Measurement logging stubs
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

// =============================================================================================
// Assertion utilities
// --------------------------------------------------------------------------------------
static void prv_assert_equal_metric_history(ActivityMetric metric,
                                            const uint32_t expected[ACTIVITY_HISTORY_DAYS],
                                            char *file, int line) {
  int32_t actual[ACTIVITY_HISTORY_DAYS];
  activity_get_metric(metric, ACTIVITY_HISTORY_DAYS, actual);
  for (int i = 0; i < ACTIVITY_HISTORY_DAYS; i++) {
    ASSERT_EQUAL_I(actual[i], expected[i], file, line);
  }
}

#define ASSERT_EQUAL_METRIC_HISTORY(metric, expected) \
  prv_assert_equal_metric_history((metric), (expected), __FILE__, __LINE__)

static void prv_assert_dls_activity_record_present(ActivitySessionDataLoggingRecord *record,
                                                   char *file, int line) {
  for (int i = 0; i < s_num_dls_activity_records; i++) {
    if (!memcmp(record, &s_dls_activity_records[i], sizeof(*record))) {
      return;
    }
  }
  printf("\nFound records:");
  for (int i = 0; i < s_num_dls_activity_records; i++) {
    printf("\ntype: %d, start_utc: %" PRIu32 ", elapsed: %" PRIu32 ", utc_to_local: %" PRIu32 " ",
           (int)s_dls_activity_records[i].activity, (uint32_t)s_dls_activity_records[i].start_utc,
           s_dls_activity_records[i].elapsed_sec, s_dls_activity_records[i].utc_to_local);
  }
  printf("\nLooking for: type: %d, start_utc: %" PRIu32 ", elapsed: %" PRIu32
         ", "
         "utc_to_local: %" PRIu32 " ",
         (int)record->activity, (uint32_t)record->start_utc, record->elapsed_sec,
         record->utc_to_local);
  clar__assert(false, file, line, "Missing activity record", "", true);
}

#define ASSERT_ACTIVITY_DLS_RECORD_PRESENT(record) \
  prv_assert_dls_activity_record_present((record), __FILE__, __LINE__)

// Assert that given number of activity sessions are present
static void prv_assert_num_activities(uint32_t num_expected, char *file, int line) {
  ActivitySession sessions[ACTIVITY_MAX_ACTIVITY_SESSIONS_COUNT];
  uint32_t num_sessions = ACTIVITY_MAX_ACTIVITY_SESSIONS_COUNT;
  activity_get_sessions(&num_sessions, sessions);
  if (num_sessions != num_expected) {
    printf("Expected %" PRIu32 " activities, but found %" PRIu32 ".\n", num_expected, num_sessions);
  }
  clar__assert(num_sessions == num_expected, file, line, "wrong number of activities", "", true);
}

// Assert that a particular step activity session is present in the sessions list
static void prv_assert_step_activity_present(ActivitySession *exp_session, char *file, int line) {
  ActivitySession sessions[ACTIVITY_MAX_ACTIVITY_SESSIONS_COUNT];
  uint32_t num_sessions = ACTIVITY_MAX_ACTIVITY_SESSIONS_COUNT;
  activity_get_sessions(&num_sessions, sessions);
  for (int i = 0; i < num_sessions; i++) {
    if (sessions[i].type == exp_session->type && sessions[i].start_utc == exp_session->start_utc &&
        sessions[i].length_min == exp_session->length_min &&
        sessions[i].step_data.active_kcalories == exp_session->step_data.active_kcalories &&
        sessions[i].step_data.resting_kcalories == exp_session->step_data.resting_kcalories &&
        sessions[i].step_data.distance_meters == exp_session->step_data.distance_meters &&
        sessions[i].step_data.steps == exp_session->step_data.steps) {
      return;
    }
  }
  printf("\nFound activities:");
  for (int i = 0; i < num_sessions; i++) {
    printf("\nFound:       type: %d, start_utc: %d, len: %" PRIu16 ", steps: %" PRIu16
           ", "
           "rest_cal: %" PRIu32 ", active_cal: %" PRIu32 ", dist: %" PRIu32 " ",
           (int)sessions[i].type, (int)sessions[i].start_utc, sessions[i].length_min,
           sessions[i].step_data.steps, sessions[i].step_data.resting_kcalories,
           sessions[i].step_data.active_kcalories, sessions[i].step_data.distance_meters);
  }
  printf("\nLooking for: type: %d, start_utc: %d, len: %" PRIu16 ", steps: %" PRIu16
         ", "
         "rest_cal: %" PRIu32 ", active_cal: %" PRIu32 ", dist: %" PRIu32 " ",
         (int)exp_session->type, (int)exp_session->start_utc, exp_session->length_min,
         exp_session->step_data.steps, exp_session->step_data.resting_kcalories,
         exp_session->step_data.active_kcalories, exp_session->step_data.distance_meters);
  clar__assert(false, file, line, "Missing activity record", "", true);
}

// Assert that a particular sleep activity session is present in the sessions list
static void prv_assert_sleep_activity_present(ActivitySession *exp_session, char *file, int line) {
  ActivitySession sessions[ACTIVITY_MAX_ACTIVITY_SESSIONS_COUNT];
  uint32_t num_sessions = ACTIVITY_MAX_ACTIVITY_SESSIONS_COUNT;
  activity_get_sessions(&num_sessions, sessions);
  for (int i = 0; i < num_sessions; i++) {
    if (sessions[i].type == exp_session->type && sessions[i].start_utc == exp_session->start_utc &&
        sessions[i].length_min == exp_session->length_min) {
      return;
    }
  }
  printf("\nFound activities:");
  for (int i = 0; i < num_sessions; i++) {
    printf("\nFound:       type: %d, start_utc: %d, len: %" PRIu16 " ", (int)sessions[i].type,
           (int)sessions[i].start_utc, sessions[i].length_min);
  }
  printf("\nLooking for: type: %d, start_utc: %d, len: %" PRIu16 " ", (int)exp_session->type,
         (int)exp_session->start_utc, exp_session->length_min);
  clar__assert(false, file, line, "Missing sleep activity record", "", true);
}

#define ASSERT_STEP_ACTIVITY_SESSION_PRESENT(session) \
  prv_assert_step_activity_present((session), __FILE__, __LINE__)

#define ASSERT_SLEEP_ACTIVITY_SESSION_PRESENT(session) \
  prv_assert_sleep_activity_present((session), __FILE__, __LINE__)

#define ASSERT_NUM_ACTIVITY_SESSIONS(num_sessions) \
  prv_assert_num_activities((num_sessions), __FILE__, __LINE__)

// =============================================================================================
// Activity algorithm stub
// For each accel sample that is fed in, it updates the metrics as follows:
//  x: increment step count by this much
//  y: sleep state
//

#define ALGORITHM_SAMPLING_RATE    ACCEL_SAMPLING_25HZ
#define TEST_ACTIVITY_MAX_SESSIONS 24
typedef struct {
  // Captured sessions
  ActivitySession sessions[TEST_ACTIVITY_MAX_SESSIONS];
  int num_sessions_created;
  time_t last_captured_utc;

  int sleep_current_container_idx; // >=0 if we have a container in progress
  ActivitySleepState sleep_state;  // Our current sleep state
} AlgorithmStateMinuteData;

typedef struct {
  uint16_t steps;

  // Captured sessions
  AlgorithmStateMinuteData minute_data;

  // Rate info
  uint16_t rate_last_steps;
  time_t rate_last_update_time;
  uint16_t rate_steps;
  uint32_t rate_elapsed_ms;

  time_t last_sleep_utc;

  uint8_t orientation;

} AlgorithmState;
static AlgorithmState s_test_alg_state;

bool activity_algorithm_init(AccelSamplingRate *sampling_rate) {
  *sampling_rate = ALGORITHM_SAMPLING_RATE;
  AlgorithmStateMinuteData minute_data = s_test_alg_state.minute_data;
  // Preserve the minute data from the last boot
  s_test_alg_state = (AlgorithmState){
    .minute_data = minute_data,
    .rate_last_update_time = rtc_get_time(),
  };

  return true;
}

// Call from unit tests to clear out the "minute data" that might have been left over
// from last time
static void prv_activity_algorithm_erase_minute_data(void) {
  s_test_alg_state.minute_data = (AlgorithmStateMinuteData){};
  s_test_alg_state.minute_data.sleep_current_container_idx = -1;
  s_test_alg_state.minute_data.sleep_state = ActivitySleepStateAwake;
}


// =============================================================================================
// Boulder: Ablage und Laden der Variante, soweit sie die Woche betreffen

#include "pbl/services/activity/boulder/speicher.h"
#include "pbl/services/activity/boulder/woche.h"
#include "pbl/services/blob_db/health_db.h"
#include "pbl/services/settings/settings_raw_iter.h"

static int s_woche_saetze;
status_t __real_settings_file_set(SettingsFile *file, const void *key, size_t key_len,
                                  const void *val, size_t val_len);
status_t __wrap_settings_file_set(SettingsFile *file, const void *key, size_t key_len,
                                  const void *val, size_t val_len) {
  if (file->iter.file_name && !strcmp(file->iter.file_name, SPEICHER_DATEI) && key_len == 5 &&
      !memcmp(key, "woche", 5)) {
    s_woche_saetze++;
  }
  return __real_settings_file_set(file, key, key_len, val, val_len);
}

static void prv_ablage(void) {
  SettingsFile f;
  cl_must_pass(settings_file_open(&f, SPEICHER_DATEI, SPEICHER_DATEI_GROESSE));
  boulder_woche_ablegen(&f, rtc_get_time());
  settings_file_close(&f);
}

// Der Anstoss nach einem Tageswechsel (woche.c): die Ablage gleich.
static bool s_ablage_angestossen;
void boulder_speicher_anstossen(bool alles) {
  s_ablage_angestossen = true;
}

// Die Ablage der Variante: im Takt alle 15 min zur Minute 3 (dauerteil.c), dazu nach einem Anstoss.
static void prv_ablage_takt(time_t utc_sec) {
  const int minute = (int)((time_utc_to_local(utc_sec) % PBL_SEC_PER_HOUR) / PBL_SEC_PER_MIN);
  if (s_ablage_angestossen || minute % 15 == 3) {
    s_ablage_angestossen = false;
    prv_ablage();
  }
}

static void prv_laden(void) {
  SettingsFile f;
  cl_must_pass(settings_file_open(&f, SPEICHER_DATEI, SPEICHER_DATEI_GROESSE));
  boulder_woche_laden(&f);
  settings_file_close(&f);
}

// Ein Neustart der Variante: alles Statische von woche.c neu, dann "Laden".
static void prv_neustart_variante(void) {
  boulder_woche_test_neustart();
  prv_laden();
}

void activity_algorithm_early_deinit(void) {
}

bool activity_algorithm_deinit(void) {
  return true;
}

void activity_algorithm_handle_accel(AccelRawData *data, uint32_t num_samples, uint64_t timestamp) {
  // For testing purposes, we'll use the x movement as the steps and y as the sleep state
  ActivitySleepState prior_state = s_test_alg_state.minute_data.sleep_state;
  time_t now_secs = rtc_get_time();
  s_test_alg_state.minute_data.last_captured_utc = now_secs;

  for (int i = 0; i < num_samples; i++) {
    s_test_alg_state.steps += data[i].x;
    s_test_alg_state.minute_data.sleep_state = data[i].y;

    // Update the length of the current sleep container if we have one
    if (s_test_alg_state.minute_data.sleep_current_container_idx >= 0) {
      cl_assert(prior_state != ActivitySleepStateAwake);
      ActivitySession *session =
          &s_test_alg_state.minute_data
               .sessions[s_test_alg_state.minute_data.sleep_current_container_idx];
      session->length_min = ROUND(now_secs - session->start_utc, PBL_SEC_PER_MIN);
      // Inform the activity service of the new state
      activity_sessions_prv_add_activity_session(session);
    }
    // If we were in restful sleep, update that session as well
    if (prior_state == ActivitySleepStateRestfulSleep) {
      cl_assert(s_test_alg_state.minute_data.num_sessions_created > 0);
      ActivitySession *session =
          &s_test_alg_state.minute_data
               .sessions[s_test_alg_state.minute_data.num_sessions_created - 1];
      session->length_min = ROUND(now_secs - session->start_utc, PBL_SEC_PER_MIN);
      // Inform the activity service of the new state
      activity_sessions_prv_add_activity_session(session);
    }

    if (s_test_alg_state.minute_data.sleep_state == prior_state) {
      // No change in state, continue
      continue;
    }

    switch (s_test_alg_state.minute_data.sleep_state) {
      // We are waking --------------------
      case ActivitySleepStateAwake:
        // End the container
        s_test_alg_state.minute_data.sleep_current_container_idx = -1;

        // Send all stored sleep sessions to the activity service now that sleep is over
        for (int i = 0; i < s_test_alg_state.minute_data.num_sessions_created; i++) {
          s_test_alg_state.minute_data.sessions[i].ongoing = false;
          activity_sessions_prv_add_activity_session(&s_test_alg_state.minute_data.sessions[i]);
        }
        s_test_alg_state.minute_data.num_sessions_created = 0;
        break;

      // We are entering light sleep ------------------
      case ActivitySleepStateLightSleep:
        // Start a light sleep session if we were awake before. If we were in restful sleep,
        // we should already have one
        if (prior_state == ActivitySleepStateAwake) {
          cl_assert(s_test_alg_state.minute_data.num_sessions_created < TEST_ACTIVITY_MAX_SESSIONS);
          cl_assert(s_test_alg_state.minute_data.sleep_current_container_idx < 0);
          s_test_alg_state.minute_data.sleep_current_container_idx =
              s_test_alg_state.minute_data.num_sessions_created;
          s_test_alg_state.minute_data
              .sessions[s_test_alg_state.minute_data.num_sessions_created++] = (ActivitySession){
            .type = ActivitySessionType_Sleep,
            .start_utc = now_secs,
            .length_min = 0,
            .ongoing = true,
          };
        } else {
          // We were in restful sleep before, we should already have a container
          cl_assert(s_test_alg_state.minute_data.sleep_current_container_idx >= 0);
        }
        break;

      // We are entering restful sleep ------------------
      case ActivitySleepStateRestfulSleep:
        // Start a container session if we don't have already
        if (s_test_alg_state.minute_data.sleep_current_container_idx < 0) {
          cl_assert(s_test_alg_state.minute_data.num_sessions_created < TEST_ACTIVITY_MAX_SESSIONS);
          s_test_alg_state.minute_data.sleep_current_container_idx =
              s_test_alg_state.minute_data.num_sessions_created;
          s_test_alg_state.minute_data
              .sessions[s_test_alg_state.minute_data.num_sessions_created++] = (ActivitySession){
            .type = ActivitySessionType_Sleep,
            .start_utc = now_secs,
            .length_min = 0,
            .ongoing = true,
          };
        }

        // Start a restful sleep session
        cl_assert(s_test_alg_state.minute_data.num_sessions_created < TEST_ACTIVITY_MAX_SESSIONS);
        s_test_alg_state.minute_data.sessions[s_test_alg_state.minute_data.num_sessions_created++] =
            (ActivitySession){
              .type = ActivitySessionType_RestfulSleep,
              .start_utc = now_secs,
              .length_min = 0,
              .ongoing = true,
            };
        break;

      case ActivitySleepStateUnknown:
        break;
    }

    prior_state = s_test_alg_state.minute_data.sleep_state;
  }

  // Update the rate info
  // The actual implementation only sends a rate update once every epoch (5 seconds), so
  // emulate that
  if ((now_secs - s_test_alg_state.rate_last_update_time) >= 5) {
    s_test_alg_state.rate_steps = s_test_alg_state.steps - s_test_alg_state.rate_last_steps;
    s_test_alg_state.rate_elapsed_ms =
        (now_secs - s_test_alg_state.rate_last_update_time) * PBL_MSEC_PER_SEC;

    s_test_alg_state.rate_last_update_time = now_secs;
    s_test_alg_state.rate_last_steps = s_test_alg_state.steps;
  }
}

bool activity_algorithm_set_user(uint32_t height_mm, uint32_t weight_g, ActivityGender gender,
                                 uint32_t age_years) {
  return true;
}

bool activity_algorithm_get_steps(uint32_t *steps) {
  *steps = s_test_alg_state.steps;
  return true;
}

bool activity_algorithm_get_step_rate(uint16_t *steps, uint32_t *elapsed_ms, time_t *end_sec) {
  *steps = s_test_alg_state.rate_steps;
  *elapsed_ms = s_test_alg_state.rate_elapsed_ms;
  *end_sec = s_test_alg_state.rate_last_update_time;
  return true;
}

bool activity_algorithm_metrics_changed_notification(void) {
  // Boulder: wie Kraepelin (und die Glue-Kopie) ab Pebbles Wert weiterzaehlen, nicht ab 0: Hebt die
  // healthdb die Schritte von heute an, bleiben sie oben (activity_algorithm_kraepelin.c).
  s_test_alg_state.steps = activity_metrics_prv_get_steps();
  s_test_alg_state.rate_last_steps = 0;
  s_test_alg_state.rate_last_update_time = rtc_get_time();
  return true;
}

bool activity_algorithm_get_sleep_sessions(time_t sleep_earliest_end_utc,
                                           time_t *last_processed_utc) {
  *last_processed_utc = s_test_alg_state.minute_data.last_captured_utc;
  for (uint32_t i = 0; i < s_test_alg_state.minute_data.num_sessions_created; i++) {
    ActivitySession *session = &s_test_alg_state.minute_data.sessions[i];
    int start_minute = time_util_get_minute_of_day(session->start_utc);
    PBL_LOG_DBG("Found session %d: start_min: %d, len_min: %" PRIu16 " ", session->type,
                start_minute, session->length_min);
    if (!activity_sessions_prv_is_sleep_activity(session->type)) {
      continue;
    }
    if (s_test_alg_state.minute_data.sessions[i].start_utc +
            (s_test_alg_state.minute_data.sessions[i].length_min * PBL_SEC_PER_MIN) <
        sleep_earliest_end_utc) {
      continue;
    }
    PBL_LOG_DBG("Returning session %d: start_min: %d, len_min: %" PRIu16 " ", session->type,
                start_minute, session->length_min);
    activity_sessions_prv_add_activity_session(&s_test_alg_state.minute_data.sessions[i]);
  }
  return true;
}

void activity_algorithm_post_process_sleep_sessions(uint16_t num_input_sessions,
                                                    ActivitySession *sessions) {
}

void activity_algorithm_minute_handler(time_t utc_sec, AlgMinuteRecord *record_out) {
  s_test_alg_state.last_sleep_utc = utc_sec;

  record_out->data.base.orientation = s_test_alg_state.orientation;
  // Boulder: wie die Glue-Kopie (D2) - der Satz gilt der eben fertigen Minute.
  boulder_woche_minute(utc_sec - PBL_SEC_PER_MIN);
  prv_ablage_takt(utc_sec);
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

// Activity-SpO2 / HR-pause APIs referenced by activity.c. Stubbed to keep the auto-activity HR
// path idle so these tests exercise only the daily HR/SpO2 schedulers.
void workout_service_set_hrm_paused(bool paused) {
  (void)paused;
}

// We simulate the activity_algorithm_get_minute_history() call to return data that reflects
// that we record chunks of ALG_MINUTES_PER_RECORD minutes at a time. If we don't ask on a
// ALG_MINUTES_PER_RECORD minute boundary, we will have up to ALG_MINUTES_PER_RECORD minutes
// of data still unavailable before the current time. The data that we do return, we will set
// the number of steps equal to (% 255) of the timestamp of that minute.
bool activity_algorithm_get_minute_history(HealthMinuteData *minute_data, uint32_t *num_records,
                                           time_t *utc_start) {
  // Get the current time
  time_t now = rtc_get_time();

  // Get the minute index
  uint32_t minute_idx = now / PBL_SEC_PER_MIN;

  // Compute the timestamp of the end of the last record we would have available
  uint32_t last_minute_avail = minute_idx - (minute_idx % ALG_MINUTES_PER_FILE_RECORD);
  time_t last_second_available = last_minute_avail * PBL_SEC_PER_MIN;

  // Return the data now
  uint32_t num_records_requested = *num_records;

  // Start on next minute boundary
  *utc_start = ((*utc_start + PBL_SEC_PER_MIN - 1) / PBL_SEC_PER_MIN) * PBL_SEC_PER_MIN;

  int num_records_returned;
  time_t record_start_time = *utc_start;
  for (num_records_returned = 0; num_records_returned < num_records_requested;
       num_records_returned++, record_start_time += PBL_SEC_PER_MIN) {
    if (record_start_time + PBL_SEC_PER_MIN > last_second_available) {
      // This record not available yet.
      break;
    }

    minute_data[num_records_returned] = (HealthMinuteData){
      .steps = record_start_time % 255,
    };
  }

  *num_records = num_records_returned;
  return true;
}

time_t activity_algorithm_get_last_sleep_utc(void) {
  return s_test_alg_state.last_sleep_utc;
}

bool activity_algorithm_test_send_fake_minute_data_dls_record(void) {
  return true;
}

// =========================================================================================
// Tests

// ---------------------------------------------------------------------------------------
// Feed in X seconds of data with the given statistics.
// The fake algorithm we plug in assumes that each accel sample contains the following:
// .x : the number of steps to increment by (either 0 or 1)
// .y : the current sleep state

// =========================================================================================
// Hilfen

// .z : 0
static void prv_feed_canned_accel_data(uint32_t num_sec, uint32_t steps_per_minute,
                                       ActivitySleepState sleep_state) {
  uint32_t num_steps = (steps_per_minute * num_sec + 30) / 60;
  uint32_t num_samples = num_sec * ALGORITHM_SAMPLING_RATE;
  uint32_t samples_per_step = 0;
  if (num_steps > 0) {
    samples_per_step = num_samples / num_steps;
  }
  int need_step_ctr = samples_per_step;

  time_t utc_secs;
  uint16_t ms;
  rtc_get_time_ms(&utc_secs, &ms);
  uint64_t start_ms = utc_secs * 1000 + ms;
  uint64_t ms_per_sample = 1000 / ALGORITHM_SAMPLING_RATE;

  for (int i = 0; i < num_samples;) {
    AccelData accel_data[ALGORITHM_SAMPLING_RATE];

    for (int j = 0; j < ALGORITHM_SAMPLING_RATE; j++, i++) {
      need_step_ctr -= 1;
      accel_data[j] = (AccelData){
        .x = (num_steps > 0) && (need_step_ctr <= 0),
        .y = sleep_state,
        .z = 0,
        .timestamp = start_ms,
      };
      start_ms += ms_per_sample;
      if (need_step_ctr <= 0) {
        need_step_ctr = samples_per_step;
        if (num_steps > 0) {
          num_steps--;
        }
      }
    }

    fake_accel_service_invoke_callbacks(accel_data, ALGORITHM_SAMPLING_RATE);

    // Advance time
    fake_rtc_increment_time(1);
    fake_rtc_increment_ticks(PBL_TICK_HZ);

    // Is it time to call the minute callback?
    utc_secs += 1;
    if ((utc_secs % 60) == 0) {
      fake_cron_job_fire();
      fake_system_task_callbacks_invoke_pending();
    }
  }
  PBL_ASSERTN(num_steps == 0);
}

static void prv_activity_init_and_set_enabled(bool enable) {
  activity_init();
  activity_set_enabled(enable);
  fake_system_task_callbacks_invoke_pending();
}

// Europe/Zurich 2026 wie die Uhr sie kennt: MEZ, Sommerzeit bis 25.10. 1:00 UTC. Die acht Tage
// gehen ueber diese Umstellung (ein Tag mit 25 Stunden).
static void prv_zone_zuerich(void) {
  TimezoneInfo tz = { .tm_zone = "CE*T", .tm_gmtoff = PBL_SEC_PER_HOUR,
                      .dst_start = 1774746000, .dst_end = 1792890000 };
  time_util_update_timezone(&tz);
}

#define BEGINN_UTC 1792476000  // Di 20.10.2026 06:00 UTC = 08:00 Ortszeit (CEST)

// Minuten ohne Schritte, je eine mit Pebbles Minutenlauf.
static void prv_minuten(int anzahl) {
  for (int i = 0; i < anzahl; i++) {
    fake_rtc_increment_time(PBL_SEC_PER_MIN);
    fake_rtc_increment_ticks(PBL_TICK_HZ * PBL_SEC_PER_MIN);
    fake_cron_job_fire();
    fake_system_task_callbacks_invoke_pending();
  }
}

// Gehen: `minuten` Minuten mit `je_minute` Schritten, Sekunde fuer Sekunde (Pebbles Weg).
static void prv_gehen(int minuten, uint32_t je_minute) {
  prv_feed_canned_accel_data(minuten * PBL_SEC_PER_MIN, je_minute, ActivitySleepStateAwake);
}

static int prv_minute_des_tages(void) {
  return (int)((time_utc_to_local(rtc_get_time()) % PBL_SEC_PER_DAY) / PBL_SEC_PER_MIN);
}

// Ohne Schritte bis zur Minute `ziel` des Ortstags (0..1439), notfalls ueber Mitternacht.
static void prv_bis(int ziel) {
  int jetzt = prv_minute_des_tages();
  prv_minuten((ziel - jetzt + 24 * 60) % (24 * 60));
}

static int32_t s_pebble[ACTIVITY_HISTORY_DAYS];
static void prv_pebble_lesen(void) {
  cl_assert(activity_get_metric(ActivityMetricStepCount, ACTIVITY_HISTORY_DAYS, s_pebble));
}

static BoulderWoche s_woche;
static void prv_woche_lesen(void) {
  cl_assert(boulder_woche_kopieren(&s_woche));
}

static uint16_t prv_heute(void) {
  return (uint16_t)((time_utc_to_local(rtc_get_time()) - 946684800) / PBL_SEC_PER_DAY);
}

// Jede bekannte Saeule ist Pebbles Tageswert dieses Tags (Historie 1-6); Rueckgabe: wie viele.
static int prv_saeulen_wie_pebble(void) {
  prv_pebble_lesen();
  prv_woche_lesen();
  cl_assert_equal_i(s_woche.tag, prv_heute() - 1);
  int bekannt = 0;
  for (int i = 0; i < WOCHE_VORTAGE; i++) {
    if (s_woche.schritte[i] == WOCHE_KEIN_WERT) {
      continue;
    }
    if ((int32_t)s_woche.schritte[i] != s_pebble[i + 1]) {
      printf("\nSaeule %d: %lu, Pebble %ld", i, (unsigned long)s_woche.schritte[i],
             (long)s_pebble[i + 1]);
    }
    cl_assert_equal_i(s_woche.schritte[i], s_pebble[i + 1]);
    bekannt++;
  }
  return bekannt;
}

// Ein Tag wie gelebt: morgens, mittags, abends gehen und noch in der letzten Minute vor Mitternacht.
// Die Schritte sind je Tag andere (d), damit keine Saeule zufaellig stimmt.
static void prv_tag_gehen(int d) {
  prv_bis(8 * 60);
  prv_gehen(30, 40 + 5 * d);
  prv_bis(12 * 60);
  prv_gehen(10, 90 + d);
  prv_bis(23 * 60 + 50);
  prv_gehen(10, 60 + 7 * d);  // bis 0:00, die letzte Minute eingeschlossen
}

void test_boulder_woche__initialize(void) {
  prv_zone_zuerich();
  fake_rtc_init(100 /*initial_ticks*/, BEGINN_UTC);
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  pfs_format(false);
  prv_activity_algorithm_erase_minute_data();
  prv_activity_init_and_set_enabled(true);
  activity_prefs_set_height_mm(ACTIVITY_DEFAULT_HEIGHT_MM);
  activity_prefs_set_weight_dag(ACTIVITY_DEFAULT_WEIGHT_DAG);
  activity_prefs_set_gender(ACTIVITY_DEFAULT_GENDER);
  activity_prefs_set_age_years(ACTIVITY_DEFAULT_AGE_YEARS);
  activity_start_tracking(false /*test_mode*/);
  fake_system_task_callbacks_invoke_pending();
  s_woche_saetze = 0;
  s_ablage_angestossen = false;
  // Wie nach dem Wechsel b26 -> V2: keine Woche in boulder_v2.
  prv_neustart_variante();
  prv_woche_lesen();
  cl_assert_equal_i(s_woche.tag, 0);
}

void test_boulder_woche__cleanup(void) {
  activity_stop_tracking();
  fake_system_task_callbacks_invoke_pending();
}

// T4.5: acht Tage, jeder Tageswechsel. Jede Saeule ist Pebbles Tageswert der Uhr, mit den Schritten
// der letzten Minute vor Mitternacht und ueber die Zeitumstellung; "woche" wird hoechstens 25-mal
// am Tag geschrieben.
void test_boulder_woche__acht_tage(void) {
  int max_saetze = 0;
  for (int d = 0; d < 8; d++) {
    const int saetze_vorher = s_woche_saetze;
    prv_tag_gehen(d);
    prv_minuten(1);  // die erste Minute des neuen Tags
    const int bekannt = prv_saeulen_wie_pebble();
    cl_assert_equal_i(bekannt, MIN(d + 1, WOCHE_VORTAGE));
    cl_assert(s_woche.schritte[0] > 0);
    const int saetze = s_woche_saetze - saetze_vorher;
    max_saetze = MAX(max_saetze, saetze);
    cl_assert(saetze <= 25);
    printf("\nTag %d: Saeule %lu = Pebble %ld, %d Saetze \"woche\"", s_woche.tag,
           (unsigned long)s_woche.schritte[0], (long)s_pebble[1], saetze);
  }
  printf("\nacht Tage: hoechstens %d Saetze \"woche\" am Tag", max_saetze);
}

// Ein Neustart der Variante mitten am Tag: Woche und Spiegel kommen aus boulder_v2 (der Spiegel
// bis zu einer Stunde alt), die naechste Minute holt Pebbles Wert, und um Mitternacht stimmt die
// Saeule.
void test_boulder_woche__neustart(void) {
  prv_tag_gehen(0);
  prv_minuten(1);
  prv_bis(9 * 60);
  prv_gehen(20, 120);
  prv_bis(9 * 60 + 40);
  prv_neustart_variante();
  const uint32_t aus_der_datei = boulder_woche_spiegel()->schritte;
  prv_minuten(1);
  prv_pebble_lesen();
  printf("\nNeustart: Spiegel aus der Datei %lu, nach einer Minute %lu, Pebble %ld",
         (unsigned long)aus_der_datei, (unsigned long)boulder_woche_spiegel()->schritte,
         (long)s_pebble[0]);
  cl_assert_equal_i(boulder_woche_spiegel()->schritte, s_pebble[0]);
  prv_bis(23 * 60 + 50);
  prv_gehen(10, 77);
  prv_minuten(1);
  cl_assert_equal_i(prv_saeulen_wie_pebble(), 2);
}

// Die Uhr ist ueber Mitternacht aus (23:30 bis 0:30). Laeuft die Variante danach ohne Neustart
// weiter, stimmt die Saeule. Startet sie neu, kommt der Spiegel aus der letzten Ablage: Der Saeule
// fehlen hoechstens die Schritte der letzten 60 min vor dem Ausschalten.
void test_boulder_woche__uhr_aus_ueber_mitternacht(void) {
  for (int neustart = 0; neustart <= 1; neustart++) {
    prv_bis(22 * 60 + 30);
    prv_gehen(30, 100);   // 22:30-23:00
    prv_gehen(30, 50);    // 23:00-23:30
    prv_pebble_lesen();
    const int32_t vor_dem_aus = s_pebble[0];
    // Aus: eine Stunde ohne Minutenlauf.
    fake_rtc_increment_time(60 * PBL_SEC_PER_MIN);
    fake_rtc_increment_ticks(PBL_TICK_HZ * 60 * PBL_SEC_PER_MIN);
    if (neustart) {
      prv_neustart_variante();
    }
    prv_minuten(1);
    prv_pebble_lesen();
    prv_woche_lesen();
    printf("\nUhr aus%s: Saeule %lu, Pebble %ld (vor dem Aus %ld)",
           neustart ? " mit Neustart" : "", (unsigned long)s_woche.schritte[0],
           (long)s_pebble[1], (long)vor_dem_aus);
    cl_assert_equal_i(s_woche.tag, prv_heute() - 1);
    cl_assert_equal_i(s_pebble[1], vor_dem_aus);
    if (!neustart) {
      cl_assert_equal_i(s_woche.schritte[0], s_pebble[1]);
    } else {
      cl_assert(s_woche.schritte[0] <= (uint32_t)s_pebble[1]);
      cl_assert(s_pebble[1] - (int32_t)s_woche.schritte[0] <= 30 * 100 + 30 * 50);
    }
  }
}

// Das Telefon in Pebbles healthdb, wie Boulder es schickt: "<wochentag>_movementData".
static void prv_telefon(int tage_zurueck, uint32_t schritte, time_t zeitstempel) {
  static const char *const s_tage[] = { "sunday", "monday", "tuesday", "wednesday",
                                        "thursday", "friday", "saturday" };
  const time_t tag = rtc_get_time() - tage_zurueck * PBL_SEC_PER_DAY;
  char schluessel[32];
  snprintf(schluessel, sizeof(schluessel), "%s_movementData",
           s_tage[time_util_get_day_in_week(tag)]);
  const uint32_t satz[7] = { 1, (uint32_t)zeitstempel, schritte, 0, 0, 0, 0 };
  cl_must_pass(health_db_insert((const uint8_t *)schluessel, strlen(schluessel),
                                (const uint8_t *)satz, sizeof(satz)));
}

// Gegenprobe (T4.5): Das Telefon ueberschreibt Pebbles Vortag - mit einem anderen Wert und, wie
// Boulder ohne Daten, mit 0. Pebbles Historie folgt ihm, die Saeulen nicht.
void test_boulder_woche__telefon_aendert_keine_saeule(void) {
  for (int d = 0; d < 3; d++) {
    prv_tag_gehen(d);
    prv_minuten(1);
  }
  prv_bis(10 * 60);
  cl_assert_equal_i(prv_saeulen_wie_pebble(), 3);
  const BoulderWoche vorher = s_woche;
  prv_telefon(1, 77777, rtc_get_time());
  prv_telefon(2, 0, rtc_get_time());
  prv_minuten(1);
  prv_pebble_lesen();
  prv_woche_lesen();
  printf("\nTelefon: Pebble Vortag %ld, vorvortag %ld; Saeulen %lu, %lu", (long)s_pebble[1],
         (long)s_pebble[2], (unsigned long)s_woche.schritte[0],
         (unsigned long)s_woche.schritte[1]);
  cl_assert_equal_i(s_pebble[1], 77777);
  cl_assert_equal_i(s_pebble[2], 0);
  cl_assert_equal_m(&s_woche, &vorher, sizeof(vorher));
}

// Der Weg eines Satzes fuer HEUTE (Spezifikation 2.7, Gegenpruefung 2 H10): Pebble hebt seinen Wert
// von heute an, wenn der Satz groesser ist und einen gueltigen Zeitstempel hat. Der Spiegel folgt
// Pebbles Wert, also auch diesem. Boulder schickt den heutigen Tag nicht (HealthStatsSync.kt); dass
// keine Telefonzahl auf die Uhr kommt, garantiert damit Boulder, nicht die Firmware.
void test_boulder_woche__heute_satz(void) {
  prv_tag_gehen(0);
  prv_minuten(1);
  prv_bis(9 * 60);
  prv_gehen(10, 100);
  prv_pebble_lesen();
  const int32_t uhr = s_pebble[0];
  prv_telefon(0, (uint32_t)uhr + 5000, rtc_get_time());
  prv_minuten(1);
  prv_pebble_lesen();
  const uint32_t spiegel = boulder_woche_spiegel()->schritte;
  printf("\nHeute-Satz: Pebble heute %ld -> %ld, Spiegel %lu", (long)uhr, (long)s_pebble[0],
         (unsigned long)spiegel);
  cl_assert_equal_i(s_pebble[0], uhr + 5000);
  cl_assert_equal_i(spiegel, (uint32_t)uhr + 5000);
  // Ein kleinerer Satz fuer heute aendert nichts (Pebble nimmt fuer heute nur mehr).
  prv_telefon(0, 10, rtc_get_time());
  prv_minuten(1);
  cl_assert_equal_i(boulder_woche_spiegel()->schritte, (uint32_t)uhr + 5000);
  prv_bis(23 * 60 + 50);
  prv_gehen(10, 77);
  prv_minuten(1);
  cl_assert_equal_i(prv_saeulen_wie_pebble(), 2);
  printf("\nHeute-Satz am Abend: Saeule %lu = Pebble %ld", (unsigned long)s_woche.schritte[0],
         (long)s_pebble[1]);
}

// Schreiblast (3.8.3, T4.5): Aendern sich die Schritte jede Minute eines ganzen Tags, schreibt die
// Ablage "woche" hoechstens stuendlich und dazu einmal beim Tageswechsel - hoechstens 25 Saetze, nicht
// 96 wie mit jeder Ablage.
void test_boulder_woche__schreiblast_jede_minute(void) {
  prv_bis(0);
  prv_minuten(1);
  const int vorher = s_woche_saetze;
  prv_gehen(24 * 60 - 1, 10);
  prv_minuten(1);
  const int saetze = s_woche_saetze - vorher;
  // Ein Satz in boulder_v2: Kopf, Schluessel "woche" (5 B ohne Nullbyte) und 36 B.
  const int satz_bytes = (int)(sizeof(SettingsRecordHeader) + 5 + sizeof(BoulderWoche) +
                               sizeof(BoulderSchrittSpiegel));
  printf("\nSchreiblast: ein Tag mit Schritten in jeder Minute, %d Saetze \"woche\" zu %d B, %d B",
         saetze, satz_bytes, saetze * satz_bytes);
  cl_assert(saetze <= 25);
  cl_assert(saetze >= 24);
  // Der Tag vor dem gezaehlten (ohne Schritte) und der gezaehlte.
  cl_assert_equal_i(prv_saeulen_wie_pebble(), 2);
  cl_assert_equal_i(s_woche.schritte[0], 10 * (24 * 60 - 1));
}

// Geht die Uhr auf einen frueheren Tag zurueck, beginnt der Spiegel dort neu; die Woche bleibt, wie
// sie ist (woche.h), und geschoben wird erst wieder an einem spaeteren Tag.
void test_boulder_woche__uhr_zurueck(void) {
  for (int d = 0; d < 2; d++) {
    prv_tag_gehen(d);
    prv_minuten(1);
  }
  prv_bis(10 * 60);
  prv_woche_lesen();
  const BoulderWoche vorher = s_woche;
  const uint16_t spiegel_tag = boulder_woche_spiegel()->tag;
  // Zwei Tage zurueck (Uhr gestellt), dort eine Minute.
  fake_rtc_increment_time(-2 * PBL_SEC_PER_DAY);
  prv_minuten(1);
  prv_woche_lesen();
  printf("\nUhr zurueck: Spiegel Tag %d -> %d, Woche bis %d bleibt", spiegel_tag,
         boulder_woche_spiegel()->tag, s_woche.tag);
  cl_assert_equal_i(boulder_woche_spiegel()->tag, spiegel_tag - 2);
  cl_assert_equal_m(&s_woche, &vorher, sizeof(vorher));
}

// Beim Tageswechsel schreibt die Ablage "woche" gleich, ohne die Stunde seit dem letzten Satz
// abzuwarten: Ein Neustart kurz nach Mitternacht findet den Vortag schon in der Datei.
void test_boulder_woche__tageswechsel_gleich_in_der_datei(void) {
  prv_bis(23 * 60 + 20);
  prv_gehen(10, 80);   // 23:20-23:30, geschrieben um 23:33
  prv_bis(23 * 60 + 40);
  prv_gehen(20, 90);   // 23:40-0:00, die Stunde seit 23:33 ist um 0:00 nicht um
  prv_minuten(1);
  prv_woche_lesen();
  const BoulderWoche nach_mitternacht = s_woche;
  cl_assert_equal_i(nach_mitternacht.schritte[0], 10 * 80 + 20 * 90);
  prv_neustart_variante();
  prv_woche_lesen();
  printf("\nTageswechsel: nach einem Neustart um 0:01 Vortag %lu aus der Datei",
         (unsigned long)s_woche.schritte[0]);
  cl_assert_equal_m(&s_woche, &nach_mitternacht, sizeof(s_woche));
}
