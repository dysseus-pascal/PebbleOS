/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "kernel/events.h"
#include "pbl/services/activity/boulder/hrv.h"
#include "pbl/services/activity/boulder/messstufe.h"
#include "pbl/services/hrm/hrm_manager.h"
#include "pbl/services/hrm/hrm_manager_private.h"
#include <pbl/drivers/rtc.h>

#include "stubs_logging.h"

#include "fake_rtc.h"
#include "fake_settings_file.h"

// Die HRV-Fenster der Boulder-Firmware: dieselbe Rechnung wie in Kieselsport
// (RMSSD, Ausreisser ueber ein Drittel verworfen, mindestens 30 Intervalle), und
// Fenster nur in Ruhe, im Takt der Messstufe. Die Abstaende kommen wie auf der
// Uhr: ueber den Rueckruf des HRM-Dienstes, mit Qualitaet.

static bool s_ruhe = true;
static uint16_t s_takt = 30;
static bool s_puls_an = true;
bool boulder_nachtruhe(time_t utc, uint16_t letzte_bewegung) { return s_ruhe; }
uint16_t boulder_hrv_takt_min(void) { return s_takt; }
bool activity_prefs_heart_rate_is_enabled(void) { return s_puls_an; }

// Der HRM-Dienst: wer abonniert, bekommt die Ereignisse ueber den Rueckruf.
static HRMSubscriberCallback s_rueckruf;
static int s_abos;
HRMSessionRef hrm_manager_subscribe_with_callback(AppInstallId app_id, uint32_t update_interval_s,
                                                  uint16_t expire_s, HRMFeature features,
                                                  bool low_latency, HRMSubscriberCallback callback,
                                                  void *context) {
  cl_assert(features & HRMFeature_HRV);
  s_rueckruf = callback;
  s_abos++;
  return (HRMSessionRef)7;
}
bool sys_hrm_manager_unsubscribe(HRMSessionRef session) {
  s_rueckruf = NULL;
  return true;
}

// Ein Abstand vom Sensor: er kommt nach seiner eigenen Dauer.
static void prv_abstand(uint16_t ppi, HRMQuality qualitaet) {
  fake_rtc_increment_ticks((RtcTicks)ppi * RTC_TICKS_HZ / 1000);
  PebbleHRMEvent e = {
    .event_type = HRMEvent_HRV,
    .hrv = { .ppi_ms = ppi, .quality = qualitaet },
  };
  cl_assert(s_rueckruf);
  s_rueckruf(&e, NULL);
}

#define BEGINN 1790445600  // 26.9.2026, 18:00 UTC - durch 30 Minuten teilbar

void test_boulder_hrv__initialize(void) {
  s_ruhe = true;
  s_takt = 30;
  s_puls_an = true;
  s_rueckruf = NULL;
  s_abos = 0;
  fake_rtc_init(1000, BEGINN);
  fake_settings_file_reset();
  boulder_hrv_test_reset();
}

void test_boulder_hrv__kein_fenster_bei_puls_aus(void) {
  s_puls_an = false;
  boulder_hrv_minute(BEGINN, 10, false);
  cl_assert_equal_i(s_abos, 0);
  boulder_hrv_minute(BEGINN + 12 * 60, 10, false);
  SchlafHrv f[4];
  cl_assert_equal_i(boulder_hrv_fenster(BEGINN - 60, BEGINN + 20 * 60, f, 4), 0);
}

void test_boulder_hrv__rmssd_wie_kieselsport(void) {
  // Ein Fenster beginnen, dann Abstaende im Wechsel 1000 und 1040 ms: jede
  // Differenz 40 ms, RMSSD also 40. Ein verpasster Schlag (1900) faellt weg.
  boulder_hrv_minute(BEGINN, 10, false);
  for (int i = 0; i < 40; i++) {
    prv_abstand((i % 2) ? 1040 : 1000, HRMQuality_Good);
    if (i == 20) {
      prv_abstand(1900, HRMQuality_Good);
    }
  }
  cl_assert_equal_i(boulder_hrv_rmssd(), 40);
  // Nach fuenf Minuten ist das Fenster fertig und liegt fuer die Nacht bereit.
  boulder_hrv_minute(BEGINN + 5 * 60, 10, false);
  SchlafHrv f[4];
  cl_assert_equal_i(boulder_hrv_fenster(BEGINN - 3600, BEGINN + 3600, f, 4), 1);
  cl_assert_equal_i(f[0].minute, 60);
  cl_assert_equal_i(f[0].rmssd, 40);
  // Der Puls aus denselben Abstaenden: im Mittel 1020 ms, also 59 Schlaege.
  cl_assert_equal_i(f[0].puls, 59);
}

void test_boulder_hrv__zu_wenige_intervalle_zaehlen_nicht(void) {
  boulder_hrv_minute(BEGINN + 30 * 60, 10, false);
  for (int i = 0; i < 20; i++) {
    prv_abstand((i % 2) ? 950 : 1000, HRMQuality_Good);
  }
  cl_assert_equal_i(boulder_hrv_rmssd(), 0);
  boulder_hrv_minute(BEGINN + 35 * 60, 10, false);
  SchlafHrv f[4];
  cl_assert_equal_i(boulder_hrv_fenster(BEGINN + 20 * 60, BEGINN + 3600, f, 4), 0);
}

void test_boulder_hrv__nicht_ausserhalb_des_takts_oder_in_bewegung(void) {
  // Nicht im Takt (Minute 7): kein Fenster, kein Sensor.
  boulder_hrv_minute(BEGINN + 7 * 60, 10, false);
  cl_assert_equal_i(s_abos, 0);
  // Die Testhilfe zaehlt jedes Intervall; ein Ergebnis wird es aber nur, wenn
  // ein Fenster es abschliesst.
  for (int i = 0; i < 40; i++) {
    boulder_hrv_intervall((i % 2) ? 1040 : 1000);
  }
  cl_assert_equal_i(boulder_hrv_rmssd(), 40);
  boulder_hrv_minute(BEGINN + 12 * 60, 10, false);
  SchlafHrv f[4];
  cl_assert_equal_i(boulder_hrv_fenster(BEGINN + 6 * 60, BEGINN + 20 * 60, f, 4), 0);
  // In Bewegung zur vollen Stunde: auch keins.
  s_ruhe = false;
  boulder_hrv_minute(BEGINN + 60 * 60, 3000, false);
  boulder_hrv_minute(BEGINN + 65 * 60, 3000, false);
  cl_assert_equal_i(s_abos, 0);
}

// M8: SOLANGE SPO2 MISST, BEGINNT KEIN FENSTER. Beide brauchen denselben
// Lichtweg des Sensors; das Fenster bekaeme kaum Abstaende.
void test_boulder_hrv__kein_fenster_solange_spo2_misst(void) {
  boulder_hrv_minute(BEGINN, 10, true);
  cl_assert_equal_i(s_abos, 0);
  boulder_hrv_minute(BEGINN + 30 * 60, 10, false);
  cl_assert_equal_i(s_abos, 1);
}

// M8: NUR SICHERE ABSTAENDE, UND KEINE DIFFERENZ UEBER EINE LUECKE. 1000 und
// 1010 im Wechsel (RMSSD 10). Dazwischen ein 700er mit "Worst" - unter einem
// Drittel Abweichung, er zoege den Wert hoch - und zwei Minuten ohne Abstaende
// (SpO2 hatte den Sensor), danach schlaegt das Herz langsamer: 1200 und 1210.
// Die Differenz ueber die Luecke (190) ist kein Schlag-zu-Schlag.
void test_boulder_hrv__schlechte_abstaende_und_luecken_zaehlen_nicht(void) {
  boulder_hrv_minute(BEGINN, 10, false);
  for (int i = 0; i < 30; i++) {
    prv_abstand((i % 2) ? 1010 : 1000, HRMQuality_Good);
    if (i == 15) {
      prv_abstand(700, HRMQuality_Worst);
    }
  }
  fake_rtc_increment_ticks(2 * 60 * RTC_TICKS_HZ);
  for (int i = 0; i < 30; i++) {
    prv_abstand((i % 2) ? 1210 : 1200, HRMQuality_Acceptable);
  }
  cl_assert_equal_i(boulder_hrv_rmssd(), 10);
  boulder_hrv_minute(BEGINN + 5 * 60, 10, false);
  SchlafHrv f[4];
  cl_assert_equal_i(boulder_hrv_fenster(BEGINN - 60, BEGINN + 3600, f, 4), 1);
  cl_assert_equal_i(f[0].rmssd, 10);
}

// N4: WIRD DIE UHR ZURUECKGESTELLT, HAENGT DAS FENSTER NICHT. Es endet sofort,
// ohne zu zaehlen, und das naechste beginnt im Takt - nicht erst, wenn die
// neue Zeit den alten Beginn wieder einholt.
void test_boulder_hrv__zurueckgestellte_uhr_haengt_das_fenster_nicht(void) {
  boulder_hrv_minute(BEGINN, 10, false);
  cl_assert_equal_i(s_abos, 1);
  for (int i = 0; i < 40; i++) {
    prv_abstand((i % 2) ? 1040 : 1000, HRMQuality_Good);
  }
  const time_t zurueck = BEGINN - 3600;
  boulder_hrv_minute(zurueck, 10, false);
  cl_assert(s_rueckruf == NULL);
  boulder_hrv_minute(zurueck + 30 * 60, 10, false);
  cl_assert_equal_i(s_abos, 2);
  // Das abgebrochene Fenster zaehlt nicht - sein Beginn laege in der Zukunft.
  SchlafHrv f[4];
  cl_assert_equal_i(boulder_hrv_fenster(BEGINN - 2 * 3600, BEGINN + 3600, f, 4), 0);
}

// M4: DIE FENSTER UEBERSTEHEN EINEN NEUSTART. Rechnet die Uhr die Nacht am
// Morgen neu, hat sie sie noch - in ihrer Reihenfolge.
void test_boulder_hrv__fenster_ueberstehen_einen_neustart(void) {
  for (int k = 0; k < 2; k++) {
    const time_t start = BEGINN + k * 30 * 60;
    boulder_hrv_minute(start, 10, false);
    for (int i = 0; i < 40; i++) {
      prv_abstand((i % 2) ? 1000 + 20 * (k + 1) : 1000, HRMQuality_Good);
    }
    boulder_hrv_minute(start + 5 * 60, 10, false);
  }
  boulder_hrv_test_reset();
  SchlafHrv f[4];
  cl_assert_equal_i(boulder_hrv_fenster(BEGINN - 60, BEGINN + 3600, f, 4), 2);
  cl_assert_equal_i(f[0].minute, 1);
  cl_assert_equal_i(f[0].rmssd, 20);
  cl_assert_equal_i(f[1].minute, 31);
  cl_assert_equal_i(f[1].rmssd, 40);
  // Das naechste kommt hinter die alten, nicht ueber sie.
  boulder_hrv_minute(BEGINN + 60 * 60, 10, false);
  for (int i = 0; i < 40; i++) {
    prv_abstand((i % 2) ? 1060 : 1000, HRMQuality_Good);
  }
  boulder_hrv_minute(BEGINN + 65 * 60, 10, false);
  cl_assert_equal_i(boulder_hrv_fenster(BEGINN - 60, BEGINN + 2 * 3600, f, 4), 3);
  cl_assert_equal_i(f[2].rmssd, 60);
}
