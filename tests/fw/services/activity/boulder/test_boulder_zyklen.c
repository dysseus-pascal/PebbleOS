/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2, AP3: die Nacht als Zyklen in Pebbles Liste, der Live-Zustand und die
// Nickerchen - mit der ECHTEN activity_sessions.c (Zustand, Logging DLS 84) und der echten
// Glue-Kopie (get_last_sleep_utc, post_process). Spezifikation 9: T3.4, T3.5 (ohne
// Neustarts, die bringt AP4), T4.1, T4.3.
//
// Jede Minute laeuft wie auf der Uhr: Minutenteil der Variante (boulder_dauerteil_minute,
// die Naht D2), dann Pebbles activity_sessions_prv_minute_handler (activity.c:869), dann
// die KernelBG-Schlange (Callback "Nachlauf"). Die Minuten kommen aus den Referenznaechten
// (AP-G); die Phasen der Referenz (b26 = V2-Endfassung, AP2) sind der Bezug.

#include "applib/data_logging.h"
#include <pbl/drivers/ambient_light.h>
#include <pbl/drivers/rtc.h>
#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/activity_algorithm.h"
#include "pbl/services/activity/activity_private.h"
#include "pbl/services/activity/boulder/eingang.h"
#include "pbl/services/activity/boulder/nacht.h"
#include "pbl/services/activity/boulder/nacht_satz.h"
#include "pbl/services/activity/boulder/schlafanalyse.h"
#include "pbl/services/activity/boulder/variante.h"
#include "pbl/services/activity/boulder/zyklen.h"
#include "pbl/services/battery/battery_state.h"
#include "pbl/services/data_logging/data_logging_service.h"
#include "pbl/services/filesystem/pfs.h"
#include "pbl/services/hrm/hrm_manager_private.h"
#include "pbl/services/time.h"
#include "pbl/util/size.h"
#include "pbl/util/units.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clar.h"

#include "stubs_activity_insights.h"
#include "stubs_alarm.h"
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

// =============================================================================================
// Pebble rund um activity_sessions.c und die Glue-Kopie

static ActivityState s_state;
ActivityState *activity_private_state(void) {
  return &s_state;
}
SettingsFile *activity_private_settings_open(void) {
  return NULL;
}
void activity_private_settings_close(SettingsFile *file) {
}

uint32_t light_get_ambient_lux(void) {
  return 64;
}
AmbientLightLevel ambient_light_level_to_enum(uint32_t light_level) {
  return AmbientLightLevelLight;
}
BatteryChargeState battery_get_charge_state(void) {
  return (BatteryChargeState){ 0 };
}
bool activity_tracking_on(void) {
  return true;
}
void activity_metrics_prv_get_spo2_sample(uint8_t *percent_out, uint8_t *quality_out) {
  *percent_out = 0;
  *quality_out = 0;
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
    *median = 60;
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
HRMSessionRef hrm_manager_subscribe_with_callback(AppInstallId app_id, uint32_t update_interval_s,
                                                  uint16_t expire_s, HRMFeature features,
                                                  bool low_latency, HRMSubscriberCallback callback,
                                                  void *context) {
  return 1;
}
bool sys_hrm_manager_unsubscribe(HRMSessionRef session) {
  return true;
}
bool sys_hrm_manager_set_update_interval(HRMSessionRef session, uint32_t update_interval_s,
                                         uint16_t expire_s) {
  return true;
}
void hrm_manager_set_activity_scene(HRMActivityScene scene) {
}
bool activity_prefs_hrm_activity_tracking_is_enabled(void) {
  return true;
}

// DLS: Tag 84 (Sitzungen) wird mitgeschrieben; die Minutensaetze der Kopie laufen hier nicht.
#define LOG_MAX 128
static ActivitySessionDataLoggingRecord s_log[LOG_MAX];
static int s_log_anzahl;
DataLoggingSession *dls_create(uint32_t tag, DataLoggingItemType item_type, uint16_t item_size,
                               bool buffered, bool resume, const Uuid *uuid) {
  return (DataLoggingSession *)(uintptr_t)tag;
}
DataLoggingResult dls_log(DataLoggingSession *logging_session, const void *data,
                          uint32_t num_items) {
  if ((uintptr_t)logging_session == DlsSystemTagActivitySession) {
    cl_assert(s_log_anzahl < LOG_MAX);
    s_log[s_log_anzahl++] = *(const ActivitySessionDataLoggingRecord *)data;
  }
  return DATA_LOGGING_SUCCESS;
}
void dls_send_all_sessions(void) {
}

// =============================================================================================
// Referenznacht (AP-G): Eingabe und die Phasen des letzten b26-Satzes

#define MINUTEN SCHLAF_MINUTEN_MAX

typedef struct {
  uint32_t beginn_utc;
  bool gueltig[MINUTEN];
  uint16_t vmc[MINUTEN];
  uint8_t puls[MINUTEN];
  NachtDlsRecord satz;   //< der letzte Satz 0x4001
  char phasen[MINUTEN + 1];
  int hrv_anzahl;
  SchlafHrv hrv[SCHLAF_HRV_MAX];
} Referenz;

static Referenz s_ref;

static void prv_lade(const char *name) {
  static char zeile[4096];
  memset(&s_ref, 0, sizeof(s_ref));
  char pfad[512];
  snprintf(pfad, sizeof(pfad), "%s/activity/boulder/referenz-b26/%s.txt", CLAR_FIXTURE_PATH,
           name);
  FILE *f = fopen(pfad, "r");
  cl_assert(f);
  while (fgets(zeile, sizeof(zeile), f)) {
    zeile[strcspn(zeile, "\r\n")] = 0;
    char *wert = strchr(zeile, ' ');
    if (zeile[0] == '#' || !wert) {
      continue;
    }
    *wert++ = 0;
    int a, b, c;
    if (!strcmp(zeile, "beginn_utc")) {
      s_ref.beginn_utc = (uint32_t)atol(wert);
    } else if (!strcmp(zeile, "eingabe") && sscanf(wert, "%d %d %d", &a, &b, &c) == 3) {
      s_ref.gueltig[a] = true;
      s_ref.vmc[a] = (uint16_t)b;
      s_ref.puls[a] = (uint8_t)c;
    } else if (!strcmp(zeile, "hrv") && sscanf(wert, "%d %d %d", &a, &b, &c) == 3) {
      cl_assert(s_ref.hrv_anzahl < SCHLAF_HRV_MAX);
      s_ref.hrv[s_ref.hrv_anzahl++] =
          (SchlafHrv){ .minute = (uint16_t)a, .rmssd = (uint16_t)b, .puls = (uint8_t)c };
    } else if (!strcmp(zeile, "satz4001")) {
      const char *hex = strchr(wert, ' ') + 1;
      uint8_t *bytes = (uint8_t *)&s_ref.satz;
      for (size_t k = 0; k < sizeof(s_ref.satz); k++) {
        const char z[3] = { hex[2 * k], hex[2 * k + 1], 0 };
        bytes[k] = (uint8_t)strtoul(z, NULL, 16);
      }
    } else if (!strcmp(zeile, "phasen")) {
      strcpy(s_ref.phasen, wert);
    }
  }
  fclose(f);
  cl_assert(s_ref.beginn_utc && s_ref.phasen[0]);
}

static bool prv_schlafphase(char p) {
  return p >= '1' && p <= '4';
}

// =============================================================================================
// Eine Minute wie auf der Uhr

typedef struct {
  int schlaf;            //< Schlafminuten der Referenz (Nacht)
  int schlaf_live_wach;  //< davon live "Awake" (K4)
  int wach;              //< andere Minuten im Nachtfenster
  int wach_live_schlaf;
  int erstes_live;       //< erste Minute, in der Pebble nicht "Awake" sagte
  int ein;               //< erste Schlafminute der Referenz
  int max_belegt;        //< hoechste Belegung von Pebbles Liste
} Messung;

static Messung s_mess;

// Regel 2 (gestaffelter Abschluss): je Minute schliesst hoechstens eine Sitzung je Klasse.
// Liegen die Zyklen in Pebbles Liste aufsteigend, schadete ein gleichzeitiger Abschluss
// nicht - die Regel schuetzt die Faelle, in denen ein Zyklus den Platz eines neutralen
// Eintrags uebernimmt (Spezifikation 4.5, Regel 3). Deshalb wird sie selbst geprueft.
static int s_abschluss_verstoss;

static int prv_laufend(time_t start[], int klasse[]) {
  int n = 0;
  for (int i = 0; i < s_state.activity_sessions_count; i++) {
    const ActivitySession *a = &s_state.activity_sessions[i];
    if (a->ongoing && a->length_min > 0) {
      start[n] = a->start_utc;
      klasse[n++] = (a->type == ActivitySessionType_RestfulSleep);
    }
  }
  return n;
}

static void prv_minute(time_t beginn, int m, uint16_t vmc, uint8_t puls, bool gueltig) {
  const time_t utc = beginn + (time_t)m * PBL_SEC_PER_MIN;
  const BoulderMinute minute = {
    .utc = utc, .vmc = gueltig ? vmc : 0, .puls = gueltig ? puls : 0,
    .nicht_getragen = !gueltig,
  };
  rtc_set_time(utc + PBL_SEC_PER_MIN);
  time_t vorher[ACTIVITY_MAX_ACTIVITY_SESSIONS_COUNT];
  int vorher_klasse[ACTIVITY_MAX_ACTIVITY_SESSIONS_COUNT];
  const int n_vorher = prv_laufend(vorher, vorher_klasse);
  boulder_dauerteil_minute(&minute);
  int geschlossen[2] = { 0, 0 };
  for (int i = 0; i < n_vorher; i++) {
    bool noch = false;
    for (int j = 0; j < s_state.activity_sessions_count; j++) {
      noch |= s_state.activity_sessions[j].start_utc == vorher[i] &&
              s_state.activity_sessions[j].ongoing;
    }
    geschlossen[vorher_klasse[i]] += !noch;
  }
  s_abschluss_verstoss += (geschlossen[0] > 1) + (geschlossen[1] > 1);
  activity_sessions_prv_minute_handler(utc + PBL_SEC_PER_MIN);
  fake_system_task_callbacks_invoke_pending();
  if (s_state.activity_sessions_count > s_mess.max_belegt) {
    s_mess.max_belegt = s_state.activity_sessions_count;
  }
}

// Die Referenznacht bis `bis` (Minute des Fensters); ab `ab_unterwegs` und nach der Eingabe
// wach und unterwegs (vmc 3000, Puls 82 wie die Referenz). Misst den Live-Zustand je Minute.
static void prv_durch(int bis, int ab_unterwegs) {
  memset(&s_mess, 0, sizeof(s_mess));
  s_mess.erstes_live = -1;
  s_mess.ein = -1;
  int hrv = 0;
  for (int m = 0; m < bis; m++) {
    const bool unterwegs = m >= ab_unterwegs || m >= MINUTEN;
    if (unterwegs) {
      prv_minute(s_ref.beginn_utc, m, 3000, 82, true);
    } else {
      prv_minute(s_ref.beginn_utc, m, s_ref.vmc[m], s_ref.puls[m], s_ref.gueltig[m]);
    }
    // HRV-Fenster kommen mit AP5; bis dahin gibt sie der Test, sobald ihre Mitte vorbei ist
    // (wie die Referenz).
    while (hrv < s_ref.hrv_anzahl && s_ref.hrv[hrv].minute + 2 < m + 1 &&
           m < ab_unterwegs && m < NACHT_MINUTEN) {
      boulder_eingang_hrv(s_ref.hrv[hrv].minute, s_ref.hrv[hrv].rmssd, s_ref.hrv[hrv].puls);
      hrv++;
    }
    if (m >= NACHT_MINUTEN || ab_unterwegs < MINUTEN) {
      continue;
    }
    const bool live_schlaf = s_state.sleep_data.cur_state != ActivitySleepStateAwake;
    if (live_schlaf && s_mess.erstes_live < 0) {
      s_mess.erstes_live = m;
    }
    if (prv_schlafphase(s_ref.phasen[m])) {
      s_mess.ein = (s_mess.ein < 0) ? m : s_mess.ein;
      s_mess.schlaf++;
      s_mess.schlaf_live_wach += !live_schlaf;
    } else {
      s_mess.wach++;
      s_mess.wach_live_schlaf += live_schlaf;
    }
  }
}

// =============================================================================================
// Pruefungen an DLS 84 und an Pebbles Liste

static bool prv_ist_schlaf(uint16_t typ) {
  return typ == ActivitySessionType_Sleep || typ == ActivitySessionType_Nap;
}

static bool prv_ist_tief(uint16_t typ) {
  return typ == ActivitySessionType_RestfulSleep || typ == ActivitySessionType_RestfulNap;
}

// T3.5: kein Satz doppelt, keiner mit 0 Minuten, je Klasse aufsteigend, Schlafstarts
// paarweise verschieden, R in einem Zyklus; nichts von der Nacht laeuft noch in der Liste.
static void prv_pruefe_log(const char *name) {
  cl_assert_equal_i(s_abschluss_verstoss, 0);
  uint32_t ende_klasse[2] = { 0, 0 };
  for (int i = 0; i < s_log_anzahl; i++) {
    const ActivitySessionDataLoggingRecord *a = &s_log[i];
    cl_assert(prv_ist_schlaf(a->activity) || prv_ist_tief(a->activity));
    if (a->elapsed_sec == 0) {
      printf("\n%s: 0-min-Satz %d", name, i);
    }
    cl_assert(a->elapsed_sec > 0);
    const int k = prv_ist_tief(a->activity);
    cl_assert(a->start_utc + a->elapsed_sec > ende_klasse[k]);
    ende_klasse[k] = a->start_utc + a->elapsed_sec;
    bool in_zyklus = !k;
    for (int j = 0; j < s_log_anzahl; j++) {
      const ActivitySessionDataLoggingRecord *b = &s_log[j];
      if (j != i) {
        cl_assert(a->start_utc != b->start_utc);
      }
      if (k && prv_ist_schlaf(b->activity) && a->start_utc >= b->start_utc &&
          a->start_utc + a->elapsed_sec <= b->start_utc + b->elapsed_sec) {
        in_zyklus = true;
      }
      // Zwei Zyklen ueberlappen nie.
      if (j != i && !k && prv_ist_schlaf(b->activity)) {
        cl_assert(a->start_utc + a->elapsed_sec <= b->start_utc ||
                  b->start_utc + b->elapsed_sec <= a->start_utc);
      }
    }
    cl_assert(in_zyklus);
  }
  for (int i = 0; i < s_state.activity_sessions_count; i++) {
    const ActivitySession *s = &s_state.activity_sessions[i];
    // Laufend bleiben nur neutrale Eintraege (Laenge 0).
    cl_assert(!s->ongoing || s->length_min == 0);
  }
}

// T3.4: Summe der Zyklen = Schlaf des Satzes, keine Wachminute in Sleep, R nur Tief, jeder
// Zyklus und jeder Tief-Abschnitt der Referenz genau einmal geloggt.
static void prv_pruefe_zyklen(const char *name) {
  const time_t b = s_ref.beginn_utc;
  int summe = 0, zyklen = 0, tief = 0, schlaf_ref = 0, laeufe = 0, tief_ref = 0;
  for (int i = 0; i < s_log_anzahl; i++) {
    const ActivitySessionDataLoggingRecord *a = &s_log[i];
    const int von = (int)(a->start_utc - b) / 60;
    const int bis = von + (int)a->elapsed_sec / 60;
    for (int m = von; m < bis; m++) {
      if (prv_ist_schlaf(a->activity)) {
        cl_assert(prv_schlafphase(s_ref.phasen[m]));
      } else {
        cl_assert_equal_i(s_ref.phasen[m], '2');
      }
    }
    if (prv_ist_schlaf(a->activity)) {
      summe += bis - von;
      zyklen++;
    } else {
      tief++;
    }
  }
  for (int m = 0; m < NACHT_MINUTEN; m++) {
    const bool s = prv_schlafphase(s_ref.phasen[m]);
    schlaf_ref += s;
    laeufe += s && (m == 0 || !prv_schlafphase(s_ref.phasen[m - 1]));
    tief_ref += s_ref.phasen[m] == '2' && (m == 0 || s_ref.phasen[m - 1] != '2');
  }
  printf("\n%s: %d Zyklen (%d min, Satz %d, Referenz %d), %d Tief (Referenz %d), "
         "Liste hoechstens %d", name, zyklen, summe, s_ref.satz.schlaf_minuten, schlaf_ref,
         tief, tief_ref, s_mess.max_belegt);
  cl_assert_equal_i(summe, s_ref.satz.schlaf_minuten);
  cl_assert_equal_i(summe, schlaf_ref);
  cl_assert_equal_i(zyklen, laeufe);
  cl_assert(tief == tief_ref || tief == ZYKLEN_R_MAX);
}

// =============================================================================================

static AccelSamplingRate s_rate;

void test_boulder_zyklen__initialize(void) {
  TimezoneInfo tz = { .tm_zone = "CEST", .tm_gmtoff = 2 * PBL_SEC_PER_HOUR };
  time_util_update_timezone(&tz);
  fake_rtc_init(100, 1758000000);
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  pfs_format(false);
  memset(&s_state, 0, sizeof(s_state));
  s_log_anzahl = 0;
  s_abschluss_verstoss = 0;
  boulder_nacht_zuruecksetzen();
  cl_assert(activity_algorithm_init(&s_rate));
}

void test_boulder_zyklen__cleanup(void) {
  fake_system_task_callbacks_invoke_pending();
  activity_algorithm_deinit();
  fake_system_task_callbacks_invoke_pending();
}

static const char *s_naechte[] = {
  "nacht-2026-09-26", "nacht-2026-09-29", "nacht-2026-09-30",
  "nacht-2026-10-03", "nacht-2026-10-05", "nacht-2026-10-05-morgen",
};

// T3.4 und T4.1 auf allen Referenznaechten: durch die Nacht bis 16 Uhr (Fensterende und
// Abschluss liegen davor).
void test_boulder_zyklen__naechte(void) {
  int unter_95 = 0;
  for (size_t k = 0; k < ARRAY_LENGTH(s_naechte); k++) {
    test_boulder_zyklen__cleanup();
    test_boulder_zyklen__initialize();
    prv_lade(s_naechte[k]);
    prv_durch(20 * 60, MINUTEN);
    const double anteil = 100.0 * (s_mess.schlaf - s_mess.schlaf_live_wach) / s_mess.schlaf;
    printf("\n%s: live schlafend %.1f %% der %d Schlafminuten (%d wach), wach live schlaf "
           "%d/%d, erstes Live-Schlafen %+d min vor dem Einschlafen",
           s_naechte[k], anteil, s_mess.schlaf, s_mess.schlaf_live_wach, s_mess.wach_live_schlaf,
           s_mess.wach, s_mess.ein - s_mess.erstes_live);
    unter_95 += anteil < 95.0;
    prv_pruefe_log(s_naechte[k]);
    prv_pruefe_zyklen(s_naechte[k]);
    const BoulderZyklenDiagnose *d = boulder_zyklen_diagnose();
    cl_assert_equal_i(d->tabelle_voll, 0);
    cl_assert_equal_i(d->zu_geaendert, 0);
  }
  printf("\n");
  cl_assert_equal_i(unter_95, 0);
}

// T3.5 (ohne Neustarts): 40 Morgen mit zufaelligem Aufstehen zwischen 04:00 und 09:00 -
// keine Doppelten, keine 0-min-Saetze, aufsteigend, alles abgeschlossen.
void test_boulder_zyklen__zufaellige_morgen(void) {
  srand(20261006);
  for (int k = 0; k < 40; k++) {
    test_boulder_zyklen__cleanup();
    test_boulder_zyklen__initialize();
    const char *name = s_naechte[k % ARRAY_LENGTH(s_naechte)];
    prv_lade(name);
    const int auf = 8 * 60 + rand() % (5 * 60);
    prv_durch(20 * 60, auf);
    prv_pruefe_log(name);
    int schlaf = 0;
    for (int i = 0; i < s_log_anzahl; i++) {
      schlaf += prv_ist_schlaf(s_log[i].activity) ? s_log[i].elapsed_sec / 60 : 0;
    }
    // Nie Schlaf nach dem Aufstehen.
    for (int i = 0; i < s_log_anzahl; i++) {
      cl_assert(s_log[i].start_utc + s_log[i].elapsed_sec <=
                s_ref.beginn_utc + (uint32_t)auf * 60);
    }
    cl_assert(s_log_anzahl > 0 && schlaf > 60);
  }
}

// T4.3 (gestellt): Nickerchen um 14 Uhr aus der Bewegung echten Schlafs. Ab etwa 45
// Minuten mit ruhigem Puls ein Nap; mit hohem Puls (Fernsehen) keins; 30 Minuten keins; ueber
// drei Stunden ein Sleep.
static int prv_tag_mit(int laenge, int puls_plus) {
  test_boulder_zyklen__cleanup();
  test_boulder_zyklen__initialize();
  prv_lade("nacht-2026-09-29");
  prv_durch(NACHT_MINUTEN, MINUTEN);  // die Nacht gibt den Ruhepuls
  cl_assert(boulder_nacht_ruhepuls() > 0);
  const int vor = s_log_anzahl;
  // Das Stueck: die Bewegung aus dem laengsten Schlaflauf der Nacht (ohne Wachminuten), ab
  // seiner zehnten Minute, laenger als er wiederholt; der Puls ruhig (Ruhepuls der Nacht)
  // oder um `puls_plus` hoeher.
  const int ruhepuls = boulder_nacht_ruhepuls();
  int quelle = 0, lauf = 0, bester = 0;
  for (int m = 0; m < NACHT_MINUTEN; m++) {
    lauf = prv_schlafphase(s_ref.phasen[m]) ? lauf + 1 : 0;
    if (lauf > bester) {
      bester = lauf;
      quelle = m - lauf + 1;
    }
  }
  quelle += 10;
  bester -= 10;
  for (int m = NACHT_MINUTEN; m < NACHT_FENSTER_MINUTEN; m++) {
    const int i = m - 18 * 60;  // 14:00 = Minute 1080
    if (i >= 0 && i < laenge) {
      const int q = quelle + i % bester;
      prv_minute(s_ref.beginn_utc, m, s_ref.vmc[q], ruhepuls + puls_plus, true);
    } else {
      prv_minute(s_ref.beginn_utc, m, 3000, 82, true);
    }
  }
  int naps = 0;
  for (int i = vor; i < s_log_anzahl; i++) {
    const ActivitySessionDataLoggingRecord *a = &s_log[i];
    printf("\n  %d min ab 14:00, Puls +%d: Typ %d, %02d:%02d, %d min", laenge, puls_plus,
           a->activity, (int)((a->start_utc + a->utc_to_local) / 3600 % 24),
           (int)((a->start_utc + a->utc_to_local) / 60 % 60), (int)a->elapsed_sec / 60);
    naps += (a->activity == ActivitySessionType_Nap) ? 1 : 0;
    naps += (a->activity == ActivitySessionType_Sleep) ? 100 : 0;
  }
  return naps;
}

void test_boulder_zyklen__nickerchen(void) {
  cl_assert_equal_i(prv_tag_mit(75, 0), 1);     // Nap
  cl_assert_equal_i(prv_tag_mit(75, 25), 0);    // Fernsehen: Puls zu hoch
  cl_assert_equal_i(prv_tag_mit(30, 0), 0);     // zu kurz (V3)
  cl_assert_equal_i(prv_tag_mit(50, 0), 1);     // ab etwa 45 Minuten (V3)
  cl_assert_equal_i(prv_tag_mit(200, 0), 100);  // ueber drei Stunden: Sleep
  printf("\n");
}
