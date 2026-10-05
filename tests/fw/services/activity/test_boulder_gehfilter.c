/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder: der Gehfilter aus b19 - unter 40 Schritten je Minute ist eine
// erkannte Sitzung kein Gehen und faellt weg.
//
// DER GANZE WEG, NICHT DER CALLBACK ALLEIN. Hier laufen die echte
// Minutenverarbeitung (activity_algorithm_kraepelin.c) und die echte
// Sitzungserkennung von Kraepelin (kraepelin_algorithm.c) zusammen. Nur die
// Schrittzaehlung aus den Rohdaten des Beschleunigungssensors ist ersetzt
// (per --wrap): fuer eine Velofahrt gibt es keine Rohdaten, und der Filter
// fragt nur, wie viele Schritte in jeder Minute gezaehlt wurden. Ein Test,
// der die Sitzung selbst in den Callback steckte, haette nicht gesehen, dass
// Kraepelin seit b1 nur noch "Bewegung" meldet - und der Filter von b19 nur
// "Walk" und "Run" pruefte, also nie griff.
//
// DAZU DIE ECHTE NACHT (boulder/nacht.c, schlafanalyse.c): ein Training am Morgen
// darf die laufende Nacht nicht abschliessen (training_am_morgen_laesst_die_nacht_laufen).

#include "applib/accel_service.h"
#include "applib/data_logging.h"
#include <pbl/drivers/ambient_light.h>
#include <pbl/drivers/rtc.h>
#include "pbl/services/battery/battery_state.h"
#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/activity_private.h"
#include "pbl/services/activity/activity_algorithm.h"
#include "pbl/services/activity/kraepelin/activity_algorithm_kraepelin.h"
#include "pbl/services/activity/kraepelin/kraepelin_algorithm.h"
#include "pbl/services/activity/boulder/nacht.h"
#include "pbl/services/activity/boulder/schlafanalyse.h"
#include "pbl/services/data_logging/data_logging_service.h"
#include "pbl/services/filesystem/pfs.h"
#include "kernel/kernel_heap.h"
#include "pbl/util/heap.h"
#include "pbl/util/math.h"
#include "pbl/util/size.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "clar.h"

// Stubs
#include "stubs_analytics.h"
#include "stubs_hexdump.h"
#include "stubs_hr_util.h"
#include "stubs_irq.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_prompt.h"
#include "stubs_sleep.h"
#include "stubs_task_watchdog.h"

// Fakes
#include "fake_accel_service.h"
#include "fake_new_timer.h"
#include "fake_rtc.h"
#include "fake_spi_flash.h"
#include "fake_system_task.h"

// Die Schwelle, festgenagelt: 40 Schritte je Minute (b19). Hier nicht aus der
// Firmware geholt - aendert jemand den Wert, soll dieser Test es merken.
#define SCHWELLE_JE_MINUTE 40

AccelSamplingRate s_sample_rate;

// --------------------------------------------------------------------------------------------
// Die Schritte der laufenden Minute, wie sie die Zaehlung aus den Rohdaten liefern wuerde.
static uint16_t s_schritte_der_minute;

uint32_t __wrap_kalg_analyze_samples(KAlgState *state, AccelRawData *data, uint32_t num_samples,
                                     uint32_t *consumed_samples) {
  *consumed_samples = 0;
  const uint16_t schritte = s_schritte_der_minute;
  // Nur einmal je Minute zaehlen: der Aufruf darf sich nicht wiederholen lassen.
  s_schritte_der_minute = 0;
  return schritte;
}

// --------------------------------------------------------------------------------------------
// Der Sitzungsspeicher, nach den Vertraegen in activity_private.h und activity.h.
static uint16_t s_anzahl;
static ActivitySession s_sitzungen[ACTIVITY_MAX_ACTIVITY_SESSIONS_COUNT];
static int s_hinzugefuegt;
static int s_zuruecknehmen_gesucht;
// Wie oft der Filter eine schon abgeschlossene Sitzung zuruecknehmen wollte. Das darf
// nie geschehen: es heisst, Kraepelin rechnet an einer Bewegung weiter, die schon fertig ist.
static int s_abgeschlossen_gefragt;
// Siehe eine_minute_beim_abschliessen_oeffnet_die_bewegung_nicht_wieder.
static bool s_minute_beim_abschliessen;
static void prv_minute(uint16_t schritte);

static bool prv_gleich(const ActivitySession *a, const ActivitySession *b) {
  return a->type == b->type && a->start_utc == b->start_utc;
}

bool activity_sessions_prv_add_activity_session(ActivitySession *session) {
  s_hinzugefuegt++;
  for (uint16_t i = 0; i < s_anzahl; i++) {
    if (prv_gleich(session, &s_sitzungen[i])) {
      s_sitzungen[i] = *session;
      // Genau jetzt kommt die naechste Minute, waehrend die Bewegung abgeschlossen wird.
      if (s_minute_beim_abschliessen && !session->ongoing) {
        s_minute_beim_abschliessen = false;
        prv_minute(100);
      }
      return true;
    }
  }
  cl_assert(s_anzahl < ARRAY_LENGTH(s_sitzungen));
  s_sitzungen[s_anzahl++] = *session;
  return true;
}

// Die Firmware haelt hier bei einer abgeschlossenen Sitzung an (PBL_ASSERT) - der Test
// auch. So loescht boulder/nacht.c ihre laufenden Schlafsitzungen bei jeder Rechnung
// (prv_melde). Der Gehfilter benutzt es nicht mehr; falls doch, soll der Test es sehen.
void activity_sessions_prv_delete_activity_session(ActivitySession *session) {
  for (uint16_t i = 0; i < s_anzahl; i++) {
    if (prv_gleich(session, &s_sitzungen[i])) {
      cl_assert(s_sitzungen[i].ongoing);
      memmove(&s_sitzungen[i], &s_sitzungen[i + 1], (s_anzahl - i - 1) * sizeof(*s_sitzungen));
      s_anzahl--;
      return;
    }
  }
}

// Nur eine laufende geht; eine abgeschlossene bleibt (activity_private.h). Das echte
// Verhalten prueft test_activity (laufende_zuruecknehmen_nur_solange_sie_laeuft).
bool activity_sessions_prv_laufende_zuruecknehmen(ActivitySession *session) {
  s_zuruecknehmen_gesucht++;
  for (uint16_t i = 0; i < s_anzahl; i++) {
    if (prv_gleich(session, &s_sitzungen[i])) {
      if (!s_sitzungen[i].ongoing) {
        s_abgeschlossen_gefragt++;
        return false;
      }
      memmove(&s_sitzungen[i], &s_sitzungen[i + 1], (s_anzahl - i - 1) * sizeof(*s_sitzungen));
      s_anzahl--;
      return true;
    }
  }
  return false;
}

// Die Nacht nimmt ihre eigenen Sitzungen zurueck, laufend oder von anderen abgeschlossen
// (activity_private.h). Gleich heisst hier wie beim Eintragen: Art und Beginn.
bool activity_sessions_prv_eigene_zuruecknehmen(ActivitySession *session) {
  for (uint16_t i = 0; i < s_anzahl; i++) {
    if (prv_gleich(session, &s_sitzungen[i])) {
      memmove(&s_sitzungen[i], &s_sitzungen[i + 1], (s_anzahl - i - 1) * sizeof(*s_sitzungen));
      s_anzahl--;
      return true;
    }
  }
  return false;
}

// Die Nacht speichert die Sitzungen, bevor sie sich "erledigt" merkt. Dieser Test hat
// keine Datei dafuer; die Sitzungen stehen hier nur im Speicher - das gilt als gespeichert.
bool activity_sessions_prv_speichern(void) {
  return true;
}

bool activity_sessions_prv_is_sleep_activity(ActivitySessionType activity_type) {
  return activity_type == ActivitySessionType_Sleep ||
         activity_type == ActivitySessionType_RestfulSleep ||
         activity_type == ActivitySessionType_Nap ||
         activity_type == ActivitySessionType_RestfulNap;
}

// Hinein geht der Platz im Feld, gefuellt wird hoechstens so viel (activity.h) - wie die
// Firmware: wer mit 0 fragt, bekommt nichts.
bool activity_get_sessions(uint32_t *session_entries, ActivitySession *sessions) {
  const uint32_t anzahl = MIN(*session_entries, s_anzahl);
  memcpy(sessions, s_sitzungen, anzahl * sizeof(ActivitySession));
  *session_entries = anzahl;
  return true;
}

// --------------------------------------------------------------------------------------------
// Was sonst um die Minutenverarbeitung herum steht: ruhig und ohne Wirkung.
uint32_t light_get_ambient_lux(void) {
  return 0;
}

AmbientLightLevel ambient_light_level_to_enum(uint32_t light_level) {
  return AmbientLightLevelUnknown;
}

BatteryChargeState battery_get_charge_state(void) {
  return (BatteryChargeState){ .charge_percent = 50 };
}

bool activity_tracking_on(void) {
  return true;
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

HRZone activity_metrics_prv_get_hr_zone(void) {
  return HRZone_Zone0;
}

void activity_metrics_prv_get_median_hr_bpm(int32_t *median, int32_t *total_weight) {
  if (median) {
    *median = 0;
  }
  if (total_weight) {
    *total_weight = 0;
  }
}

void activity_metrics_prv_get_spo2_sample(uint8_t *percent_out, uint8_t *quality_out) {
  *percent_out = 0;
  *quality_out = 0;
}

void activity_metrics_prv_peek_spo2_sample(uint8_t *percent_out, uint8_t *quality_out) {
  *percent_out = 0;
  *quality_out = 0;
}

void activity_metrics_prv_reset_hr_stats(void) {
}

void activity_metrics_prv_set_hrm_worn_status(time_t now_utc, bool is_offwrist) {
}

bool activity_metrics_prv_is_hrm_offwrist(time_t now_utc) {
  return false;
}

DataLoggingResult dls_log(DataLoggingSession *logging_session, const void *data,
                          uint32_t num_items) {
  return DATA_LOGGING_SUCCESS;
}

DataLoggingSession *dls_create(uint32_t tag, DataLoggingItemType item_type, uint16_t item_size,
                               bool buffered, bool resume, const Uuid *uuid) {
  return (DataLoggingSession *)1;
}

void dls_send_all_sessions(void) {
}

// Was die Nachtrechnung sonst fragt: ohne HRV-Fenster rechnet sie wie die Firmware,
// wenn der Sensor keine liefert.
int boulder_hrv_fenster(time_t von, time_t bis, SchlafHrv *aus, int max) {
  return 0;
}

uint8_t boulder_messstufe(void) {
  return 3;
}

void health_sync_schicken(void) {
}

// Der Kernel-Heap, aus dem die Nacht ihre Rechnung holt: ein echter Heap, gross
// wie der der Uhr (rund 96 KB), leer.
static uint8_t s_heap_speicher[96 * 1024];
static Heap s_kernel_heap;
Heap *kernel_heap_get(void) {
  return &s_kernel_heap;
}

// --------------------------------------------------------------------------------------------
// DIE MINUTENDATEI FUER DIE NACHT: die echte Nacht vom 26./27.9.2026 (22:00 bis 06:59,
// nacht-2026-09-26.csv), gelegt auf das Nachtfenster dieses Tests. Davor fehlen Daten,
// ab 07:00 ist man wach und unterwegs. Wie die der Uhr liefert sie alle Minuten bis zur
// eben vergangenen (sie liest den RAM-Ring mit, activity_algorithm_kraepelin.c).
#define NACHT_MINUTEN (16 * 60)
// 07:00 in Minuten ab 20:00, dem Beginn des Nachtfensters.
#define AUFSTEHEN_MIN 660
static time_t s_nacht_beginn;
static HealthMinuteData s_nacht[NACHT_MINUTEN];

bool __wrap_activity_algorithm_get_minute_history(HealthMinuteData *minute_data,
                                                  uint32_t *num_records, time_t *utc_start) {
  memset(minute_data, 0xFF, *num_records * sizeof(HealthMinuteData));
  int32_t anzahl = (rtc_get_time() - *utc_start) / SECONDS_PER_MINUTE;
  anzahl = MAX(0, MIN(anzahl, (int32_t)*num_records));
  for (int32_t k = 0; k < anzahl; k++) {
    const int32_t i = (*utc_start - s_nacht_beginn) / SECONDS_PER_MINUTE + k;
    if (i >= 0 && i < NACHT_MINUTEN) {
      minute_data[k] = s_nacht[i];
    }
  }
  *num_records = anzahl;
  return true;
}

static void prv_lade_nacht(void) {
  memset(s_nacht, 0xFF, sizeof(s_nacht));
  char pfad[512];
  snprintf(pfad, sizeof(pfad), "%s/activity/boulder/nacht-2026-09-26.csv", CLAR_FIXTURE_PATH);
  FILE *f = fopen(pfad, "r");
  cl_assert(f);
  char zeile[256];
  int n = 0;
  while (fgets(zeile, sizeof(zeile), f)) {
    if (zeile[0] == '\n' || zeile[0] == '\r') {
      break;  // danach kommen die HRV-Fenster
    }
    if (zeile[0] == '#' || !strncmp(zeile, "minute,", 7)) {
      continue;
    }
    // Spalten: minute, uhrzeit, bewegung, vmc, puls, ...
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
    const int i = 120 + n++;  // 22:00
    if (feld[2][0]) {
      // Die Datei der Uhr traegt das VMC = b*b/16 aus dem Bewegungsbyte b.
      const int b = atoi(feld[2]);
      s_nacht[i] = (HealthMinuteData){
        .vmc = (uint16_t)((b * b + 8) / 16),
        .heart_rate_bpm = feld[4][0] ? atoi(feld[4]) : 0,
      };
    }
  }
  fclose(f);
  cl_assert_equal_i(n, 540);
  for (int i = AUFSTEHEN_MIN; i < NACHT_MINUTEN; i++) {
    s_nacht[i] = (HealthMinuteData){ .vmc = 3000, .steps = 100, .heart_rate_bpm = 82 };
  }
}

// --------------------------------------------------------------------------------------------
// Eine Minute durch die echte Verarbeitung: erst die Schritte, dann der Minutentakt.
static void prv_minute(uint16_t schritte) {
  fake_rtc_increment_time(SECONDS_PER_MINUTE);
  s_schritte_der_minute = schritte;
  AccelRawData leer[1] = {};
  activity_algorithm_handle_accel(leer, 0, 0);
  AlgMinuteRecord satz = {};
  activity_algorithm_minute_handler(rtc_get_time(), &satz);
}

static void prv_minuten(const uint16_t *schritte, int anzahl) {
  for (int i = 0; i < anzahl; i++) {
    prv_minute(schritte[i]);
  }
}

// Danach eine Viertelstunde still: Kraepelin schliesst die Sitzung nach mehr
// als sechs stillen Minuten ab und meldet sie ein letztes Mal.
static void prv_ruhe(void) {
  for (int i = 0; i < 15; i++) {
    prv_minute(0);
  }
}

static int prv_bewegungen(void) {
  int n = 0;
  for (uint16_t i = 0; i < s_anzahl; i++) {
    n += (s_sitzungen[i].type == ActivitySessionType_Movement) ? 1 : 0;
  }
  return n;
}

void test_boulder_gehfilter__initialize(void) {
  // 1.10.2026, 17:00 UTC
  struct tm beginn = { .tm_year = 126, .tm_mon = 9, .tm_mday = 1, .tm_hour = 17 };
  fake_rtc_init(100, mktime(&beginn));
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  pfs_format(false);
  s_anzahl = 0;
  s_hinzugefuegt = 0;
  s_zuruecknehmen_gesucht = 0;
  s_abgeschlossen_gefragt = 0;
  s_minute_beim_abschliessen = false;
  s_schritte_der_minute = 0;
  heap_init(&s_kernel_heap, s_heap_speicher, s_heap_speicher + sizeof(s_heap_speicher), false);
  boulder_nacht_test_reset();
  cl_assert(activity_algorithm_init(&s_sample_rate));
  // Erst eine ruhige Viertelstunde, damit Kraepelin einen Anfang hat.
  prv_ruhe();
}

void test_boulder_gehfilter__cleanup(void) {
  fake_system_task_callbacks_invoke_pending();
  activity_algorithm_deinit();
}

// --------------------------------------------------------------------------------------------

// GENAU AN DER SCHWELLE BLEIBT ES GEHEN: zwanzig Minuten mit je 40 Schritten,
// 800 Schritte. Die Sitzung steht danach da, abgeschlossen, mit allen Schritten.
void test_boulder_gehfilter__an_der_schwelle_bleibt_die_sitzung(void) {
  uint16_t schritte[20];
  for (int i = 0; i < 20; i++) {
    schritte[i] = SCHWELLE_JE_MINUTE;
  }
  prv_minuten(schritte, ARRAY_LENGTH(schritte));
  prv_ruhe();
  cl_assert_equal_i(prv_bewegungen(), 1);
  cl_assert_equal_i(s_sitzungen[0].type, ActivitySessionType_Movement);
  cl_assert(!s_sitzungen[0].ongoing);
  cl_assert_equal_i(s_sitzungen[0].length_min, 20);
  cl_assert_equal_i(s_sitzungen[0].step_data.steps, 800);
}

// EINEN SCHRITT DARUNTER FAELLT SIE WEG: dieselben zwanzig Minuten, eine davon
// mit 39 statt 40 - 799 Schritte. Die ersten zehn Minuten lagen noch auf der
// Schwelle, die Sitzung stand schon als laufend da; sie wird zurueckgenommen.
// WARUM NICHT ZWANZIG MAL 39: unter 40 Schritten ist eine Minute fuer
// Kraepelin gar nicht aktiv, es gaebe keine Sitzung, und der Filter kaeme nie
// dran. Die eine Minute mit 39 zaehlt als stille Minute innerhalb der Sitzung.
void test_boulder_gehfilter__knapp_darunter_faellt_die_sitzung_weg(void) {
  uint16_t schritte[20];
  for (int i = 0; i < 20; i++) {
    schritte[i] = SCHWELLE_JE_MINUTE;
  }
  schritte[10] = SCHWELLE_JE_MINUTE - 1;
  prv_minuten(schritte, 10);
  // Nach zehn Minuten auf der Schwelle steht die Sitzung als laufend da.
  cl_assert_equal_i(prv_bewegungen(), 1);
  cl_assert(s_sitzungen[0].ongoing);
  cl_assert_equal_i(s_sitzungen[0].step_data.steps, 400);
  // Ab der zwoelften Minute (der naechsten aktiven nach der 39) fehlt immer
  // genau ein Schritt: 479 von 480, ..., 799 von 800 - die Sitzung ist weg.
  prv_minuten(&schritte[10], 10);
  cl_assert_equal_i(prv_bewegungen(), 0);
  prv_ruhe();
  cl_assert_equal_i(prv_bewegungen(), 0);
}

// DIE VELOFAHRT VOM 1.10.2026: 22 Minuten, Pace 2:17:30 je Kilometer, also
// rund 160 Meter "Weg". Ihre Minutenwerte liegen nicht vor; nachgebaut nach
// diesen Zahlen: alle sechs Minuten eine Minute mit gut 40 Stoessen am Lenker,
// die als Schritte zaehlen, dazwischen fast nichts. Fuer Kraepelin ist das eine
// Bewegung (eine aktive Minute je sechs haelt sie offen); im Mittel sind es
// neun Schritte je Minute.
void test_boulder_gehfilter__velofahrt_ist_kein_spaziergang(void) {
  static const uint16_t fahrt[22] = {
    42, 3, 0, 5, 0, 2,
    44, 0, 4, 0, 1, 0,
    41, 2, 0, 3, 0, 0,
    45, 1, 0, 2,
  };
  prv_minuten(fahrt, ARRAY_LENGTH(fahrt));
  prv_ruhe();
  // Kraepelin hat die Fahrt gemeldet (sonst prueft dieser Test nichts): jede
  // Meldung unter der Schwelle sucht die Sitzung zum Zuruecknehmen.
  cl_assert(s_zuruecknehmen_gesucht > 0);
  cl_assert_equal_i(s_hinzugefuegt, 0);
  cl_assert_equal_i(prv_bewegungen(), 0);
}

// EIN ECHTER SPAZIERGANG MIT PAUSE AN DER AMPEL: 25 Minuten um 95 Schritte,
// dazwischen drei Minuten Stehen. Er bleibt, mit allen Schritten.
void test_boulder_gehfilter__spaziergang_mit_ampel_bleibt(void) {
  uint16_t schritte[25];
  int summe = 0;
  for (int i = 0; i < 25; i++) {
    schritte[i] = (i >= 11 && i < 14) ? 0 : 95;
    summe += schritte[i];
  }
  prv_minuten(schritte, ARRAY_LENGTH(schritte));
  prv_ruhe();
  // Genau eine Sitzung, und sie ist Bewegung - Walk und Run melden seit b1 keine.
  cl_assert_equal_i(s_anzahl, 1);
  cl_assert_equal_i(prv_bewegungen(), 1);
  cl_assert_equal_i(s_sitzungen[0].length_min, 25);
  cl_assert_equal_i(s_sitzungen[0].step_data.steps, summe);
}

// --------------------------------------------------------------------------------------------
// TRAINING MITTEN IN DER BEWEGUNG. Kieselsport und der Workout-Dienst halten die
// Erkennung beim Start an und geben sie am Ende frei (dienst.c, workout_service.c).

static void prv_gleichmaessig(uint16_t schritte, int minuten) {
  for (int i = 0; i < minuten; i++) {
    prv_minute(schritte);
  }
}

// Eine Stunde Training: die Uhr zaehlt weiter Schritte, erkannt wird nichts.
static void prv_training(int minuten) {
  activity_algorithm_enable_activity_tracking(false);
  prv_gleichmaessig(150, minuten);
  activity_algorithm_enable_activity_tracking(true);
}

// DIE BEWEGUNG VOR DEM TRAINING ENDET MIT SEINEM START: zwoelf Minuten Gehen, eine
// Stunde Training, danach Ruhe. Es bleibt eine abgeschlossene Bewegung von zwoelf
// Minuten. Bis b23 blieb sie offen (gefragt wurde mit Platz 0), Kraepelin nahm sie
// nach dem Training mit der alten Startzeit wieder auf: 72 Minuten, 1200 Schritte.
// Mit dem Filter auf Movement waere sie dann ganz weg gewesen - und abgeschlossen
// haette das Loeschen die Uhr angehalten (PBL_ASSERT).
void test_boulder_gehfilter__training_schliesst_die_bewegung_ab(void) {
  prv_gleichmaessig(100, 12);
  cl_assert_equal_i(prv_bewegungen(), 1);
  cl_assert(s_sitzungen[0].ongoing);

  activity_algorithm_enable_activity_tracking(false);
  // Abgeschlossen mit dem Start, nicht erst nach dem Training.
  cl_assert(!s_sitzungen[0].ongoing);
  prv_gleichmaessig(150, 60);
  activity_algorithm_enable_activity_tracking(true);
  prv_ruhe();

  cl_assert_equal_i(prv_bewegungen(), 1);
  cl_assert(!s_sitzungen[0].ongoing);
  cl_assert_equal_i(s_sitzungen[0].length_min, 12);
  cl_assert_equal_i(s_sitzungen[0].step_data.steps, 1200);
  cl_assert_equal_i(s_abgeschlossen_gefragt, 0);
}

// NACH DEM TRAINING BEGINNT EINE NEUE BEWEGUNG: zwoelf Minuten Gehen, eine Stunde
// Training, eine Viertelstunde Gehen. Zwei Bewegungen, die zweite beginnt genau
// mit dem Ende des Trainings. Bis b23 wurde daraus eine von 87 Minuten - mit dem
// Filter auf Movement waeren beide weg gewesen (2700 Schritte in 87 Minuten).
void test_boulder_gehfilter__nach_dem_training_beginnt_eine_neue_bewegung(void) {
  prv_gleichmaessig(100, 12);
  prv_training(60);
  prv_gleichmaessig(100, 15);
  prv_ruhe();

  cl_assert_equal_i(prv_bewegungen(), 2);
  cl_assert(!s_sitzungen[0].ongoing);
  cl_assert_equal_i(s_sitzungen[0].length_min, 12);
  cl_assert_equal_i(s_sitzungen[0].step_data.steps, 1200);
  cl_assert(!s_sitzungen[1].ongoing);
  cl_assert_equal_i(s_sitzungen[1].length_min, 15);
  cl_assert_equal_i(s_sitzungen[1].step_data.steps, 1500);
  cl_assert_equal_i(s_sitzungen[1].start_utc - s_sitzungen[0].start_utc,
                    (12 + 60) * SECONDS_PER_MINUTE);
  cl_assert_equal_i(s_abgeschlossen_gefragt, 0);
}

// EINE MINUTE GENAU BEIM ABSCHLIESSEN oeffnet die Bewegung nicht wieder. Der
// Minutentakt laeuft auf KernelBG, der Trainingsstart in einer anderen Aufgabe -
// die Minute kann zwischen zwei Schritte fallen. Erst wird die Erkennung angehalten,
// dann abgeschlossen; andersherum meldete die Minute die Bewegung wieder als laufend,
// und sie ginge nie ans Telefon.
void test_boulder_gehfilter__eine_minute_beim_abschliessen_oeffnet_die_bewegung_nicht_wieder(
    void) {
  prv_gleichmaessig(100, 12);
  s_minute_beim_abschliessen = true;
  activity_algorithm_enable_activity_tracking(false);
  // Die Minute kam wirklich (sonst prueft dieser Test nichts).
  cl_assert(!s_minute_beim_abschliessen);
  cl_assert_equal_i(prv_bewegungen(), 1);
  cl_assert(!s_sitzungen[0].ongoing);
  cl_assert_equal_i(s_sitzungen[0].length_min, 12);

  prv_gleichmaessig(150, 60);
  activity_algorithm_enable_activity_tracking(true);
  prv_ruhe();
  cl_assert_equal_i(prv_bewegungen(), 1);
  cl_assert(!s_sitzungen[0].ongoing);
  cl_assert_equal_i(s_sitzungen[0].length_min, 12);
}

// EINE SCHON ABGESCHLOSSENE BEWEGUNG HAELT DIE UHR NICHT AN. So schliesst der
// Workout-Dienst der Vorlage ab: erst die Sitzungen, dann haelt er die Erkennung
// an (workout_service.c). Heute fragt er mit Platz 0 und schliesst nichts ab;
// nach einer Berichtigung dort kann eine Minute dazwischen fallen. Sinkt die
// Bewegung in ihr unter die Schwelle, will der Filter sie zuruecknehmen - das
// alte Loeschen hielte die Uhr an (PBL_ASSERT). Sie bleibt stehen, wie sie war.
void test_boulder_gehfilter__abgeschlossene_bewegung_haelt_die_uhr_nicht_an(void) {
  prv_gleichmaessig(SCHWELLE_JE_MINUTE, 10);
  cl_assert_equal_i(prv_bewegungen(), 1);
  // Abschliessen wie workout_service.c, bevor die Erkennung anhaelt.
  ActivitySession fertig = s_sitzungen[0];
  fertig.ongoing = false;
  activity_sessions_prv_add_activity_session(&fertig);
  // Die Minuten dazwischen: 39, dann 40 - 479 Schritte in 12 Minuten.
  prv_minute(SCHWELLE_JE_MINUTE - 1);
  prv_minute(SCHWELLE_JE_MINUTE);
  // Der Filter hat die abgeschlossene wirklich getroffen (sonst prueft das nichts).
  cl_assert(s_abgeschlossen_gefragt > 0);
  activity_algorithm_enable_activity_tracking(false);

  cl_assert_equal_i(prv_bewegungen(), 1);
  cl_assert(!s_sitzungen[0].ongoing);
  cl_assert_equal_i(s_sitzungen[0].length_min, 10);
  cl_assert_equal_i(s_sitzungen[0].step_data.steps, 400);
}

// EINE LANGE WANDERUNG UEBER 65535 SCHRITTE BLEIBT: acht Stunden ohne Pause zu
// 140 Schritten, 67200 Schritte. Kraepelin summierte in 16 Bit, daraus wurden 1664 -
// unter 40 je Minute, der Filter haette die ganze Wanderung verworfen. Die Sitzung
// selbst zaehlt in 16 Bit: sie bekommt 65535, nicht 1664.
void test_boulder_gehfilter__lange_wanderung_ueber_65535_schritte_bleibt(void) {
  prv_gleichmaessig(140, 480);
  prv_ruhe();
  cl_assert_equal_i(prv_bewegungen(), 1);
  cl_assert(!s_sitzungen[0].ongoing);
  cl_assert_equal_i(s_sitzungen[0].length_min, 480);
  cl_assert_equal_i(s_sitzungen[0].step_data.steps, 65535);
}

// --------------------------------------------------------------------------------------------
// TRAINING AM MORGEN, DIE NACHT LAEUFT NOCH. Aufstehen um 07:00, zwoelf Minuten gehen,
// dann ein Lauf (Kieselsport haelt die Erkennung an: plattform.c, ks_bewegung_erkennen).
// Die Nacht ist da noch nicht fertig - fertig ist sie erst nach einer Viertelstunde wach
// in der Minutendatei, um 07:15; abgeschlossen sind ihre Sitzungen erst, wenn sie
// endgueltig ist (zwei Stunden wach, 09:00). Bis dahin stehen sie als laufend da, und
// nacht.c nimmt sie bei jeder Rechnung zurueck, um sie neu zu melden.
// Schloss der Trainingsstart auch sie ab, hielt das Loeschen die Uhr an (PBL_ASSERT):
// Neustart mitten im Lauf. So stand es nach der Berichtigung vom 3.10. kurz im Code.
//
// Jede Minute wie in activity.c: erst Kraepelin, dann die Nacht. Die Uhr rechnet hier
// in UTC (keine Zeitzone gesetzt); das Nachtfenster ist 20:00 bis 11:00 UTC.
static void prv_uhr_minute(uint16_t schritte) {
  prv_minute(schritte);
  boulder_nacht_minute(rtc_get_time());
}

static int prv_laufend(ActivitySessionType art) {
  int n = 0;
  for (uint16_t i = 0; i < s_anzahl; i++) {
    n += (s_sitzungen[i].type == art && s_sitzungen[i].ongoing) ? 1 : 0;
  }
  return n;
}

static const ActivitySession *prv_bewegung(void) {
  for (uint16_t i = 0; i < s_anzahl; i++) {
    if (s_sitzungen[i].type == ActivitySessionType_Movement) {
      return &s_sitzungen[i];
    }
  }
  return NULL;
}

void test_boulder_gehfilter__training_am_morgen_laesst_die_nacht_laufen(void) {
  prv_lade_nacht();
  time_t ende;
  boulder_nacht_fenster(rtc_get_time() + 4 * SECONDS_PER_HOUR, &s_nacht_beginn, &ende);
  const time_t aufstehen = s_nacht_beginn + AUFSTEHEN_MIN * SECONDS_PER_MINUTE;
  while (rtc_get_time() < aufstehen) {
    prv_uhr_minute(0);
  }
  for (int i = 0; i < 12; i++) {
    prv_uhr_minute(100);
  }

  // 07:12: die Nacht laeuft noch, mit Schlaf und Tiefschlaf (sonst prueft das nichts),
  // und die Bewegung seit 07:00 auch.
  NachtBild bild;
  cl_assert(boulder_nacht_letzte(&bild));
  cl_assert(!bild.fertig);
  const int schlaf_vorher = prv_laufend(ActivitySessionType_Sleep);
  const int tief_vorher = prv_laufend(ActivitySessionType_RestfulSleep);
  cl_assert(schlaf_vorher > 0);
  cl_assert(tief_vorher > 0);
  cl_assert(prv_bewegung() && prv_bewegung()->ongoing);

  // Der Lauf beginnt: die Bewegung ist abgeschlossen, die Nacht laeuft weiter.
  activity_algorithm_enable_activity_tracking(false);
  cl_assert(!prv_bewegung()->ongoing);
  cl_assert_equal_i(prv_laufend(ActivitySessionType_Sleep), schlaf_vorher);
  cl_assert_equal_i(prv_laufend(ActivitySessionType_RestfulSleep), tief_vorher);

  // Der Lauf. Um 07:15 ist die Nacht fertig (eine Viertelstunde wach) und geht ans
  // Telefon - ihre Sitzungen laufen weiter; jede Rechnung ersetzt sie.
  for (int i = 0; i < 2; i++) {
    prv_uhr_minute(150);
  }
  cl_assert(boulder_nacht_letzte(&bild));
  cl_assert(!bild.fertig);
  prv_uhr_minute(150);
  cl_assert(boulder_nacht_letzte(&bild));
  cl_assert(bild.fertig);
  for (int i = 3; i < 60; i++) {
    prv_uhr_minute(150);
  }
  activity_algorithm_enable_activity_tracking(true);
  for (int i = 0; i < 15; i++) {
    prv_uhr_minute(0);
  }
  // 08:27: die Nacht ist fertig und ganz, eingeschlafen 22:17, aufgewacht 07:00 (wie in
  // test_boulder_nacht) - EIN Schlafblock, noch laufend, seine Schlafminuten die der Nacht.
  cl_assert(boulder_nacht_letzte(&bild));
  cl_assert(bild.fertig);
  cl_assert_equal_i(bild.einschlafen, 120 + 17);
  cl_assert_equal_i(bild.aufwachen, AUFSTEHEN_MIN);
  cl_assert_equal_i(prv_laufend(ActivitySessionType_Sleep), 1);

  // Bis 09:01 still: die Nacht ist endgueltig, nichts laeuft mehr.
  while (rtc_get_time() <= aufstehen + 121 * SECONDS_PER_MINUTE) {
    prv_uhr_minute(0);
  }
  int bloecke = 0;
  for (uint16_t i = 0; i < s_anzahl; i++) {
    cl_assert(!s_sitzungen[i].ongoing);
    if (s_sitzungen[i].type == ActivitySessionType_Sleep) {
      bloecke++;
      cl_assert_equal_i(s_sitzungen[i].length_min - s_sitzungen[i].nicht_schlaf_min,
                        bild.schlaf_minuten);
    }
  }
  cl_assert_equal_i(bloecke, 1);
  // Und die Bewegung vor dem Lauf: zwoelf Minuten, 1200 Schritte, eine einzige.
  cl_assert_equal_i(prv_bewegungen(), 1);
  cl_assert_equal_i(prv_bewegung()->length_min, 12);
  cl_assert_equal_i(prv_bewegung()->step_data.steps, 1200);
  cl_assert_equal_i(s_abgeschlossen_gefragt, 0);
}
