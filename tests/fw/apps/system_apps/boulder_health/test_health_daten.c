/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "stubs_logging.h"

#include "fake_health_quellen.h"

// Die Daten der Health-App (daten.c): Schritte je Stunde des Tages, auch an
// den Tagen der Zeitumstellung, und ob die Uhr SpO2 misst.

static HealthDaten s_daten;

static void prv_tag_lesen(void) {
  memset(&s_daten, 0, sizeof(s_daten));
  daten_sammeln(&s_daten);
  MinutenLesen m;
  daten_minuten_beginnen(&m);
  for (int i = 0; i < 100 && !daten_minuten_schritt(&m, &s_daten); i++) {
  }
}

void test_health_daten__initialize(void) {
  memset(s_metriken, 0, sizeof(s_metriken));
  s_spo2_intervall = HRMonitoringInterval_10Min;
}

void test_health_daten__cleanup(void) {}

void test_health_daten__fruehling_saeulen_zur_richtigen_stunde(void) {
  // 29.3.2026: um 2 Uhr (MEZ, 01:00 UTC) wird es 3 Uhr.
  s_umstellung = 1774746000;
  s_versatz_vor = 3600;
  s_versatz_nach = 7200;
  s_jetzt = 1774780200;   // 12:30 MESZ
  prv_tag_lesen();
  cl_assert_equal_i(s_daten.stunde_jetzt, 12);
  cl_assert_equal_i(s_daten.schritte_stunde[1], 60);
  // Die Stunde von 2 bis 3 gab es nicht.
  cl_assert_equal_i(s_daten.schritte_stunde[2], 0);
  cl_assert_equal_i(s_daten.schritte_stunde[3], 60);
  cl_assert_equal_i(s_daten.schritte_stunde[11], 60);
  cl_assert_equal_i(s_daten.schritte_stunde[12], 30);
}

void test_health_daten__herbst_die_letzte_stunde_zaehlt(void) {
  // 25.10.2026: um 3 Uhr (MESZ, 01:00 UTC) wird es wieder 2 Uhr.
  s_umstellung = 1792890000;
  s_versatz_vor = 7200;
  s_versatz_nach = 3600;
  s_jetzt = 1792967400;   // 23:30 MEZ
  prv_tag_lesen();
  cl_assert_equal_i(s_daten.stunde_jetzt, 23);
  // Die Stunde von 2 bis 3 gab es zweimal.
  cl_assert_equal_i(s_daten.schritte_stunde[2], 120);
  cl_assert_equal_i(s_daten.schritte_stunde[22], 60);
  cl_assert_equal_i(s_daten.schritte_stunde[23], 30);
}

void test_health_daten__gewoehnlicher_tag(void) {
  s_umstellung = 0;
  s_versatz_vor = s_versatz_nach = 7200;
  s_jetzt = 1790493000;   // 27.9.2026, 9:10 MESZ
  prv_tag_lesen();
  cl_assert_equal_i(s_daten.stunde_jetzt, 9);
  cl_assert_equal_i(s_daten.schritte_stunde[0], 60);
  cl_assert_equal_i(s_daten.schritte_stunde[8], 60);
  cl_assert_equal_i(s_daten.schritte_stunde[9], 10);
}

// SPO2 "AUS" AUCH BEI PEBBLES AUSWAHL "DISABLED": dann misst activity.c nicht
// (prv_spo2_subscription_update), also zeigt der Ring "off" statt eines
// veralteten Werts. Eine bewusste Angleichung an activity.c seit 1abe14a79,
// auch mit allen Schaltern an (BOULDER.md, "Health-App").
void test_health_daten__spo2_aus_bei_pebbles_auswahl_disabled(void) {
  s_umstellung = 0;
  s_versatz_vor = s_versatz_nach = 7200;
  s_jetzt = 1790493000;
  prv_tag_lesen();
  cl_assert(!s_daten.spo2_aus);
  s_spo2_intervall = HRMonitoringInterval_Disabled;
  prv_tag_lesen();
  cl_assert(s_daten.spo2_aus);
}
