/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2, AP1: die Abweichungen D1-D14 der Glue-Kopie, mit dem echten (kopierten) Kern,
// echter settings_file.c auf Fake-Flash und dem steuerbaren Dauerteil (fake_dauerteil.c).
// Spezifikation 9: T2.4, T6.6, T7.1, T7.1b, T7.2, T7.5, T7.6, T7.7 und die Grundfaelle von
// D1 (Schlaf verworfen, Gehfilter) und D3-D5 (Zurueckhalten, Streichen).
//
// Minutenwerte kommen auf zwei Wegen: echt aus Beschleunigung (T6.6) oder als Vorgabe je
// Minute; dann ersetzt --wrap nur die beiden Messfunktionen des Kerns (Schritte, VMC),
// Schlaf- und Gehdetektor rechnen echt.

#include "applib/accel_service.h"
#include "applib/data_logging.h"
#include <pbl/drivers/ambient_light.h>
#include <pbl/drivers/rtc.h>
#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/activity_algorithm.h"
#include "pbl/services/activity/activity_private.h"
#include "pbl/services/activity/boulder/kraepelin/activity_algorithm_kraepelin.h"
#include "pbl/services/activity/boulder/kraepelin/kraepelin_algorithm.h"
#include "pbl/services/activity/boulder/variante.h"
#include "pbl/services/battery/battery_state.h"
#include "pbl/services/data_logging/data_logging_service.h"
#include "pbl/services/filesystem/pfs.h"
#include "pbl/services/hrm/hrm_manager_private.h"
#include "pbl/services/settings/settings_file.h"
#include "pbl/services/settings/settings_raw_iter.h"
#include "pbl/services/system_task.h"
#include "pbl/services/time.h"
#include "pbl/util/math.h"
#include "pbl/util/size.h"
#include "pbl/util/units.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "clar.h"

#include "stubs_analytics.h"
#include "stubs_hexdump.h"
#include "stubs_hr_util.h"
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

#include "fake_dauerteil.h"
#include "fixture_lesen.h"

// =============================================================================================
// Sperren: die Activity-Sperre ist die des Tests; jede andere gehaltene Sperre ist die der
// Kopie (oder eine innere Pebbles). T7.5: nie Activity unter einer anderen.
static struct pbl_mutex s_aktivitaet;
static struct pbl_mutex *s_gehalten[16];
static int s_tiefe;
static int s_verstoesse;
static int s_aktivitaet_genommen;

void pbl_mutex_init(struct pbl_mutex *m) {
}
void pbl_mutex_deinit(struct pbl_mutex *m) {
}
int pbl_mutex_lock_lr(struct pbl_mutex *m, pbl_timeout_t timeout, uintptr_t lr) {
  if (m == &s_aktivitaet) {
    s_aktivitaet_genommen++;
    for (int i = 0; i < s_tiefe; i++) {
      if (s_gehalten[i] != &s_aktivitaet) {
        s_verstoesse++;
      }
    }
  }
  cl_assert(s_tiefe < (int)ARRAY_LENGTH(s_gehalten));
  s_gehalten[s_tiefe++] = m;
  return 0;
}
void pbl_mutex_unlock(struct pbl_mutex *m) {
  for (int i = s_tiefe - 1; i >= 0; i--) {
    if (s_gehalten[i] == m) {
      memmove(&s_gehalten[i], &s_gehalten[i + 1], (s_tiefe - i - 1) * sizeof(s_gehalten[0]));
      s_tiefe--;
      return;
    }
  }
  cl_fail("Sperre freigegeben, die nicht gehalten war");
}
bool pbl_mutex_is_owner(const struct pbl_mutex *m) {
  return true;
}
void pbl_mutex_assert_held(const struct pbl_mutex *m, bool held) {
}

// =============================================================================================
// Zaehler fuer den Minutenteil (T7.1, T7.2): was dort passiert, waehrend s_im_minutenteil.
static bool s_im_minutenteil;
static int s_heap_im_minutenteil;
static int s_datei_im_minutenteil;
static int s_verdichtungen;
static int s_verdichtungen_im_minutenteil;
static int s_gruppen_in_datei;

// Jede Verdichtung der Minutendatei (auch die, die settings_file.c selbst ausloest) holt
// genau diesen Puffer (settings_file.c:222); --wrap auf die Funktion saehe die inneren
// Aufrufe nicht.
#define VERDICHTUNGS_PUFFER (SETTINGS_KEY_MAX_LEN + SETTINGS_VAL_MAX_LEN)

void *__real_kernel_malloc(size_t bytes);
void *__wrap_kernel_malloc(size_t bytes) {
  s_heap_im_minutenteil += s_im_minutenteil;
  if (bytes == VERDICHTUNGS_PUFFER) {
    s_verdichtungen++;
    s_verdichtungen_im_minutenteil += s_im_minutenteil;
  }
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
void *__real_task_malloc(size_t bytes);
void *__wrap_task_malloc(size_t bytes) {
  s_heap_im_minutenteil += s_im_minutenteil;
  return __real_task_malloc(bytes);
}
void *__real_task_zalloc(size_t bytes);
void *__wrap_task_zalloc(size_t bytes) {
  s_heap_im_minutenteil += s_im_minutenteil;
  return __real_task_zalloc(bytes);
}

status_t __real_settings_file_open(SettingsFile *file, const char *name, int max_used_space);
status_t __wrap_settings_file_open(SettingsFile *file, const char *name, int max_used_space) {
  s_datei_im_minutenteil += s_im_minutenteil;
  return __real_settings_file_open(file, name, max_used_space);
}
status_t __real_settings_file_each(SettingsFile *file, SettingsFileEachCallback cb, void *ctx);
status_t __wrap_settings_file_each(SettingsFile *file, SettingsFileEachCallback cb, void *ctx) {
  s_datei_im_minutenteil += s_im_minutenteil;
  return __real_settings_file_each(file, cb, ctx);
}
status_t __real_settings_file_set(SettingsFile *file, const void *key, size_t key_len,
                                  const void *val, size_t val_len);
status_t __wrap_settings_file_set(SettingsFile *file, const void *key, size_t key_len,
                                  const void *val, size_t val_len) {
  s_datei_im_minutenteil += s_im_minutenteil;
  s_gruppen_in_datei++;
  return __real_settings_file_set(file, key, key_len, val, val_len);
}

// =============================================================================================
// Kern: Vorgabe je Minute statt Messung (nur die beiden Messfunktionen)
static bool s_vorgabe_an;
static uint8_t s_vorgabe_schritte;
static uint16_t s_vorgabe_vmc;
static uint8_t s_vorgabe_lage;

uint32_t __real_boulder_kalg_analyze_samples(KAlgState *state, AccelRawData *data,
                                             uint32_t num_samples, uint32_t *consumed_samples);
uint32_t __wrap_boulder_kalg_analyze_samples(KAlgState *state, AccelRawData *data,
                                             uint32_t num_samples, uint32_t *consumed_samples) {
  if (!s_vorgabe_an) {
    return __real_boulder_kalg_analyze_samples(state, data, num_samples, consumed_samples);
  }
  *consumed_samples = num_samples;
  const uint32_t schritte = s_vorgabe_schritte;
  s_vorgabe_schritte = 0;
  return schritte;
}
void __real_boulder_kalg_minute_stats(KAlgState *state, uint16_t *vmc, uint8_t *orientation,
                                      bool *still);
void __wrap_boulder_kalg_minute_stats(KAlgState *state, uint16_t *vmc, uint8_t *orientation,
                                      bool *still) {
  if (!s_vorgabe_an) {
    __real_boulder_kalg_minute_stats(state, vmc, orientation, still);
    return;
  }
  *vmc = s_vorgabe_vmc;
  *orientation = s_vorgabe_lage;
  *still = false;
}

// =============================================================================================
// Pebble rund um die Glue
uint32_t light_get_ambient_lux(void) {
  return 64;
}
AmbientLightLevel ambient_light_level_to_enum(uint32_t light_level) {
  return AmbientLightLevelLight;
}
static bool s_geladen;
BatteryChargeState battery_get_charge_state(void) {
  return (BatteryChargeState){.is_plugged = s_geladen};
}
bool activity_tracking_on(void) {
  return true;
}

// Sitzungen: wie activity_sessions.c unter der Activity-Sperre
static ActivitySession s_sitzungen[64];
static int s_sitzungen_anzahl;
void activity_sessions_prv_add_activity_session(ActivitySession *session) {
  pbl_mutex_lock(&s_aktivitaet, PBL_FOREVER);
  for (int i = 0; i < s_sitzungen_anzahl; i++) {
    if (s_sitzungen[i].type == session->type && s_sitzungen[i].start_utc == session->start_utc) {
      s_sitzungen[i] = *session;
      pbl_mutex_unlock(&s_aktivitaet);
      return;
    }
  }
  cl_assert(s_sitzungen_anzahl < (int)ARRAY_LENGTH(s_sitzungen));
  s_sitzungen[s_sitzungen_anzahl++] = *session;
  pbl_mutex_unlock(&s_aktivitaet);
}
bool activity_sessions_prv_is_sleep_activity(ActivitySessionType activity_type) {
  return activity_type == ActivitySessionType_Sleep ||
         activity_type == ActivitySessionType_RestfulSleep ||
         activity_type == ActivitySessionType_Nap ||
         activity_type == ActivitySessionType_RestfulNap;
}
void activity_sessions_prv_delete_activity_session(ActivitySession *session) {
  pbl_mutex_lock(&s_aktivitaet, PBL_FOREVER);
  pbl_mutex_unlock(&s_aktivitaet);
}

// Metriken: SpO2, "nicht am Arm" und das Zuruecksetzen nehmen die Activity-Sperre
// (activity_metrics.c:562, :576, :605); Median und Summen nicht.
static uint8_t s_spo2_wartend;
static int32_t s_puls = 60;
void activity_metrics_prv_get_spo2_sample(uint8_t *percent_out, uint8_t *quality_out) {
  pbl_mutex_lock(&s_aktivitaet, PBL_FOREVER);
  *percent_out = s_spo2_wartend;
  *quality_out = s_spo2_wartend ? 3 : 0;
  s_spo2_wartend = 0;
  pbl_mutex_unlock(&s_aktivitaet);
}
void activity_metrics_prv_reset_hr_stats(void) {
  pbl_mutex_lock(&s_aktivitaet, PBL_FOREVER);
  pbl_mutex_unlock(&s_aktivitaet);
}
bool activity_metrics_prv_is_hrm_offwrist(time_t now_utc) {
  pbl_mutex_lock(&s_aktivitaet, PBL_FOREVER);
  pbl_mutex_unlock(&s_aktivitaet);
  return false;
}
void activity_metrics_prv_set_hrm_worn_status(time_t now_utc, bool is_offwrist) {
}
void activity_metrics_prv_get_median_hr_bpm(int32_t *median, int32_t *total_weight) {
  if (median) {
    *median = s_puls;
  }
  if (total_weight) {
    *total_weight = 100;
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

// DLS 81
static bool s_dls_scheitert;
static int s_dls_angelegt;
static int s_dls_im_minutenteil;
static AlgMinuteDLSRecord s_dls[128];
static int s_dls_anzahl;
DataLoggingSession *dls_create(uint32_t tag, DataLoggingItemType item_type, uint16_t item_size,
                               bool buffered, bool resume, const Uuid *uuid) {
  s_dls_im_minutenteil += s_im_minutenteil;
  s_dls_angelegt++;
  return s_dls_scheitert ? NULL : (DataLoggingSession *)1;
}
DataLoggingResult dls_log(DataLoggingSession *logging_session, const void *data,
                          uint32_t num_items) {
  cl_assert(s_dls_anzahl < (int)ARRAY_LENGTH(s_dls));
  s_dls[s_dls_anzahl++] = *(const AlgMinuteDLSRecord *)data;
  return DATA_LOGGING_SUCCESS;
}
void dls_send_all_sessions(void) {
}

// HRM fuer Kraepelins Walk/Run-Abo (CONFIG_HRM)
static HRMSessionRef s_hrm_naechste = 1;
static HRMActivityScene s_szene = HRMActivityScene_Default;
static int s_szene_aufrufe;
HRMSessionRef hrm_manager_subscribe_with_callback(AppInstallId app_id, uint32_t update_interval_s,
                                                  uint16_t expire_s, HRMFeature features,
                                                  bool low_latency, HRMSubscriberCallback callback,
                                                  void *context) {
  return s_hrm_naechste++;
}
bool sys_hrm_manager_unsubscribe(HRMSessionRef session) {
  return true;
}
bool sys_hrm_manager_set_update_interval(HRMSessionRef session, uint32_t update_interval_s,
                                         uint16_t expire_s) {
  return true;
}
void hrm_manager_set_activity_scene(HRMActivityScene scene) {
  s_szene = scene;
  s_szene_aufrufe++;
}
bool activity_prefs_hrm_activity_tracking_is_enabled(void) {
  return true;
}

// =============================================================================================
// Helfer
static AccelSamplingRate s_rate;
static time_t s_t0;

// Eine Minute mit Vorgabewerten; die KernelBG-Schlange laeuft danach (wie auf der Uhr),
// ausser `ohne_schlange`.
static void prv_minute_ohne_schlange(uint8_t schritte, uint16_t vmc, uint8_t lage,
                                     bool ohne_schlange) {
  s_vorgabe_an = true;
  s_vorgabe_schritte = schritte;
  s_vorgabe_vmc = vmc;
  s_vorgabe_lage = lage;
  AccelRawData leer[KALG_SAMPLE_HZ] = {};
  activity_algorithm_handle_accel(leer, ARRAY_LENGTH(leer), 0);
  fake_rtc_increment_time(PBL_SEC_PER_MIN);
  AlgMinuteRecord satz = {};
  s_im_minutenteil = true;
  activity_algorithm_minute_handler(rtc_get_time(), &satz);
  s_im_minutenteil = false;
  if (!ohne_schlange) {
    fake_system_task_callbacks_invoke_pending();
  }
}

static void prv_minute(uint8_t schritte, uint16_t vmc, uint8_t lage) {
  prv_minute_ohne_schlange(schritte, vmc, lage, false);
}

static void prv_minuten(int anzahl, uint8_t schritte) {
  for (int i = 0; i < anzahl; i++) {
    prv_minute(schritte, 3000, 0x52);
  }
}

static uint32_t prv_schritte(void) {
  uint32_t schritte;
  cl_assert(activity_algorithm_get_steps(&schritte));
  return schritte;
}

static const ActivitySession *prv_sitzung(ActivitySessionType typ, time_t start) {
  for (int i = 0; i < s_sitzungen_anzahl; i++) {
    if (s_sitzungen[i].type == typ && s_sitzungen[i].start_utc == start) {
      return &s_sitzungen[i];
    }
  }
  return NULL;
}

static int prv_sitzungen_vom_typ(ActivitySessionType typ) {
  int n = 0;
  for (int i = 0; i < s_sitzungen_anzahl; i++) {
    n += (s_sitzungen[i].type == typ);
  }
  return n;
}

// Die Minute `utc` aus den geloggten DLS-Saetzen.
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

// =============================================================================================
void test_boulder_glue__initialize(void) {
  struct tm start = {.tm_hour = 9, .tm_mday = 1, .tm_mon = 0, .tm_year = 115};
  s_t0 = mktime(&start);
  fake_rtc_init(100, s_t0);
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  pfs_format(false);
  fake_dauerteil_zuruecksetzen();
  s_vorgabe_an = false;
  s_geladen = false;
  s_tiefe = s_verstoesse = s_aktivitaet_genommen = 0;
  s_heap_im_minutenteil = s_datei_im_minutenteil = 0;
  s_verdichtungen = s_verdichtungen_im_minutenteil = s_gruppen_in_datei = 0;
  s_sitzungen_anzahl = 0;
  s_dls_scheitert = false;
  s_dls_angelegt = s_dls_im_minutenteil = s_dls_anzahl = 0;
  s_spo2_wartend = 0;
  s_puls = 60;
  boulder_szene_kraepelin(HRMActivityScene_Default);
  boulder_szene_aktiv(false);
  s_szene_aufrufe = 0;
  cl_assert(activity_algorithm_init(&s_rate));
}

void test_boulder_glue__cleanup(void) {
  fake_system_task_callbacks_invoke_pending();
  activity_algorithm_deinit();
  fake_system_task_callbacks_invoke_pending();
}

// ---------------------------------------------------------------------------------------------
// D1: Kraepelins Schlaf kommt nicht in Pebbles Liste; sein innerer Detektor nullt weiter die
// Schritte im Schlaf. Der Gehfilter unterdrueckt Gehen unter 40 Schritten/min im Mittel und
// unter einer Fahrtkandidatin, entschieden beim ersten Melden.
void test_boulder_glue__schlaf_verworfen_gehfilter(void) {
  static FixMinute nacht[1000];
  const int n = fixture_minuten_lesen(CLAR_FIXTURE_PATH "activity/sleep_samples/pbl-28371.c",
                                      nacht, 0, ARRAY_LENGTH(nacht));
  cl_assert(n > 800);
  uint32_t summe = 0;
  for (int i = 0; i < n; i++) {
    prv_minute(nacht[i].schritte, nacht[i].vmc, nacht[i].lage);
    summe += nacht[i].schritte;
  }
  cl_assert_equal_i(prv_sitzungen_vom_typ(ActivitySessionType_Sleep), 0);
  cl_assert_equal_i(prv_sitzungen_vom_typ(ActivitySessionType_RestfulSleep), 0);
  // Der innere Detektor arbeitet: Schritte im erkannten Schlaf sind abgezogen.
  printf("\nNacht: %d min, %" PRIu32 " Schritte, gezaehlt %" PRIu32 "\n", n, summe,
         prv_schritte());
  cl_assert(prv_schritte() < summe);
  // D6: der Schlaf-Stand kommt vom Dauerteil.
  cl_assert_equal_i(activity_algorithm_get_last_sleep_utc(), 4242);

  // Ein Spaziergang: gemeldet.
  prv_minuten(10, 0);
  time_t start = rtc_get_time();
  prv_minuten(20, 100);
  prv_minuten(10, 0);
  cl_assert(prv_sitzung(ActivitySessionType_Walk, start) != NULL);
  cl_assert_equal_i(prv_sitzung(ActivitySessionType_Walk, start)->step_data.steps, 2000);

  // Pedal-Schritte: 60 und 0 im Wechsel, im Mittel 30/min. Kraepelin meldet Walk, der
  // Gehfilter nicht - auch nicht, als danach 20 Minuten echtes Gehen folgen (eingefroren).
  start = rtc_get_time();
  for (int i = 0; i < 12; i++) {
    prv_minute(60, 3000, 0x52);
    prv_minute(0, 3000, 0x52);
  }
  prv_minuten(20, 120);
  prv_minuten(10, 0);
  cl_assert(prv_sitzung(ActivitySessionType_Walk, start) == NULL);

  // Unter einer Fahrtkandidatin: kein Spaziergang.
  fake_dauerteil.ueberdeckt = true;
  start = rtc_get_time();
  prv_minuten(20, 100);
  prv_minuten(10, 0);
  cl_assert(prv_sitzung(ActivitySessionType_Walk, start) == NULL);
}

// ---------------------------------------------------------------------------------------------
// T7.5: nie die Activity-Sperre unter der Kraepelin-Sperre - im Minutenlauf und beim
// Einstecken (early_deinit von KernelMain unter der Activity-Sperre).
void test_boulder_glue__sperr_reihenfolge(void) {
  s_spo2_wartend = 95;
  prv_minuten(5, 0);
  const time_t start = rtc_get_time();
  prv_minuten(15, 100);
  cl_assert(prv_sitzung(ActivitySessionType_Walk, start) != NULL);
  cl_assert(prv_sitzung(ActivitySessionType_Walk, start)->ongoing);

  pbl_mutex_lock(&s_aktivitaet, PBL_FOREVER);
  activity_algorithm_early_deinit();
  pbl_mutex_unlock(&s_aktivitaet);

  // early_deinit beendet den Spaziergang; gemeldet nach dem Entsperren.
  cl_assert(!prv_sitzung(ActivitySessionType_Walk, start)->ongoing);
  cl_assert(s_aktivitaet_genommen > 40);
  cl_assert_equal_i(s_verstoesse, 0);
  cl_assert_equal_i(s_tiefe, 0);
}

// ---------------------------------------------------------------------------------------------
// T7.1/T7.2: der Minutenteil holt keinen Heap, oeffnet keine Datei, legt keine DLS-Sitzung
// an. Geschrieben wird trotzdem - in den Callbacks. D2: der Satz geht an den Dauerteil.
void test_boulder_glue__minutenteil_ohne_heap_ohne_datei(void) {
  s_spo2_wartend = 96;
  s_puls = 72;
  prv_minute(17, 1234, 0x33);
  prv_minuten(59, 50);

  cl_assert_equal_i(s_heap_im_minutenteil, 0);
  cl_assert_equal_i(s_datei_im_minutenteil, 0);
  cl_assert_equal_i(s_dls_im_minutenteil, 0);
  cl_assert_equal_i(s_dls_angelegt, 1);
  cl_assert_equal_i(s_dls_anzahl, 4);
  cl_assert_equal_i(s_gruppen_in_datei, 4);

  cl_assert_equal_i(fake_dauerteil.minuten_anzahl, 60);
  const BoulderMinute *m = &fake_dauerteil.minuten[0];
  cl_assert_equal_i(m->utc, s_t0);
  cl_assert_equal_i(m->vmc, 1234);
  cl_assert_equal_i(m->lage, 0x33);
  cl_assert_equal_i(m->schritte, 17);
  cl_assert_equal_i(m->puls, 72);
  cl_assert_equal_i(m->spo2, 96);
  cl_assert_equal_i(fake_dauerteil.minuten[1].spo2, 0);
  cl_assert_equal_i(prv_dls_minute(s_t0)->spo2_percent, 96);
}

// ---------------------------------------------------------------------------------------------
// T7.1b: volle Minutendatei - die Verdichtung laeuft nur im Callback "Minutendatei".
// test_fill_minute_file verdichtet selbst einmal (auf die Haelfte); danach fuellt der Test
// die Datei bis zum Rand nach, damit die naechste Gruppe die Verdichtung ausloest.
static void prv_minutendatei_randvoll(void) {
  SettingsFile datei;
  cl_assert_equal_i(settings_file_open(&datei, "activity_sleep", ALG_MINUTE_DATA_FILE_LEN),
                    S_SUCCESS);
  AlgMinuteFileRecord satz = {.hdr = {.version = ALG_MINUTE_FILE_RECORD_VERSION,
                                      .sample_size = sizeof(AlgMinuteFileSample),
                                      .num_samples = ALG_MINUTES_PER_FILE_RECORD}};
  const uint32_t sekunden = ALG_MINUTES_PER_FILE_RECORD * PBL_SEC_PER_MIN;
  uint32_t schluessel = rtc_get_time() / sekunden - 2 * ALG_MINUTE_FILE_MAX_ENTRIES;
  while (true) {
    satz.hdr.time_utc = schluessel * sekunden;
    if (settings_file_set(&datei, &schluessel, sizeof(schluessel), &satz, sizeof(satz)) !=
        S_SUCCESS) {
      break;
    }
    schluessel++;
  }
  settings_file_close(&datei);
}

void test_boulder_glue__verdichtung_nur_im_callback(void) {
  cl_assert(activity_algorithm_test_fill_minute_file());
  prv_minutendatei_randvoll();
  s_verdichtungen = 0;
  prv_minuten(45, 30);
  cl_assert(s_verdichtungen > 0);
  cl_assert_equal_i(s_verdichtungen_im_minutenteil, 0);
  cl_assert_equal_i(s_heap_im_minutenteil, 0);
}

// ---------------------------------------------------------------------------------------------
// T7.6: deinit zwischen Einplanen und Ausfuehren; D13 schreibt die nur wegen einer offenen
// Fahrt zurueckgehaltenen Gruppen, ungenullt (offen = "Nein").
void test_boulder_glue__deinit_vor_callback_und_fahrtminuten(void) {
  for (int i = 0; i < 15; i++) {
    prv_minute_ohne_schlange(20, 3000, 0x52, true);
  }
  cl_assert(fake_system_task_count_callbacks() > 0);
  activity_algorithm_deinit();
  cl_assert_equal_i(s_gruppen_in_datei, 1);  // D13: die anstehende Gruppe
  fake_system_task_callbacks_invoke_pending();  // laufen ohne Tracking-Teil ins Leere
  cl_assert_equal_i(s_gruppen_in_datei, 1);
  cl_assert_equal_i(s_dls_anzahl, 0);

  cl_assert(activity_algorithm_init(&s_rate));
  prv_minuten(15, 20);
  cl_assert_equal_i(s_dls_anzahl, 1);
  cl_assert_equal_i(s_gruppen_in_datei, 2);

  const time_t offen = rtc_get_time();
  fake_dauerteil.fahrt_offen_seit = offen;
  prv_minuten(40, 70);
  cl_assert_equal_i(s_dls_anzahl, 1);
  cl_assert_equal_i(s_gruppen_in_datei, 2);

  activity_algorithm_deinit();
  cl_assert_equal_i(fake_dauerteil.tracking_aus, 2);
  cl_assert_equal_i(s_dls_anzahl, 3);
  cl_assert_equal_i(s_gruppen_in_datei, 4);
  cl_assert_equal_i(prv_dls_minute(offen)->base.steps, 70);
  cl_assert(activity_algorithm_init(&s_rate));
}

// ---------------------------------------------------------------------------------------------
// T7.7: init gibt dem Dauerteil die Minuten des Fensters aus dem ohnehin noetigen
// Dateidurchlauf - dieselben, die ein Ganzlesen der Historie liefert.
void test_boulder_glue__ring_wie_ganzlesen(void) {
  for (int i = 0; i < 180; i++) {
    s_puls = 50 + (i % 40);
    prv_minute(i % 50, 1000 + i, i % 200);
  }
  activity_algorithm_deinit();

  fake_dauerteil.ring_an = true;
  fake_dauerteil.ring_von = s_t0 + 20 * PBL_SEC_PER_MIN;
  fake_dauerteil.ring_bis = s_t0 + 170 * PBL_SEC_PER_MIN;
  cl_assert(activity_algorithm_init(&s_rate));
  cl_assert_equal_i(fake_dauerteil.ring_anzahl, 150);

  HealthMinuteData ganz[150];
  uint32_t anzahl = ARRAY_LENGTH(ganz);
  time_t von = fake_dauerteil.ring_von;
  cl_assert(activity_algorithm_get_minute_history(ganz, &anzahl, &von));
  cl_assert_equal_i(anzahl, 150);
  cl_assert_equal_i(von, fake_dauerteil.ring_von);

  bool gesehen[150] = {};
  for (int k = 0; k < fake_dauerteil.ring_anzahl; k++) {
    const BoulderMinute *m = &fake_dauerteil.ring[k];
    const int i = (m->utc - von) / PBL_SEC_PER_MIN;
    cl_assert(i >= 0 && i < 150 && !gesehen[i]);
    gesehen[i] = true;
    cl_assert_equal_i(m->schritte, ganz[i].steps);
    cl_assert_equal_i(m->vmc, ganz[i].vmc);
    cl_assert_equal_i(m->lage, ganz[i].orientation);
    cl_assert_equal_i(m->puls, ganz[i].heart_rate_bpm);
  }
  // Nach init fuellt keiner mehr: ein spaeteres Zaehlen (Shell) gibt nichts weiter.
  uint32_t saetze, bytes, minuten;
  cl_assert(activity_algorithm_minute_file_info(false, &saetze, &bytes, &minuten));
  cl_assert_equal_i(fake_dauerteil.ring_anzahl, 150);
}

// ---------------------------------------------------------------------------------------------
// T6.6: Historie mitten in der Minute lesen setzt die Minuten-VMC nicht zurueck und
// verbraucht keine SpO2-Probe (echter Kern, echte Beschleunigung).
static uint16_t prv_minute_mit_lesen(bool lesen, uint8_t *spo2) {
  rtc_set_time(s_t0 + 10 * PBL_SEC_PER_MIN);
  for (int s = 0; s < 60; s++) {
    AccelRawData proben[KALG_SAMPLE_HZ];
    for (int k = 0; k < KALG_SAMPLE_HZ; k++) {
      const double t = s + (double)k / KALG_SAMPLE_HZ;
      proben[k] = (AccelRawData){.x = 400 * sin(2 * 3.14159265358979 * 1.8 * t),
                                 .y = 250 * cos(2 * 3.14159265358979 * 1.8 * t),
                                 .z = -1000 + 150 * sin(2 * 3.14159265358979 * 0.9 * t)};
    }
    activity_algorithm_handle_accel(proben, KALG_SAMPLE_HZ, 0);
    fake_rtc_increment_time(1);
    if (s == 30) {
      s_spo2_wartend = 97;
      if (lesen) {
        HealthMinuteData daten[2];
        uint32_t anzahl = 2;
        time_t von = rtc_get_time() - PBL_SEC_PER_MIN;
        cl_assert(activity_algorithm_get_minute_history(daten, &anzahl, &von));
      }
    }
  }
  AlgMinuteRecord satz = {};
  activity_algorithm_minute_handler(rtc_get_time(), &satz);
  fake_system_task_callbacks_invoke_pending();
  *spo2 = satz.data.spo2_percent;
  return satz.data.base.vmc;
}

void test_boulder_glue__historie_teilminute(void) {
  uint8_t spo2_ohne, spo2_mit;
  const uint16_t vmc_ohne = prv_minute_mit_lesen(false, &spo2_ohne);
  activity_algorithm_deinit();
  cl_assert(activity_algorithm_init(&s_rate));
  const uint16_t vmc_mit = prv_minute_mit_lesen(true, &spo2_mit);
  printf("\nVMC ohne Lesen %d, mit Lesen %d\n", vmc_ohne, vmc_mit);
  cl_assert(vmc_ohne > 0);
  cl_assert_equal_i(vmc_mit, vmc_ohne);
  cl_assert_equal_i(spo2_ohne, 97);
  cl_assert_equal_i(spo2_mit, 97);
}

// ---------------------------------------------------------------------------------------------
// D3-D5: offene Fahrt haelt zurueck, "Ja" zieht die Fahrtschritte ab, waehrend der Fahrt
// zaehlt nichts, die Fahrtminuten gehen genullt hinaus.
void test_boulder_glue__fahrt_streichen(void) {
  prv_minuten(20, 100);
  cl_assert_equal_i(prv_schritte(), 2000);
  cl_assert_equal_i(s_dls_anzahl, 1);

  const time_t von = rtc_get_time();
  fake_dauerteil.fahrt_offen_seit = von;
  prv_minuten(12, 80);
  cl_assert_equal_i(prv_schritte(), 2960);
  cl_assert_equal_i(s_dls_anzahl, 1);  // zurueckgehalten

  // "Ja": erst streichen, dann ist die Fahrt bestaetigt und nicht mehr offen.
  boulder_kraepelin_fahrt_streichen(von, INT32_MAX);
  fake_dauerteil.fahrt_von = von;
  fake_dauerteil.fahrt_bis = INT32_MAX;
  fake_dauerteil.fahrt_laeuft = true;
  fake_dauerteil.fahrt_offen_seit = 0;
  cl_assert_equal_i(prv_schritte(), 2000);
  cl_assert_equal_i(boulder_kraepelin_gestrichen(), 960);

  prv_minuten(5, 80);
  cl_assert_equal_i(prv_schritte(), 2000);
  cl_assert_equal_i(boulder_kraepelin_gestrichen(), 1360);
  uint16_t rate;
  uint32_t ms;
  time_t ende;
  cl_assert(activity_algorithm_get_step_rate(&rate, &ms, &ende));
  cl_assert_equal_i(rate, 0);

  const time_t bis = rtc_get_time();
  fake_dauerteil.fahrt_laeuft = false;
  fake_dauerteil.fahrt_bis = bis;
  prv_minuten(15, 10);
  cl_assert_equal_i(prv_schritte(), 2150);
  cl_assert_equal_i(s_dls_anzahl, 3);
  cl_assert_equal_i(prv_dls_minute(von - PBL_SEC_PER_MIN)->base.steps, 100);
  for (time_t t = von; t < bis; t += PBL_SEC_PER_MIN) {
    const AlgMinuteDLSSample *m = prv_dls_minute(t);
    cl_assert(m);
    cl_assert_equal_i(m->base.steps, 0);
    cl_assert_equal_i(m->base.active, 0);
    cl_assert_equal_i(m->distance_cm, 0);
    cl_assert_equal_i(m->active_calories, 0);
  }
  cl_assert_equal_i(prv_dls_minute(bis)->base.steps, 10);
}

// Ohne DLS-Sitzung (Pool voll) wird trotzdem abgezogen; die Anlage wartet 30 min.
void test_boulder_glue__fahrt_streichen_ohne_dls(void) {
  s_dls_scheitert = true;
  prv_minuten(20, 100);
  cl_assert_equal_i(s_dls_angelegt, 1);
  const time_t von = rtc_get_time();
  fake_dauerteil.fahrt_offen_seit = von;
  prv_minuten(5, 80);
  boulder_kraepelin_fahrt_streichen(von, INT32_MAX);
  cl_assert_equal_i(prv_schritte(), 2000);
  prv_minuten(19, 0);
  cl_assert_equal_i(s_dls_angelegt, 1);
  prv_minuten(10, 0);
  cl_assert_equal_i(s_dls_angelegt, 2);
  cl_assert_equal_i(s_dls_anzahl, 0);
}

// ---------------------------------------------------------------------------------------------
// T2.4: eine lange Wanderung meldet 65 535 Schritte statt eines Ueberlaufrests.
void test_boulder_glue__sitzungsschritte_32bit(void) {
  const time_t start = rtc_get_time();
  prv_minuten(600, 120);
  const ActivitySession *s = prv_sitzung(ActivitySessionType_Walk, start);
  cl_assert(s);
  cl_assert_equal_i(s->step_data.steps, 65535);
  prv_minuten(10, 0);
  cl_assert_equal_i(prv_sitzung(ActivitySessionType_Walk, start)->step_data.steps, 65535);
  // Kraepelins Abo setzt die Szene ueber die Variante (D10).
  cl_assert(s_szene_aufrufe >= 2);
  cl_assert_equal_i(s_szene, HRMActivityScene_Default);
}

// ---------------------------------------------------------------------------------------------
// D10: das Abo "Aktiv" zaehlt fuer activity.c mit; die Szene setzt eine Stelle, Kraepelin
// geht vor, zurueckgesetzt wird nur die eigene.
void test_boulder_glue__aktiv_abo_und_szene(void) {
  cl_assert(!activity_algorithm_activity_hrm_is_active());
  fake_dauerteil.aktiv_abo = true;
  cl_assert(activity_algorithm_activity_hrm_is_active());
  activity_algorithm_activity_hrm_set_paused(true);
  cl_assert_equal_i(fake_dauerteil.pausiert_aufrufe, 1);
  cl_assert(fake_dauerteil.pausiert);

  boulder_szene_aktiv(true);
  cl_assert_equal_i(s_szene, HRMActivityScene_HighIntensity);
  boulder_szene_kraepelin(HRMActivityScene_Walk);
  cl_assert_equal_i(s_szene, HRMActivityScene_Walk);
  boulder_szene_kraepelin(HRMActivityScene_Default);
  cl_assert_equal_i(s_szene, HRMActivityScene_HighIntensity);
  boulder_szene_aktiv(false);
  cl_assert_equal_i(s_szene, HRMActivityScene_Default);
  const int aufrufe = s_szene_aufrufe;
  // Ein Training setzt seine Szene; die Variante laesst sie stehen.
  hrm_manager_set_activity_scene(HRMActivityScene_Run);
  boulder_szene_kraepelin(HRMActivityScene_Default);
  boulder_szene_aktiv(false);
  cl_assert_equal_i(s_szene_aufrufe, aufrufe + 1);
  cl_assert_equal_i(s_szene, HRMActivityScene_Run);
}

// ---------------------------------------------------------------------------------------------
// T5.6, die b25-Faelle des Gehfilters (main 217428e90, test_boulder_gehfilter.c:459-497).
// DIE VELOFAHRT VOM 1.10.2026: 22 min, nachgebaut aus ihren Zahlen - alle sechs Minuten gut 40
// Stoesse am Lenker, die als Schritte zaehlen, dazwischen fast nichts; im Mittel neun je
// Minute. Kraepelin meldet daraus Gehen, der Filter unterdrueckt es schon beim ersten Melden.
// EIN SPAZIERGANG MIT AMPEL: 25 min um 95 Schritte, drei Minuten Stehen - er bleibt ganz.
void test_boulder_glue__gehfilter_b25_faelle(void) {
  static const uint8_t fahrt[22] = {
    42, 3, 0, 5, 0, 2, 44, 0, 4, 0, 1, 0, 41, 2, 0, 3, 0, 0, 45, 1, 0, 2,
  };
  prv_minuten(15, 0);
  const time_t velo = rtc_get_time();
  for (size_t i = 0; i < ARRAY_LENGTH(fahrt); i++) {
    prv_minute(fahrt[i], 3000, 0x52);
  }
  prv_minuten(15, 0);
  cl_assert_equal_i(prv_sitzungen_vom_typ(ActivitySessionType_Walk), 0);
  cl_assert_equal_i(prv_sitzungen_vom_typ(ActivitySessionType_Run), 0);
  // Dass Kraepelin die Fahrt gemeldet hat, zeigt das Log ("Gehfilter unterdrueckt").
  (void)velo;

  const time_t ampel = rtc_get_time();
  uint32_t summe = 0;
  for (int i = 0; i < 25; i++) {
    const uint8_t schritte = (i >= 11 && i < 14) ? 0 : 95;
    prv_minute(schritte, 3000, 0x52);
    summe += schritte;
  }
  prv_minuten(15, 0);
  const ActivitySession *s = prv_sitzung(ActivitySessionType_Walk, ampel);
  cl_assert(s);
  cl_assert_equal_i(s->length_min, 25);
  cl_assert_equal_i(s->step_data.steps, summe);
}
