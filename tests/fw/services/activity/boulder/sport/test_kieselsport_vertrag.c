/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/workout_service.h"
#include "pbl/services/hrm/hrm_manager.h"
#include "pbl/services/time.h"
#include "drivers/hrm.h"
#include "kernel/events.h"
#include "process_management/app_install_types.h"

#include "stubs_evented_timer.h"
#include "stubs_logging.h"
#include "stubs_pbl_malloc.h"
#include "stubs_regular_timer.h"

#include "fake_events.h"
#include "fake_mutex.h"
#include "fake_rtc.h"

#include <string.h>

// DER VERTRAG ZWISCHEN KIESELSPORT UND PEBBLES WORKOUT-DIENST (Spezifikation Schritt 2, 2.2
// und 2.5). Kieselsport baut auf Verhalten, das Pebble nicht zusichert: Laeuft eines davon
// nach einem Upstream-Merge anders, wird dieser Test rot, bevor ein Training auf der Uhr
// falsch endet. Geprueft wird die echte workout_service.c, wie sie auf obelix und qemu_emery
// gebaut wird (mit CONFIG_BOULDER_KIESELSPORT und CONFIG_HRM). Was die Attrappen unten
// liefern, ist nur das Umfeld: Sitzungsliste, Insights, HRM und Profil zeichnen Aufrufe auf.

extern void prv_workout_timer_cb(void *data);
extern bool workout_service_get_avg_hr(int32_t *avg_hr_out);
extern void workout_service_reset(void);

// ---------------------------------------------------------------------------------------
// Umfeld

#define MAX_GESPEICHERT 4

static ActivitySession s_gespeichert[MAX_GESPEICHERT];
static int s_anzahl_gespeichert;
void activity_sessions_prv_add_activity_session(ActivitySession *session) {
  cl_assert(s_anzahl_gespeichert < MAX_GESPEICHERT);
  s_gespeichert[s_anzahl_gespeichert++] = *session;
}
void activity_sessions_prv_delete_activity_session(ActivitySession *session) {
}

// Laufende, von der Uhr erkannte Sitzungen (etwa eine Velofahrt der Variante).
static ActivitySession s_erkannt[2];
static uint32_t s_anzahl_erkannt;
bool activity_get_sessions(uint32_t *session_entries, ActivitySession *sessions) {
  const uint32_t n = MIN(*session_entries, s_anzahl_erkannt);
  memcpy(sessions, s_erkannt, n * sizeof(ActivitySession));
  *session_entries = n;
  return true;
}

// Auto-Erkennung: Folge der Aufrufe, 1 = an, 0 = aus.
static char s_erkennung[8];
void activity_algorithm_enable_activity_tracking(bool enable) {
  const size_t n = strlen(s_erkennung);
  cl_assert(n + 1 < sizeof(s_erkennung));
  s_erkennung[n] = enable ? '1' : '0';
}

static int s_meldungen;
static int32_t s_meldung_puls;
void activity_insights_push_activity_session_notification(time_t notif_time,
                                                          ActivitySession *session, int32_t avg_hr,
                                                          int32_t *hr_zone_time_s) {
  s_meldungen++;
  s_meldung_puls = avg_hr;
}

void workout_utils_send_abandoned_workout_notification(void) {
}

uint8_t activity_prefs_get_age_years(void) { return 30; }
ActivityGender activity_prefs_get_gender(void) { return ActivityGenderMale; }
uint16_t activity_prefs_get_weight_dag(void) { return 7500; }
uint16_t activity_prefs_get_height_mm(void) { return 1800; }
uint8_t activity_prefs_heart_get_elevated_hr(void) { return 100; }
uint8_t activity_prefs_heart_get_zone1_threshold(void) { return 130; }
uint8_t activity_prefs_heart_get_zone2_threshold(void) { return 154; }
uint8_t activity_prefs_heart_get_zone3_threshold(void) { return 172; }
HRMonitoringInterval activity_prefs_get_hrm_measurement_interval(void) {
  return HRMonitoringInterval_10Min;
}

AppInstallId app_get_app_id(void) {
  return 0;
}

uint32_t time_get_uptime_seconds(void) {
  return PBL_SEC_PER_DAY + rtc_get_time();
}

static int32_t s_schritte_tag;
bool activity_get_metric(ActivityMetric metric, uint32_t history_len, int32_t *history) {
  *history = s_schritte_tag;
  return metric == ActivityMetricStepCount;
}

// HRM: Kieselsport haelt das 1-Hz-Abo selbst (F2 = A), Pebble darf keines anlegen.
static int s_abos;
static HRMSessionRef s_intervall_sitzung = (HRMSessionRef)-1;
static HRMActivityScene s_szene;
HRMSessionRef sys_hrm_manager_app_subscribe(AppInstallId app_id, uint32_t update_interval_s,
                                            uint16_t expire_s, HRMFeature features) {
  s_abos++;
  return 1;
}
bool sys_hrm_manager_unsubscribe(HRMSessionRef ref) {
  return true;
}
bool sys_hrm_manager_set_update_interval(HRMSessionRef session, uint32_t update_interval_s,
                                         uint16_t expire_s) {
  s_intervall_sitzung = session;
  return true;
}
bool sys_hrm_manager_set_features(HRMSessionRef session, HRMFeature features) {
  return true;
}
void hrm_manager_set_activity_scene(HRMActivityScene scene) {
  s_szene = scene;
}

// ---------------------------------------------------------------------------------------
// Hilfen

// Eine Sekunde wie auf der Uhr: die RTC laeuft, Pebbles 1-s-Timer tickt einmal.
static void prv_sekunden(int n) {
  for (int i = 0; i < n; i++) {
    fake_rtc_increment_time(1);
    prv_workout_timer_cb(NULL);
  }
}

// Ein Pulswert ueber Pebbles Health-Rundruf (activity.c), egal wer den Sensor haelt.
static void prv_puls(uint8_t bpm) {
  PebbleHealthEvent e = {
    .type = HealthEventHeartRateUpdate,
    .data.heart_rate_update = {.current_bpm = bpm, .quality = HRMQuality_Good},
  };
  workout_service_health_event_handler(&e);
}

static void prv_schritte(int n) {
  s_schritte_tag += n;
  PebbleHealthEvent e = {
    .type = HealthEventMovementUpdate,
    .data.movement_update.steps = s_schritte_tag,
  };
  workout_service_health_event_handler(&e);
}

static int32_t prv_dauer(void) {
  int32_t dauer = -1;
  cl_assert(workout_service_get_current_workout_info(NULL, &dauer, NULL, NULL, NULL));
  return dauer;
}

static int32_t prv_schritte_training(void) {
  int32_t schritte = -1;
  cl_assert(workout_service_get_current_workout_info(&schritte, NULL, NULL, NULL, NULL));
  return schritte;
}

void test_kieselsport_vertrag__initialize(void) {
  fake_rtc_init(0, 1700000000);
  fake_event_init();
  fake_mutex_reset(true);
  workout_service_reset();
  workout_service_init();
  memset(s_gespeichert, 0, sizeof(s_gespeichert));
  s_anzahl_gespeichert = 0;
  s_anzahl_erkannt = 0;
  memset(s_erkennung, 0, sizeof(s_erkennung));
  s_meldungen = 0;
  s_meldung_puls = 0;
  s_schritte_tag = 5000;
  s_abos = 0;
  s_intervall_sitzung = (HRMSessionRef)-1;
  s_szene = HRMActivityScene_Default;
}

void test_kieselsport_vertrag__cleanup(void) {
  workout_service_reset();
  fake_mutex_reset(true);
}

// ---------------------------------------------------------------------------------------
// Regel 2.5.7: Pebble lehnt einen zweiten Start ab. Kieselsport erkennt daran sein eigenes
// Training aus dem 30-s-Fenster (V4), stoppt es und startet neu.
void test_kieselsport_vertrag__zweiter_start_abgelehnt(void) {
  cl_assert(workout_service_start_workout(ActivitySessionType_Open));
  cl_assert(!workout_service_start_workout(ActivitySessionType_Run));
  ActivitySessionType typ = ActivitySessionType_None;
  cl_assert(workout_service_get_current_workout_type(&typ));
  cl_assert_equal_i(typ, ActivitySessionType_Open);
  cl_assert(workout_service_stop_workout());
  cl_assert(workout_service_start_workout(ActivitySessionType_Run));
}

// Kieselsport bildet jede Art auf Run, Walk oder Open ab; mehr kennt Pebble nicht.
void test_kieselsport_vertrag__nur_run_walk_open(void) {
  cl_assert(workout_service_is_workout_type_supported(ActivitySessionType_Run));
  cl_assert(workout_service_is_workout_type_supported(ActivitySessionType_Walk));
  cl_assert(workout_service_is_workout_type_supported(ActivitySessionType_Open));
  cl_assert(!workout_service_start_workout(ActivitySessionType_Sleep));
  cl_assert(!workout_service_is_workout_ongoing());
}

// K4: Laeuft beim Start eine erkannte Fahrt, endet sie am Trainingsstart. Pebble traegt sie
// abgeschlossen wieder ein, bevor das Training laeuft.
void test_kieselsport_vertrag__start_schliesst_erkannte_fahrt(void) {
  s_erkannt[0] = (ActivitySession){
    .type = ActivitySessionType_Open,
    .start_utc = rtc_get_time() - 20 * PBL_SEC_PER_MIN,
    .length_min = 20,
    .ongoing = true,
  };
  s_anzahl_erkannt = 1;
  cl_assert(workout_service_start_workout(ActivitySessionType_Open));
  cl_assert_equal_i(s_anzahl_gespeichert, 1);
  cl_assert_equal_b(s_gespeichert[0].ongoing, false);
  cl_assert_equal_b(s_gespeichert[0].manual, false);
  cl_assert_equal_i(s_gespeichert[0].start_utc, s_erkannt[0].start_utc);
}

// K2: Pebble schaltet die Auto-Erkennung beim Start aus und beim Stopp wieder an, auch wenn
// das Training unter 60 s bleibt.
void test_kieselsport_vertrag__erkennung_aus_und_an(void) {
  cl_assert(workout_service_start_workout(ActivitySessionType_Open));
  cl_assert_equal_s(s_erkennung, "0");
  prv_sekunden(10);
  cl_assert(workout_service_stop_workout());
  cl_assert_equal_s(s_erkennung, "01");
}

// T2.1: Ab 60 s entsteht genau eine Sitzung (manual, Typ, Start der Uhr) und genau eine
// Meldung. Der Puls der Meldung ist Pebbles Mittel aus dem Rundruf (F2 = A, V4).
void test_kieselsport_vertrag__ab_60s_eine_sitzung(void) {
  const time_t start = rtc_get_time();
  cl_assert(workout_service_start_workout(ActivitySessionType_Open));
  prv_puls(120);
  prv_sekunden(30);
  prv_puls(140);
  prv_sekunden(150);
  cl_assert(workout_service_stop_workout());
  cl_assert_equal_i(s_anzahl_gespeichert, 1);
  cl_assert_equal_i(s_gespeichert[0].type, ActivitySessionType_Open);
  cl_assert_equal_b(s_gespeichert[0].manual, true);
  cl_assert_equal_b(s_gespeichert[0].ongoing, false);
  cl_assert_equal_i(s_gespeichert[0].start_utc, start);
  cl_assert_equal_i(s_gespeichert[0].length_min, 3);
  cl_assert_equal_i(s_meldungen, 1);
  cl_assert_equal_i(s_meldung_puls, 130);
  cl_assert(!workout_service_stop_workout());
  cl_assert_equal_i(s_anzahl_gespeichert, 1);
}

// T2.1, Stopp unter 60 s: weder Sitzung noch Meldung. Verwerfen (F3) baut darauf auf.
void test_kieselsport_vertrag__unter_60s_nichts(void) {
  cl_assert(workout_service_start_workout(ActivitySessionType_Run));
  prv_sekunden(59);
  cl_assert(workout_service_stop_workout());
  cl_assert_equal_i(s_anzahl_gespeichert, 0);
  cl_assert_equal_i(s_meldungen, 0);
  cl_assert(!workout_service_is_workout_ongoing());
}

// Regel 2.5.4: Laden pausiert (TrackingStopped), und nichts setzt von selbst fort. Kieselsport
// traegt die Pause ein und beendet das Training nach 30 min (Auto-Ende).
void test_kieselsport_vertrag__laden_pausiert_ohne_fortsetzen(void) {
  cl_assert(workout_service_start_workout(ActivitySessionType_Open));
  prv_sekunden(100);
  PebbleActivityEvent gestoppt = {.type = PebbleActivityEvent_TrackingStopped};
  workout_service_activity_event_handler(&gestoppt);
  cl_assert(workout_service_is_paused());
  PebbleActivityEvent gestartet = {.type = PebbleActivityEvent_TrackingStarted};
  workout_service_activity_event_handler(&gestartet);
  prv_sekunden(31 * PBL_SEC_PER_MIN);
  cl_assert(workout_service_is_paused());
  cl_assert(workout_service_is_workout_ongoing());
  cl_assert_equal_i(prv_dauer(), 100);
}

// V4: In der Pause bleiben Dauer, Schritte und Pulsmittel stehen.
void test_kieselsport_vertrag__pause_haelt_alles_an(void) {
  cl_assert(workout_service_start_workout(ActivitySessionType_Run));
  prv_puls(100);
  prv_schritte(50);
  prv_sekunden(60);
  cl_assert(workout_service_pause_workout(true));
  prv_puls(180);
  prv_schritte(70);
  prv_sekunden(600);
  cl_assert_equal_i(prv_dauer(), 60);
  cl_assert_equal_i(prv_schritte_training(), 50);
  int32_t mittel = 0;
  cl_assert(workout_service_get_avg_hr(&mittel));
  cl_assert_equal_i(mittel, 100);
  cl_assert(workout_service_pause_workout(false));
  prv_sekunden(5);
  cl_assert_equal_i(prv_dauer(), 65);
}

// Regel 2.5.3: duration_s kommt aus der RTC und wird nur im 1-s-Timer (oder bei Pause/Weiter)
// nachgefuehrt. Laesst der Timer einen Tick aus, springt sie beim naechsten um 2 s; ohne Tick
// steht sie. Kieselsports Takt verarbeitet deshalb die Differenz und keine feste Sekunde.
void test_kieselsport_vertrag__dauer_aus_rtc_im_timer(void) {
  cl_assert(workout_service_start_workout(ActivitySessionType_Open));
  prv_sekunden(10);
  cl_assert_equal_i(prv_dauer(), 10);
  fake_rtc_increment_time(2);
  cl_assert_equal_i(prv_dauer(), 10);
  prv_workout_timer_cb(NULL);
  cl_assert_equal_i(prv_dauer(), 12);
}

// F2 = A, K2: Ohne frontend_opened legt Pebble kein Puls-Abo an. Der Stopp stellt das
// Intervall der leeren Sitzung 0 um, und die tut im echten hrm_manager.c nichts.
void test_kieselsport_vertrag__ohne_frontend_kein_abo(void) {
  cl_assert(workout_service_start_workout(ActivitySessionType_Open));
  prv_puls(120);
  prv_sekunden(90);
  cl_assert(workout_service_stop_workout());
  cl_assert_equal_i(s_abos, 0);
  cl_assert_equal_i(s_intervall_sitzung, 0);
}

// V5: Pebble setzt die Szene nach dem Typ, Open = HighIntensity. Kieselsport stellt Yoga und
// Schwimmen nach dem Start auf Default. Der Stopp setzt Default.
void test_kieselsport_vertrag__szene_nach_typ(void) {
  cl_assert(workout_service_start_workout(ActivitySessionType_Open));
  cl_assert_equal_i(s_szene, HRMActivityScene_HighIntensity);
  cl_assert(workout_service_stop_workout());
  cl_assert_equal_i(s_szene, HRMActivityScene_Default);
  cl_assert(workout_service_start_workout(ActivitySessionType_Run));
  cl_assert_equal_i(s_szene, HRMActivityScene_Run);
  cl_assert(workout_service_stop_workout());
  cl_assert(workout_service_start_workout(ActivitySessionType_Walk));
  cl_assert_equal_i(s_szene, HRMActivityScene_Walk);
}
