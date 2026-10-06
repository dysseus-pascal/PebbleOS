/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2, AP9: der taegliche Diagnosesatz (diagnose.h) mit der echten Messung (takt.c),
// dem echten Heap (lib/util/heap.c ueber fake_heap.c) und der echten settings_file.c.

#include "kernel/kernel_heap.h"
#include "pbl/services/activity/boulder/diagnose.h"
#include "pbl/services/activity/boulder/takt.h"
#include "pbl/services/filesystem/pfs.h"
#include "pbl/services/settings/settings_file.h"
#include "pbl/util/units.h"

#include <string.h>

#include "clar.h"

#include "stubs_analytics.h"
#include "stubs_hexdump.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_sleep.h"
#include "stubs_task_wdt.h"

#include "fake_new_timer.h"
#include "fake_rtc.h"
#include "fake_spi_flash.h"
#include "fake_system_task.h"

#define DATEI "boulder_v2"
// 6.10.2026 12:00 UTC; der Tag ist time_util_get_day dieser Zeit.
#define MITTAG ((time_t)1791288000)

// RTC_TICKS_HZ ist 1024: ms auf ganze Ticks gerundet.
static void prv_ms(uint32_t ms) {
  fake_rtc_increment_ticks(((RtcTicks)ms * RTC_TICKS_HZ + 500) / 1000);
}

static BoulderAufgabe s_rechnen = BOULDER_AUFGABE("Boulder Rechnen", NULL);

// Eine Arbeit, die `ms` rechnet und bei `frueh` mitten drin zurueckspringt.
static int prv_arbeit(uint32_t ms, bool frueh) {
  BOULDER_AUFGABE_LAEUFT(&s_rechnen);
  prv_ms(ms);
  if (frueh) {
    return 1;
  }
  prv_ms(1);
  return 0;
}

static void prv_minutenteil(uint32_t ms) {
  BOULDER_AUFGABE_LAEUFT(boulder_messung_minutenteil());
  prv_ms(ms);
}

static void prv_ablegen(void) {
  SettingsFile f;
  cl_must_pass(settings_file_open(&f, DATEI, 8 * 1024));
  boulder_diagnose_ablegen(&f);
  settings_file_close(&f);
}

static void prv_laden(time_t jetzt) {
  SettingsFile f;
  cl_must_pass(settings_file_open(&f, DATEI, 8 * 1024));
  boulder_diagnose_laden(&f, jetzt);
  settings_file_close(&f);
}

void test_boulder_diagnose__initialize(void) {
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  fake_rtc_init(0, MITTAG);
  boulder_diagnose_test_zuruecksetzen();
  s_rechnen = (BoulderAufgabe)BOULDER_AUFGABE("Boulder Rechnen", NULL);
}

void test_boulder_diagnose__cleanup(void) {
}

// Die Messung endet bei jedem Verlassen: auch der fruehe Ruecksprung zaehlt mit seiner Zeit.
void test_boulder_diagnose__laufzeit_auch_beim_fruehen_ruecksprung(void) {
  prv_arbeit(30, false);
  cl_assert_equal_i(s_rechnen.max_ms, 31);
  prv_arbeit(120, true);
  cl_assert_equal_i(s_rechnen.laeufe, 2);
  cl_assert_equal_i(s_rechnen.max_ms, 120);
  cl_assert_equal_i((s_rechnen.summe_ticks * 1000 + RTC_TICKS_HZ / 2) / RTC_TICKS_HZ, 151);
  BoulderAufgabe *const *liste;
  const int n = boulder_messung_liste(&liste);
  bool drin = false;
  for (int i = 0; i < n; i++) {
    drin |= liste[i] == &s_rechnen;
  }
  cl_assert(drin);
}

// Der Satz des Tages: laengster Schritt mit Namen, Minutenteil, Heap-Niedrigstand aus dem
// echten Heap; der Tageswechsel beginnt neu.
void test_boulder_diagnose__tagessatz_und_tageswechsel(void) {
  prv_laden(MITTAG);
  prv_arbeit(80, true);
  prv_minutenteil(7);
  void *block = heap_malloc(kernel_heap_get(), 4000, 0);
  cl_assert(block);
  heap_free(kernel_heap_get(), block, 0);
  boulder_diagnose_takt(MITTAG + 60);
  const BoulderDiagnoseTag *d = boulder_diagnose_heute();
  cl_assert_equal_i(d->max_ms, 80);
  cl_assert_equal_s(d->max_name, "Rechnen");
  cl_assert_equal_i(d->minute_max_ms, 7);
  cl_assert(d->heap_min <= heap_size(kernel_heap_get()) - 4000);
  cl_assert_equal_i(d->neustarts, 1);
  // Naechster Tag: alles neu, die Tageshoechstwerte der Arbeiten auch.
  boulder_diagnose_takt(MITTAG + PBL_SEC_PER_DAY);
  cl_assert_equal_i(d->tag, time_util_get_day(MITTAG + PBL_SEC_PER_DAY));
  cl_assert_equal_i(d->max_ms, 0);
  cl_assert_equal_i(d->minute_max_ms, 0);
  cl_assert_equal_i(d->neustarts, 0);
  cl_assert_equal_i(s_rechnen.tag_max_ms, 0);
  cl_assert_equal_i(s_rechnen.max_ms, 80);  // seit Boot bleibt
}

// Neustarts: "Laden" zaehlt, der Satz ueberlebt den Neustart samt Hoechstwert; am naechsten
// Tag beginnt er mit dem einen Neustart dieses Tages.
void test_boulder_diagnose__neustarts_ueber_die_datei(void) {
  prv_laden(MITTAG);
  prv_arbeit(95, false);
  boulder_diagnose_takt(MITTAG + 60);
  prv_ablegen();
  // Neustart am selben Tag
  boulder_diagnose_test_zuruecksetzen();
  s_rechnen.tag_max_ms = 0;
  prv_laden(MITTAG + 3600);
  const BoulderDiagnoseTag *d = boulder_diagnose_heute();
  cl_assert_equal_i(d->neustarts, 2);
  cl_assert_equal_i(d->max_ms, 96);
  cl_assert_equal_s(d->max_name, "Rechnen");
  boulder_diagnose_takt(MITTAG + 3660);
  cl_assert_equal_i(d->max_ms, 96);
  prv_ablegen();
  // Neustart am naechsten Tag
  boulder_diagnose_test_zuruecksetzen();
  prv_laden(MITTAG + PBL_SEC_PER_DAY);
  cl_assert_equal_i(d->neustarts, 1);
  cl_assert_equal_i(d->max_ms, 0);
}

// Ohne Datei (nicht geoeffnet) beginnt der Tag neu und zaehlt den Neustart trotzdem.
void test_boulder_diagnose__ohne_datei(void) {
  boulder_diagnose_laden(NULL, MITTAG);
  cl_assert_equal_i(boulder_diagnose_heute()->neustarts, 1);
}
