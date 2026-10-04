/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder: DER SMART ALARM IN EINER ECHTEN NACHT. Der echte Wecker (alarm.c)
// fragt den Schlafzustand der Uhr, den die echte activity_sessions.c aus den
// Schlafbloecken der echten Nachtrechnung (boulder/nacht.c) bildet - Minute fuer
// Minute, mit der echten Nacht vom 26./27.9.2026.
//
// WAS DER WECKER TUT (alarm.c, prv_should_smart_alarm_trigger): im Fenster der
// letzten 30 Minuten vor der Weckzeit prueft er jede Minute. Wach oder
// unbekannt - er klingelt sofort. Schlafend - er klingelt, sobald die letzte
// Minute Bewegung hatte (VMC > 0), spaetestens zur Weckzeit.
//
// Frueher galt man zwischen zwei Nachtrechnungen meist als wach, und der
// Smart Alarm klingelte gleich zu Beginn seines Fensters.

#include "clar.h"

#include "kernel/events.h"
#include "kernel/kernel_heap.h"
#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/activity_algorithm.h"
#include "pbl/services/activity/activity_private.h"
#include "pbl/services/activity/boulder/nacht.h"
#include "pbl/services/activity/boulder/schlafanalyse.h"
#include "pbl/services/alarms/alarm.h"
#include "pbl/services/alarms/alarm_pin.h"
#include "pbl/services/battery/battery_state.h"
#include "pbl/services/data_logging/data_logging_service.h"
#include "pbl/services/filesystem/pfs.h"
#include "pbl/services/settings/settings_file.h"
#include "pbl/services/system_task.h"
#include "pbl/services/timeline/item.h"
#include "pbl/util/heap.h"
#include <pbl/cron/cron.h>
#include <pbl/drivers/rtc.h>
#include "process_management/pebble_process_md.h"
#include "applib/data_logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stubs_analytics.h"
#include "stubs_app_cache.h"
#include "stubs_app_install_manager.h"
#include "stubs_blob_db.h"
#include "stubs_blob_db_sync.h"
#include "stubs_blob_db_sync_util.h"
#include "stubs_calendar.h"
#include "stubs_hexdump.h"
#include "stubs_i18n.h"
#include "stubs_layout_layer.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pebble_tasks.h"
#include "stubs_prompt.h"
#include "stubs_rand_ptr.h"
#include "stubs_regular_timer.h"
#include "stubs_sleep.h"
#include "stubs_task_watchdog.h"
#include "stubs_timeline_event.h"

#include "fake_new_timer.h"
#include "fake_rtc.h"
#include "fake_spi_flash.h"

#define MINUTEN (16 * 60)

// 11.3.2015, 20:00 UTC - die Uhr rechnet in UTC, das Nachtfenster ist 20 bis 11 Uhr.
static const time_t s_beginn = 1426118400 - 4 * 3600;
static time_t s_jetzt;
static HealthMinuteData s_minuten[MINUTEN];

// --- Der Wecker und seine Umgebung -------------------------------------------------

int rand(void) {
  return 0;
}

bool activity_tracking_on(void) {
  return true;
}

status_t reminder_db_delete_with_parent(const TimelineItemId *id) {
  return S_SUCCESS;
}

const PebbleProcessMd *alarms_app_get_info() {
  static const PebbleProcessMdSystem s_info = { .common = { .uuid = { 0x67, 0xa3 } } };
  return (const PebbleProcessMd *)&s_info;
}

const char *timeline_get_private_data_source(Uuid *parent_id) {
  return NULL;
}

status_t pin_db_insert_item_without_event(TimelineItem *item) {
  return S_SUCCESS;
}

status_t pin_db_delete(const uint8_t *key, int key_len) {
  return S_SUCCESS;
}

bool system_task_add_callback(SystemTaskEventCallback cb, void *data) {
  cb(data);
  return true;
}

// Wann der Wecker klingelte; 0 = nicht.
static time_t s_geklingelt_um;
void event_put(PebbleEvent *event) {
  if (event->type == PEBBLE_ALARM_CLOCK_EVENT && s_geklingelt_um == 0) {
    s_geklingelt_um = rtc_get_time();
  }
}

// Der Schlafzustand und die Bewegung, wie activity_metrics.c sie aus dem
// Zustand des Aktivitaetsdienstes liest.
static ActivityState s_state;
static uint16_t s_letzte_vmc;
bool activity_get_metric(ActivityMetric metric, uint32_t history_len, int32_t *history) {
  cl_assert_equal_i(history_len, 1);
  switch (metric) {
    case ActivityMetricSleepState:
      *history = s_state.sleep_data.cur_state;
      return true;
    case ActivityMetricSleepStateSeconds:
      *history = s_state.sleep_data.cur_state_elapsed_minutes * SECONDS_PER_MINUTE;
      return true;
    case ActivityMetricLastVMC:
      *history = s_letzte_vmc;
      return true;
    default:
      cl_fail("Unerwartete Metrik");
      return false;
  }
}

// --- Der Aktivitaetsdienst um die Nacht herum ------------------------------------

ActivityState *activity_private_state(void) {
  return &s_state;
}
SettingsFile *activity_private_settings_open(void) {
  return NULL;
}
void activity_private_settings_close(SettingsFile *file) {
}
void activity_algorithm_post_process_sleep_sessions(uint16_t n, ActivitySession *sessions) {
}
// Die Schlaferkennung der Uhr hinkt der Minute vier Minuten nach (kraepelin_algorithm.c).
time_t activity_algorithm_get_last_sleep_utc(void) {
  return s_jetzt - 4 * SECONDS_PER_MINUTE;
}
// Die Minutendatei: alle Minuten bis zur eben vergangenen.
bool activity_algorithm_get_minute_history(HealthMinuteData *minute_data, uint32_t *num_records,
                                           time_t *utc_start) {
  memset(minute_data, 0xFF, *num_records * sizeof(HealthMinuteData));
  int32_t anzahl = (s_jetzt - *utc_start) / SECONDS_PER_MINUTE;
  anzahl = (anzahl < 0) ? 0 : anzahl;
  anzahl = ((uint32_t)anzahl > *num_records) ? (int32_t)*num_records : anzahl;
  for (int32_t k = 0; k < anzahl; k++) {
    const int32_t i = (*utc_start - s_beginn) / SECONDS_PER_MINUTE + k;
    if (i >= 0 && i < MINUTEN) {
      minute_data[k] = s_minuten[i];
    }
  }
  *num_records = anzahl;
  return true;
}

int boulder_hrv_fenster(time_t von, time_t bis, SchlafHrv *aus, int max) {
  return 0;
}
uint8_t boulder_messstufe(void) {
  return 3;
}
BatteryChargeState battery_get_charge_state(void) {
  return (BatteryChargeState){ .charge_percent = 70 };
}
void health_sync_schicken(void) {
}
void system_task_watchdog_feed(void) {
}
DataLoggingSession *dls_create(uint32_t tag, DataLoggingItemType item_type, uint16_t item_size,
                               bool buffered, bool resume, const Uuid *uuid) {
  return (DataLoggingSession *)1;
}
DataLoggingResult dls_log(DataLoggingSession *session, const void *data, uint32_t num_items) {
  return DATA_LOGGING_SUCCESS;
}
static uint8_t s_heap_speicher[96 * 1024];
static Heap s_kernel_heap;
Heap *kernel_heap_get(void) {
  return &s_kernel_heap;
}

// --- Die Nacht ------------------------------------------------------------------

static void prv_lade_nacht(void) {
  memset(s_minuten, 0xFF, sizeof(s_minuten));
  char pfad[512];
  snprintf(pfad, sizeof(pfad), "%s/activity/boulder/nacht-2026-09-26.csv", CLAR_FIXTURE_PATH);
  FILE *f = fopen(pfad, "r");
  cl_assert(f);
  char zeile[256];
  int n = 0;
  while (fgets(zeile, sizeof(zeile), f)) {
    if (zeile[0] == '\n' || zeile[0] == '\r') {
      break;
    }
    if (zeile[0] == '#' || !strncmp(zeile, "minute,", 7)) {
      continue;
    }
    char *feld[8] = { 0 };
    int k = 0;
    char *p = zeile;
    feld[k++] = p;
    while (*p && k < 8) {
      if (*p == ',') {
        *p = 0;
        feld[k++] = p + 1;
      }
      p++;
    }
    const int i = 120 + n++;
    if (feld[2][0]) {
      const int b = atoi(feld[2]);
      s_minuten[i] = (HealthMinuteData){
        .vmc = (uint16_t)((b * b + 8) / 16),
        .heart_rate_bpm = feld[4][0] ? atoi(feld[4]) : 0,
      };
    }
  }
  fclose(f);
  cl_assert_equal_i(n, 540);
  for (int i = 660; i < MINUTEN; i++) {
    s_minuten[i] = (HealthMinuteData){ .vmc = 3000, .steps = 60, .heart_rate_bpm = 82 };
  }
}

static bool prv_bewegt(int m) {
  return m >= 0 && s_minuten[m].vmc > 0 && s_minuten[m].vmc != 0xFFFF;
}

void test_boulder_wecker__initialize(void) {
  TimezoneInfo tz = { .tm_zone = "UTC" };
  time_util_update_timezone(&tz);
  fake_rtc_init(100, s_beginn);
  rtc_set_timezone(&tz);
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  pfs_format(false);
  pbl_cron_init();
  alarm_init();
  alarm_service_enable_alarms(true);
  heap_init(&s_kernel_heap, s_heap_speicher, s_heap_speicher + sizeof(s_heap_speicher), false);
  memset(&s_state, 0, sizeof(s_state));
  boulder_nacht_test_reset();
  s_geklingelt_um = 0;
  prv_lade_nacht();
}

void test_boulder_wecker__cleanup(void) {
  pbl_cron_deinit();
}

// Die Nacht Minute fuer Minute bis `bis` (Minuten ab 20:00): erst der
// Aktivitaetsdienst (Schlafzustand, dann die Nacht), dann der Wecker - zur
// vollen Minute (cron) und mit seinem Nachsehen eine Minute spaeter.
static void prv_bis(int bis) {
  for (int m = 0; m <= bis; m++) {
    s_jetzt = s_beginn + m * SECONDS_PER_MINUTE;
    rtc_set_time(s_jetzt);
    s_letzte_vmc = prv_bewegt(m - 1) ? s_minuten[m - 1].vmc : 0;
    activity_sessions_prv_minute_handler(s_jetzt);
    boulder_nacht_minute(s_jetzt);
    if (stub_new_timer_get_next() != TIMER_INVALID_ID &&
        stub_new_timer_timeout(stub_new_timer_get_next()) <= SECONDS_PER_MINUTE * 1000) {
      stub_new_timer_invoke(1);
    }
    pbl_cron_wakeup();
  }
}

// Ein Smart Alarm, dessen Fenster in eine ruhige Strecke faellt: die erste
// Viertelstunde ab 03:00, in der sich zehn Minuten lang nichts regt. Er darf
// nicht klingeln, bevor sich etwas regt.
void test_boulder_wecker__smart_alarm_wartet_im_schlaf_auf_bewegung(void) {
  int fenster = -1;
  for (int w = 7 * 60; w < 10 * 60 && fenster < 0; w++) {
    bool still = true;
    for (int m = w - 1; m < w + 10; m++) {
      still = still && !prv_bewegt(m);
    }
    fenster = still ? w : -1;
  }
  cl_assert(fenster > 0);
  int erwartet = fenster + 30;
  for (int m = fenster; m < fenster + 30; m++) {
    if (prv_bewegt(m - 1)) {
      erwartet = m;
      break;
    }
  }
  const int weckzeit = fenster + 30 + 20 * 60;
  alarm_create(&(AlarmInfo){ .hour = (weckzeit / 60) % 24, .minute = weckzeit % 60,
                             .kind = ALARM_KIND_EVERYDAY, .is_smart = true });
  prv_bis(fenster + 31);
  cl_assert(s_geklingelt_um != 0);
  const int geklingelt = (s_geklingelt_um - s_beginn) / SECONDS_PER_MINUTE;
  printf("\nSmart Alarm %02d:%02d: Fenster ab %02d:%02d, erste Regung %02d:%02d, geklingelt "
         "%02d:%02d\n", ((weckzeit / 60) % 24), weckzeit % 60, ((fenster / 60 + 20) % 24),
         fenster % 60, ((erwartet / 60 + 20) % 24), erwartet % 60, ((geklingelt / 60 + 20) % 24),
         geklingelt % 60);
  cl_assert(erwartet >= fenster + 10);
  cl_assert_equal_i(geklingelt, erwartet);
}

// Um 06:30 wie im Alltag: er klingelt mit der ersten Regung im Fenster.
void test_boulder_wecker__smart_alarm_um_halb_sieben(void) {
  int erwartet = 630;
  for (int m = 600; m < 630; m++) {
    if (prv_bewegt(m - 1)) {
      erwartet = m;
      break;
    }
  }
  alarm_create(&(AlarmInfo){ .hour = 6, .minute = 30, .kind = ALARM_KIND_EVERYDAY,
                             .is_smart = true });
  prv_bis(631);
  cl_assert(s_geklingelt_um != 0);
  const int geklingelt = (s_geklingelt_um - s_beginn) / SECONDS_PER_MINUTE;
  printf("\nSmart Alarm 06:30: erste Regung %02d:%02d, geklingelt %02d:%02d\n",
         (erwartet / 60 + 20) % 24, erwartet % 60, (geklingelt / 60 + 20) % 24, geklingelt % 60);
  cl_assert_equal_i(geklingelt, erwartet);
}
