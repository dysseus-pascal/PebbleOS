/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2, Schritt 2, AP4: Training ohne Schritte (D17, V1) - mit der echten Glue-Kopie und dem
// echten Kern (Zaehler, Puffer, DLS 81, Minutendatei), dem echten Dauerteil (Merker "Training",
// D15) und der echten activity_sessions.c. Trainings laufen ueber Pebbles echten Workout-Dienst
// (workout_service.c mit P1 und kalorien.c, wie auf obelix): Sein Start schliesst laufende
// Sitzungen und erreicht die Variante ueber D15, sein Stopp speichert das Training in der echten
// Sitzungsliste. Den Merker "ohne Schritte" setzt der Test dort, wo Kieselsport ihn setzt (nach dem
// Start); welche Art ihn bekommt, prueft test_kieselsport mit Kieselsports echter Plattform.
// Spezifikation 6.1: T4.1; P4 (Satz der Sitzung).
//
// Minutenwerte: --wrap ersetzt nur die beiden Messfunktionen des Kerns (Schritte, VMC/Lage);
// Kraepelins Geh- und Schlafdetektor rechnen echt. Der Puls ist ausgeduennt wie auf Stufe 1
// (ein Wert alle zehn Minuten).

#include "applib/data_logging.h"
#include <pbl/drivers/ambient_light.h>
#include <pbl/drivers/rtc.h>
#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/activity_algorithm.h"
#include "pbl/services/activity/activity_private.h"
#include <pbl/cron/cron.h>
#include "pbl/services/activity/boulder/kalorien.h"
#include "pbl/services/activity/boulder/kraepelin/activity_algorithm_kraepelin.h"
#include "pbl/services/activity/boulder/kraepelin/kraepelin_algorithm.h"
#include "pbl/services/activity/boulder/takt.h"
#include "pbl/services/activity/boulder/variante.h"
#include "pbl/services/activity/workout_service.h"
#include "pbl/services/battery/battery_state.h"
#include "pbl/services/data_logging/data_logging_service.h"
#include "pbl/services/filesystem/pfs.h"
#include "pbl/services/hrm/hrm_manager_private.h"
#include "pbl/services/settings/settings_file.h"
#include "pbl/services/time.h"
#include "pbl/util/math.h"
#include "pbl/util/size.h"
#include "pbl/util/units.h"

#include <stdio.h>
#include <string.h>

#include "clar.h"

#include "stubs_activity_insights.h"
#include "stubs_alarm.h"
// Der Wecklauf (wecklauf.c) prueft test_boulder_wecker mit dem echten alarm.c; hier laeuft
// kein Smart Alarm, die Minute hat nichts zu tun.
void boulder_wecklauf_minute(void) {
}
#include "stubs_analytics.h"
#include "stubs_evented_timer.h"
#include "stubs_hexdump.h"
// hr_util.c laeuft echt (Pebbles Workout-Dienst, Zonen).
#include "stubs_irq.h"
#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_sleep.h"
#include "stubs_task_wdt.h"

#include "fake_accel_service.h"
#include "fake_new_timer.h"
#include "fake_rtc.h"
#include "fake_spi_flash.h"
#include "fake_system_task.h"

// =============================================================================================
// Minutenteil: kein Heap, keine DLS-Anlage (T7.1, T7.2)
static bool s_im_minutenteil;
static int s_heap_im_minutenteil;

void *__real_kernel_malloc(size_t bytes);
void *__wrap_kernel_malloc(size_t bytes) {
  s_heap_im_minutenteil += s_im_minutenteil;
  return __real_kernel_malloc(bytes);
}
void *__real_kernel_zalloc(size_t bytes);
void *__wrap_kernel_zalloc(size_t bytes) {
  s_heap_im_minutenteil += s_im_minutenteil;
  return __real_kernel_zalloc(bytes);
}
void *__real_kernel_malloc_check(size_t bytes);
void *__wrap_kernel_malloc_check(size_t bytes) {
  s_heap_im_minutenteil += s_im_minutenteil;
  return __real_kernel_malloc_check(bytes);
}
void *__real_kernel_zalloc_check(size_t bytes);
void *__wrap_kernel_zalloc_check(size_t bytes) {
  s_heap_im_minutenteil += s_im_minutenteil;
  return __real_kernel_zalloc_check(bytes);
}

// Kern: Vorgabe je Minute statt Messung
static uint8_t s_vorgabe_schritte;
static uint16_t s_vorgabe_vmc;
static uint8_t s_vorgabe_lage;
uint32_t __wrap_boulder_kalg_analyze_samples(KAlgState *state, AccelRawData *data,
                                             uint32_t num_samples, uint32_t *consumed_samples) {
  *consumed_samples = num_samples;
  const uint32_t schritte = s_vorgabe_schritte;
  s_vorgabe_schritte = 0;
  return schritte;
}
void __wrap_boulder_kalg_minute_stats(KAlgState *state, uint16_t *vmc, uint8_t *orientation,
                                      bool *still) {
  *vmc = s_vorgabe_vmc;
  *orientation = s_vorgabe_lage;
  *still = false;
}

// =============================================================================================
// Pebble rund um activity_sessions.c und die Glue-Kopie

static ActivityState s_state;
ActivityState *activity_private_state(void) {
  return &s_state;
}

// Sperren: die Activity-Sperre zaehlt mit; am Ende jedes Tests ist sie frei.
static int s_activity_sperre;
void pbl_mutex_init(struct pbl_mutex *m) {
}
void pbl_mutex_deinit(struct pbl_mutex *m) {
}
int pbl_mutex_lock_lr(struct pbl_mutex *m, pbl_timeout_t timeout, uintptr_t lr) {
  if (m == &s_state.mutex) {
    s_activity_sperre++;
  }
  return 0;
}
void pbl_mutex_unlock(struct pbl_mutex *m) {
  if (m == &s_state.mutex) {
    cl_assert(s_activity_sperre > 0);
    s_activity_sperre--;
  }
}
bool pbl_mutex_is_owner(const struct pbl_mutex *m) {
  return true;
}
void pbl_mutex_assert_held(const struct pbl_mutex *m, bool held) {
  if (m == &s_state.mutex) {
    cl_assert_equal_b(s_activity_sperre > 0, held);
  }
}
SettingsFile *activity_private_settings_open(void) {
  return NULL;
}
void activity_private_settings_close(SettingsFile *file) {
}
uint32_t light_get_ambient_lux(void) {
  return 64;
}
AmbientLightLevel ambient_light_level_to_enum(uint32_t light_level) {
  return AmbientLightLevelLight;
}
BatteryChargeState battery_get_charge_state(void) {
  return (BatteryChargeState){ .charge_percent = 70 };
}
bool activity_tracking_on(void) {
  return true;
}
void activity_metrics_prv_get_spo2_sample(uint8_t *percent_out, uint8_t *quality_out) {
  *percent_out = 0;
  *quality_out = 0;
}
void activity_metrics_prv_reset_hr_stats(void) {
}
bool activity_metrics_prv_is_hrm_offwrist(time_t now_utc) {
  return false;
}
void activity_metrics_prv_set_hrm_worn_status(time_t now_utc, bool is_offwrist) {
}
static uint8_t s_puls;  //< Minutenmedian, 0 = kein Wert
void activity_metrics_prv_get_median_hr_bpm(int32_t *median, int32_t *total_weight) {
  if (median) {
    *median = s_puls;
  }
  if (total_weight) {
    *total_weight = s_puls ? 100 : 0;
  }
}
HRZone activity_metrics_prv_get_hr_zone(void) {
  return HRZone_Zone0;
}
uint32_t activity_metrics_prv_get_steps(void) {
  return 0;
}
uint32_t activity_metrics_prv_get_distance_mm(void) {
  return 0;
}
uint32_t activity_metrics_prv_get_resting_calories(void) {
  return 0;
}
uint32_t activity_metrics_prv_get_active_calories(void) {
  return 0;
}
HRMSessionRef hrm_manager_subscribe_with_callback(AppInstallId app_id, uint32_t update_interval_s,
                                                  uint16_t expire_s, HRMFeature features,
                                                  bool low_latency, HRMSubscriberCallback callback,
                                                  void *context) {
  return 1;
}
bool sys_hrm_manager_unsubscribe(HRMSessionRef session) {
  return true;
}
bool sys_hrm_manager_set_update_interval(HRMSessionRef session, uint32_t update_interval_s,
                                         uint16_t expire_s) {
  return true;
}
static HRMActivityScene s_szene;
static int s_szene_aufrufe;
void hrm_manager_set_activity_scene(HRMActivityScene scene) {
  s_szene = scene;
  s_szene_aufrufe++;
}
bool activity_prefs_hrm_activity_tracking_is_enabled(void) {
  return true;
}

// =============================================================================================
// Pebbles Workout-Dienst (echt) braucht sein Umfeld: Activitys Getter, Insights, Profil, HRM.

extern void prv_workout_timer_cb(void *data);
extern void workout_service_reset(void);

// Activitys Getter ueber die Liste der echten activity_sessions.c (activity.c:1511-1529).
bool activity_get_sessions(uint32_t *session_entries, ActivitySession *sessions) {
  pbl_mutex_lock(&s_state.mutex, PBL_FOREVER);
  const uint32_t n = MIN(*session_entries, s_state.activity_sessions_count);
  memcpy(sessions, s_state.activity_sessions, n * sizeof(ActivitySession));
  *session_entries = n;
  pbl_mutex_unlock(&s_state.mutex);
  return true;
}
bool activity_get_metric(ActivityMetric metric, uint32_t history_len, int32_t *history) {
  uint32_t schritte = 0;
  activity_algorithm_get_steps(&schritte);
  *history = (int32_t)schritte;
  return metric == ActivityMetricStepCount;
}
void workout_utils_send_abandoned_workout_notification(void) {
}
// Das Profil fuer Pebbles Rechnungen und die Pulskalorien (kalorien.c): 70 kg, 41 Jahre, Mann.
uint8_t activity_prefs_get_age_years(void) {
  return 41;
}
ActivityGender activity_prefs_get_gender(void) {
  return ActivityGenderMale;
}
uint16_t activity_prefs_get_weight_dag(void) {
  return 7000;
}
uint16_t activity_prefs_get_height_mm(void) {
  return 1800;
}
uint8_t activity_prefs_heart_get_elevated_hr(void) {
  return 100;
}
uint8_t activity_prefs_heart_get_zone1_threshold(void) {
  return 130;
}
uint8_t activity_prefs_heart_get_zone2_threshold(void) {
  return 154;
}
uint8_t activity_prefs_heart_get_zone3_threshold(void) {
  return 172;
}
HRMonitoringInterval activity_prefs_get_hrm_measurement_interval(void) {
  return HRMonitoringInterval_10Min;
}
AppInstallId app_get_app_id(void) {
  return 0;
}
HRMSessionRef sys_hrm_manager_app_subscribe(AppInstallId app_id, uint32_t update_interval_s,
                                            uint16_t expire_s, HRMFeature features) {
  return 2;
}
bool sys_hrm_manager_set_features(HRMSessionRef session, HRMFeature features) {
  return true;
}
// Kieselsport selbst laeuft hier nicht; der erste Lauf der Variante stoesst ihn nur an.
void kieselsport_init(void) {
}

// DLS: 81 (Minuten der Kopie) und 84 (Sitzungen) werden mitgeschrieben.
#define DLS_MAX 64
static AlgMinuteDLSRecord s_dls[DLS_MAX];
static int s_dls_anzahl;
static bool s_dls_scheitert;
static int s_dls_im_minutenteil;
static ActivitySessionDataLoggingRecord s_dls84[DLS_MAX];
static int s_dls84_anzahl;
DataLoggingSession *dls_create(uint32_t tag, DataLoggingItemType item_type, uint16_t item_size,
                               bool buffered, bool resume, const Uuid *uuid) {
  s_dls_im_minutenteil += s_im_minutenteil;
  if (tag == DlsSystemTagActivityMinuteData && s_dls_scheitert) {
    return NULL;
  }
  return (DataLoggingSession *)(uintptr_t)tag;
}
DataLoggingResult dls_log(DataLoggingSession *logging_session, const void *data,
                          uint32_t num_items) {
  if ((uintptr_t)logging_session == DlsSystemTagActivityMinuteData) {
    cl_assert(s_dls_anzahl < DLS_MAX);
    s_dls[s_dls_anzahl++] = *(const AlgMinuteDLSRecord *)data;
  } else if ((uintptr_t)logging_session == DlsSystemTagActivitySession) {
    cl_assert(s_dls84_anzahl < DLS_MAX);
    s_dls84[s_dls84_anzahl++] = *(const ActivitySessionDataLoggingRecord *)data;
  }
  return DATA_LOGGING_SUCCESS;
}
void dls_send_all_sessions(void) {
}

// =============================================================================================
// Eine Minute wie auf der Uhr, mit Buchfuehrung: was jede Minute in Tagessumme, DLS 81 und
// Minutendatei haben soll.

#define MINUTEN_MAX 600
static time_t s_utc[MINUTEN_MAX];
static uint8_t s_roh[MINUTEN_MAX];
static int s_n;
static int s_minute_nr;
static time_t s_t0;
static AccelSamplingRate s_rate;

// Ein Beschleunigungsblock mit so vielen Schritten (Kraepelin zaehlt sie, die Kopie bucht sie).
static void prv_schritte(uint8_t schritte) {
  s_vorgabe_schritte = schritte;
  AccelRawData leer[KALG_SAMPLE_HZ] = {};
  activity_algorithm_handle_accel(leer, ARRAY_LENGTH(leer), 0);
}

// Schritt 2: Der Test erwartet, dass die Variante Schritte gerade nirgends bucht (Training ohne
// Schritte). Eine Erwartung des Tests, nicht aus dem Code gelesen.
static bool s_erwartet_ohne_schritte;
static bool s_ohne[MINUTEN_MAX];  //< ganze Minute im Training ohne Schritte

// Eine Minute wie auf der Uhr. Mit `mitte` laeuft nach 30 s etwas (Start, Stopp), danach kommen
// `nach` weitere Schritte; `gezaehlt` = was die Minute in Tagessumme und DLS 81 haben soll.
static void prv_minute_geteilt(uint8_t vor, void (*mitte)(void), uint8_t nach, uint8_t gezaehlt,
                               uint16_t vmc, uint8_t lage, uint8_t puls) {
  const time_t beginn = rtc_get_time();
  s_vorgabe_vmc = vmc;
  s_vorgabe_lage = lage;
  prv_schritte(vor);
  if (mitte) {
    fake_rtc_increment_time(PBL_SEC_PER_MIN / 2);
    mitte();
    prv_schritte(nach);
    fake_rtc_increment_time(PBL_SEC_PER_MIN / 2);
  } else {
    fake_rtc_increment_time(PBL_SEC_PER_MIN);
  }
  if (workout_service_is_workout_ongoing()) {
    prv_workout_timer_cb(NULL);  // Pebbles Sekundentakt: die Dauer aus der RTC
  }
  s_puls = puls;
  cl_assert(s_n < MINUTEN_MAX);
  s_utc[s_n] = beginn;
  s_roh[s_n] = gezaehlt;
  s_ohne[s_n] = s_erwartet_ohne_schritte && !mitte;
  s_n++;
  s_minute_nr++;
  const time_t jetzt = rtc_get_time();
  AlgMinuteRecord satz = {};
  s_im_minutenteil = true;
  activity_algorithm_minute_handler(jetzt, &satz);
  s_im_minutenteil = false;
  activity_sessions_prv_minute_handler(jetzt);
  fake_system_task_callbacks_invoke_pending();
  boulder_dauerteil_takt(jetzt + 10);
  fake_system_task_callbacks_invoke_pending();
}

static void prv_minute(uint8_t schritte, uint16_t vmc, uint8_t lage, uint8_t puls) {
  prv_minute_geteilt(schritte, NULL, 0, s_erwartet_ohne_schritte ? 0 : schritte, vmc, lage, puls);
}

// Puls wie auf Stufe 1: alle zehn Minuten ein Wert.
static uint8_t prv_puls(uint8_t wert) {
  return ((s_minute_nr % 10) == 0) ? wert : 0;
}

static void prv_gehen(int minuten) {
  for (int i = 0; i < minuten; i++) {
    prv_minute(95 + (i % 7), 2600 + (i % 5) * 90, (uint8_t)(0x41 + (i % 4)), prv_puls(92));
  }
}

// Am Lenker: wenige Schritte, viel Bewegung, die Hand ruhig (eine Lage).
static void prv_lenker(int minuten) {
  for (int i = 0; i < minuten; i++) {
    const int k = s_minute_nr;
    prv_minute((uint8_t)(3 + (k * 7) % 16), (uint16_t)(700 + (k * 37) % 600), 0x37, prv_puls(104));
  }
}

static uint32_t prv_zaehler(void) {
  uint32_t schritte;
  cl_assert(activity_algorithm_get_steps(&schritte));
  return schritte;
}

static uint32_t prv_soll_summe(void) {
  uint32_t summe = 0;
  for (int i = 0; i < s_n; i++) {
    summe += s_roh[i];
  }
  return summe;
}

static const AlgMinuteDLSSample *prv_dls_minute(time_t utc) {
  for (int j = 0; j < s_dls_anzahl; j++) {
    const time_t erste = s_dls[j].hdr.time_utc;
    const int i = (utc - erste) / PBL_SEC_PER_MIN;
    if (utc >= erste && i < s_dls[j].hdr.num_samples) {
      return &s_dls[j].samples[i];
    }
  }
  return NULL;
}

// DLS 81 und Minutendatei Minute fuer Minute: jede mit ihren Schritten, im Training ohne Schritte
// auch nicht aktiv, ohne Strecke und kcal. Geprueft wird, was sicher geschrieben ist (aelter als
// 35 min); Rueckgabe: so viele Minuten aus der Datei.
static int prv_pruefe_minuten(void) {
  static HealthMinuteData datei[MINUTEN_MAX];
  uint32_t n_datei = ARRAY_LENGTH(datei);
  time_t ab = s_t0;
  cl_assert(activity_algorithm_get_minute_history(datei, &n_datei, &ab));
  const time_t geschrieben = rtc_get_time() - 35 * PBL_SEC_PER_MIN;
  int geprueft = 0;
  for (int i = 0; i < s_n && s_utc[i] < geschrieben; i++) {
    const int soll = s_roh[i];
    const AlgMinuteDLSSample *m = prv_dls_minute(s_utc[i]);
    if (m) {
      if (m->base.steps != soll) {
        printf("Minute %d: DLS %d, soll %d (roh %d)\n", i, m->base.steps, soll, s_roh[i]);
      }
      cl_assert_equal_i(m->base.steps, soll);
      if (s_ohne[i]) {
        cl_assert(!m->base.active);
        cl_assert_equal_i(m->distance_cm, 0);
        cl_assert_equal_i(m->active_calories, 0);
      }
    }
    const int d = (int)((s_utc[i] - ab) / PBL_SEC_PER_MIN);
    cl_assert(d >= 0 && d < (int)n_datei);
    cl_assert(!datei[d].is_invalid);
    cl_assert_equal_i(datei[d].steps, soll);
    geprueft++;
  }
  return geprueft;
}

void test_boulder_training__initialize(void) {
  TimezoneInfo tz = { .tm_zone = "CEST", .tm_gmtoff = 2 * PBL_SEC_PER_HOUR };
  time_util_update_timezone(&tz);
  s_t0 = 1759302000;  // 1.10.2026, 09:00 CEST
  fake_rtc_init(100, s_t0);
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  pfs_format(false);
  memset(&s_state, 0, sizeof(s_state));
  s_activity_sperre = 0;
  s_dls_anzahl = 0;
  s_dls_scheitert = false;
  s_dls_im_minutenteil = 0;
  s_heap_im_minutenteil = 0;
  s_n = 0;
  s_minute_nr = 1;
  s_puls = 0;
  s_szene = HRMActivityScene_Default;
  s_szene_aufrufe = 0;
  memset(s_ohne, 0, sizeof(s_ohne));
  s_erwartet_ohne_schritte = false;
  s_dls84_anzahl = 0;
  workout_service_reset();
  workout_service_init();
  // Kieselsports geprueftes Maximum (V2), wie ks_workout_starten es vor dem Start setzt.
  boulder_kalorien_maxpuls(190);
  pbl_cron_init();
  boulder_dauerteil_test_neustart();
  cl_assert(activity_algorithm_init(&s_rate));
  fake_system_task_callbacks_invoke_pending();
}

void test_boulder_training__cleanup(void) {
  fake_system_task_callbacks_invoke_pending();
  workout_service_reset();
  activity_algorithm_deinit();
  fake_system_task_callbacks_invoke_pending();
  pbl_cron_deinit();
  cl_assert_equal_i(s_activity_sperre, 0);
}

// =============================================================================================
// Training ohne Schritte (D17, V1)

// Ein Training ueber Pebbles echten Workout-Dienst, wie Kieselsport es startet (plattform.c,
// ks_workout_starten): erst Pebbles Start, dann - ohne Sperre - der Merker der Variante.
static ActivitySessionType s_start_typ;
static bool s_start_ohne_schritte;
static time_t s_start_um;
static void prv_training_start(void) {
  s_start_um = rtc_get_time();
  cl_assert(workout_service_start_workout(s_start_typ));
  boulder_variante_training_ohne_schritte(s_start_ohne_schritte);
  cl_assert_equal_i(s_activity_sperre, 0);
}
static time_t s_stopp_um;
static void prv_training_stopp(void) {
  s_stopp_um = rtc_get_time();
  prv_workout_timer_cb(NULL);  // Pebbles Sekundentakt lief bis jetzt
  cl_assert(workout_service_stop_workout());
}

static uint16_t prv_rate(void) {
  uint16_t schritte;
  uint32_t ms;
  time_t ende;
  cl_assert(activity_algorithm_get_step_rate(&schritte, &ms, &ende));
  return schritte;
}

// Pebbles Sitzungen eines Typs: die erkannte (manual = 0) und die des Trainings (manual = 1).
static const ActivitySession *prv_sitzung_manuell(ActivitySessionType typ, bool manuell) {
  const ActivitySession *gefunden = NULL;
  for (int i = 0; i < s_state.activity_sessions_count; i++) {
    const ActivitySession *a = &s_state.activity_sessions[i];
    if (a->type == typ && a->manual == manuell) {
      cl_assert(!gefunden);  // hoechstens eine
      gefunden = a;
    }
  }
  return gefunden;
}

// T4.1 (Kriterium 4, V1 vom Nutzer geaendert): Ein Training ohne Schritte (Velo, MTB, Kraft, Yoga,
// Schwimmen) zaehlt vom Start bis zum Stopp keine Schritte - nicht in der Tagessumme, nicht in der
// Minute (DLS 81, Minutendatei), Rate 0. Auch viele Schritte (Kraft, Schwimmen: Kraepelin sieht
// Gehen) nicht. Was vor dem Start und nach dem Stopp in derselben Minute kommt, zaehlt.
void test_boulder_training__t4_1_ohne_schritte_zaehlt_nichts(void) {
  // Der Merker ohne Training wirkt nicht.
  boulder_variante_training_ohne_schritte(true);
  prv_gehen(10);
  cl_assert(prv_rate() > 0);
  s_start_typ = ActivitySessionType_Open;
  s_start_ohne_schritte = true;
  prv_minute_geteilt(60, prv_training_start, 35, 60, 2600, 0x41, 0);
  const uint32_t summe = prv_zaehler();
  s_erwartet_ohne_schritte = true;
  for (int i = 0; i < 20; i++) {
    prv_minute((i % 3) ? 14 : 118, (uint16_t)(700 + i * 40), 0x37, prv_puls(120));
    cl_assert_equal_i(prv_zaehler(), summe);
    cl_assert_equal_i(prv_rate(), 0);
  }
  s_erwartet_ohne_schritte = false;
  prv_minute_geteilt(50, prv_training_stopp, 45, 45, 2600, 0x41, 0);
  cl_assert(prv_rate() > 0);
  // Pebble hat das Training gespeichert, die Variante zaehlt wieder.
  const ActivitySession *t = prv_sitzung_manuell(ActivitySessionType_Open, true);
  cl_assert(t);
  cl_assert_equal_i(t->start_utc, s_start_um);
  cl_assert_equal_i(t->length_min, 21);
  prv_gehen(40);
  cl_assert_equal_i(prv_zaehler(), prv_soll_summe());
  cl_assert(prv_pruefe_minuten() >= 10 + 22);
}

// T4.1, die andere Seite: Laufen und Wandern (Run, Walk) zaehlen Schritte wie Pebble. Ein Training
// ohne Schritte davor laesst nichts zurueck: Der Merker endet mit seinem Training - auch fuer ein
// Training aus Pebbles eigener Workout-App, die den Merker nie anfasst (das erste unten).
void test_boulder_training__t4_1_laufen_zaehlt_schritte(void) {
  prv_gehen(5);
  s_start_typ = ActivitySessionType_Open;
  s_start_ohne_schritte = true;
  prv_training_start();
  s_erwartet_ohne_schritte = true;
  prv_lenker(5);
  s_erwartet_ohne_schritte = false;
  prv_training_stopp();
  const ActivitySessionType arten[] = {ActivitySessionType_Run, ActivitySessionType_Walk};
  for (size_t k = 0; k < ARRAY_LENGTH(arten); k++) {
    if (k == 0) {
      cl_assert(workout_service_start_workout(arten[k]));  // Pebbles Workout-App
    } else {
      s_start_typ = arten[k];
      s_start_ohne_schritte = false;
      prv_training_start();  // Kieselsport (Wandern)
    }
    const uint32_t vorher = prv_zaehler();
    for (int i = 0; i < 12; i++) {
      prv_minute(150, 3200, 0x42, prv_puls(140));
      cl_assert(prv_rate() > 0);
    }
    cl_assert_equal_i(prv_zaehler(), vorher + 12 * 150);
    prv_training_stopp();
  }
  prv_gehen(40);
  cl_assert_equal_i(prv_zaehler(), prv_soll_summe());
  cl_assert(prv_pruefe_minuten() >= 5 + 5 + 24);
}

// Entscheid 2 vom 08.10.2026: Bei einem Training ohne Schritte streicht die Variante nur, solange
// es LAEUFT. Kieselsport hebt den Merker bei Pause, Speichern und Verwerfen auf und setzt ihn beim
// Weiter wieder, jedes Mal nach Pebbles Aufruf und ohne Sperre (dienst.c; test_kieselsport prueft
// diese Seite). Hier die Wirkung an der echten Variante, waehrend Pebbles Training die ganze Zeit
// besteht: laufend 0, in der Pause zaehlen Tagessumme, Minute (DLS 81) und Rate, Weiter wieder 0,
// nach dem Speichern zaehlt es auch in den 30 s bis zu Pebbles Stopp (V4), nach dem Verwerfen
// sofort.
static void prv_kieselsport_pause(void) {
  cl_assert(workout_service_pause_workout(true));
  boulder_variante_training_ohne_schritte(false);
}
static void prv_kieselsport_weiter(void) {
  cl_assert(workout_service_pause_workout(false));
  boulder_variante_training_ohne_schritte(true);
}
static void prv_kieselsport_verwerfen(void) {
  boulder_variante_training_ohne_schritte(false);
  cl_assert(boulder_training_verwerfen());
}

// Laufend: viele Schritte (an der Hantel, im Wasser), keiner zaehlt.
static void prv_training_minuten_ohne(int minuten) {
  const uint32_t vorher = prv_zaehler();
  s_erwartet_ohne_schritte = true;
  for (int i = 0; i < minuten; i++) {
    prv_minute((i % 2) ? 40 : 110, 2400, 0x37, prv_puls(125));
    cl_assert_equal_i(prv_zaehler(), vorher);
    cl_assert_equal_i(prv_rate(), 0);
  }
  s_erwartet_ohne_schritte = false;
}

// Pause: zum Auto, zur Garderobe - jeder Schritt zaehlt.
static void prv_training_minuten_mit(int minuten) {
  const uint32_t vorher = prv_zaehler();
  for (int i = 0; i < minuten; i++) {
    prv_minute(90, 2600, 0x41, prv_puls(100));
    cl_assert(prv_rate() > 0);
  }
  cl_assert_equal_i(prv_zaehler(), vorher + (uint32_t)minuten * 90);
}

void test_boulder_training__pause_zaehlt_schritte(void) {
  prv_gehen(5);
  s_start_typ = ActivitySessionType_Open;
  s_start_ohne_schritte = true;
  prv_training_start();
  prv_training_minuten_ohne(4);
  prv_kieselsport_pause();
  prv_training_minuten_mit(3);
  cl_assert(workout_service_is_workout_ongoing());
  prv_kieselsport_weiter();
  prv_training_minuten_ohne(4);
  // Speichern: Kieselsport pausiert Pebble und hebt den Merker auf; Pebbles Stopp kommt 30 s
  // spaeter, mitten in dieser Minute. Beide Haelften zaehlen.
  prv_kieselsport_pause();
  prv_minute_geteilt(40, prv_training_stopp, 45, 85, 2600, 0x41, 0);
  cl_assert(prv_rate() > 0);
  const ActivitySession *t = prv_sitzung_manuell(ActivitySessionType_Open, true);
  cl_assert(t);
  cl_assert_equal_i(t->start_utc, s_start_um);
  // Verwerfen: zaehlt sofort wieder.
  s_start_ohne_schritte = true;
  prv_training_start();
  prv_training_minuten_ohne(3);
  prv_kieselsport_verwerfen();
  cl_assert(!workout_service_is_workout_ongoing());
  prv_training_minuten_mit(3);
  prv_gehen(40);
  cl_assert_equal_i(prv_zaehler(), prv_soll_summe());
  cl_assert(prv_pruefe_minuten() >= 5 + 4 + 3 + 4 + 1 + 3 + 3);
}

// P4 (F4 = A): Pebbles Satz einer Sitzung (DLS 84) in Fassung 7, 27 Byte; `manual` traegt nur das
// Training. Boulder traegt es damit nicht noch einmal als Workout ein; Spaziergaenge bleiben erkannt.
void test_boulder_training__dls84_manual_nur_beim_training(void) {
  prv_gehen(20);
  s_start_typ = ActivitySessionType_Open;
  s_start_ohne_schritte = true;
  prv_training_start();
  s_erwartet_ohne_schritte = true;
  prv_lenker(15);
  s_erwartet_ohne_schritte = false;
  prv_training_stopp();
  prv_gehen(40);
  int training = 0, gehen = 0;
  for (int i = 0; i < s_dls84_anzahl; i++) {
    const ActivitySessionDataLoggingRecord *r = &s_dls84[i];
    const uint8_t *bytes = (const uint8_t *)r;
    cl_assert_equal_i(r->version, 7);
    cl_assert_equal_i(r->size, 27);
    cl_assert_equal_i(bytes[26], r->start_utc == (uint32_t)s_start_um);
    training += r->start_utc == (uint32_t)s_start_um;
    gehen += r->activity == ActivitySessionType_Walk;
  }
  cl_assert_equal_i(training, 1);
  cl_assert(gehen > 0);
  cl_assert_equal_i(s_heap_im_minutenteil, 0);
}
