/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2, AP3: die Nacht als Zyklen in Pebbles Liste, der Live-Zustand und die
// Nickerchen - mit der ECHTEN activity_sessions.c (Zustand, Logging DLS 84) und der echten
// Glue-Kopie (get_last_sleep_utc, post_process). Spezifikation 9: T3.4, T3.5, T4.1, T4.3.
// AP4: die Saetze 0x4001/0x4002 und das NachtBild Byte fuer Byte wie b26 (T3.2, T6.4), die
// Ablage in der echten settings_file.c, Neustarts (T3.5) und Zeitspruenge (7.4), die Anlage.
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
#include <pbl/cron/cron.h>
#include "pbl/services/activity/boulder/ausgabe.h"
#include "pbl/services/activity/boulder/eingang.h"
#include "pbl/services/activity/boulder/nacht.h"
#include "pbl/services/activity/boulder/nacht_satz.h"
#include "pbl/services/activity/boulder/puls.h"
#include "pbl/services/activity/boulder/schlafanalyse.h"
#include "pbl/services/activity/boulder/speicher.h"
#include "pbl/services/activity/boulder/takt.h"
#include "pbl/services/activity/boulder/variante.h"
#include "pbl/services/activity/boulder/zyklen.h"
#include "pbl/services/battery/battery_state.h"
#include "pbl/services/data_logging/data_logging_service.h"
#include "pbl/services/filesystem/pfs.h"
#include "pbl/services/settings/settings_file.h"
#include "pbl/services/hrm/hrm_manager_private.h"
#include "pbl/services/time.h"
#include "pbl/util/size.h"
#include "pbl/util/units.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clar.h"

#include "stubs_activity_insights.h"
// Die Weckzeit gibt der Test selbst (Ruhezeit der Anlage, AP4).
#define alarm_get_next_enabled_alarm stubs_alarm_get_next_enabled_alarm
#include "stubs_alarm.h"
#undef alarm_get_next_enabled_alarm
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
// AP4: das Akku-Modell der Referenz (80 % um 20:00, -1 je Stunde), sonst 0.
static bool s_akku_modell;
static bool s_am_kabel;
static time_t s_akku_ab;
BatteryChargeState battery_get_charge_state(void) {
  if (!s_akku_modell) {
    return (BatteryChargeState){ 0 };
  }
  return (BatteryChargeState){ .charge_percent = 80 - (rtc_get_time() - s_akku_ab) / 3600,
                               .is_plugged = s_am_kabel };
}

// AP4: die naechste Weckzeit fuer die Ruhezeit der Anlage (stubs_alarm.h ist schwach).
static time_t s_weckzeit;
bool alarm_get_next_enabled_alarm(time_t *next_alarm_time_out) {
  *next_alarm_time_out = s_weckzeit;
  return s_weckzeit != 0;
}
extern uint8_t g_fake_messstufe;
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

// DLS: Tag 84 (Sitzungen), 0x4001 und 0x4002 werden mitgeschrieben; die Minutensaetze der Kopie
// laufen hier nicht.
#define LOG_MAX 160
static ActivitySessionDataLoggingRecord s_log[LOG_MAX];
static int s_log_anzahl;
#define SAETZE_MAX 24
static NachtDlsRecord s_nacht[SAETZE_MAX];
static int s_nacht_um[SAETZE_MAX];  //< Minute des Fensters, zu der der Satz geloggt wurde
static int s_nacht_anzahl;
static NachtHrvDlsRecord s_hrv_saetze[SAETZE_MAX];
static int s_hrv_saetze_anzahl;
static int s_anlagen[2];          //< dls_create fuer 0x4001, 0x4002 seit dem letzten Boot
static int s_anlagen_im_minutenteil;
static bool s_im_minutenteil;
static int s_anlage_null;         //< so viele dls_create geben NULL (Pool voll)
static time_t s_anlage_um[8];
static int s_anlage_versuche;
static int s_schub;
DataLoggingSession *dls_create(uint32_t tag, DataLoggingItemType item_type, uint16_t item_size,
                               bool buffered, bool resume, const Uuid *uuid) {
  if (tag == BOULDER_DLS_TAG_NACHT || tag == BOULDER_DLS_TAG_NACHT_HRV) {
    cl_assert(!resume && !buffered);
    s_anlagen_im_minutenteil += s_im_minutenteil;
    if (s_anlage_versuche < 8) {
      s_anlage_um[s_anlage_versuche] = rtc_get_time();
    }
    s_anlage_versuche++;
    if (s_anlage_null > 0) {
      s_anlage_null--;
      return NULL;
    }
    s_anlagen[tag == BOULDER_DLS_TAG_NACHT_HRV]++;
  }
  return (DataLoggingSession *)(uintptr_t)tag;
}
DataLoggingResult dls_log(DataLoggingSession *logging_session, const void *data,
                          uint32_t num_items) {
  const uintptr_t tag = (uintptr_t)logging_session;
  if (tag == DlsSystemTagActivitySession) {
    cl_assert(s_log_anzahl < LOG_MAX);
    s_log[s_log_anzahl++] = *(const ActivitySessionDataLoggingRecord *)data;
  } else if (tag == BOULDER_DLS_TAG_NACHT) {
    cl_assert(s_nacht_anzahl < SAETZE_MAX);
    s_nacht_um[s_nacht_anzahl] = (int)((rtc_get_time() - s_akku_ab) / PBL_SEC_PER_MIN);
    s_nacht[s_nacht_anzahl++] = *(const NachtDlsRecord *)data;
  } else if (tag == BOULDER_DLS_TAG_NACHT_HRV) {
    cl_assert(s_hrv_saetze_anzahl < SAETZE_MAX);
    s_hrv_saetze[s_hrv_saetze_anzahl++] = *(const NachtHrvDlsRecord *)data;
  }
  return DATA_LOGGING_SUCCESS;
}
void dls_send_all_sessions(void) {
  s_schub++;
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
  uint8_t stufe;
  int saetze;            //< alle Saetze 0x4001 der Referenz, in Folge
  int satz_um[SAETZE_MAX];
  NachtDlsRecord satz_alle[SAETZE_MAX];
  int hrv_saetze;
  NachtHrvDlsRecord hrv_satz[SAETZE_MAX];
  NachtBild bild;
  int hrv_anzahl;
  SchlafHrv hrv[SCHLAF_HRV_MAX];
} Referenz;

static Referenz s_ref;

static void prv_hex(const char *hex, void *aus, size_t n) {
  uint8_t *bytes = aus;
  for (size_t k = 0; k < n; k++) {
    const char z[3] = { hex[2 * k], hex[2 * k + 1], 0 };
    bytes[k] = (uint8_t)strtoul(z, NULL, 16);
  }
}

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
      prv_hex(hex, &s_ref.satz, sizeof(s_ref.satz));
      cl_assert(s_ref.saetze < SAETZE_MAX);
      s_ref.satz_um[s_ref.saetze] = atoi(wert);
      s_ref.satz_alle[s_ref.saetze++] = s_ref.satz;
    } else if (!strcmp(zeile, "satz4002")) {
      cl_assert(s_ref.hrv_saetze < SAETZE_MAX);
      prv_hex(strchr(wert, ' ') + 1, &s_ref.hrv_satz[s_ref.hrv_saetze++],
              sizeof(NachtHrvDlsRecord));
    } else if (!strcmp(zeile, "bild")) {
      prv_hex(wert, &s_ref.bild, sizeof(s_ref.bild));
    } else if (!strcmp(zeile, "messstufe")) {
      s_ref.stufe = (uint8_t)atoi(wert);
    } else if (!strcmp(zeile, "phasen")) {
      strcpy(s_ref.phasen, wert);
    }
  }
  fclose(f);
  cl_assert(s_ref.beginn_utc && s_ref.phasen[0]);
  s_akku_ab = s_ref.beginn_utc;
  g_fake_messstufe = s_ref.stufe;
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
  s_im_minutenteil = true;
  boulder_dauerteil_minute(&minute);
  s_im_minutenteil = false;
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
  // Der eigene Takt (AP4, Cron zur Sekunde 10).
  boulder_dauerteil_takt(utc + PBL_SEC_PER_MIN + 10);
  fake_system_task_callbacks_invoke_pending();
  if (s_state.activity_sessions_count > s_mess.max_belegt) {
    s_mess.max_belegt = s_state.activity_sessions_count;
  }
}

// Die Referenznacht bis `bis` (Minute des Fensters); ab `ab_unterwegs` und nach der Eingabe
// wach und unterwegs (vmc 3000, Puls 82 wie die Referenz). Misst den Live-Zustand je Minute.
// Minute m der Referenz; ab `ab_unterwegs` und nach der Eingabe wach und unterwegs. Die
// HRV-Fenster gibt der Test (die Messung prueft test_boulder_messung) ueber die Naht des
// Dauerteils, sobald ihre Mitte vor dem Nachlauf dieser Minute liegt - wie die Referenz.
static void prv_schritt(int m, int ab_unterwegs, int *hrv) {
  while (*hrv < s_ref.hrv_anzahl && s_ref.hrv[*hrv].minute + 2 < m + 1 && m < ab_unterwegs &&
         m < NACHT_MINUTEN) {
    boulder_dauerteil_hrv_fenster(s_ref.beginn_utc + s_ref.hrv[*hrv].minute * PBL_SEC_PER_MIN,
                                  s_ref.hrv[*hrv].rmssd, s_ref.hrv[*hrv].puls);
    (*hrv)++;
  }
  if (m >= ab_unterwegs || m >= MINUTEN) {
    prv_minute(s_ref.beginn_utc, m, 3000, 82, true);
  } else {
    prv_minute(s_ref.beginn_utc, m, s_ref.vmc[m], s_ref.puls[m], s_ref.gueltig[m]);
  }
}

static void prv_durch(int bis, int ab_unterwegs) {
  memset(&s_mess, 0, sizeof(s_mess));
  s_mess.erstes_live = -1;
  s_mess.ein = -1;
  int hrv = 0;
  for (int m = 0; m < bis; m++) {
    prv_schritt(m, ab_unterwegs, &hrv);
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
  s_nacht_anzahl = 0;
  s_hrv_saetze_anzahl = 0;
  memset(s_anlagen, 0, sizeof(s_anlagen));
  s_anlagen_im_minutenteil = 0;
  s_anlage_null = 0;
  s_anlage_versuche = 0;
  s_schub = 0;
  s_akku_modell = true;
  s_am_kabel = false;
  s_weckzeit = 0;
  pbl_cron_init();
  boulder_dauerteil_test_neustart();
  cl_assert(activity_algorithm_init(&s_rate));
}

void test_boulder_zyklen__cleanup(void) {
  fake_system_task_callbacks_invoke_pending();
  activity_algorithm_deinit();
  fake_system_task_callbacks_invoke_pending();
  pbl_cron_deinit();
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

// =============================================================================================
// AP4: die Nacht nach aussen und ueber Neustarts

// Die Saetze von V2 gegen die der Referenz; jeder Unterschied wird ausgegeben.
static int prv_saetze_anders(const char *name) {
  int anders = 0;
  printf("\n%s: 0x4001 %d (b26 %d), 0x4002 %d (b26 %d), gesendet um", name, s_nacht_anzahl,
         s_ref.saetze, s_hrv_saetze_anzahl, s_ref.hrv_saetze);
  for (int k = 0; k < s_nacht_anzahl && k < s_ref.saetze; k++) {
    printf(" %d (b26 %d)", s_nacht_um[k], s_ref.satz_um[k]);
    if (memcmp(&s_nacht[k], &s_ref.satz_alle[k], sizeof(NachtDlsRecord))) {
      const NachtDlsRecord *a = &s_nacht[k], *b = &s_ref.satz_alle[k];
      printf("\n  Satz %d ANDERS: ein %d/%d auf %d/%d schlaf %d/%d akku %d/%d %d/%d stufe %d/%d",
             k, a->einschlafen, b->einschlafen, a->aufwachen, b->aufwachen, a->schlaf_minuten,
             b->schlaf_minuten, a->akku_beginn, b->akku_beginn, a->akku_ende, b->akku_ende,
             a->messstufe, b->messstufe);
      anders++;
    }
  }
  for (int k = 0; k < s_hrv_saetze_anzahl && k < s_ref.hrv_saetze; k++) {
    if (memcmp(&s_hrv_saetze[k], &s_ref.hrv_satz[k], sizeof(NachtHrvDlsRecord))) {
      printf("\n  HRV-Satz %d ANDERS", k);
      anders++;
    }
  }
  return anders + (s_nacht_anzahl != s_ref.saetze) + (s_hrv_saetze_anzahl != s_ref.hrv_saetze);
}

static bool prv_bild_in_datei(NachtBild *b) {
  SettingsFile f;
  if (settings_file_open(&f, "boulder_nacht", 2048) != S_SUCCESS) {
    return false;
  }
  const bool da = settings_file_get(&f, "letzte", sizeof("letzte"), b, sizeof(*b)) == S_SUCCESS;
  settings_file_close(&f);
  return da;
}

// T3.2/T6.4: 0x4001, 0x4002 und das NachtBild Byte fuer Byte wie b26 (die Referenzbytes liefen
// in AP-G durch Boulders NachtParserTest und NachtHrvParserTest), ueber die ganze V2-Kette:
// Minutenteil, Nachlauf, Lebenslauf, Ausgabe-Callbacks, Anlage, Takt mit Protokoll.
void test_boulder_zyklen__ausgabe_wie_b26(void) {
  int anders = 0;
  for (size_t k = 0; k < ARRAY_LENGTH(s_naechte); k++) {
    test_boulder_zyklen__cleanup();
    test_boulder_zyklen__initialize();
    prv_lade(s_naechte[k]);
    prv_durch(20 * 60, MINUTEN);
    anders += prv_saetze_anders(s_naechte[k]);
    NachtBild bild, datei;
    cl_assert(boulder_nacht_letzte(&bild));
    cl_assert(prv_bild_in_datei(&datei));
    anders += memcmp(&bild, &s_ref.bild, sizeof(bild)) != 0;
    anders += memcmp(&datei, &s_ref.bild, sizeof(datei)) != 0;
    // Die Anlage: je Tag einmal, nie im Minutenteil; der Schub nach dem Senden.
    cl_assert_equal_i(s_anlagen[0], 1);
    cl_assert_equal_i(s_anlagen[1], s_ref.hrv_saetze > 0);
    cl_assert_equal_i(s_anlagen_im_minutenteil, 0);
    cl_assert(s_schub >= 1);
  }
  printf("\n");
  cl_assert_equal_i(anders, 0);
}

// Die Eingabe der Minute m wie prv_schritt.
static BoulderMinute prv_eingabe(int m, int ab_unterwegs) {
  const bool unterwegs = m >= ab_unterwegs || m >= MINUTEN;
  const bool gueltig = unterwegs || s_ref.gueltig[m];
  return (BoulderMinute){
    .utc = s_ref.beginn_utc + (time_t)m * PBL_SEC_PER_MIN,
    .vmc = unterwegs ? 3000 : (gueltig ? s_ref.vmc[m] : 0),
    .puls = unterwegs ? 82 : (gueltig ? s_ref.puls[m] : 0),
    .nicht_getragen = !gueltig,
  };
}

// Ein Neustart nach Minute m: alles Statische ist weg, die Dateien bleiben, Pebble verwirft
// die laufenden Sitzungen (activity_sessions.c:632). Pebbles Minutendatei hat die Minuten bis
// zur vorletzten vollen Viertelstunde (Kraepelin haelt zurueck, D3/D12); init liefert sie (D14,
// hier vom Test, wie der Glue im Durchlauf). Der Boot verschluckt die Minute m + 1.
static void prv_neustart(int m, int ab_unterwegs) {
  fake_system_task_callbacks_invoke_pending();
  activity_algorithm_deinit();
  fake_system_task_callbacks_invoke_pending();
  pbl_cron_deinit();
  const time_t jetzt = s_ref.beginn_utc + (time_t)(m + 2) * PBL_SEC_PER_MIN;
  rtc_set_time(jetzt);
  activity_sessions_prv_remove_out_of_range_activity_sessions(jetzt, true /*remove_ongoing*/);
  memset(s_anlagen, 0, sizeof(s_anlagen));
  pbl_cron_init();
  boulder_dauerteil_test_neustart();
  cl_assert(activity_algorithm_init(&s_rate));
  int datei_bis = (m + 1) - (m + 1) % 15 - 15;
  datei_bis = (datei_bis > NACHT_MINUTEN) ? NACHT_MINUTEN : datei_bis;
  for (int i = 0; i < datei_bis; i++) {
    const BoulderMinute minute = prv_eingabe(i, ab_unterwegs);
    boulder_dauerteil_ring_minute(&minute);
  }
  fake_system_task_callbacks_invoke_pending();  // "Laden"
}

typedef struct {
  int saetze_gleich;   //< letzter Satz 0x4001 = b26
  int schlaf_diff;     //< Schlaf des letzten Satzes - b26
} Neustarts;

// Die Referenznacht bis 16 Uhr mit Neustarts nach den Minuten `neu` (aufsteigend) und einem
// im gestaffelten Abschluss, wenn `im_abschluss`.
static Neustarts prv_mit_neustarts(const char *name, const int *neu, int n, bool im_abschluss,
                                   int ab_unterwegs) {
  test_boulder_zyklen__cleanup();
  test_boulder_zyklen__initialize();
  prv_lade(name);
  int hrv = 0;
  int k = 0;
  bool abschluss_gesehen = false;
  for (int m = 0; m < 20 * 60; m++) {
    prv_schritt(m, ab_unterwegs, &hrv);
    const bool abschluss = im_abschluss && !abschluss_gesehen && boulder_zyklen_abschluss_offen();
    if ((k < n && m == neu[k]) || abschluss) {
      abschluss_gesehen |= abschluss;
      k += !abschluss;
      prv_neustart(m, ab_unterwegs);
      m++;
      // Was der Boot verschluckt, sieht auch die HRV nicht.
      while (hrv < s_ref.hrv_anzahl && s_ref.hrv[hrv].minute + 2 < m + 1) {
        hrv++;
      }
    }
  }
  prv_pruefe_log(name);
  // Dieselbe Nacht nie zweimal mit denselben Werten (der Kopf kennt das Gesendete).
  for (int i = 0; i < s_nacht_anzahl; i++) {
    for (int j = i + 1; j < s_nacht_anzahl; j++) {
      if (s_nacht[i].teil == 0 && s_nacht[j].teil == 0) {
        cl_assert(s_nacht[i].beginn_utc != s_nacht[j].beginn_utc ||
                  s_nacht[i].aufwachen != s_nacht[j].aufwachen ||
                  s_nacht[i].schlaf_minuten != s_nacht[j].schlaf_minuten);
      }
    }
  }
  cl_assert(s_nacht_anzahl > 0);
  const NachtDlsRecord *letzter = &s_nacht[s_nacht_anzahl - 1];
  // Die Zyklen in DLS 84 tragen genau den Schlaf des zuletzt geschickten Satzes.
  int schlaf = 0;
  for (int i = 0; i < s_log_anzahl; i++) {
    schlaf += prv_ist_schlaf(s_log[i].activity) ? s_log[i].elapsed_sec / 60 : 0;
  }
  printf("\n%s mit Neustarts: Zyklen %d min, Satz %d min (b26 %d), %d Saetze", name, schlaf,
         letzter->schlaf_minuten, s_ref.satz.schlaf_minuten, s_nacht_anzahl);
  cl_assert_equal_i(schlaf, letzter->schlaf_minuten);
  NachtDlsRecord soll = s_ref.satz;
  soll.akku_ende = letzter->akku_ende;
  if (memcmp(letzter, &soll, sizeof(soll))) {
    int erstes = -1;
    for (size_t b = 0; b < sizeof(soll) && erstes < 0; b++) {
      erstes = (((const uint8_t *)letzter)[b] != ((const uint8_t *)&soll)[b]) ? (int)b : -1;
    }
    printf(" (erstes anderes Byte %d: ein %d/%d auf %d/%d ruhepuls %d/%d hrv %d/%d akku %d/%d "
           "stufe %d/%d flags %d/%d)", erstes, letzter->einschlafen, soll.einschlafen,
           letzter->aufwachen, soll.aufwachen, letzter->ruhepuls, soll.ruhepuls, letzter->hrv,
           soll.hrv, letzter->akku_beginn, soll.akku_beginn, letzter->messstufe, soll.messstufe,
           letzter->flags, soll.flags);
  }
  return (Neustarts){
    .saetze_gleich = !memcmp(letzter, &soll, sizeof(soll)),
    .schlaf_diff = letzter->schlaf_minuten - s_ref.satz.schlaf_minuten,
  };
}

// T3.5 mit Neustarts: vor dem Einschlafen, im Schlaf, kurz vor dem Aufwachen, nach "fertig" und
// mitten im gestaffelten Abschluss. Keine Doppelten in DLS 84, kein Satz doppelt, die Zyklen
// gleich dem Satz, der letzte Satz gleich b26. Dazwischen darf ein Neustart einen vorlaeufigen
// Satz ausloesen (die verschluckten Minuten sehen wach aus); Boulder ersetzt ihn (beginn_utc).
void test_boulder_zyklen__neustarts(void) {
  int gleich = 0;
  for (size_t k = 0; k < ARRAY_LENGTH(s_naechte); k++) {
    prv_lade(s_naechte[k]);
    const int ein = s_ref.satz.einschlafen, auf = s_ref.satz.aufwachen;
    const int neu[] = { ein - 30, ein + 100, auf - 20, auf + 30 };
    const Neustarts r = prv_mit_neustarts(s_naechte[k], neu, ARRAY_LENGTH(neu), true, MINUTEN);
    printf(" - letzter Satz %s b26 (Schlaf %+d)", r.saetze_gleich ? "=" : "!=", r.schlaf_diff);
    gleich += r.saetze_gleich;
    cl_assert(abs(r.schlaf_diff) <= 20);
  }
  // 40 zufaellige Morgen mit je zwei zufaelligen Neustarts.
  srand(20261006);
  for (int k = 0; k < 40; k++) {
    const char *name = s_naechte[k % ARRAY_LENGTH(s_naechte)];
    const int auf = 8 * 60 + rand() % (5 * 60);
    int neu[2] = { 60 + rand() % 800, 60 + rand() % 800 };
    if (neu[0] > neu[1]) {
      const int x = neu[0];
      neu[0] = neu[1];
      neu[1] = x;
    }
    neu[1] += (neu[1] < neu[0] + 3) ? 3 : 0;
    prv_mit_neustarts(name, neu, 2, k % 2, auf);
  }
  printf("\nNeustarts: letzter Satz auf %d von %d Naechten = b26\n", gleich,
         (int)ARRAY_LENGTH(s_naechte));
  // Fuenf Neustarts je Nacht, und der letzte Satz ist trotzdem der von b26: Buch, HRV,
  // Protokoll, Ringrest und Kopf tragen die Nacht ueber den Boot (die verschluckten Minuten
  // ueberbrueckt die Rechnung).
  cl_assert_equal_i(gleich, ARRAY_LENGTH(s_naechte));
}

// Spezifikation 7.4: Zeitsprung vor (Luecke ungueltig) und zurueck (nichts verlaengert, bis
// die Zeit aufgeholt hat; doppelte Minuten verworfen).
void test_boulder_zyklen__zeitsprung(void) {
  prv_lade("nacht-2026-09-29");
  int hrv = 0;
  for (int m = 0; m < 20 * 60; m++) {
    if (m == 300) {
      m += 40;  // 40 Minuten vor: kein Satz dazwischen
    }
    prv_schritt(m, MINUTEN, &hrv);
    if (m == 500) {
      // 20 Minuten zurueck: dieselben Minuten noch einmal, mit anderen Werten.
      time_t enden = 0;
      for (int i = 0; i < s_state.activity_sessions_count; i++) {
        const ActivitySession *s = &s_state.activity_sessions[i];
        enden += s->start_utc + s->length_min * 60;
      }
      for (int z = 481; z <= 500; z++) {
        prv_minute(s_ref.beginn_utc, z, 3000, 90, true);
      }
      time_t danach = 0;
      for (int i = 0; i < s_state.activity_sessions_count; i++) {
        const ActivitySession *s = &s_state.activity_sessions[i];
        danach += s->start_utc + s->length_min * 60;
      }
      cl_assert(danach == enden);
    }
  }
  prv_pruefe_log("Zeitsprung");
  cl_assert(s_nacht_anzahl >= 1);
  printf("\nZeitsprung: %d Saetze, letzter Schlaf %d min (ohne Sprung %d)\n", s_nacht_anzahl,
         s_nacht[s_nacht_anzahl - 1].schlaf_minuten, s_ref.satz.schlaf_minuten);
}

// Die Anlage (Spezifikation 4.8, 7.4): Pool voll -> naechster Versuch nach 30 min; die Nacht
// wird erst endgueltig, wenn sie ganz im Datalogging liegt (hier erst nach dem Zeitpunkt, zu
// dem sie sonst endgueltig waere). Ruhezeit um eine Weckzeit -> keine Anlage in [T - 30, T + 5].
// Die Nacht kommt trotzdem, Byte fuer Byte.
void test_boulder_zyklen__anlage(void) {
  prv_lade("nacht-2026-09-26");
  s_anlage_null = 4;
  prv_durch(20 * 60, MINUTEN);
  printf("\nAnlage: Versuche um %d, %d, ... %d (Satz b26 um %d, endgueltig sonst um %d)",
         (int)(s_anlage_um[0] - s_ref.beginn_utc) / 60,
         (int)(s_anlage_um[1] - s_ref.beginn_utc) / 60,
         (int)(s_anlage_um[4] - s_ref.beginn_utc) / 60, s_ref.satz_um[0],
         s_ref.satz.aufwachen + NACHT_WACH_ENDGUELTIG);
  cl_assert_equal_i(s_nacht_anzahl, 1);
  NachtDlsRecord soll = s_ref.satz;
  soll.akku_ende = 80 - s_nacht_um[0] / 60;
  cl_assert(!memcmp(&s_nacht[0], &soll, sizeof(soll)));
  cl_assert_equal_i(s_anlagen_im_minutenteil, 0);
  for (int i = 1; i < 5; i++) {
    cl_assert(s_anlage_um[i] - s_anlage_um[i - 1] >= 30 * PBL_SEC_PER_MIN);
  }
  cl_assert(s_anlage_um[4] > s_ref.beginn_utc +
                                 (s_ref.satz.aufwachen + NACHT_WACH_ENDGUELTIG) * PBL_SEC_PER_MIN);
  prv_pruefe_log("Anlage");

  test_boulder_zyklen__cleanup();
  test_boulder_zyklen__initialize();
  prv_lade("nacht-2026-09-26");
  const time_t weckzeit = s_ref.beginn_utc + (time_t)(s_ref.satz_um[0] + 10) * PBL_SEC_PER_MIN;
  s_weckzeit = weckzeit;
  prv_durch(20 * 60, MINUTEN);
  printf("\nRuhezeit: Weckzeit Minute %d, Anlage um %d\n", s_ref.satz_um[0] + 10,
         (int)(s_anlage_um[0] - s_ref.beginn_utc) / 60);
  cl_assert(s_anlage_um[0] > weckzeit + 5 * PBL_SEC_PER_MIN);
  cl_assert_equal_i(s_nacht_anzahl, 1);
}

// Spezifikation 7.4 "Laden": Am Kabel laeuft kein Minutenteil (das Tracking stoppt). Der Takt
// traegt die Minuten als ungueltig ein, und die Nacht wird trotzdem fertig und geschickt -
// wie in b26 am Fensterende (die 15 Minuten wach zaehlen an vorhandenen Minuten), mit dem
// Flag "geladen" (der Verbrauch sagt dann nichts), sonst Byte fuer Byte wie b26.
void test_boulder_zyklen__am_kabel(void) {
  prv_lade("nacht-2026-09-26");
  int hrv = 0;
  const int kabel_ab = s_ref.satz.aufwachen + 5;
  for (int m = 0; m < kabel_ab; m++) {
    prv_schritt(m, MINUTEN, &hrv);
  }
  s_am_kabel = true;
  for (int m = kabel_ab; m < NACHT_MINUTEN + 30; m++) {
    const time_t jetzt = s_ref.beginn_utc + (time_t)(m + 1) * PBL_SEC_PER_MIN;
    rtc_set_time(jetzt);
    boulder_dauerteil_takt(jetzt + 10);
    fake_system_task_callbacks_invoke_pending();
  }
  printf("\nAm Kabel: %d Saetze, um %d (b26 %d), flags %d\n", s_nacht_anzahl, s_nacht_um[0],
         s_ref.satz_um[0], s_nacht_anzahl ? s_nacht[0].flags : -1);
  cl_assert_equal_i(s_nacht_anzahl, 1);
  NachtDlsRecord soll = s_ref.satz;
  soll.akku_ende = s_nacht[0].akku_ende;
  soll.flags = NACHT_DLS_FLAG_GELADEN;
  cl_assert(!memcmp(&s_nacht[0], &soll, sizeof(soll)));
}
