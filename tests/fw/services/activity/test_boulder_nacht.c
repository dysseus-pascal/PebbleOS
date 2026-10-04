/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "applib/data_logging.h"
#include "kernel/events.h"
#include "kernel/kernel_heap.h"
#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/activity_algorithm.h"
#include "pbl/services/activity/activity_private.h"
#include "pbl/services/activity/boulder/nacht.h"
#include "pbl/services/activity/boulder/schlafanalyse.h"
#include "pbl/services/data_logging/data_logging_service.h"
#include "pbl/services/battery/battery_state.h"
#include "pbl/services/kieselsport/kieselsport.h"
#include "pbl/services/settings/settings_file.h"
#include "pbl/util/heap.h"

#include "stubs_hexdump.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"

#include "fake_new_timer.h"
#include "fake_settings_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Die Nachtrechnung der Boulder-Firmware im Takt der Uhr: jede Minute ein
// Aufruf, wie in activity.c - erst die Sitzungen und der Schlafzustand der Uhr
// (die ECHTE activity_sessions.c), dann die Nacht. Am Morgen muss genau eine
// fertige Nacht dastehen - als ein Schlafblock und als Satz fuer Boulder.
//
// DIE MINUTENDATEI LIEFERT, WAS DIE UHR LIEFERT: alle Minuten bis zur eben
// vergangenen (activity_algorithm_get_minute_history liest den RAM-Ring mit).
// Frueher hielt die Nachbildung hier 19 Minuten zurueck - so sah der Test nie,
// dass der Schlafblock zwischen zwei Rechnungen hinter der Zeit zurueckblieb.
//
// DIE NACHT IST ECHT (26./27.9.2026, 22:00 bis 06:59). Davor, ab Beginn des
// Fensters um 20 Uhr, fehlen Daten; danach ist man wach und geht umher.

#define OFFSET_LOKAL (2 * 60 * 60)   // Sommerzeit in der Schweiz
#define MINUTEN (16 * 60)
// Die Schlaferkennung der Uhr ist so weit hinter der laufenden Minute
// (kraepelin_algorithm.c, KALG_SLEEP_HALF_WIDTH).
#define SCHLAF_VERZUG_MIN 4

static time_t s_beginn;              // 20:00 Ortszeit, Minute 0
static time_t s_jetzt;
static HealthMinuteData s_minuten[MINUTEN];

// --- Was die Uhr sonst liefert -------------------------------------------------

// HRV-Fenster kommen nur, wo ein Test sie anlegt: sonst wird die Nacht wie in
// der Firmware ohne sie gerechnet, wenn der Sensor keine liefert. Wie die
// echte: Fenster, die ab `von` beginnen und deren Mitte vor `bis` liegt, als
// Minuten ab `von`.
#define HRV_FENSTER 80
static time_t s_hrv_start[HRV_FENSTER];
static int s_hrv_anzahl;
int boulder_hrv_fenster(time_t von, time_t bis, SchlafHrv *aus, int max) {
  int n = 0;
  for (int k = 0; k < s_hrv_anzahl && n < max; k++) {
    if (s_hrv_start[k] < von || s_hrv_start[k] + 2 * 60 >= bis) {
      continue;
    }
    aus[n++] = (SchlafHrv){
      .minute = (s_hrv_start[k] - von) / SECONDS_PER_MINUTE,
      .rmssd = 40 + k % 20,
      .puls = 50 + k % 10,
    };
  }
  return n;
}

// Ein Kieselsport-Training laeuft in [s_training_von, s_training_bis), ab
// s_pause_ab pausiert.
static time_t s_training_von;
static time_t s_training_bis;
static time_t s_pause_ab;

// Der Akku faellt im Test um ein Prozent je Stunde, ab 80 um 20 Uhr.
BatteryChargeState battery_get_charge_state(void) {
  return (BatteryChargeState){ .charge_percent = 80 - (s_jetzt - s_beginn) / 3600 };
}
// Die Messstufe: 3, ab s_messstufe_5_ab 5.
static time_t s_messstufe_5_ab;
uint8_t boulder_messstufe(void) {
  return (s_jetzt >= s_messstufe_5_ab) ? 5 : 3;
}

// Ab s_umstellung gilt Winterzeit (eine Stunde weniger).
static time_t s_umstellung = 0x7FFFFFFF;
time_t time_utc_to_local(time_t utc) {
  return utc + (utc < s_umstellung ? OFFSET_LOKAL : OFFSET_LOKAL - 3600);
}
time_t time_util_get_midnight_of(time_t ts) {
  return ts - (time_utc_to_local(ts) % SECONDS_PER_DAY);
}
int time_util_get_minute_of_day(time_t utc_sec) {
  return (time_utc_to_local(utc_sec) % SECONDS_PER_DAY) / SECONDS_PER_MINUTE;
}

// Wie oft die Minutendatei gelesen wird - je Griff ein Durchgang durch die
// ganze Datei auf der Uhr.
static int s_griffe;
bool activity_algorithm_get_minute_history(HealthMinuteData *minute_data, uint32_t *num_records,
                                           time_t *utc_start) {
  s_griffe++;
  memset(minute_data, 0xFF, *num_records * sizeof(HealthMinuteData));
  int32_t anzahl = (s_jetzt - *utc_start) / SECONDS_PER_MINUTE;
  if (anzahl < 0) {
    anzahl = 0;
  }
  if ((uint32_t)anzahl > *num_records) {
    anzahl = *num_records;
  }
  for (int32_t k = 0; k < anzahl; k++) {
    const int32_t i = (*utc_start - s_beginn) / SECONDS_PER_MINUTE + k;
    if (i >= 0 && i < MINUTEN) {
      minute_data[k] = s_minuten[i];
    }
  }
  *num_records = anzahl;
  return true;
}

// Die Schlaferkennung der Uhr hinkt der Minute nach - die echte Grenze, an der
// activity_sessions.c wach von schlafend trennt.
time_t activity_algorithm_get_last_sleep_utc(void) {
  return s_jetzt - SCHLAF_VERZUG_MIN * SECONDS_PER_MINUTE;
}
void activity_algorithm_post_process_sleep_sessions(uint16_t num_input_sessions,
                                                    ActivitySession *sessions) {
}

// Der Sitzungsspeicher der Uhr ist der echte; sein Zustand liegt hier.
static ActivityState s_state;
ActivityState *activity_private_state(void) {
  return &s_state;
}
SettingsFile *activity_private_settings_open(void) {
  return NULL;
}
void activity_private_settings_close(SettingsFile *file) {
}
void event_put(PebbleEvent *e) {
}

// --- Datalogging ------------------------------------------------------------------

#define SAETZE 16
static NachtDlsRecord s_saetze[SAETZE];
static int s_saetze_anzahl;
static time_t s_gesendet_um;
// Zu wie vielen Zeitpunkten Saetze geschrieben wurden - eine Nacht aus
// mehreren Teilen zaehlt einmal.
static int s_schreibzeiten;

// Die HRV-Fenster der Nacht, auf Tag 89.
static NachtHrvDlsRecord s_hrv_saetze[SAETZE];
static int s_hrv_saetze_anzahl;
static time_t s_hrv_gesendet_um;

// Die Sitzungen auf Tag 84 (activity_sessions.c, prv_log_activities).
#define SITZUNGSSAETZE 64
static ActivitySessionDataLoggingRecord s_tag84[SITZUNGSSAETZE];
static int s_tag84_anzahl;

#define SITZUNG_NACHT ((DataLoggingSession *)1)
#define SITZUNG_HRV ((DataLoggingSession *)2)
#define SITZUNG_84 ((DataLoggingSession *)3)

// Fehler des Dataloggings, wie sie vorkommen: so viele Anlagen scheitern noch,
// und so viele Saetze der Nacht kommen mit diesem Ergebnis zurueck.
static int s_anlage_scheitert;
static int s_nacht_scheitert;
static DataLoggingResult s_nacht_fehler;
static int s_anlagen_nacht;

// Was in einem Minutenlauf geschieht, in der Reihenfolge: W = Waechter
// gefuettert, L = Minutendatei gelesen, A/H = Sitzung fuer Tag 88/89 angelegt.
static char s_ablauf[64];
static int s_ablauf_laenge;
static void prv_ablauf(char c) {
  if (s_ablauf_laenge < (int)sizeof(s_ablauf) - 1) {
    s_ablauf[s_ablauf_laenge++] = c;
  }
}
void system_task_watchdog_feed(void) {
  prv_ablauf('W');
}

DataLoggingSession *dls_create(uint32_t tag, DataLoggingItemType item_type, uint16_t item_size,
                               bool buffered, bool resume, const Uuid *uuid) {
  if (tag == DlsSystemTagActivitySession) {
    cl_assert_equal_i(item_size, sizeof(ActivitySessionDataLoggingRecord));
    return SITZUNG_84;
  }
  if (s_anlage_scheitert > 0) {
    s_anlage_scheitert--;
    return NULL;
  }
  if (tag == DlsSystemTagBoulderNachtHrv) {
    cl_assert_equal_i(item_size, sizeof(NachtHrvDlsRecord));
    prv_ablauf('H');
    return SITZUNG_HRV;
  }
  cl_assert_equal_i(tag, DlsSystemTagBoulderNacht);
  cl_assert_equal_i(item_size, sizeof(NachtDlsRecord));
  prv_ablauf('A');
  s_anlagen_nacht++;
  return SITZUNG_NACHT;
}

DataLoggingResult dls_log(DataLoggingSession *session, const void *data, uint32_t num_items) {
  if (session == SITZUNG_84) {
    cl_assert(s_tag84_anzahl < SITZUNGSSAETZE);
    memcpy(&s_tag84[s_tag84_anzahl++], data, sizeof(ActivitySessionDataLoggingRecord));
    return DATA_LOGGING_SUCCESS;
  }
  if (session == SITZUNG_HRV) {
    cl_assert(s_hrv_saetze_anzahl < SAETZE);
    memcpy(&s_hrv_saetze[s_hrv_saetze_anzahl++], data, sizeof(NachtHrvDlsRecord));
    s_hrv_gesendet_um = s_jetzt;
    return DATA_LOGGING_SUCCESS;
  }
  cl_assert(session == SITZUNG_NACHT);
  if (s_nacht_scheitert > 0) {
    s_nacht_scheitert--;
    return s_nacht_fehler;
  }
  cl_assert(s_saetze_anzahl < SAETZE);
  memcpy(&s_saetze[s_saetze_anzahl++], data, sizeof(NachtDlsRecord));
  if (s_schreibzeiten == 0 || s_gesendet_um != s_jetzt) {
    s_schreibzeiten++;
  }
  s_gesendet_um = s_jetzt;
  return DATA_LOGGING_SUCCESS;
}

// Der Schub ans Telefon, nach dem Senden der Nacht.
static int s_geschickt;
static time_t s_geschickt_um;
void health_sync_schicken(void) {
  s_geschickt++;
  s_geschickt_um = s_jetzt;
}

void kieselsport_stand(KsStand *aus) {
  memset(aus, 0, sizeof(*aus));
  if (s_jetzt >= s_training_von && s_jetzt < s_training_bis) {
    aus->zustand = (s_pause_ab && s_jetzt >= s_pause_ab) ? LaufPause : LaufLaeuft;
  }
}

// --- Speicher ----------------------------------------------------------------------

// So viel Kernel-Heap sieht die Nacht frei - und am Stueck.
static unsigned int s_heap_frei;
static unsigned int s_heap_am_stueck;
static Heap s_kernel_heap;
Heap *kernel_heap_get(void) {
  return &s_kernel_heap;
}
void heap_calc_totals(Heap *const heap, unsigned int *used, unsigned int *free_bytes,
                      unsigned int *max_free) {
  *used = 0;
  *free_bytes = s_heap_frei;
  *max_free = s_heap_am_stueck;
}

// Wie oft das Bild der Nacht in die Datei kommt.
static int s_bild_geschrieben;
status_t __real_settings_file_set(SettingsFile *file, const void *key, size_t key_len,
                                  const void *val, size_t val_len);
status_t __wrap_settings_file_set(SettingsFile *file, const void *key, size_t key_len,
                                  const void *val, size_t val_len) {
  if (key_len == sizeof("letzte") && !memcmp(key, "letzte", key_len)) {
    s_bild_geschrieben++;
  }
  return __real_settings_file_set(file, key, key_len, val, val_len);
}

// --- Die Uhr, Minute fuer Minute ------------------------------------------------

// Der Zeitgeber fuer den Schub: er laeuft, sobald seine Zeit um ist.
static int s_starts_gesehen;
static time_t s_zeitgeber_faellig;

static void prv_zeitgeber_faellig(void) {
  const TimerID t = stub_new_timer_get_next();
  if (t != TIMER_INVALID_ID && s_jetzt >= s_zeitgeber_faellig) {
    stub_new_timer_fire(t);
  }
}

static void prv_zeitgeber_gestartet(void) {
  if (s_num_new_timer_start_calls != s_starts_gesehen) {
    s_starts_gesehen = s_num_new_timer_start_calls;
    s_zeitgeber_faellig = s_jetzt + s_new_timer_start_param_timeout_ms / 1000;
  }
}

// Eine Minute wie in activity.c: erst die Sitzungen und der Schlafzustand,
// dann die Nacht. Der Zeitgeber laeuft dazwischen weiter.
static int s_griffe_max;
static void prv_minute(void) {
  prv_zeitgeber_faellig();
  activity_sessions_prv_minute_handler(s_jetzt);
  s_ablauf_laenge = 0;
  s_ablauf[0] = 0;
  const int griffe = s_griffe;
  boulder_nacht_minute(s_jetzt);
  s_ablauf[s_ablauf_laenge] = 0;
  prv_zeitgeber_gestartet();
  if (s_griffe - griffe > s_griffe_max) {
    s_griffe_max = s_griffe - griffe;
  }
}

// Minuten [von, bis] ab 20:00.
static void prv_durch(int von, int bis) {
  for (int m = von; m <= bis; m++) {
    s_jetzt = s_beginn + m * SECONDS_PER_MINUTE;
    prv_minute();
  }
}

// Jede Minute einen Tag lang, von 20 Uhr bis zum naechsten Mittag.
static void prv_durch_die_nacht(void) {
  prv_durch(0, MINUTEN);
}

// Ein Neustart: der Speicher der Nacht ist weg, die Datei bleibt; laufende
// Sitzungen verwirft die Uhr beim Laden (activity_sessions_prv_init).
static void prv_neustart(void) {
  boulder_nacht_test_reset();
  activity_sessions_prv_remove_out_of_range_activity_sessions(s_jetzt, true /*remove_ongoing*/);
}

// --- Die Nacht ------------------------------------------------------------------

static void prv_lade_nacht(void) {
  // Vor 22 Uhr: keine Daten (so liefert die Datei fehlende Minuten).
  memset(s_minuten, 0xFF, sizeof(s_minuten));
  char pfad[512];
  snprintf(pfad, sizeof(pfad), "%s/activity/boulder/nacht-2026-09-26.csv", CLAR_FIXTURE_PATH);
  FILE *f = fopen(pfad, "r");
  cl_assert(f);
  char zeile[256];
  int n = 0;
  while (fgets(zeile, sizeof(zeile), f)) {
    if (zeile[0] == '\n' || zeile[0] == '\r') {
      break;  // danach kommen die HRV-Fenster; die kennt die Firmware noch nicht
    }
    // Die Kopfzeilen mit '#' sagen, was in der Datei echt ist und was nicht.
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
      // Der Export traegt das Bewegungsbyte b; die Datei der Uhr das VMC = b*b/16.
      const int b = atoi(feld[2]);
      s_minuten[i] = (HealthMinuteData){
        .vmc = (uint16_t)((b * b + 8) / 16),
        .heart_rate_bpm = feld[4][0] ? atoi(feld[4]) : 0,
      };
    }
  }
  fclose(f);
  cl_assert_equal_i(n, 540);
  // Ab 07:00 wach und unterwegs.
  for (int i = 660; i < MINUTEN; i++) {
    s_minuten[i] = (HealthMinuteData){ .vmc = 3000, .steps = 60, .heart_rate_bpm = 82 };
  }
}

// Minuten [von, bis) ab 20:00: schlafen (ruhig, Puls 55 jede vierte Minute)
// oder wach (unterwegs, Puls 85).
static void prv_setze(int von, int bis, bool wach) {
  for (int i = von; i < bis && i < MINUTEN; i++) {
    s_minuten[i] = (HealthMinuteData){
      .vmc = wach ? 3000 : 2,
      .steps = wach ? 60 : 0,
      .heart_rate_bpm = wach ? 85 : (i % 4 == 0 ? 55 : 0),
    };
  }
}

// --- Was ueber die Sitzungen zu sagen ist ----------------------------------------

static bool prv_schlafart(ActivitySessionType t) {
  return t == ActivitySessionType_Sleep || t == ActivitySessionType_Nap;
}

// Ueberlappen sich zwei Sitzungen gleicher Klasse (Schlaf oder Tiefschlaf)?
static int prv_ueberlappungen(void) {
  int n = 0;
  for (int a = 0; a < s_state.activity_sessions_count; a++) {
    const ActivitySession *x = &s_state.activity_sessions[a];
    for (int b = a + 1; b < s_state.activity_sessions_count; b++) {
      const ActivitySession *y = &s_state.activity_sessions[b];
      if (prv_schlafart(x->type) != prv_schlafart(y->type) ||
          !activity_sessions_prv_is_sleep_activity(x->type) ||
          !activity_sessions_prv_is_sleep_activity(y->type)) {
        continue;
      }
      const time_t xe = x->start_utc + x->length_min * 60;
      const time_t ye = y->start_utc + y->length_min * 60;
      n += (x->start_utc < ye && y->start_utc < xe) ? 1 : 0;
    }
  }
  return n;
}

// Die Schlafminuten der Bloecke - ohne ihr Wachliegen.
static int prv_schlafsumme(time_t ab) {
  int summe = 0;
  for (int a = 0; a < s_state.activity_sessions_count; a++) {
    const ActivitySession *x = &s_state.activity_sessions[a];
    if (prv_schlafart(x->type) && x->start_utc >= ab) {
      summe += x->length_min - x->nicht_schlaf_min;
    }
  }
  return summe;
}

static int prv_anzahl(ActivitySessionType art) {
  int n = 0;
  for (int a = 0; a < s_state.activity_sessions_count; a++) {
    n += (s_state.activity_sessions[a].type == art) ? 1 : 0;
  }
  return n;
}

static const ActivitySession *prv_erste(ActivitySessionType art) {
  for (int a = 0; a < s_state.activity_sessions_count; a++) {
    if (s_state.activity_sessions[a].type == art) {
      return &s_state.activity_sessions[a];
    }
  }
  return NULL;
}

// Ueberlappen sich zwei Schlafsaetze auf Tag 84?
static int prv_tag84_ueberlappungen(void) {
  int n = 0;
  for (int a = 0; a < s_tag84_anzahl; a++) {
    for (int b = a + 1; b < s_tag84_anzahl; b++) {
      const ActivitySessionDataLoggingRecord *x = &s_tag84[a];
      const ActivitySessionDataLoggingRecord *y = &s_tag84[b];
      if (x->activity != ActivitySessionType_Sleep || y->activity != ActivitySessionType_Sleep) {
        continue;
      }
      n += (x->start_utc < y->start_utc + y->elapsed_sec &&
            y->start_utc < x->start_utc + x->elapsed_sec) ? 1 : 0;
    }
  }
  return n;
}

// Der letzte Satz der Nacht, die bei `beginn` anfing.
static const NachtDlsRecord *prv_letzter_satz(time_t beginn) {
  for (int k = s_saetze_anzahl - 1; k >= 0; k--) {
    if (s_saetze[k].beginn_utc == beginn && s_saetze[k].teil == 0) {
      return &s_saetze[k];
    }
  }
  return NULL;
}

void test_boulder_nacht__initialize(void) {
  // 26.9.2026, 20:00 Ortszeit = 18:00 UTC
  s_beginn = 1790445600;
  s_jetzt = s_beginn;
  s_umstellung = 0x7FFFFFFF;
  s_messstufe_5_ab = 0x7FFFFFFF;
  memset(&s_state, 0, sizeof(s_state));
  s_saetze_anzahl = 0;
  s_gesendet_um = 0;
  s_schreibzeiten = 0;
  s_geschickt = 0;
  s_geschickt_um = 0;
  s_hrv_anzahl = 0;
  s_hrv_saetze_anzahl = 0;
  s_hrv_gesendet_um = 0;
  s_tag84_anzahl = 0;
  s_training_von = 0;
  s_training_bis = 0;
  s_pause_ab = 0;
  s_anlage_scheitert = 0;
  s_nacht_scheitert = 0;
  s_anlagen_nacht = 0;
  s_heap_frei = 96 * 1024;
  s_heap_am_stueck = 64 * 1024;
  fake_malloc_set_largest_free_block(~0);
  s_bild_geschrieben = 0;
  s_griffe = 0;
  s_griffe_max = 0;
  stub_new_timer_cleanup();
  s_num_new_timer_start_calls = 0;
  s_starts_gesehen = 0;
  s_zeitgeber_faellig = 0;
  // Jeder Test beginnt wie eine frische Uhr: ohne Datei und ohne Zustand.
  fake_settings_file_reset();
  boulder_nacht_test_reset();
  prv_lade_nacht();
}

void test_boulder_nacht__eine_fertige_nacht_am_morgen(void) {
  prv_durch_die_nacht();

  // Genau eine Nacht - das Wachliegen um 23 Uhr zerlegt sie nicht, der Schlaf
  // davor ist kuerzer als eine Stunde. Fertig nach einer Viertelstunde wach:
  // 07:00 + 15 min = 07:15.
  cl_assert(s_saetze_anzahl >= 1);
  const NachtDlsRecord *s = &s_saetze[0];
  cl_assert_equal_i(s->teil, 0);
  cl_assert_equal_i(s_saetze_anzahl, s->teile);
  cl_assert_equal_i(s->beginn_utc, s_beginn);
  cl_assert_equal_i(s->utc_to_local, OFFSET_LOKAL);
  cl_assert_equal_i(s_gesendet_um, s_beginn + (11 * 60 + 15) * SECONDS_PER_MINUTE);
  // Der Schub ans Telefon kommt danach mit eigenem Zeitgeber, genau einmal je
  // Mal, dass die Nacht ging - nicht je Teil.
  cl_assert(s_geschickt >= 1);
  cl_assert_equal_i(s_geschickt, s_schreibzeiten);
  cl_assert(s_geschickt_um > s_gesendet_um);
  cl_assert(s_geschickt_um <= s_gesendet_um + 2 * SECONDS_PER_MINUTE);
  // Das Protokoll: eingeschlafen 22:17, der Takt danach 22:30 (78 %), fertig
  // 07:15 (69 %).
  cl_assert_equal_i(s->version, NACHT_DLS_VERSION);
  cl_assert_equal_i(s->akku_beginn, 78);
  cl_assert_equal_i(s->akku_ende, 69);
  cl_assert_equal_i(s->flags, 0);
  cl_assert_equal_i(s->messstufe, 3);
  // Ohne HRV-Fenster geht kein Satz auf Tag 89.
  cl_assert_equal_i(s_hrv_saetze_anzahl, 0);
  // Eingeschlafen 22:17, aufgewacht 07:00.
  cl_assert_equal_i(s->einschlafen, 120 + 17);
  cl_assert_equal_i(s->aufwachen, 660);

  // DIE SITZUNGEN: EIN Schlafblock vom Einschlafen bis zum Aufwachen,
  // abgeschlossen (um 09:00 war die Nacht endgueltig). Sein Wachliegen traegt
  // er mit; die Schlafsumme der Uhr ist die der Nacht. Jeder Tiefschlaf liegt
  // im Block.
  cl_assert_equal_i(prv_anzahl(ActivitySessionType_Sleep), 1);
  const ActivitySession *block = prv_erste(ActivitySessionType_Sleep);
  cl_assert_equal_i(block->start_utc, s_beginn + s->einschlafen * SECONDS_PER_MINUTE);
  cl_assert_equal_i(block->length_min, s->aufwachen - s->einschlafen);
  cl_assert_equal_i(block->length_min - block->nicht_schlaf_min, s->schlaf_minuten);
  cl_assert(block->nicht_schlaf_min > 0);
  cl_assert_equal_i(s_state.sleep_data.total_minutes, s->schlaf_minuten);
  int tief = 0;
  for (int k = 0; k < s_state.activity_sessions_count; k++) {
    const ActivitySession *a = &s_state.activity_sessions[k];
    cl_assert(!a->ongoing);
    if (a->type == ActivitySessionType_RestfulSleep) {
      tief += a->length_min - a->nicht_schlaf_min;
      cl_assert(a->start_utc > block->start_utc);
      cl_assert(a->start_utc + a->length_min * 60 <= block->start_utc + block->length_min * 60);
    }
  }
  cl_assert(tief > 0);
  cl_assert_equal_i(s_state.sleep_data.restful_minutes, tief);
  // Tag 84 bekommt genau diesen einen Block, nachdem die Nacht endgueltig war -
  // im bisherigen Format: das Wachliegen geht nicht mit (die Bytes bleiben 0).
  int tag84_schlaf = 0;
  for (int k = 0; k < s_tag84_anzahl; k++) {
    if (s_tag84[k].activity == ActivitySessionType_Sleep) {
      tag84_schlaf++;
      cl_assert_equal_i(s_tag84[k].start_utc, block->start_utc);
      cl_assert_equal_i(s_tag84[k].elapsed_sec, block->length_min * 60);
      cl_assert_equal_i(s_tag84[k].step_data.steps, 0);
    }
  }
  cl_assert_equal_i(tag84_schlaf, 1);

  // Zum Nachlesen im Testprotokoll: die Nacht, wie die Uhr sie sieht.
  int je_phase[5] = { 0 };
  for (int t = 0; t < s_saetze_anzahl; t++) {
    for (int k = 0; k < s_saetze[t].anzahl; k++) {
      je_phase[s_saetze[t].abschnitte[k].phase] +=
          s_saetze[t].abschnitte[k].bis - s_saetze[t].abschnitte[k].von;
    }
  }
  printf("\nNacht: Schlaf %d min (leicht %d, tief %d, REM %d), wach %d min, Ruhepuls %d, "
         "%d Sitzungen\n", s->schlaf_minuten, je_phase[SchlafPhaseLeicht],
         je_phase[SchlafPhaseTief], je_phase[SchlafPhaseRem], je_phase[SchlafPhaseWach],
         s->ruhepuls, s_state.activity_sessions_count);
  // Das Wachliegen im Block ist das Wach des Satzes.
  cl_assert_equal_i(block->nicht_schlaf_min, je_phase[SchlafPhaseWach]);

  // Die Abschnitte des Satzes: lueckenlos von Einschlafen bis Aufwachen.
  cl_assert_equal_i(s->abschnitte[0].von, s->einschlafen);
  int bis = s->einschlafen;
  for (int t = 0; t < s_saetze_anzahl; t++) {
    for (int k = 0; k < s_saetze[t].anzahl; k++) {
      cl_assert_equal_i(s_saetze[t].abschnitte[k].von, bis);
      bis = s_saetze[t].abschnitte[k].bis;
    }
  }
  cl_assert_equal_i(bis, s->aufwachen);

  // DIE UHR SELBST KENNT DIE NACHT (Health-App): dieselben Zahlen, die
  // Minuten je Phase wie im Satz, die Abschnitte lueckenlos und hoechstens
  // NACHT_BILD_ABSCHNITTE.
  NachtBild b;
  cl_assert(boulder_nacht_letzte(&b));
  cl_assert(b.fertig);
  cl_assert_equal_i(b.version, NACHT_BILD_VERSION);
  cl_assert_equal_i(b.beginn_utc, s->beginn_utc);
  cl_assert_equal_i(b.einschlafen, s->einschlafen);
  cl_assert_equal_i(b.aufwachen, s->aufwachen);
  cl_assert_equal_i(b.schlaf_minuten, s->schlaf_minuten);
  cl_assert_equal_i(b.ruhepuls, s->ruhepuls);
  for (int p = 0; p < NACHT_PHASEN; p++) {
    cl_assert_equal_i(b.minuten[p], je_phase[p]);
  }
  cl_assert(b.anzahl > 0 && b.anzahl <= NACHT_BILD_ABSCHNITTE);
  cl_assert_equal_i(b.abschnitte[0].von, b.einschlafen);
  for (int k = 1; k < b.anzahl; k++) {
    cl_assert_equal_i(b.abschnitte[k].von, b.abschnitte[k - 1].bis);
    cl_assert(b.abschnitte[k].phase != b.abschnitte[k - 1].phase);
  }
  cl_assert_equal_i(b.abschnitte[b.anzahl - 1].bis, b.aufwachen);
}

void test_boulder_nacht__ausserhalb_des_fensters_ruhe(void) {
  // Tagsueber (12 bis 19 Uhr) wird nichts gerechnet, auch wenn die Uhr still liegt.
  memset(s_minuten, 0, sizeof(s_minuten));
  for (s_jetzt = s_beginn - 8 * 60 * SECONDS_PER_MINUTE; s_jetzt < s_beginn - 60;
       s_jetzt += SECONDS_PER_MINUTE) {
    prv_minute();
  }
  cl_assert_equal_i(s_state.activity_sessions_count, 0);
  cl_assert_equal_i(s_saetze_anzahl, 0);
  cl_assert_equal_i(s_geschickt, 0);
}

void test_boulder_nacht__langes_wachliegen_bleibt_eine_nacht(void) {
  // 22:00 bis 01:00 Schlaf, 40 Minuten wach und unterwegs, dann Schlaf bis 06:30.
  // Nach einer Viertelstunde wach geht die Nacht vorlaeufig ans Telefon; kommt
  // der zweite Schlaf dazu, geht dieselbe Nacht - gleicher Beginn - noch einmal,
  // jetzt bis 06:30. Frueher wurden es zwei Naechte, und das Telefon zeigte nur
  // den Rest.
  memset(s_minuten, 0xFF, sizeof(s_minuten));
  prv_setze(120, MINUTEN, false);
  prv_setze(300, 340, true);
  prv_setze(630, MINUTEN, true);
  prv_durch_die_nacht();
  cl_assert(s_saetze_anzahl >= 2);
  const NachtDlsRecord *erste = &s_saetze[0];
  const NachtDlsRecord *letzte = &s_saetze[s_saetze_anzahl - 1];
  cl_assert_equal_i(erste->beginn_utc, s_beginn);
  cl_assert_equal_i(erste->aufwachen, 300);
  cl_assert(erste->schlaf_minuten >= 170);
  // Dieselbe Nacht, gewachsen: gleicher Beginn, Aufwachen 06:30 = Minute 630,
  // beide Schlafbloecke zusammen.
  cl_assert_equal_i(letzte->beginn_utc, s_beginn);
  cl_assert_equal_i(letzte->aufwachen, 630);
  cl_assert(letzte->schlaf_minuten >= 440);
  // AUF DER UHR EIN BLOCK, nicht zwei: das Wachliegen bleibt darin und zaehlt
  // nicht. Nichts laeuft mehr.
  cl_assert_equal_i(prv_anzahl(ActivitySessionType_Sleep), 1);
  const ActivitySession *block = prv_erste(ActivitySessionType_Sleep);
  cl_assert_equal_i(block->length_min, letzte->aufwachen - letzte->einschlafen);
  cl_assert(block->nicht_schlaf_min >= 40);
  cl_assert_equal_i(s_state.sleep_data.total_minutes, letzte->schlaf_minuten);
  for (int k = 0; k < s_state.activity_sessions_count; k++) {
    cl_assert(!s_state.activity_sessions[k].ongoing);
  }
}

void test_boulder_nacht__verdichten_laesst_die_langen_phasen_stehen(void) {
  // Leicht 60, Tief 2, Leicht 30, REM 40, Wach 1, REM 20. Auf vier verdichtet
  // verschwindet das Wachflackern, und die beiden REM werden einer.
  SchlafAbschnitt a[] = {
    { 0, 60, SchlafPhaseLeicht }, { 60, 62, SchlafPhaseTief }, { 62, 92, SchlafPhaseLeicht },
    { 92, 132, SchlafPhaseRem }, { 132, 133, SchlafPhaseWach }, { 133, 153, SchlafPhaseRem },
  };
  cl_assert_equal_i(schlafanalyse_verdichten(a, 6, 4), 4);
  cl_assert_equal_i(a[3].phase, SchlafPhaseRem);
  cl_assert_equal_i(a[3].von, 92);
  cl_assert_equal_i(a[3].bis, 153);
  // Auf drei: der kurze Tiefschlaf geht, und die beiden Leicht werden einer.
  const int n = schlafanalyse_verdichten(a, 4, 3);
  cl_assert_equal_i(n, 2);
  cl_assert_equal_i(a[0].phase, SchlafPhaseLeicht);
  cl_assert_equal_i(a[0].von, 0);
  cl_assert_equal_i(a[0].bis, 92);
  cl_assert_equal_i(a[1].phase, SchlafPhaseRem);
  cl_assert_equal_i(a[1].bis, 153);
}

void test_boulder_nacht__fenster_bleibt_in_der_umstellungsnacht(void) {
  // Ende der Sommerzeit: um 03:00 Ortszeit geht die Uhr eine Stunde zurueck.
  // Das Fenster, das um 20:00 begann, beginnt danach immer noch dort - sonst
  // rutschte der Beginn der laufenden Nacht mitten in der Rechnung.
  s_umstellung = s_beginn + 7 * 3600;
  time_t b1, e1, b2, e2;
  boulder_nacht_fenster(s_beginn + 2 * 3600, &b1, &e1);
  boulder_nacht_fenster(s_beginn + 8 * 3600, &b2, &e2);
  cl_assert_equal_i(b1, s_beginn);
  cl_assert_equal_i(b2, s_beginn);
  cl_assert_equal_i(e2, e1);
}

void test_boulder_nacht__neustart_schickt_die_nacht_nicht_noch_einmal(void) {
  // Die Nacht am Morgen einmal durchlaufen, dann "Neustart": alle Zustaende
  // weg, nur die Datei bleibt. Dieselben Minuten noch einmal - die Nacht geht
  // nicht ein zweites Mal ans Telefon.
  prv_durch_die_nacht();
  const int vorher = s_saetze_anzahl;
  const int geschickt_vorher = s_geschickt;
  cl_assert(vorher >= 1);
  cl_assert(geschickt_vorher >= 1);
  prv_neustart();
  prv_durch_die_nacht();
  cl_assert_equal_i(s_saetze_anzahl, vorher);
  // Nichts Neues, also auch nichts zu schicken.
  cl_assert_equal_i(s_geschickt, geschickt_vorher);
  cl_assert_equal_i(s_geschickt, s_schreibzeiten);
  cl_assert_equal_i(prv_ueberlappungen(), 0);
}

// ---------------------------------------------------------------------------
// Firmware-Etappe 3.10.2026

void test_boulder_nacht__hrv_fenster_gehen_mit_der_nacht(void) {
  // Die echte Nacht, dazu ein HRV-Fenster alle zehn Minuten von 22:00 bis
  // 07:00 - mehr, als in einen Satz passen. Sie gehen im selben Augenblick wie die Nacht,
  // mit demselben Beginn, in zwei Teilen.
  for (int k = 0; k < 55; k++) {
    s_hrv_start[k] = s_beginn + (120 + 10 * k) * SECONDS_PER_MINUTE;
  }
  s_hrv_anzahl = 55;
  prv_durch_die_nacht();
  cl_assert(s_saetze_anzahl >= 1);
  cl_assert_equal_i(s_hrv_saetze_anzahl, 2);
  cl_assert_equal_i(s_hrv_gesendet_um, s_gesendet_um);
  const NachtHrvDlsRecord *a = &s_hrv_saetze[0];
  const NachtHrvDlsRecord *b = &s_hrv_saetze[1];
  cl_assert_equal_i(a->version, NACHT_HRV_DLS_VERSION);
  cl_assert_equal_i(a->size, sizeof(NachtHrvDlsRecord));
  cl_assert_equal_i(a->beginn_utc, s_saetze[0].beginn_utc);
  cl_assert_equal_i(a->teil, 0);
  cl_assert_equal_i(a->teile, 2);
  cl_assert_equal_i(a->anzahl, NACHT_HRV_DLS_FENSTER);
  cl_assert_equal_i(b->teil, 1);
  cl_assert_equal_i(b->anzahl, 55 - NACHT_HRV_DLS_FENSTER);
  // Minute ab dem Beginn der Nacht, RMSSD und Puls wie gemessen.
  cl_assert_equal_i(a->fenster[0].minute, 120);
  cl_assert_equal_i(a->fenster[0].rmssd, 40);
  cl_assert_equal_i(a->fenster[0].puls, 50);
  cl_assert_equal_i(b->fenster[0].minute, 120 + 10 * NACHT_HRV_DLS_FENSTER);
  cl_assert_equal_i(b->fenster[0].rmssd, 40 + NACHT_HRV_DLS_FENSTER % 20);
}

void test_boulder_nacht__training_ist_kein_schlaf(void) {
  // Yoga von 21:00 bis 22:00, ganz still - danach gleich Schlaf bis 06:30.
  // Ohne das Training begaenne die Nacht schon um 21:00.
  memset(s_minuten, 0xFF, sizeof(s_minuten));
  prv_setze(60, 630, false);
  prv_setze(630, MINUTEN, true);
  s_training_von = s_beginn + 60 * SECONDS_PER_MINUTE;
  s_training_bis = s_beginn + 120 * SECONDS_PER_MINUTE;
  prv_durch_die_nacht();
  cl_assert(s_saetze_anzahl >= 1);
  const NachtDlsRecord *s = &s_saetze[s_saetze_anzahl - 1];
  cl_assert(s->einschlafen >= 120);
  cl_assert_equal_i(s->aufwachen, 630);
}

void test_boulder_nacht__kurze_pause_gehoert_zum_training(void) {
  // Wie eben, aber die letzten zwanzig Minuten des Yoga pausiert, still auf
  // der Matte: auch die Pause ist kein Schlaf.
  memset(s_minuten, 0xFF, sizeof(s_minuten));
  prv_setze(60, 630, false);
  prv_setze(630, MINUTEN, true);
  s_training_von = s_beginn + 60 * SECONDS_PER_MINUTE;
  s_training_bis = s_beginn + 120 * SECONDS_PER_MINUTE;
  s_pause_ab = s_beginn + 100 * SECONDS_PER_MINUTE;
  prv_durch_die_nacht();
  cl_assert(s_saetze_anzahl >= 1);
  cl_assert(s_saetze[s_saetze_anzahl - 1].einschlafen >= 120);
}

void test_boulder_nacht__vergessene_pause_loescht_die_nacht_nicht(void) {
  // Ein Abendspaziergang ab 20:00, um 20:15 pausiert und nie beendet; Schlaf
  // 22:00-06:30. Die Pause zaehlt nur eine halbe Stunde - die Nacht kommt,
  // eingeschlafen kurz nach 22:00 (Cole-Kripke und Webster haengen an das
  // Umhergehen ein paar Minuten an).
  memset(s_minuten, 0xFF, sizeof(s_minuten));
  prv_setze(0, 120, true);
  prv_setze(120, 630, false);
  prv_setze(630, MINUTEN, true);
  s_training_von = s_beginn;
  s_training_bis = s_beginn + 2 * SECONDS_PER_DAY;
  s_pause_ab = s_beginn + 15 * SECONDS_PER_MINUTE;
  prv_durch_die_nacht();
  cl_assert(s_saetze_anzahl >= 1);
  const NachtDlsRecord *s = &s_saetze[s_saetze_anzahl - 1];
  cl_assert(s->einschlafen >= 120 && s->einschlafen <= 130);
  cl_assert_equal_i(s->aufwachen, 630);
}

void test_boulder_nacht__vergessenes_training_zaehlt_hoechstens_drei_stunden(void) {
  // Ein Training ab 20:00, das laeuft und laeuft; Schlaf ab 22:00. Nach drei
  // Stunden ist es vergessen: die Nacht beginnt um 23:00, statt ganz zu fehlen.
  memset(s_minuten, 0xFF, sizeof(s_minuten));
  prv_setze(0, 120, true);
  prv_setze(120, 630, false);
  prv_setze(630, MINUTEN, true);
  s_training_von = s_beginn;
  s_training_bis = s_beginn + 2 * SECONDS_PER_DAY;
  prv_durch_die_nacht();
  cl_assert(s_saetze_anzahl >= 1);
  const NachtDlsRecord *s = &s_saetze[s_saetze_anzahl - 1];
  cl_assert(s->einschlafen >= 180 && s->einschlafen <= 185);
  cl_assert_equal_i(s->aufwachen, 630);
}

void test_boulder_nacht__morgenschlaf_ist_keine_eigene_nacht(void) {
  // Schlaf 22:00-06:00, zweieinhalb Stunden auf, dann 75 Minuten Schlaf ab
  // 08:30. Die Nacht ist nach zwei Stunden wach endgueltig; der Morgenschlaf
  // wird keine zweite Nacht - er ist kuerzer als anderthalb Stunden.
  memset(s_minuten, 0xFF, sizeof(s_minuten));
  prv_setze(120, 600, false);
  prv_setze(600, 750, true);
  prv_setze(750, 825, false);
  prv_setze(825, MINUTEN, true);
  prv_durch_die_nacht();
  cl_assert(s_saetze_anzahl >= 1);
  for (int k = 0; k < s_saetze_anzahl; k++) {
    cl_assert_equal_i(s_saetze[k].beginn_utc, s_beginn);
    cl_assert_equal_i(s_saetze[k].aufwachen, 600);
  }
  cl_assert_equal_i(prv_anzahl(ActivitySessionType_Sleep), 1);
}

void test_boulder_nacht__langer_morgenschlaf_bleibt_eine_nacht(void) {
  // Wie eben, aber zwei Stunden Schlaf ab 08:30: das ist kein Nickerchen
  // mehr, sondern eine eigene Nacht (nach dem Aufwachen der ersten).
  memset(s_minuten, 0xFF, sizeof(s_minuten));
  prv_setze(120, 600, false);
  prv_setze(600, 750, true);
  prv_setze(750, 870, false);
  prv_setze(870, MINUTEN, true);
  prv_durch_die_nacht();
  cl_assert(s_saetze_anzahl >= 2);
  const NachtDlsRecord *erste = &s_saetze[0];
  const NachtDlsRecord *zweite = &s_saetze[s_saetze_anzahl - 1];
  cl_assert_equal_i(erste->beginn_utc, s_beginn);
  cl_assert_equal_i(erste->aufwachen, 600);
  cl_assert_equal_i(zweite->beginn_utc, s_beginn + 600 * SECONDS_PER_MINUTE);
  cl_assert(zweite->schlaf_minuten >= 90);
  // Zwei Bloecke, nebeneinander, nicht uebereinander.
  cl_assert_equal_i(prv_anzahl(ActivitySessionType_Sleep), 2);
  cl_assert_equal_i(prv_ueberlappungen(), 0);
}

// ---------------------------------------------------------------------------
// Audit 4.10.2026

// F-S1: WAEHREND DES SCHLAFS SIEHT DIE UHR "SCHLAEFT". Der Schlafzustand
// (Smart Alarm, health_service_peek_current_activities) kommt aus dem Abstand
// zwischen dem Ende des Schlafblocks und der Schlaferkennung. Wuchs der Block
// nur alle 15 Minuten, galt man zwischen den Rechnungen meist als wach.
void test_boulder_nacht__schlafzustand_ist_nachts_schlafend(void) {
  int schlafend = 0;
  int gesamt = 0;
  int erstes_wach_nach_aufstehen = -1;
  for (int m = 0; m <= MINUTEN; m++) {
    s_jetzt = s_beginn + m * SECONDS_PER_MINUTE;
    prv_minute();
    // 01:00 bis 06:00 schlaeft man in dieser Nacht.
    if (m >= 300 && m < 600) {
      gesamt++;
      schlafend += (s_state.sleep_data.cur_state != ActivitySleepStateAwake) ? 1 : 0;
    }
    if (m > 660 && erstes_wach_nach_aufstehen < 0 &&
        s_state.sleep_data.cur_state == ActivitySleepStateAwake) {
      erstes_wach_nach_aufstehen = m;
    }
  }
  printf("\nSchlafzustand 01:00-06:00: %d von %d Minuten schlafend\n", schlafend, gesamt);
  cl_assert(schlafend * 100 >= gesamt * 99);
  // Nach dem Aufstehen um 07:00 ist man spaetestens mit der naechsten Rechnung
  // (07:15) wach.
  cl_assert(erstes_wach_nach_aufstehen > 660);
  cl_assert(erstes_wach_nach_aufstehen <= 660 + 16);
}

// Ein Morgen voller Zufall: unterwegs, still wach, doesen, Regungen.
static uint32_t s_zufall;
static uint32_t prv_zufall(void) {
  s_zufall = s_zufall * 1103515245u + 12345u;
  return (s_zufall >> 8) & 0xFFFFFF;
}

static void prv_morgen_zufall(void) {
  int i = 660;
  while (i < MINUTEN) {
    const int art = prv_zufall() % 4;
    const int laenge = 5 + prv_zufall() % 30;
    for (int k = 0; k < laenge && i < MINUTEN; k++, i++) {
      uint16_t vmc;
      uint8_t puls;
      switch (art) {
        case 0: vmc = 3000; puls = 85; break;
        case 1: vmc = (prv_zufall() % 3 == 0) ? 300 : 0; puls = 60 + prv_zufall() % 20; break;
        case 2: vmc = (prv_zufall() % 10 == 0) ? 260 : 0; puls = 48 + prv_zufall() % 10; break;
        default: vmc = (prv_zufall() % 2) ? 600 : 0; puls = 55 + prv_zufall() % 30; break;
      }
      s_minuten[i] = (HealthMinuteData){ .vmc = vmc, .steps = vmc > 1000 ? 60 : 0,
                                         .heart_rate_bpm = (i % 2 == 0) ? puls : 0 };
    }
  }
}

// Eine Nacht pruefen: keine zwei Schlafbloecke uebereinander, auf der Uhr wie
// auf Tag 84, und die Schlafminuten der Bloecke sind die der Nacht.
static void prv_pruefe_eine_nacht(void) {
  cl_assert_equal_i(prv_ueberlappungen(), 0);
  cl_assert_equal_i(prv_tag84_ueberlappungen(), 0);
  const NachtDlsRecord *s = prv_letzter_satz(s_beginn);
  cl_assert(s);
  const time_t ende = s_beginn + s->aufwachen * SECONDS_PER_MINUTE;
  int summe = 0;
  for (int a = 0; a < s_state.activity_sessions_count; a++) {
    const ActivitySession *x = &s_state.activity_sessions[a];
    if (prv_schlafart(x->type) && x->start_utc < ende) {
      summe += x->length_min - x->nicht_schlaf_min;
    }
  }
  cl_assert_equal_i(summe, s->schlaf_minuten);
}

// F-S2: KEIN DOPPELTER BLOCK, WENN MAN NACH DEM AUFWACHEN NOCH DOEST. Der
// feste Morgen der Nachpruefung: 07:00-07:25 still bei Puls 69-78, 07:25-08:15
// unterwegs, ab 08:15 still bei Puls 57-67. Frueher stand die um 07:15 fertige
// Nacht fest, die Neurechnung danach verschob die Bloecke, und beide blieben.
void test_boulder_nacht__doesen_am_morgen_ergibt_keinen_doppelten_block(void) {
  for (int i = 660; i < 685; i++) {
    s_minuten[i] = (HealthMinuteData){ .vmc = (i % 5) ? 0 : 200,
                                       .heart_rate_bpm = 69 + (i % 10) };
  }
  prv_setze(685, 735, true);
  for (int i = 735; i < MINUTEN; i++) {
    s_minuten[i] = (HealthMinuteData){ .vmc = (i % 9) ? 0 : 150,
                                       .heart_rate_bpm = 57 + (i % 11) };
  }
  prv_durch_die_nacht();
  prv_pruefe_eine_nacht();
  cl_assert_equal_i(prv_anzahl(ActivitySessionType_Sleep), 1);
}

void test_boulder_nacht__zufaellige_morgen_ergeben_keinen_doppelten_block(void) {
  for (uint32_t seed = 1; seed <= 40; seed++) {
    test_boulder_nacht__initialize();
    s_zufall = seed;
    prv_morgen_zufall();
    prv_durch_die_nacht();
    prv_pruefe_eine_nacht();
  }
}

// F-S3: EINE VON ANDERER SEITE ABGESCHLOSSENE SITZUNG HAELT DIE UHR NICHT AN.
// So schloesse workout_service.c beim Trainingsstart die laufenden Sitzungen
// ab, sobald dort num_sessions berichtigt ist - etwa um 07:30, die Nacht ist
// fertig, aber noch nicht endgueltig. Die naechste Rechnung loeschte sie -
// activity_sessions.c haelt dann an (PBL_ASSERT): Neustart.
void test_boulder_nacht__fremd_abgeschlossene_sitzung_haelt_die_uhr_nicht_an(void) {
  prv_durch(0, 11 * 60 + 30);
  ActivitySession *laufend = NULL;
  for (int k = 0; k < s_state.activity_sessions_count; k++) {
    if (s_state.activity_sessions[k].type == ActivitySessionType_Sleep &&
        s_state.activity_sessions[k].ongoing) {
      laufend = &s_state.activity_sessions[k];
    }
  }
  cl_assert(laufend);
  ActivitySession fertig = *laufend;
  fertig.ongoing = false;
  activity_sessions_prv_add_activity_session(&fertig);
  // Die naechsten Rechnungen bis zum Mittag: kein Halt. Der neue Block hat
  // denselben Beginn und ersetzt den abgeschlossenen - kein doppelter.
  prv_durch(11 * 60 + 31, MINUTEN);
  cl_assert(s_saetze_anzahl >= 1);
  cl_assert_equal_i(prv_anzahl(ActivitySessionType_Sleep), 1);
  cl_assert_equal_i(prv_ueberlappungen(), 0);
}

// SCHLIESST JEMAND ANDERS DIE LAUFENDE NACHT AB, UND BEGINNT SIE DANACH ANDERS,
// bleibt kein zweiter Block stehen. Wer abschliesst: workout_service_start_workout
// schliesst beim Trainingsstart jede laufende Sitzung ab (heute ohne Wirkung, es
// liest mit num_sessions = 0 keine). Hier mit denselben zwei Schritten wie dort.
// Wach am Ende der Rechnung schreibt die Nacht ihren Block nicht fort - nichts
// oeffnet ihn wieder. Der spaetere Beginn steht fuer alles, was das Einschlafen
// verschiebt: die erste Stunde Schlaf ist in der naechsten Rechnung wach.
void test_boulder_nacht__fremd_abgeschlossene_nacht_bleibt_nicht_doppelt(void) {
  // 07:20, wach seit 07:00: die Nacht ist fertig, aber noch nicht endgueltig -
  // ihre Sitzungen laufen. Jetzt beginnt ein Training.
  const int training = 11 * 60 + 20;
  prv_durch(0, training);
  const ActivitySession *block = prv_erste(ActivitySessionType_Sleep);
  cl_assert(block && block->ongoing);
  const time_t beginn_vorher = block->start_utc;
  int abgeschlossen = 0;
  for (int k = 0; k < s_state.activity_sessions_count; k++) {
    if (s_state.activity_sessions[k].ongoing) {
      ActivitySession s = s_state.activity_sessions[k];
      s.ongoing = false;
      activity_sessions_prv_add_activity_session(&s);
      abgeschlossen++;
    }
  }
  cl_assert(abgeschlossen >= 2);   // der Block und Tiefschlaf darin
  const int einschlafen = (beginn_vorher - s_beginn) / SECONDS_PER_MINUTE;
  prv_setze(einschlafen, einschlafen + 60, true);

  prv_durch(training + 1, MINUTEN);
  cl_assert_equal_i(prv_anzahl(ActivitySessionType_Sleep) + prv_anzahl(ActivitySessionType_Nap),
                    1);
  cl_assert_equal_i(prv_ueberlappungen(), 0);
  block = prv_erste(ActivitySessionType_Sleep);
  cl_assert(block && !block->ongoing);
  cl_assert(block->start_utc >= beginn_vorher + 60 * SECONDS_PER_MINUTE);
  // Kein Tiefschlaf aus der ersten Fassung der Nacht liegt noch vor ihr.
  for (int k = 0; k < s_state.activity_sessions_count; k++) {
    if (s_state.activity_sessions[k].type == ActivitySessionType_RestfulSleep) {
      cl_assert(s_state.activity_sessions[k].start_utc > block->start_utc);
    }
  }
  // Die Schlafsumme der Uhr ist die der Nacht, nicht die zweier.
  const NachtDlsRecord *s = prv_letzter_satz(s_beginn);
  cl_assert(s);
  cl_assert_equal_i(prv_schlafsumme(s_beginn), s->schlaf_minuten);
}

// M2: FEHLT DER SPEICHER FUER DIE RECHNUNG, kommt die Nacht eine Minute
// spaeter - nicht erst mit dem naechsten Takt. Und der Heap wird vorher
// gefragt: die Rechnung nimmt ihn sonst anderen weg, die ohne ihn neu starten.
void test_boulder_nacht__knapper_heap_verschiebt_die_nacht_um_eine_minute(void) {
  const int fertig = 11 * 60 + 15;
  prv_durch(0, fertig - 1);
  s_heap_frei = boulder_nacht_test_rechnung_groesse() + 1024;
  prv_durch(fertig, fertig);
  cl_assert_equal_i(s_saetze_anzahl, 0);
  s_heap_frei = 96 * 1024;
  prv_durch(fertig + 1, MINUTEN);
  cl_assert(s_saetze_anzahl >= 1);
  cl_assert_equal_i(s_saetze[0].beginn_utc, s_beginn);
  cl_assert_equal_i(s_gesendet_um, s_beginn + (fertig + 1) * SECONDS_PER_MINUTE);
}

void test_boulder_nacht__kein_speicher_verschiebt_die_nacht_um_eine_minute(void) {
  const int fertig = 11 * 60 + 15;
  prv_durch(0, fertig - 1);
  fake_malloc_set_largest_free_block(1024);
  prv_durch(fertig, fertig);
  fake_malloc_set_largest_free_block(~0);
  cl_assert_equal_i(s_saetze_anzahl, 0);
  prv_durch(fertig + 1, MINUTEN);
  cl_assert(s_saetze_anzahl >= 1);
  cl_assert_equal_i(s_gesendet_um, s_beginn + (fertig + 1) * SECONDS_PER_MINUTE);
}

// Auch das fruehe Nachsehen geht nicht verloren: aufgewacht um 06:53, der Takt
// um 07:00 sieht sieben Minuten wach und rechnet um 07:08 noch einmal - fehlt
// dann der Speicher, eben eine Minute spaeter, nicht erst um 07:15.
void test_boulder_nacht__nachsehen_ohne_speicher_geht_nicht_verloren(void) {
  memset(s_minuten, 0xFF, sizeof(s_minuten));
  prv_setze(120, 653, false);
  prv_setze(653, MINUTEN, true);
  prv_durch(0, 667);
  cl_assert_equal_i(s_saetze_anzahl, 0);
  fake_malloc_set_largest_free_block(1024);
  prv_durch(668, 668);
  fake_malloc_set_largest_free_block(~0);
  cl_assert_equal_i(s_saetze_anzahl, 0);
  prv_durch(669, MINUTEN);
  cl_assert(s_saetze_anzahl >= 1);
  cl_assert_equal_i(s_saetze[0].aufwachen, 653);
  cl_assert_equal_i(s_gesendet_um, s_beginn + 669 * SECONDS_PER_MINUTE);
}

// M3: JE RECHNUNG EIN GRIFF IN DIE MINUTENDATEI. Jeder Griff oeffnet sie und
// geht ihre Saetze vom Anfang an durch; frueher waren es bis zu 16 je Rechnung.
void test_boulder_nacht__eine_rechnung_liest_die_datei_einmal(void) {
  prv_durch_die_nacht();
  cl_assert(s_saetze_anzahl >= 1);
  cl_assert_equal_i(s_griffe_max, 1);
}

// M4: EIN NEUSTART AM MORGEN RECHNET DIE ENDGUELTIGE NACHT NICHT NEU. Schlaf
// 22-06 Uhr, wach bis 08:30, Schlaf bis 10:30 - zwei Naechte. Startet die Uhr
// um 10:40 neu, kannte sie frueher nicht, was schon erledigt war: sie rechnete
// ab 20 Uhr und schickte eine 590-Minuten-Nacht mit zweieinhalb Stunden Wach,
// neben dem schon festen Block der ersten.
void test_boulder_nacht__neustart_am_morgen_verschmilzt_die_naechte_nicht(void) {
  memset(s_minuten, 0xFF, sizeof(s_minuten));
  prv_setze(120, 600, false);
  prv_setze(600, 750, true);
  prv_setze(750, 870, false);
  prv_setze(870, MINUTEN, true);
  prv_durch(0, 880);
  const int vorher = s_saetze_anzahl;
  cl_assert(vorher >= 1);
  prv_neustart();
  prv_durch(881, MINUTEN);
  for (int k = vorher; k < s_saetze_anzahl; k++) {
    cl_assert(s_saetze[k].beginn_utc != s_beginn || s_saetze[k].aufwachen == 600);
  }
  cl_assert_equal_i(prv_ueberlappungen(), 0);
  const NachtDlsRecord *zweite = prv_letzter_satz(s_beginn + 600 * SECONDS_PER_MINUTE);
  cl_assert(zweite);
  cl_assert_equal_i(zweite->aufwachen, 270);
}

// M5: GEHT DER SATZ NICHT INS DATALOGGING, IST DIE NACHT NICHT GESCHICKT. Eine
// geschlossene Sitzung wird neu angelegt, und der naechste Takt schickt die
// Nacht noch einmal.
void test_boulder_nacht__geschlossene_datalogging_sitzung_wird_neu_angelegt(void) {
  s_nacht_scheitert = 1;
  s_nacht_fehler = DATA_LOGGING_CLOSED;
  prv_durch_die_nacht();
  cl_assert_equal_i(s_anlagen_nacht, 2);
  cl_assert(s_saetze_anzahl >= 1);
  cl_assert_equal_i(s_saetze[0].beginn_utc, s_beginn);
  cl_assert_equal_i(s_saetze[0].aufwachen, 660);
  cl_assert_equal_i(s_gesendet_um, s_beginn + (11 * 60 + 30) * SECONDS_PER_MINUTE);
}

void test_boulder_nacht__ohne_datalogging_sitzung_kommt_die_nacht_spaeter(void) {
  s_anlage_scheitert = 1;
  prv_durch_die_nacht();
  cl_assert(s_saetze_anzahl >= 1);
  cl_assert_equal_i(s_saetze[0].beginn_utc, s_beginn);
  cl_assert_equal_i(s_gesendet_um, s_beginn + (11 * 60 + 30) * SECONDS_PER_MINUTE);
}

// Scheitert es bis ueber das Endgueltig hinaus (09:00), bleibt die Nacht offen
// und kommt, sobald es wieder geht - als endgueltig gerechnet kaeme sie nie mehr.
void test_boulder_nacht__die_nacht_bleibt_offen_bis_sie_geschickt_ist(void) {
  s_nacht_scheitert = 10;
  s_nacht_fehler = DATA_LOGGING_INTERNAL_ERR;
  prv_durch_die_nacht();
  cl_assert(s_saetze_anzahl >= 1);
  cl_assert_equal_i(s_saetze[0].beginn_utc, s_beginn);
  cl_assert_equal_i(s_saetze[0].aufwachen, 660);
  // Zehn Takte gescheitert: 07:15 bis 09:30, geschickt um 09:45.
  cl_assert_equal_i(s_gesendet_um, s_beginn + (13 * 60 + 45) * SECONDS_PER_MINUTE);
  for (int k = 0; k < s_state.activity_sessions_count; k++) {
    cl_assert(!s_state.activity_sessions[k].ongoing);
  }
  cl_assert_equal_i(prv_anzahl(ActivitySessionType_Sleep), 1);
}

// M7: DIE NACHT LAESST PLATZ IM TAG. Eine zerrissene Nacht mit vielen
// Tiefschlaf-Abschnitten und eine Folgenacht: zusammen hoechstens 16 Sitzungen,
// und am Mittag ist noch Platz fuer ein Training. Die zusammengelegten
// Tiefschlafbloecke zaehlen ihre Luecken nicht.
void test_boulder_nacht__die_nacht_laesst_platz_im_tag(void) {
  // Schon tagsueber: vierzehn Bewegungen.
  for (int k = 0; k < 14; k++) {
    ActivitySession bewegung = {
      .start_utc = s_beginn - (10 - k / 2) * SECONDS_PER_HOUR + (k % 2) * 30 * 60,
      .length_min = 15,
      .type = ActivitySessionType_Movement,
    };
    cl_assert(activity_sessions_prv_add_activity_session(&bewegung));
  }
  // Die Nacht: 22:00-06:00, alle halbe Stunde 25 Minuten ganz still bei
  // ruhigem Puls (Tiefschlaf), dazwischen fuenf Minuten Regungen.
  memset(s_minuten, 0xFF, sizeof(s_minuten));
  for (int i = 120; i < 600; i++) {
    const bool unruhig = ((i - 120) % 30) >= 25;
    s_minuten[i] = (HealthMinuteData){ .vmc = unruhig ? 300 : 0,
                                       .heart_rate_bpm = unruhig ? 58 : 50 };
  }
  prv_setze(600, 750, true);
  prv_setze(750, 870, false);
  prv_setze(870, MINUTEN, true);
  prv_durch_die_nacht();
  int nacht = 0;
  for (int k = 0; k < s_state.activity_sessions_count; k++) {
    nacht += activity_sessions_prv_is_sleep_activity(s_state.activity_sessions[k].type) ? 1 : 0;
  }
  int tief_abschnitte = 0;
  for (int t = 0; t < s_saetze_anzahl; t++) {
    if (s_saetze[t].beginn_utc != s_beginn) {
      continue;
    }
    for (int k = 0; k < s_saetze[t].anzahl; k++) {
      tief_abschnitte += (s_saetze[t].abschnitte[k].phase == SchlafPhaseTief) ? 1 : 0;
    }
  }
  printf("\nZerrissene Nacht: %d Tiefschlaf-Abschnitte (alle Saetze), %d Schlafsitzungen, %d "
         "insgesamt\n", tief_abschnitte, nacht, s_state.activity_sessions_count);
  cl_assert(nacht <= 16);
  ActivitySession training = {
    .start_utc = s_jetzt, .length_min = 45, .type = ActivitySessionType_Open, .manual = true,
  };
  cl_assert(activity_sessions_prv_add_activity_session(&training));
  // Der Tiefschlaf der ersten Nacht, wie die Uhr ihn zaehlt, ist der des Satzes.
  const NachtDlsRecord *s = prv_letzter_satz(s_beginn);
  cl_assert(s);
  int tief_satz = 0;
  for (int k = 0; k < s->anzahl; k++) {
    if (s->abschnitte[k].phase == SchlafPhaseTief) {
      tief_satz += s->abschnitte[k].bis - s->abschnitte[k].von;
    }
  }
  int tief_uhr = 0;
  const time_t ende = s_beginn + s->aufwachen * SECONDS_PER_MINUTE;
  for (int k = 0; k < s_state.activity_sessions_count; k++) {
    const ActivitySession *a = &s_state.activity_sessions[k];
    if (a->type == ActivitySessionType_RestfulSleep && a->start_utc < ende) {
      tief_uhr += a->length_min - a->nicht_schlaf_min;
    }
  }
  cl_assert(tief_satz > 0);
  cl_assert(tief_uhr <= tief_satz);
  cl_assert(tief_uhr >= tief_satz - 1);
}

// N1: EIN BILD ANDERER FASSUNG GILT NICHT. Gleiche Groesse, falsche Fassung:
// die Health-App zeigte sonst Unsinn.
void test_boulder_nacht__bild_anderer_fassung_gilt_nicht(void) {
  NachtBild alt;
  memset(&alt, 0, sizeof(alt));
  alt.beginn_utc = s_beginn;
  alt.aufwachen = 660;
  alt.fertig = true;
  alt.version = NACHT_BILD_VERSION + 1;
  SettingsFile datei;
  cl_assert_equal_i(settings_file_open(&datei, "boulder_nacht", 2048), S_SUCCESS);
  cl_assert_equal_i(settings_file_set(&datei, "letzte", sizeof("letzte"), &alt, sizeof(alt)),
                    S_SUCCESS);
  settings_file_close(&datei);
  NachtBild b;
  cl_assert(!boulder_nacht_letzte(&b));
}

// Ein Bild, wie b24 es in die Datei schrieb: der Aufbau von damals, Byte fuer
// Byte (little endian wie die Uhr), unabhaengig vom heutigen NachtBild. Die
// drei Fuellbytes am Ende sind 0 - b24 nullte das ganze Bild vor dem Fuellen.
#define B24_BILD_GROESSE 512
static void prv_b24_u16(uint8_t *p, int o, uint16_t v) {
  p[o] = v & 0xFF;
  p[o + 1] = v >> 8;
}
static void prv_b24_u32(uint8_t *p, int o, uint32_t v) {
  for (int i = 0; i < 4; i++) {
    p[o + i] = (v >> (8 * i)) & 0xFF;
  }
}
static void prv_b24_bild(uint8_t *alt, const NachtBild *n) {
  memset(alt, 0, B24_BILD_GROESSE);
  prv_b24_u32(alt, 0, n->beginn_utc);
  prv_b24_u32(alt, 4, (uint32_t)n->utc_to_local);
  prv_b24_u16(alt, 8, n->einschlafen);
  prv_b24_u16(alt, 10, n->aufwachen);
  prv_b24_u16(alt, 12, n->schlaf_minuten);
  for (int p = 0; p < 5; p++) {
    prv_b24_u16(alt, 14 + 2 * p, n->minuten[p]);
  }
  prv_b24_u16(alt, 24, n->hrv);
  alt[26] = n->ruhepuls;
  alt[27] = n->fertig;
  alt[28] = n->anzahl;
  for (int k = 0; k < n->anzahl; k++) {
    prv_b24_u16(alt, 29 + 5 * k, n->abschnitte[k].von);
    prv_b24_u16(alt, 31 + 5 * k, n->abschnitte[k].bis);
    alt[33 + 5 * k] = n->abschnitte[k].phase;
  }
}

// DAS BILD VON b24 GILT NACH DEM UPDATE WEITER. Die Health-App zeigt die letzte
// Nacht sofort, und die Uhr weiss daraus, dass diese Nacht schon geschickt ist
// (b24 kannte den Zustand in der Datei noch nicht) - sie geht nicht noch einmal.
void test_boulder_nacht__bild_aus_b24_gilt_nach_dem_update(void) {
  // Bis 09:30: die Nacht ist geschickt und endgueltig, das Fenster noch offen.
  const int update = 13 * 60 + 30;
  prv_durch(0, update);
  NachtBild n;
  cl_assert(boulder_nacht_letzte(&n));
  cl_assert(n.fertig);
  cl_assert(n.anzahl > 1);
  const int saetze = s_saetze_anzahl;
  cl_assert(saetze >= 1);

  // Das Update: in der Datei steht nur, was b24 dort hatte.
  uint8_t alt[B24_BILD_GROESSE];
  prv_b24_bild(alt, &n);
  fake_settings_file_reset();
  SettingsFile datei;
  cl_assert_equal_i(settings_file_open(&datei, "boulder_nacht", 2048), S_SUCCESS);
  cl_assert_equal_i(settings_file_set(&datei, "letzte", sizeof("letzte"), alt, sizeof(alt)),
                    S_SUCCESS);
  settings_file_close(&datei);
  prv_neustart();

  NachtBild b;
  cl_assert(boulder_nacht_letzte(&b));
  cl_assert_equal_i(b.version, NACHT_BILD_VERSION);
  cl_assert_equal_i(b.beginn_utc, n.beginn_utc);
  cl_assert_equal_i(b.utc_to_local, n.utc_to_local);
  cl_assert_equal_i(b.einschlafen, n.einschlafen);
  cl_assert_equal_i(b.aufwachen, n.aufwachen);
  cl_assert_equal_i(b.schlaf_minuten, n.schlaf_minuten);
  for (int p = 0; p < NACHT_PHASEN; p++) {
    cl_assert_equal_i(b.minuten[p], n.minuten[p]);
  }
  cl_assert_equal_i(b.ruhepuls, n.ruhepuls);
  cl_assert(b.fertig);
  cl_assert_equal_i(b.anzahl, n.anzahl);
  for (int k = 0; k < n.anzahl; k++) {
    cl_assert_equal_i(b.abschnitte[k].von, n.abschnitte[k].von);
    cl_assert_equal_i(b.abschnitte[k].bis, n.abschnitte[k].bis);
    cl_assert_equal_i(b.abschnitte[k].phase, n.abschnitte[k].phase);
  }

  // Bis Mittag: dieselbe Nacht geht nicht noch einmal ans Telefon, und die
  // Datei traegt das Bild danach in dieser Fassung.
  prv_durch(update + 1, MINUTEN);
  cl_assert_equal_i(s_saetze_anzahl, saetze);
  cl_assert_equal_i(prv_ueberlappungen(), 0);
  uint8_t jetzt[B24_BILD_GROESSE];
  cl_assert_equal_i(settings_file_open(&datei, "boulder_nacht", 2048), S_SUCCESS);
  cl_assert_equal_i(settings_file_get(&datei, "letzte", sizeof("letzte"), jetzt, sizeof(jetzt)),
                    S_SUCCESS);
  settings_file_close(&datei);
  cl_assert_equal_i(jetzt[509], NACHT_BILD_VERSION);
  cl_assert_equal_m(jetzt, alt, 509);
}

// N2: DAS BILD KOMMT NUR IN DIE DATEI, WENN ES SICH AENDERT. Die Uhr rechnet
// die fertige Nacht bis zum Endgueltig (09:00) nach - immer dieselbe.
void test_boulder_nacht__das_bild_wird_nur_bei_aenderung_geschrieben(void) {
  prv_durch_die_nacht();
  cl_assert(s_saetze_anzahl >= 1);
  cl_assert_equal_i(s_bild_geschrieben, 1);
}

// N5: DIE MESSSTUFE IM SATZ IST DIE DER NACHT, nicht die beim Senden am Morgen.
void test_boulder_nacht__messstufe_der_nacht_geht_mit(void) {
  s_messstufe_5_ab = s_beginn + 3 * SECONDS_PER_HOUR;
  prv_durch_die_nacht();
  cl_assert(s_saetze_anzahl >= 1);
  cl_assert_equal_i(s_saetze[0].messstufe, 3);
}

// Die Monotonuhr des Rechners (Linux: CLOCK_MONOTONIC = 1, timespec = zwei long).
struct prv_zeitpunkt {
  long s;
  long ns;
};
extern int clock_gettime(int uhr, struct prv_zeitpunkt *t);
static long prv_ns(void) {
  struct prv_zeitpunkt t;
  clock_gettime(1, &t);
  return t.s * 1000000000L + t.ns;
}

// HAENGER AM MORGEN: der Minutenlauf, der die Nacht abschliesst, darf den
// Waechter von KernelBG (6,5 s) nicht ueberziehen. Jede Datalogging-Sitzung
// schickt beim Anlegen ein Oeffnen ans Telefon und wartet bis zu vier Sekunden;
// zwei davon (Tag 88 und 89) brauchen dazwischen frisches Futter. Der Schub
// ans Telefon kommt spaeter, mit eigenem Zeitgeber.
void test_boulder_nacht__morgenlauf_fuettert_den_waechter(void) {
  for (int k = 0; k < 20; k++) {
    s_hrv_start[k] = s_beginn + (120 + 20 * k) * SECONDS_PER_MINUTE;
  }
  s_hrv_anzahl = 20;
  const int fertig = 11 * 60 + 15;
  prv_durch(0, fertig - 1);
  s_jetzt = s_beginn + fertig * SECONDS_PER_MINUTE;
  activity_sessions_prv_minute_handler(s_jetzt);
  s_ablauf_laenge = 0;
  // Die Zeitmessung des Rechners (die time.h der Firmware kennt sie nicht).
  const long ns0 = prv_ns();
  boulder_nacht_minute(s_jetzt);
  const long ns1 = prv_ns();
  s_ablauf[s_ablauf_laenge] = 0;
  prv_zeitgeber_gestartet();
  const long us = (ns1 - ns0) / 1000;
  printf("\nMorgenlauf (Rechnung, Bild, Tag 88/89) auf dem Rechner: %ld us, Ablauf %s\n", us,
         s_ablauf);
  cl_assert_equal_i(s_saetze_anzahl, 1);
  cl_assert_equal_i(s_hrv_saetze_anzahl, 1);
  // Gelesen, gefuettert; vor und nach jedem Anlegen gefuettert.
  const char *a = strchr(s_ablauf, 'A');
  const char *h = strchr(s_ablauf, 'H');
  cl_assert(a && h && a < h);
  cl_assert(a > s_ablauf && a[-1] == 'W' && a[1] == 'W');
  cl_assert(h[-1] == 'W' && h[1] == 'W');
  // Kein Schub in diesem Lauf - erst mit dem Zeitgeber.
  cl_assert_equal_i(s_geschickt, 0);
  prv_durch(fertig + 1, fertig + 3);
  cl_assert_equal_i(s_geschickt, 1);
}
