/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/activity_calculators.h"
#include "pbl/services/activity/boulder/kalorien.h"
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

// P1 IM ECHTEN WORKOUT-DIENST (Spezifikation Schritt 2, AP1): die echte workout_service.c, gebaut
// wie auf obelix und qemu_emery, mit der echten kalorien.c. Puls und Schritte kommen ueber
// Pebbles Health-Rundruf wie auf der Uhr. Geprueft wird, was in Pebbles Sitzung und Meldung
// ankommt (T5.1 bis zur Sitzung, T5.2, T3.1). Die Attrappen zeichnen nur auf.

extern void prv_workout_timer_cb(void *data);
extern void workout_service_reset(void);
extern void workout_service_get_active_kcalories(int32_t *active);
extern void fake_ruhepuls_setzen(uint8_t bpm);

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
bool activity_get_sessions(uint32_t *session_entries, ActivitySession *sessions) {
  *session_entries = 0;
  return true;
}

static char s_erkennung[8];
void activity_algorithm_enable_activity_tracking(bool enable) {
  const size_t n = strlen(s_erkennung);
  cl_assert(n + 1 < sizeof(s_erkennung));
  s_erkennung[n] = enable ? '1' : '0';
}

static int s_meldungen;
static ActivitySession s_meldung;
void activity_insights_push_activity_session_notification(time_t notif_time,
                                                          ActivitySession *session, int32_t avg_hr,
                                                          int32_t *hr_zone_time_s) {
  s_meldungen++;
  s_meldung = *session;
}

void workout_utils_send_abandoned_workout_notification(void) {
}

static uint16_t s_gewicht_dag;
static uint8_t s_alter;
uint8_t activity_prefs_get_age_years(void) { return s_alter; }
ActivityGender activity_prefs_get_gender(void) { return ActivityGenderMale; }
uint16_t activity_prefs_get_weight_dag(void) { return s_gewicht_dag; }
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

static HRMActivityScene s_szene;
HRMSessionRef sys_hrm_manager_app_subscribe(AppInstallId app_id, uint32_t update_interval_s,
                                            uint16_t expire_s, HRMFeature features) {
  return 1;
}
bool sys_hrm_manager_unsubscribe(HRMSessionRef ref) {
  return true;
}
bool sys_hrm_manager_set_update_interval(HRMSessionRef session, uint32_t update_interval_s,
                                         uint16_t expire_s) {
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

static void prv_sekunden(int n) {
  for (int i = 0; i < n; i++) {
    fake_rtc_increment_time(1);
    prv_workout_timer_cb(NULL);
  }
}

static void prv_puls_mit(uint8_t bpm, HRMQuality q) {
  PebbleHealthEvent e = {
    .type = HealthEventHeartRateUpdate,
    .data.heart_rate_update = {.current_bpm = bpm, .quality = q},
  };
  workout_service_health_event_handler(&e);
}

static void prv_puls(uint8_t bpm) {
  prv_puls_mit(bpm, HRMQuality_Good);
}

static void prv_schritte(int n) {
  s_schritte_tag += n;
  PebbleHealthEvent e = {
    .type = HealthEventMovementUpdate,
    .data.movement_update.steps = s_schritte_tag,
  };
  workout_service_health_event_handler(&e);
}

// n Sekunden mit einem Pulswert je Sekunde (am Anfang jeder Sekunde)
static void prv_puls_sekunden(uint8_t bpm, int n) {
  for (int i = 0; i < n; i++) {
    prv_puls(bpm);
    prv_sekunden(1);
  }
}

// Pebbles eigene Summe (active_calories), in kcal gerundet: der einzige Getter dafuer
static int32_t prv_pebble_kcal(void) {
  int32_t kcal = -1;
  workout_service_get_active_kcalories(&kcal);
  return kcal;
}

// Die aktiven kcal der gespeicherten Sitzung nach dem Stopp
static int prv_stopp_kcal(void) {
  const int vorher = s_anzahl_gespeichert;
  cl_assert(workout_service_stop_workout());
  cl_assert_equal_i(s_anzahl_gespeichert, vorher + 1);
  return s_gespeichert[vorher].step_data.active_kcalories;
}

// Was Kieselsport waehrend des Trainings anzeigt (Pebbles Dauer als aktive Zeit), in Kalorien.
// Der letzte Pulswert zaehlt darin schon bis zur Dauer; Pebbles Summe bekommt ihn erst mit dem
// naechsten Wert oder beim Stopp.
static int32_t prv_bisher(void) {
  int32_t dauer = 0;
  cl_assert(workout_service_get_current_workout_info(NULL, &dauer, NULL, NULL, NULL));
  return boulder_kalorien_bisher(dauer);
}

// Pebbles Schritt-kcal fuer eine Meldung (n Schritte in dt Sekunden), mit Pebbles eigenen
// Rechnern wie in Pebbles test_workout_service.c:313-318.
static int32_t prv_pebble_schritt_cal(int n, int dt) {
  const int32_t mm = activity_private_compute_distance_mm(n, dt * PBL_MSEC_PER_SEC);
  return activity_private_compute_active_calories(mm, dt * PBL_MSEC_PER_SEC);
}

void test_kieselsport_kalorien__initialize(void) {
  fake_rtc_init(0, 1700000000);
  fake_event_init();
  fake_mutex_reset(true);
  workout_service_reset();
  workout_service_init();
  memset(s_gespeichert, 0, sizeof(s_gespeichert));
  s_anzahl_gespeichert = 0;
  memset(s_erkennung, 0, sizeof(s_erkennung));
  s_meldungen = 0;
  s_meldung = (ActivitySession){};
  s_schritte_tag = 5000;
  s_szene = HRMActivityScene_Default;
  s_gewicht_dag = 7000;
  s_alter = 41;
  fake_ruhepuls_setzen(54);
  boulder_kalorien_maxpuls(190);
}

void test_kieselsport_kalorien__cleanup(void) {
  workout_service_reset();
  fake_mutex_reset(true);
}

// ---------------------------------------------------------------------------------------
// T5.1 bis in Pebbles Sitzung: Boulders Radfahrt ueber Pebbles Rundruf. Open ohne Schritte
// bekam bei Pebble 0 kcal; jetzt steht Boulders Wert in Sitzung und Meldung (B: 156,17).
void test_kieselsport_kalorien__fahrt_in_der_sitzung(void) {
  static const uint8_t FAHRT[] = {
    95, 94, 126, 121, 120, 122, 121, 128, 130, 132, 128, 122,
    129, 125, 140, 139, 137, 140, 135, 125, 129, 125, 143, 145,
  };
  cl_assert(workout_service_start_workout(ActivitySessionType_Open));
  for (unsigned i = 0; i < sizeof(FAHRT); i++) {
    prv_puls(FAHRT[i]);
    prv_sekunden(i + 1 < sizeof(FAHRT) ? 60 : 10);
  }
  // Kieselsport liest den Wert in der Pause vor dem Stopp; dort steht er.
  cl_assert(workout_service_pause_workout(true));
  prv_sekunden(30);
  const int32_t angezeigt = prv_bisher();
  cl_assert(angezeigt >= 156170 && angezeigt <= 156180);
  cl_assert_equal_i(prv_stopp_kcal(), 156);
  cl_assert_equal_i(s_meldungen, 1);
  cl_assert_equal_i(s_meldung.step_data.active_kcalories, 156);
  // Pebbles Ruhe-kcal bleiben, wie sie sind
  const int32_t ruhe_cal = activity_private_compute_resting_calories(23);
  cl_assert_equal_i(s_gespeichert[0].step_data.resting_kcalories,
                    ROUND(ruhe_cal, ACTIVITY_CALORIES_PER_KCAL));
}

// T5.2 Pause: Was in der Pause geschieht (Puls alle 10 s, Schritte), zaehlt nicht, und die
// Anzeige steht. Gleich viel wie dasselbe Training ohne Pause.
void test_kieselsport_kalorien__pause_zaehlt_nicht(void) {
  cl_assert(workout_service_start_workout(ActivitySessionType_Open));
  prv_puls_sekunden(150, 120);
  cl_assert(workout_service_pause_workout(true));
  const int32_t vor_der_pause = prv_bisher();
  for (int i = 0; i < 30; i++) {
    prv_puls(170);
    prv_schritte(20);
    prv_sekunden(10);
  }
  cl_assert_equal_i(prv_bisher(), vor_der_pause);
  cl_assert(workout_service_pause_workout(false));
  prv_puls_sekunden(150, 60);
  const int mit_pause = prv_stopp_kcal();

  cl_assert(workout_service_start_workout(ActivitySessionType_Open));
  prv_puls_sekunden(150, 180);
  const int ohne_pause = prv_stopp_kcal();
  cl_assert(mit_pause > 0);
  cl_assert_equal_i(mit_pause, ohne_pause);
}

// T5.2 Luecke > 60 s: Der letzte Wert vor der Luecke gilt 60 s, nicht 300 s.
void test_kieselsport_kalorien__luecke_nicht_voll(void) {
  cl_assert(workout_service_start_workout(ActivitySessionType_Open));
  prv_puls_sekunden(150, 100);
  prv_sekunden(300);
  prv_puls_sekunden(150, 100);
  const int32_t luecke = prv_bisher();
  const int luecke_kcal = prv_stopp_kcal();

  // 99 + 60 + 100 s gedeckt
  cl_assert(workout_service_start_workout(ActivitySessionType_Open));
  prv_puls_sekunden(150, 259);
  cl_assert_equal_i(prv_bisher(), luecke);
  cl_assert_equal_i(prv_stopp_kcal(), luecke_kcal);
}

// T5.2 Off-Wrist und F5 a = B: Nach dem letzten Pulswert deckt er 60 s, danach rechnen die
// Schritte mit Pebbles Formel. Davor zaehlen die Schritte nicht.
void test_kieselsport_kalorien__off_wrist_dann_schritte(void) {
  cl_assert(workout_service_start_workout(ActivitySessionType_Run));
  // Puls bei 0 ... 100, dann ab; der Wert von 100 gilt bis 160
  for (int t = 0; t <= 100; t++) {
    prv_puls(150);
    if (t < 100) {
      prv_sekunden(1);
    }
  }
  prv_puls_mit(0, HRMQuality_OffWrist);
  int32_t schritte_cal = 0;
  for (int t = 110; t <= 400; t += 10) {
    prv_sekunden(10);
    prv_schritte(27);
    if (t > 160) {
      schritte_cal += prv_pebble_schritt_cal(27, 10);
    }
  }
  const int32_t gesamt = prv_bisher();
  cl_assert(workout_service_stop_workout());

  // Derselbe Puls ohne Schritte und ohne Off-Wrist: 160 s gedeckt
  cl_assert(workout_service_start_workout(ActivitySessionType_Open));
  prv_puls_sekunden(150, 100);
  prv_puls(150);
  prv_sekunden(300);
  const int32_t nur_puls = prv_bisher();
  cl_assert(schritte_cal > 0);
  cl_assert(nur_puls > 0);
  cl_assert_equal_i(gesamt, nur_puls + schritte_cal);
}

// F5 a = B, kein Puls: genau Pebbles Schritt-kcal, wie ohne Kieselsport.
void test_kieselsport_kalorien__ohne_puls_wie_pebble(void) {
  cl_assert(workout_service_start_workout(ActivitySessionType_Run));
  int32_t pebble = 0;
  for (int i = 0; i < 60; i++) {
    prv_sekunden(10);
    prv_schritte(28);
    pebble += prv_pebble_schritt_cal(28, 10);
  }
  cl_assert_equal_i(prv_bisher(), pebble);
  cl_assert_equal_i(prv_pebble_kcal(), ROUND(pebble, ACTIVITY_CALORIES_PER_KCAL));
  cl_assert_equal_i(prv_stopp_kcal(), ROUND(pebble, ACTIVITY_CALORIES_PER_KCAL));
}

// Mit Puls zaehlt der Schritt nicht doppelt: ein Lauf mit Schritten = derselbe Puls ohne.
void test_kieselsport_kalorien__mit_puls_keine_schritt_kcal(void) {
  cl_assert(workout_service_start_workout(ActivitySessionType_Run));
  for (int i = 0; i < 600; i++) {
    prv_puls(160);
    if (i % 5 == 4) {
      prv_schritte(14);
    }
    prv_sekunden(1);
  }
  const int lauf = prv_stopp_kcal();
  cl_assert(workout_service_start_workout(ActivitySessionType_Walk));
  prv_puls_sekunden(160, 600);
  cl_assert_equal_i(prv_stopp_kcal(), lauf);
  cl_assert(lauf > 0);
}

// Ohne brauchbares Profil (hier kein Gewicht) bleibt Pebbles Rechnung, auch mit Puls.
void test_kieselsport_kalorien__ohne_profil_pebbles_rechnung(void) {
  s_gewicht_dag = 0;
  cl_assert(workout_service_start_workout(ActivitySessionType_Run));
  int32_t pebble = 0;
  for (int i = 0; i < 60; i++) {
    prv_puls(160);
    prv_sekunden(10);
    prv_schritte(28);
    pebble += prv_pebble_schritt_cal(28, 10);
  }
  cl_assert_equal_i(prv_bisher(), pebble);
  cl_assert_equal_i(prv_stopp_kcal(), ROUND(pebble, ACTIVITY_CALORIES_PER_KCAL));
}

// ---------------------------------------------------------------------------------------
// T3.1 Verwerfen (F3 = A): keine Sitzung, keine Meldung; sonst endet das Training wie jeder Stopp.

void test_kieselsport_kalorien__verwerfen_speichert_nichts(void) {
  cl_assert(workout_service_start_workout(ActivitySessionType_Open));
  for (int i = 0; i < 300; i++) {
    prv_puls(140);
    prv_schritte(2);
    prv_sekunden(1);
  }
  cl_assert(boulder_training_verwerfen());
  cl_assert_equal_i(s_anzahl_gespeichert, 0);
  cl_assert_equal_i(s_meldungen, 0);
  cl_assert(!workout_service_is_workout_ongoing());
  cl_assert_equal_s(s_erkennung, "01");
  cl_assert_equal_i(s_szene, HRMActivityScene_Default);
  // Ein zweites Verwerfen findet nichts mehr
  cl_assert(!boulder_training_verwerfen());
}

// Aus der Pause verworfen (Kieselsport speichert und verwirft nur aus der Pause)
void test_kieselsport_kalorien__verwerfen_aus_der_pause(void) {
  cl_assert(workout_service_start_workout(ActivitySessionType_Run));
  prv_puls_sekunden(150, 200);
  cl_assert(workout_service_pause_workout(true));
  prv_sekunden(20);
  cl_assert(boulder_training_verwerfen());
  cl_assert_equal_i(s_anzahl_gespeichert, 0);
  cl_assert_equal_i(s_meldungen, 0);
  cl_assert(!workout_service_is_workout_ongoing());
}

// Der Merker trifft kein spaeteres Training: Das naechste wird wieder gespeichert.
void test_kieselsport_kalorien__nach_verwerfen_wieder_speichern(void) {
  cl_assert(workout_service_start_workout(ActivitySessionType_Open));
  prv_puls_sekunden(150, 90);
  cl_assert(boulder_training_verwerfen());
  cl_assert(workout_service_start_workout(ActivitySessionType_Open));
  prv_puls_sekunden(150, 90);
  cl_assert(workout_service_stop_workout());
  cl_assert_equal_i(s_anzahl_gespeichert, 1);
  cl_assert_equal_i(s_meldungen, 1);
  cl_assert(s_gespeichert[0].step_data.active_kcalories > 0);
}
