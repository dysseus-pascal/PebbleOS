/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "pbl/services/activity/boulder/hrv.h"
#include "pbl/services/activity/boulder/messstufe.h"

#include "stubs_logging.h"

// Die HRV-Fenster der Boulder-Firmware: dieselbe Rechnung wie in Kieselsport
// (RMSSD, Ausreisser ueber ein Drittel verworfen, mindestens 30 Intervalle), und
// Fenster nur in Ruhe, im Takt der Messstufe.

static bool s_ruhe = true;
static uint16_t s_takt = 30;
static bool s_puls_an = true;
bool boulder_nachtruhe(time_t utc, uint16_t letzte_bewegung) { return s_ruhe; }
uint16_t boulder_hrv_takt_min(void) { return s_takt; }
bool activity_prefs_heart_rate_is_enabled(void) { return s_puls_an; }

#define BEGINN 1790445600  // 26.9.2026, 18:00 UTC - durch 30 Minuten teilbar

void test_boulder_hrv__initialize(void) {
  s_ruhe = true;
  s_takt = 30;
  s_puls_an = true;
}

void test_boulder_hrv__kein_fenster_bei_puls_aus(void) {
  s_puls_an = false;
  boulder_hrv_minute(BEGINN, 10);
  for (int i = 0; i < 40; i++) {
    boulder_hrv_intervall((i % 2) ? 1040 : 1000);
  }
  boulder_hrv_minute(BEGINN + 12 * 60, 10);
  SchlafHrv f[4];
  cl_assert_equal_i(boulder_hrv_fenster(BEGINN - 60, BEGINN + 20 * 60, f, 4), 0);
}

void test_boulder_hrv__rmssd_wie_kieselsport(void) {
  // Ein Fenster beginnen, dann Abstaende im Wechsel 1000 und 1040 ms: jede
  // Differenz 40 ms, RMSSD also 40. Ein verpasster Schlag (2000) faellt weg.
  boulder_hrv_minute(BEGINN, 10);
  for (int i = 0; i < 40; i++) {
    boulder_hrv_intervall((i % 2) ? 1040 : 1000);
    if (i == 20) {
      boulder_hrv_intervall(1900);
    }
  }
  cl_assert_equal_i(boulder_hrv_rmssd(), 40);
  // Nach fuenf Minuten ist das Fenster fertig und liegt fuer die Nacht bereit.
  boulder_hrv_minute(BEGINN + 5 * 60, 10);
  SchlafHrv f[4];
  cl_assert_equal_i(boulder_hrv_fenster(BEGINN - 3600, BEGINN + 3600, f, 4), 1);
  cl_assert_equal_i(f[0].minute, 60);
  cl_assert_equal_i(f[0].rmssd, 40);
}

void test_boulder_hrv__zu_wenige_intervalle_zaehlen_nicht(void) {
  boulder_hrv_minute(BEGINN + 30 * 60, 10);
  for (int i = 0; i < 20; i++) {
    boulder_hrv_intervall((i % 2) ? 950 : 1000);
  }
  cl_assert_equal_i(boulder_hrv_rmssd(), 0);
  boulder_hrv_minute(BEGINN + 35 * 60, 10);
  SchlafHrv f[4];
  // Nur das Fenster aus dem Test davor: dieses zaehlt nicht.
  cl_assert_equal_i(boulder_hrv_fenster(BEGINN + 20 * 60, BEGINN + 3600, f, 4), 0);
}

void test_boulder_hrv__nicht_ausserhalb_des_takts_oder_in_bewegung(void) {
  // Nicht im Takt (Minute 7): kein Fenster.
  boulder_hrv_minute(BEGINN + 7 * 60, 10);
  for (int i = 0; i < 40; i++) {
    boulder_hrv_intervall((i % 2) ? 1040 : 1000);
  }
  // Die Testhilfe zaehlt jedes Intervall; ein Ergebnis wird es aber nur, wenn
  // ein Fenster es abschliesst.
  cl_assert_equal_i(boulder_hrv_rmssd(), 40);
  boulder_hrv_minute(BEGINN + 12 * 60, 10);
  SchlafHrv f[4];
  cl_assert_equal_i(boulder_hrv_fenster(BEGINN + 6 * 60, BEGINN + 20 * 60, f, 4), 0);
  // In Bewegung zur vollen Stunde: auch keins.
  s_ruhe = false;
  boulder_hrv_minute(BEGINN + 60 * 60, 3000);
  boulder_hrv_minute(BEGINN + 65 * 60, 3000);
  cl_assert_equal_i(boulder_hrv_fenster(BEGINN + 50 * 60, BEGINN + 90 * 60, f, 4), 0);
}
