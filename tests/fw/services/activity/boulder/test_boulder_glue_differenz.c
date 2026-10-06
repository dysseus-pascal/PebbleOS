/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2, T2.2b: Glue-Differenztest (Spezifikation 3.1, Nachweis fuer Kriterium 2).
//
// Dieselbe Datei wird zweimal gebaut: mit Pebbles Glue und Kern (Original) und mit der
// Kopie (BOULDER_KOPIE, steuerbarer Dauerteil ohne Fahrt). Beide bekommen denselben
// kuenstlichen Tag: die Beschleunigung aller step_samples (echte Schrittzaehlung), dann
// die Minuten der activity_samples und zweier Naechte aus sleep_samples (Minutenwerte
// statt Messung, Schlaf- und Gehdetektor rechnen echt), dazu Health-Sync zu festen Zeiten.
// Jede Zeile mit "#B " geht in den Vergleich (glue_differenz.py): DLS 81 Byte fuer Byte,
// die Minutendatei am Ende, die Schrittsumme je Minute, die Sitzungen ausser Schlaf.
//
// Zwei Laeufe: ohne und mit Lesen der Historie mitten in jeder siebten Minute (D7, V2).

#include "applib/accel_service.h"
#include "applib/data_logging.h"
#include <pbl/drivers/ambient_light.h>
#include <pbl/drivers/rtc.h>
#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/activity_algorithm.h"
#include "pbl/services/activity/activity_private.h"
#ifdef BOULDER_KOPIE
#include "pbl/services/activity/boulder/kraepelin/activity_algorithm_kraepelin.h"
#include "pbl/services/activity/boulder/kraepelin/kraepelin_algorithm.h"
#include "fake_dauerteil.h"
#define WRAP_ANALYZE __wrap_boulder_kalg_analyze_samples
#define REAL_ANALYZE __real_boulder_kalg_analyze_samples
#define WRAP_STATS __wrap_boulder_kalg_minute_stats
#define REAL_STATS __real_boulder_kalg_minute_stats
#else
#include "pbl/services/activity/kraepelin/activity_algorithm_kraepelin.h"
#include "pbl/services/activity/kraepelin/kraepelin_algorithm.h"
#define WRAP_ANALYZE __wrap_kalg_analyze_samples
#define REAL_ANALYZE __real_kalg_analyze_samples
#define WRAP_STATS __wrap_kalg_minute_stats
#define REAL_STATS __real_kalg_minute_stats
#endif
#include "pbl/services/battery/battery_state.h"
#include "pbl/services/data_logging/data_logging_service.h"
#include "pbl/services/filesystem/pfs.h"
#include "pbl/services/settings/settings_file.h"
#include "pbl/services/system_task.h"
#include "pbl/services/time.h"
#include "pbl/util/math.h"
#include "pbl/util/size.h"
#include "pbl/util/units.h"

#include <dirent.h>
extern char *strdup(const char *s);
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "clar.h"

#include "stubs_analytics.h"
#include "stubs_hexdump.h"
#include "stubs_hr_util.h"
#include "stubs_irq.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_sleep.h"
#include "stubs_task_wdt.h"

#include "fake_accel_service.h"
#include "fake_new_timer.h"
#include "fake_rtc.h"
#include "fake_spi_flash.h"
#include "fake_system_task.h"

#include "fixture_lesen.h"

// =============================================================================================
// Ausgabe
static void prv_hex(const char *art, const void *daten, size_t laenge) {
  printf("#B %s ", art);
  for (size_t i = 0; i < laenge; i++) {
    printf("%02x", ((const uint8_t *)daten)[i]);
  }
  printf("\n");
}

// =============================================================================================
// Kern: Minutenwerte als Vorgabe (nur in den Minuten-Abschnitten)
static bool s_vorgabe_an;
static uint8_t s_vorgabe_schritte;
static uint16_t s_vorgabe_vmc;
static uint8_t s_vorgabe_lage;

uint32_t REAL_ANALYZE(KAlgState *state, AccelRawData *data, uint32_t num_samples,
                      uint32_t *consumed_samples);
uint32_t WRAP_ANALYZE(KAlgState *state, AccelRawData *data, uint32_t num_samples,
                      uint32_t *consumed_samples) {
  if (!s_vorgabe_an) {
    return REAL_ANALYZE(state, data, num_samples, consumed_samples);
  }
  *consumed_samples = num_samples;
  const uint32_t schritte = s_vorgabe_schritte;
  s_vorgabe_schritte = 0;
  return schritte;
}
void REAL_STATS(KAlgState *state, uint16_t *vmc, uint8_t *orientation, bool *still);
void WRAP_STATS(KAlgState *state, uint16_t *vmc, uint8_t *orientation, bool *still) {
  if (!s_vorgabe_an) {
    REAL_STATS(state, vmc, orientation, still);
    return;
  }
  *vmc = s_vorgabe_vmc;
  *orientation = s_vorgabe_lage;
  *still = false;
}

// =============================================================================================
// Pebble rund um die Glue (beide Bauten gleich)
static uint8_t s_licht = 4;
static bool s_geladen;
uint32_t light_get_ambient_lux(void) {
  return s_licht * ALG_RAW_LIGHT_SENSOR_DIVIDE_BY;
}
AmbientLightLevel ambient_light_level_to_enum(uint32_t light_level) {
  return AmbientLightLevelLight;
}
BatteryChargeState battery_get_charge_state(void) {
  return (BatteryChargeState){.is_plugged = s_geladen};
}
bool activity_tracking_on(void) {
  return true;
}
bool activity_sessions_prv_is_sleep_activity(ActivitySessionType activity_type) {
  return activity_type == ActivitySessionType_Sleep ||
         activity_type == ActivitySessionType_RestfulSleep ||
         activity_type == ActivitySessionType_Nap ||
         activity_type == ActivitySessionType_RestfulNap;
}

static int s_schlafmeldungen;
static void prv_sitzung(const char *art, const ActivitySession *s) {
  if (activity_sessions_prv_is_sleep_activity(s->type)) {
    s_schlafmeldungen++;
    return;
  }
  printf("#B SITZUNG %s typ=%d start=%ld len=%u laufend=%d schritte=%u akcal=%u rkcal=%u m=%u\n",
         art, (int)s->type, (long)s->start_utc, (unsigned)s->length_min, (int)s->ongoing,
         (unsigned)s->step_data.steps, (unsigned)s->step_data.active_kcalories,
         (unsigned)s->step_data.resting_kcalories, (unsigned)s->step_data.distance_meters);
}
void activity_sessions_prv_add_activity_session(ActivitySession *session) {
  prv_sitzung("neu", session);
}
void activity_sessions_prv_delete_activity_session(ActivitySession *session) {
  prv_sitzung("weg", session);
}

// Metriken: einfache, feste Verlaeufe; die SpO2-Probe wird wie in activity_metrics.c
// beim Lesen verbraucht.
static uint32_t s_distanz_mm, s_ruhe_cal, s_aktiv_cal;
static int32_t s_puls;
static uint8_t s_spo2_wartend;
void activity_metrics_prv_get_spo2_sample(uint8_t *percent_out, uint8_t *quality_out) {
  *percent_out = s_spo2_wartend;
  *quality_out = s_spo2_wartend ? 3 : 0;
  s_spo2_wartend = 0;
}
void activity_metrics_prv_reset_hr_stats(void) {
}
bool activity_metrics_prv_is_hrm_offwrist(time_t now_utc) {
  return false;
}
void activity_metrics_prv_set_hrm_worn_status(time_t now_utc, bool is_offwrist) {
}
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
  return s_distanz_mm;
}
uint32_t activity_metrics_prv_get_resting_calories(void) {
  return s_ruhe_cal;
}
uint32_t activity_metrics_prv_get_active_calories(void) {
  return s_aktiv_cal;
}

DataLoggingSession *dls_create(uint32_t tag, DataLoggingItemType item_type, uint16_t item_size,
                               bool buffered, bool resume, const Uuid *uuid) {
  return (DataLoggingSession *)1;
}
DataLoggingResult dls_log(DataLoggingSession *logging_session, const void *data,
                          uint32_t num_items) {
  prv_hex("DLS", data, num_items * sizeof(AlgMinuteDLSRecord));
  return DATA_LOGGING_SUCCESS;
}
void dls_send_all_sessions(void) {
}

// =============================================================================================
// Ablauf
static AccelSamplingRate s_rate;
static bool s_historie_lesen;
static int s_minute;

// Bei jeder Minutenwende: Werte der Minute setzen (nur von der Minutennummer abhaengig).
static void prv_metriken_der_minute(void) {
  s_puls = (s_minute % 9 == 0) ? 0 : 50 + (s_minute % 50);
  if (s_minute % 10 == 3) {
    s_spo2_wartend = 90 + (s_minute % 10);
  }
  s_distanz_mm += 700 * (s_minute % 4);
  s_ruhe_cal += 1100;
  s_aktiv_cal += 300 * (s_minute % 3);
}

static void prv_historie_lesen(void) {
  if (!s_historie_lesen || (s_minute % 7) != 0) {
    return;
  }
  HealthMinuteData daten[20];
  uint32_t anzahl = ARRAY_LENGTH(daten);
  time_t von = rtc_get_time() - 20 * PBL_SEC_PER_MIN;
  cl_assert(activity_algorithm_get_minute_history(daten, &anzahl, &von));
}

static void prv_minutenwende(void) {
  AlgMinuteRecord satz = {};
  activity_algorithm_minute_handler(rtc_get_time(), &satz);
  fake_system_task_callbacks_invoke_pending();
  uint32_t schritte;
  cl_assert(activity_algorithm_get_steps(&schritte));
  printf("#B SCHRITTE %d %" PRIu32 "\n", s_minute, schritte);
  s_minute++;
  prv_metriken_der_minute();
  if (s_minute % 97 == 0) {
    activity_algorithm_send_minutes();
    fake_system_task_callbacks_invoke_pending();
  }
}

static int prv_namen_vergleichen(const void *a, const void *b) {
  return strcmp(*(char *const *)a, *(char *const *)b);
}

// Der Tag: die Beschleunigung aller step_samples hintereinander, 25 Hz, Sekunde fuer Sekunde.
static void prv_tag_aus_beschleunigung(void) {
  static AccelRawData proben[700000];
  const char *ordner = CLAR_FIXTURE_PATH "activity/step_samples";
  DIR *dir = opendir(ordner);
  cl_assert(dir);
  char *namen[128];
  int n_namen = 0;
  struct dirent *e;
  while ((e = readdir(dir)) && n_namen < (int)ARRAY_LENGTH(namen)) {
    if (strstr(e->d_name, ".c")) {
      namen[n_namen++] = strdup(e->d_name);
    }
  }
  closedir(dir);
  qsort(namen, n_namen, sizeof(namen[0]), prv_namen_vergleichen);
  int anzahl = 0;
  for (int i = 0; i < n_namen; i++) {
    char pfad[512];
    snprintf(pfad, sizeof(pfad), "%s/%s", ordner, namen[i]);
    anzahl = fixture_accel_lesen(pfad, proben, anzahl, ARRAY_LENGTH(proben));
    free(namen[i]);
  }
  printf("#B TAG %d Dateien, %d Proben\n", n_namen, anzahl);
  cl_assert(anzahl > 100000);

  s_vorgabe_an = false;
  for (int i = 0; i + KALG_SAMPLE_HZ <= anzahl; i += KALG_SAMPLE_HZ) {
    activity_algorithm_handle_accel(&proben[i], KALG_SAMPLE_HZ,
                                    (uint64_t)rtc_get_time() * PBL_MSEC_PER_SEC);
    fake_rtc_increment_time(1);
    const int sekunde = rtc_get_time() % PBL_SEC_PER_MIN;
    if (sekunde == 30) {
      prv_historie_lesen();
    } else if (sekunde == 0) {
      prv_minutenwende();
    }
  }
}

// Minuten aus einer Fixture-Datei: Vorgabe fuer Schritte, VMC, Lage, Licht, Ladekabel.
static void prv_minuten_aus(const char *datei) {
  static FixMinute minuten[3000];
  const int n = fixture_minuten_lesen(datei, minuten, 0, ARRAY_LENGTH(minuten));
  printf("#B MINUTEN %d\n", n);
  cl_assert(n > 100);
  s_vorgabe_an = true;
  for (int i = 0; i < n; i++) {
    s_vorgabe_schritte = minuten[i].schritte;
    s_vorgabe_vmc = minuten[i].vmc;
    s_vorgabe_lage = minuten[i].lage;
    s_licht = minuten[i].licht;
    s_geladen = minuten[i].geladen;
    AccelRawData leer[KALG_SAMPLE_HZ] = {};
    activity_algorithm_handle_accel(leer, ARRAY_LENGTH(leer),
                                    (uint64_t)rtc_get_time() * PBL_MSEC_PER_SEC);
    fake_rtc_increment_time(30);
    prv_historie_lesen();
    fake_rtc_increment_time(30);
    prv_minutenwende();
  }
  s_vorgabe_an = false;
  s_geladen = false;
}

static bool prv_datei_satz(SettingsFile *file, SettingsRecordInfo *info, void *context) {
  uint32_t schluessel;
  AlgMinuteFileRecord satz;
  info->get_key(file, &schluessel, sizeof(schluessel));
  cl_assert_equal_i(info->val_len, sizeof(satz));
  info->get_val(file, &satz, sizeof(satz));
  printf("#B DATEI %08" PRIx32 " ", schluessel);
  prv_hex("", &satz, sizeof(satz));
  return true;
}

static void prv_lauf(bool historie_lesen) {
  s_historie_lesen = historie_lesen;
  prv_tag_aus_beschleunigung();
  prv_minuten_aus(CLAR_FIXTURE_PATH "activity/activity_samples/walk_activities_0.c");
  prv_minuten_aus(CLAR_FIXTURE_PATH "activity/activity_samples/run_shut_down.c");
  prv_minuten_aus(CLAR_FIXTURE_PATH "activity/sleep_samples/pbl-28371.c");
  prv_minuten_aus(CLAR_FIXTURE_PATH "activity/sleep_samples/temp-113048.c");
  activity_algorithm_send_minutes();
  fake_system_task_callbacks_invoke_pending();

  SettingsFile datei;
  cl_assert_equal_i(settings_file_open(&datei, "activity_sleep", ALG_MINUTE_DATA_FILE_LEN),
                    S_SUCCESS);
  cl_assert_equal_i(settings_file_each(&datei, prv_datei_satz, NULL), S_SUCCESS);
  settings_file_close(&datei);
  printf("#B ENDE %d Minuten\n", s_minute);
  // Schlaf meldet nur das Original (D1); er geht nicht in den Vergleich.
  printf("Schlafmeldungen: %d\n", s_schlafmeldungen);
}

// =============================================================================================
void test_boulder_glue_differenz__initialize(void) {
  struct tm start = {.tm_hour = 6, .tm_mday = 2, .tm_mon = 2, .tm_year = 116};
  fake_rtc_init(100, mktime(&start));
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  pfs_format(false);
#ifdef BOULDER_KOPIE
  fake_dauerteil_zuruecksetzen();
#endif
  s_minute = 0;
  s_distanz_mm = s_ruhe_cal = s_aktiv_cal = 0;
  s_spo2_wartend = 0;
  s_schlafmeldungen = 0;
  s_licht = 4;
  s_geladen = false;
  s_vorgabe_an = false;
  prv_metriken_der_minute();
  cl_assert(activity_algorithm_init(&s_rate));
}

void test_boulder_glue_differenz__cleanup(void) {
  fake_system_task_callbacks_invoke_pending();
  activity_algorithm_deinit();
  fake_system_task_callbacks_invoke_pending();
}

void test_boulder_glue_differenz__ohne_historie(void) {
  printf("#B LAUF ohne\n");
  prv_lauf(false);
}

void test_boulder_glue_differenz__mit_historie(void) {
  printf("#B LAUF mit\n");
  prv_lauf(true);
}
