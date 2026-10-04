/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Die Quellen der Health-App, nachgebaut: Aktivitaetsdienst (Tageszaehler,
// Minutenhistorie, Puls), Nacht und SpO2 der Boulder-Dienste und die Uhr mit
// Zeitzone samt Umstellung. Grenzen der App - was dahinter rechnet, ist
// nicht ihr Teil.

#include "apps/system/health/daten.h"
#include "applib/health_service.h"
#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/boulder/haken.h"
#include "pbl/services/activity/boulder/spo2.h"
#undef malloc
#undef calloc
#undef free
#undef time
#undef localtime

#include <stdlib.h>
#include <time.h>

// --- Uhr und Zeitzone: vor [s_umstellung] gilt s_versatz_vor, danach
// s_versatz_nach (Sekunden zu UTC). ---

static time_t s_jetzt;
static time_t s_umstellung;
static int32_t s_versatz_vor;
static int32_t s_versatz_nach;

time_t rtc_get_time(void) { return s_jetzt; }

static int32_t prv_versatz(time_t t) { return t >= s_umstellung ? s_versatz_nach : s_versatz_vor; }

// Die Uhrzeit zu [t], wie die App sie braucht (Sekunde, Minute, Stunde). Von
// Hand: das struct tm der Firmware ist kleiner als das der glibc, deren
// gmtime_r schriebe ueber sein Ende hinaus.
static void prv_zerlegen(time_t t, struct tm *aus) {
  memset(aus, 0, sizeof(*aus));
  const time_t im_tag = ((t % 86400) + 86400) % 86400;
  aus->tm_sec = (int)(im_tag % 60);
  aus->tm_min = (int)(im_tag / 60 % 60);
  aus->tm_hour = (int)(im_tag / 3600);
}

struct tm *pbl_override_localtime(const time_t *t) {
  static struct tm ergebnis;
  prv_zerlegen(*t + prv_versatz(*t), &ergebnis);
  return &ergebnis;
}

// Mitternacht des heutigen Ortstags als UTC - wie time_start_of_today mit mktime.
time_t time_start_of_today(void) {
  const time_t ort = s_jetzt + prv_versatz(s_jetzt);
  const time_t mitternacht_ort = ort - ort % (24 * 3600);
  // Um Mitternacht galt der Versatz, der vor der Umstellung galt, wenn die
  // Umstellung heute war.
  time_t utc = mitternacht_ort - s_versatz_nach;
  if (utc >= s_umstellung) return utc;
  return mitternacht_ort - s_versatz_vor;
}

// --- Aktivitaet ---

static int32_t s_metriken[ActivityMetricNumMetrics];

bool activity_get_metric(ActivityMetric metric, uint32_t history_len, int32_t *history) {
  history[0] = s_metriken[metric];
  return true;
}

bool activity_prefs_blood_oxygen_is_enabled(void) { return true; }
HRMonitoringInterval activity_prefs_get_spo2_measurement_interval(void) {
  return HRMonitoringInterval_10Min;
}
uint32_t boulder_spo2_takt_s(void) { return 600; }
int boulder_spo2_werte(time_t von, time_t bis, Spo2Wert *aus, int max) { return 0; }
bool boulder_nacht_letzte(NachtBild *aus) { return false; }

// Die Minutenhistorie: jede Minute ein Schritt, ohne Puls.
uint32_t health_service_get_minute_history(HealthMinuteData *minute_data, uint32_t max_records,
                                           time_t *time_start, time_t *time_end) {
  time_t von = *time_start - *time_start % 60;
  uint32_t n = 0;
  while (n < max_records && von + (time_t)n * 60 < *time_end && von + (time_t)n * 60 < s_jetzt) {
    minute_data[n] = (HealthMinuteData){ .steps = 1 };
    n++;
  }
  *time_start = von;
  *time_end = von + (time_t)n * 60;
  return n;
}

void *app_malloc_check(size_t bytes) { return malloc(bytes); }
void app_free(void *ptr) { free(ptr); }
