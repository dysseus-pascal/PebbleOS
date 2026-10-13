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
#include "pbl/services/activity/boulder/kalorien.h"
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
// Der Wecklauf (wecklauf.c) prueft test_boulder_wecker mit dem echten alarm.c; hier laeuft
// kein Smart Alarm, die Minute hat nichts zu tun.
void boulder_wecklauf_minute(void) {
}
#include "stubs_analytics.h"
#include "stubs_hexdump.h"
#include "stubs_hr_util.h"
#include "stubs_irq.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
// AP8 (T8.2): Pebbles WTF beim Laden einer Liste mit Typ 8 soll der Test sehen, nicht scheitern.
#define wtf stubs_wtf
#include "stubs_passert.h"
#undef wtf
[[noreturn]] void wtf(void) {
  if (clar_expecting_passert) {
    clar_passert_occurred = true;
    longjmp(clar_passert_jmp_buf, 1);
  }
  stubs_wtf();
}
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
// Das Profil der Pulskalorien (kalorien.c, V3): nur der Ruhepuls kommt aus der Variante.
uint16_t activity_prefs_get_weight_dag(void) {
  return 7000;
}
uint8_t activity_prefs_get_age_years(void) {
  return 41;
}
ActivityGender activity_prefs_get_gender(void) {
  return ActivityGenderMale;
}
bool workout_service_stop_workout(void) {
  return false;
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

// AP9: die schlimmste Nacht des QEMU-Pruefbaus (prueflauf.c) - Puls jede Minute, zehn Zyklen,
// 84 HRV-Fenster, also zwei Saetze 0x4002. Sie muss ganz ins Datalogging kommen.
static void prv_schlimmste_minute(int m, uint16_t *vmc, uint8_t *puls) {
  const uint32_t zufall = (uint32_t)m * 2654435761u;
  if (m < 60 || m >= 900) {
    *vmc = 2400 + zufall % 900;
    *puls = 80 + zufall % 12;
    return;
  }
  const int p = (m - 60) % 84;
  *vmc = (p < 10) ? 40 + zufall % 80 : (p < 38) ? zufall % 6 : (p < 62) ? 20 + zufall % 70
       : (p < 79) ? 5 + zufall % 10 : 600 + zufall % 600;
  *puls = 48 + (p / 4) + zufall % 6;
}

void test_boulder_zyklen__schlimmste_nacht_zwei_hrv_teile(void) {
  prv_lade("nacht-2026-09-26");  // nur fuer das Fenster
  for (int m = 0; m < 19 * 60; m++) {
    uint16_t vmc;
    uint8_t puls;
    prv_schlimmste_minute(m, &vmc, &puls);
    if (m >= 63 && m < 900 && (m - 63) % 10 == 0) {
      boulder_dauerteil_hrv_fenster(s_ref.beginn_utc + (m - 3) * PBL_SEC_PER_MIN,
                                    30 + (m % 50), puls);
    }
    prv_minute(s_ref.beginn_utc, m, vmc, puls, true);
  }
  printf("\nschlimmste Nacht: %d Saetze 0x4001, %d Saetze 0x4002, geschickt %u min",
         s_nacht_anzahl, s_hrv_saetze_anzahl, boulder_ausgabe_gesendet()->schlaf);
  cl_assert(s_nacht_anzahl >= 1);
  cl_assert_equal_i(s_hrv_saetze_anzahl % 2, 0);
  cl_assert(s_hrv_saetze_anzahl >= 2);
  cl_assert_equal_i(s_hrv_saetze[s_hrv_saetze_anzahl - 1].teile, 2);
  cl_assert_equal_i(boulder_ausgabe_gesendet()->schlaf,
                    s_nacht[s_nacht_anzahl - 1].schlaf_minuten);
}

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

// V3 (Schritt 2): Der Ruhepuls fuer Kalorien und Maximalpuls ist der Median der letzten sieben
// Naechte wie in Boulder (Trainingsanalyse.kt ruhepulsAusNaechten), nicht die letzte Nacht. Fuenf
// Referenznaechte hintereinander, nach der dritten ein Neustart. Ihre Ruhepulse (Satz 0x4001 von
// b26, Byte 18): 52, 55, 54, 51, 55. Der Median nach jeder Nacht, von Hand: 52; 53,5 -> 53
// (abgeschnitten wie Boulders toInt); 54; 53; 54. Das Profil der Pulskalorien rechnet damit.
static void prv_neustart(int m, int ab_unterwegs);
void test_boulder_zyklen__ruhepuls_aus_sieben_naechten(void) {
  static const uint8_t soll[] = {52, 53, 54, 53, 54};
  cl_assert_equal_i(boulder_nacht_ruhepuls_median(), 0);
  for (int k = 0; k < 5; k++) {
    prv_lade(s_naechte[k]);
    prv_durch(20 * 60, MINUTEN);
    printf("\n%s: Ruhepuls %d, Median %d", s_naechte[k], boulder_nacht_ruhepuls(),
           boulder_nacht_ruhepuls_median());
    cl_assert_equal_i(boulder_nacht_ruhepuls(), s_ref.satz.ruhepuls);
    cl_assert_equal_i(boulder_nacht_ruhepuls_median(), soll[k]);
    BoulderKalorienProfil profil;
    cl_assert(boulder_kalorien_profil_jetzt(&profil));
    cl_assert_equal_i(profil.ruhe_z, 10 * soll[k]);
    if (k == 2) {
      // Die Naechte stehen in boulder_v2 ("ruhe") und kommen mit "Laden" zurueck.
      prv_neustart(20 * 60 - 1, MINUTEN);
      cl_assert_equal_i(boulder_nacht_ruhepuls_median(), soll[k]);
    }
  }
  // Die fuenfte Nacht noch einmal fertig (spaetere Endfassung derselben Nacht): ihr Wert wird
  // ersetzt, nicht ein sechster angehaengt.
  BoulderRuheRing ring;
  boulder_nacht_ruhe_ablage(&ring);
  int naechte = 0;
  for (int i = 0; i < NACHT_RUHE_NAECHTE; i++) {
    naechte += ring.puls[i] != 0;
  }
  cl_assert_equal_i(naechte, 5);
  printf("\n");
}

// V3: Sieben Naechte, nicht mehr - die achte verdraengt die aelteste. Vorher lagen sieben Naechte
// mit 70 in der Datei; nach jeder neuen faellt eine 70 weg. Median von Hand: 70, 70, 70, dann
// (51 52 54 55 70 70 70) 55 und (51 52 54 55 55 70 70) 55.
void test_boulder_zyklen__ruhepuls_achte_nacht_verdraengt_die_aelteste(void) {
  static const uint8_t soll[] = {70, 70, 70, 55, 55};
  BoulderRuheRing alt = { .neu = 6, .nacht = 1790000000 };
  memset(alt.puls, 70, sizeof(alt.puls));
  boulder_nacht_ruhe_laden(&alt);
  cl_assert_equal_i(boulder_nacht_ruhepuls_median(), 70);
  for (int k = 0; k < 5; k++) {
    prv_lade(s_naechte[k]);
    prv_durch(20 * 60, MINUTEN);
    cl_assert_equal_i(boulder_nacht_ruhepuls_median(), soll[k]);
  }
  BoulderRuheRing ring;
  boulder_nacht_ruhe_ablage(&ring);
  int siebziger = 0;
  for (int i = 0; i < NACHT_RUHE_NAECHTE; i++) {
    siebziger += ring.puls[i] == 70;
  }
  cl_assert_equal_i(siebziger, 2);
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
static void prv_hochfahren(int m, int ab_unterwegs);
static bool s_datei_wie_b25;
static void prv_neustart(int m, int ab_unterwegs) {
  fake_system_task_callbacks_invoke_pending();
  activity_algorithm_deinit();
  fake_system_task_callbacks_invoke_pending();
  pbl_cron_deinit();
  prv_hochfahren(m, ab_unterwegs);
}

// Der Boot selbst: Uhr, Pebbles Bereinigung, init mit dem Durchlauf der Minutendatei, "Laden".
static void prv_hochfahren(int m, int ab_unterwegs) {
  const time_t jetzt = s_ref.beginn_utc + (time_t)(m + 2) * PBL_SEC_PER_MIN;
  rtc_set_time(jetzt);
  activity_sessions_prv_remove_out_of_range_activity_sessions(jetzt, true /*remove_ongoing*/);
  memset(s_anlagen, 0, sizeof(s_anlagen));
  pbl_cron_init();
  boulder_dauerteil_test_neustart();
  cl_assert(activity_algorithm_init(&s_rate));
  // b25s Glue (Kraepelins Original) hielt keine Viertelstunde zurueck (AP8, erster Start).
  int datei_bis = (m + 1) - (m + 1) % 15 - (s_datei_wie_b25 ? 0 : 15);
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

// =============================================================================================
// AP8: der Wechsel von b25/b26 (Spezifikation 8, Kriterium 8, F1)
//
// Die Uhr, wie b25 sie hinterlaesst: die Dateien Byte fuer Byte nach M: geschrieben, nicht ueber
// V2s eigene Strukturen - sonst fiele ein falscher Aufbau auf beiden Seiten gleich aus.

static void prv_le(uint8_t *p, uint32_t v, int n) {
  for (int i = 0; i < n; i++) {
    p[i] = (uint8_t)(v >> (8 * i));
  }
}

// boulder_nacht/"zustand" (M: nacht.c:180-189): Fassung 1, Fenster, erledigt_bis, fest,
// gesendet_beginn, gesendet_aufwachen, gesendet_schlaf.
static void prv_b25_zustand(uint32_t fenster, uint32_t erledigt_bis, uint32_t g_beginn,
                            uint16_t g_auf, uint16_t g_schlaf) {
  uint8_t z[18];
  z[0] = 1;
  prv_le(&z[1], fenster, 4);
  prv_le(&z[5], erledigt_bis, 4);
  z[9] = 2;
  prv_le(&z[10], g_beginn, 4);
  prv_le(&z[14], g_auf, 2);
  prv_le(&z[16], g_schlaf, 2);
  SettingsFile f;
  cl_must_pass(settings_file_open(&f, "boulder_nacht", 2048));
  cl_must_pass(settings_file_set(&f, "zustand", sizeof("zustand"), z, sizeof(z)));
  settings_file_close(&f);
}

// boulder_nacht/"letzte" in Fassung 1 (b25/b26) oder 0 (b24: ein Fuellbyte).
static void prv_b25_bild(const NachtBild *b, uint8_t fassung) {
  NachtBild x = *b;
  x.version = fassung;
  SettingsFile f;
  cl_must_pass(settings_file_open(&f, "boulder_nacht", 2048));
  cl_must_pass(settings_file_set(&f, "letzte", sizeof("letzte"), &x, sizeof(x)));
  settings_file_close(&f);
}

// boulder_hrv (M: hrv.c:32-39, :186-195): Schluessel = Platz im Ring, Wert 8 Byte wie auf der
// Uhr (time_t 32 Bit): start, rmssd, puls, ein Fuellbyte.
static void prv_b25_hrv(uint8_t platz, uint32_t start, uint16_t rmssd, uint8_t puls) {
  uint8_t v[8] = { 0 };
  prv_le(&v[0], start, 4);
  prv_le(&v[4], rmssd, 2);
  v[6] = puls;
  SettingsFile f;
  cl_must_pass(settings_file_open(&f, "boulder_hrv", 4096));
  cl_must_pass(settings_file_set(&f, &platz, 1, v, sizeof(v)));
  settings_file_close(&f);
}

static void prv_sitzung(time_t start, int laenge, int typ, uint16_t steps) {
  ActivitySession s = {
    .start_utc = start, .length_min = (uint16_t)laenge, .type = (ActivitySessionType)typ,
  };
  s.step_data.steps = steps;
  cl_assert(s_state.activity_sessions_count < ACTIVITY_MAX_ACTIVITY_SESSIONS_COUNT);
  s_state.activity_sessions[s_state.activity_sessions_count++] = s;
}

// Die Tief-Abschnitte des Bilds (aufeinanderfolgende als einer), hoechstens `max`.
static int prv_tief_abschnitte(const NachtBild *b, SchlafAbschnitt *t, int max) {
  int n = 0;
  for (int k = 0; k < b->anzahl && n < max; k++) {
    if (b->abschnitte[k].phase != SchlafPhaseTief) {
      continue;
    }
    if (n > 0 && t[n - 1].bis == b->abschnitte[k].von) {
      t[n - 1].bis = b->abschnitte[k].bis;
      continue;
    }
    t[n++] = (SchlafAbschnitt){ .von = b->abschnitte[k].von, .bis = b->abschnitte[k].bis };
  }
  return n;
}

// b25s Sitzungen der endgueltigen Nacht (M: nacht.c:400-432): ein Block vom Einschlafen bis
// zum Aufwachen mit dem Wachliegen in nicht_schlaf_min (wo U: step_data.steps liest), die
// Tief-Abschnitte - mehr als 7 an der kleinsten Luecke zusammengelegt, die Luecke in
// nicht_schlaf_min (M: nacht.c:345-365), beim Einschlafen eine Minute spaeter -, mit `typ8`
// eine Bewegung danach. Pebble hat alles geloggt: die Merker stehen auf den Enden.
static void prv_b25_liste(bool typ8) {
  const NachtBild *b = &s_ref.bild;
  const time_t beginn = b->beginn_utc;
  const int laenge = b->aufwachen - b->einschlafen;
  prv_sitzung(beginn + b->einschlafen * 60, laenge, ActivitySessionType_Sleep,
              (uint16_t)(laenge - b->schlaf_minuten));
  s_state.logged_sleep_activity_exit_at_utc = beginn + b->aufwachen * 60;
  SchlafAbschnitt t[NACHT_BILD_ABSCHNITTE];
  int n = prv_tief_abschnitte(b, t, NACHT_BILD_ABSCHNITTE);
  while (n > 7) {
    int k = 0;
    for (int i = 1; i + 1 < n; i++) {
      k = (t[i + 1].von - t[i].bis < t[k + 1].von - t[k].bis) ? i : k;
    }
    t[k].bis = t[k + 1].bis;
    memmove(&t[k + 1], &t[k + 2], (n - k - 2) * sizeof(t[0]));
    n--;
  }
  for (int k = 0; k < n; k++) {
    const int von = t[k].von + (t[k].von == b->einschlafen);
    // nicht_schlaf_min: die Minuten ohne Tiefschlaf im Block.
    int tief = 0;
    for (int m = von; m < t[k].bis; m++) {
      tief += s_ref.phasen[m] == '2';
    }
    prv_sitzung(beginn + von * 60, t[k].bis - von, ActivitySessionType_RestfulSleep,
                (uint16_t)(t[k].bis - von - tief));
    s_state.logged_restful_sleep_activity_exit_at_utc = beginn + t[k].bis * 60;
  }
  if (typ8) {
    // ActivitySessionType_Movement in b25 (M: activity.h:188).
    prv_sitzung(beginn + (b->aufwachen + 30) * 60, 25, 8, 1500);
    s_state.logged_step_activity_exit_at_utc = beginn + (b->aufwachen + 55) * 60;
  }
}

// F1: Pebble laedt b25s Liste mit Typ 8 - ungueltig fuer U:, die Liste wird in der Datei
// genullt und die Uhr startet neu (WTF). Der zweite Start ist sauber und leer. Echter Code:
// activity_sessions_prv_init.
static void prv_pebble_typ8_boot(void) {
  SettingsFile f;
  cl_must_pass(settings_file_open(&f, "activity_t8", 8192));
  ActivitySettingsKey key = ActivitySettingsKeyStoredActivities;
  cl_must_pass(settings_file_set(&f, &key, sizeof(key), s_state.activity_sessions,
                                 sizeof(s_state.activity_sessions)));
  memset(s_state.activity_sessions, 0, sizeof(s_state.activity_sessions));
  s_state.activity_sessions_count = 0;
  cl_assert_passert(activity_sessions_prv_init(&f, rtc_get_time()));
  memset(s_state.activity_sessions, 0, sizeof(s_state.activity_sessions));
  s_state.activity_sessions_count = 0;
  activity_sessions_prv_init(&f, rtc_get_time());
  cl_assert_equal_i(s_state.activity_sessions_count, 0);
  settings_file_close(&f);
}

// Der erste Start von V2 nach Minute m: der Start aus initialize (vor der Nacht) gilt nicht.
static void prv_v2_erster_start(int m) {
  activity_algorithm_deinit();
  fake_system_task_callbacks_cleanup();
  pbl_cron_deinit();
  s_datei_wie_b25 = true;
  prv_hochfahren(m, MINUTEN);
  s_datei_wie_b25 = false;
}

typedef struct {
  int schlaf;
  int zyklen;
  int tief;
  int sitzungen;
} NachtListe;

// Pebbles Liste: der Schlaf der Referenznacht, jede Sitzung abgeschlossen, Sleep nur auf
// Schlafminuten, RestfulSleep nur auf Tiefschlaf der Referenz.
static NachtListe prv_nacht_liste(void) {
  NachtListe n = { 0 };
  const time_t b = s_ref.beginn_utc;
  // Keine Minute zweimal: nicht in zwei Zyklen, nicht in zwei Tief-Sitzungen (nichts doppelt).
  static uint8_t s_mal[2][NACHT_MINUTEN];
  memset(s_mal, 0, sizeof(s_mal));
  for (int i = 0; i < s_state.activity_sessions_count; i++) {
    const ActivitySession *s = &s_state.activity_sessions[i];
    n.sitzungen++;
    if (!activity_sessions_prv_is_sleep_activity(s->type) || s->start_utc < b ||
        s->start_utc >= b + NACHT_MINUTEN * 60) {
      continue;
    }
    cl_assert(!s->ongoing);
    const int von = (int)(s->start_utc - b) / 60;
    for (int m = von; m < von + s->length_min; m++) {
      cl_assert(m < NACHT_MINUTEN);
      if (s->type == ActivitySessionType_Sleep) {
        cl_assert(prv_schlafphase(s_ref.phasen[m]));
      } else {
        cl_assert_equal_i(s_ref.phasen[m], '2');
      }
      cl_assert_equal_i(s_mal[s->type == ActivitySessionType_Sleep][m]++, 0);
    }
    if (s->type == ActivitySessionType_Sleep) {
      cl_assert_equal_i(s->step_data.steps, 0);
      n.schlaf += s->length_min;
      n.zyklen++;
    } else {
      n.tief++;
    }
  }
  return n;
}

static int prv_laeufe(void) {
  int n = 0;
  for (int m = 0; m < NACHT_MINUTEN; m++) {
    n += prv_schlafphase(s_ref.phasen[m]) && (m == 0 || !prv_schlafphase(s_ref.phasen[m - 1]));
  }
  return n;
}

// Gestellt: die Nacht beginnt mit Tiefschlaf (keine Referenznacht tut das). Der erste Abschnitt
// des Bilds wird Tief, in Bild und Phasen gleich.
static bool s_tief_beim_einschlafen;
static void prv_tief_beim_einschlafen(void) {
  NachtBild *b = &s_ref.bild;
  for (int k = 0; k < b->anzahl; k++) {
    if (b->abschnitte[k].von == b->einschlafen) {
      b->abschnitte[k].phase = SchlafPhaseTief;
      for (int m = b->abschnitte[k].von; m < b->abschnitte[k].bis; m++) {
        s_ref.phasen[m] = '2';
      }
      return;
    }
  }
  cl_fail("kein Abschnitt beim Einschlafen");
}

// Die b25-Nacht ist endgueltig (zwei Stunden wach), das Update kommt eine halbe Stunde danach.
// Zustand, Bild und Liste wie b25 sie hinterlaesst; `typ8`: mit einer Bewegung in der Liste.
static void prv_umzug_nach_der_nacht(const char *name, bool typ8) {
  test_boulder_zyklen__cleanup();
  test_boulder_zyklen__initialize();
  prv_lade(name);
  if (s_tief_beim_einschlafen) {
    s_tief_beim_einschlafen = false;  // nur fuer diesen Durchlauf, auch wenn er scheitert
    prv_tief_beim_einschlafen();
  }
  const uint32_t b = s_ref.beginn_utc;
  const int auf = s_ref.bild.aufwachen;
  const int u = auf + 150;
  // b25: erledigt bis zum Aufwachen, am Fensterende bis dahin (M: nacht.c:1006-1007).
  const uint32_t erledigt = (auf + 120 < NACHT_MINUTEN) ? b + auf * 60 : b + NACHT_MINUTEN * 60;
  prv_b25_zustand(b, erledigt, s_ref.satz.beginn_utc, s_ref.satz.aufwachen,
                  s_ref.satz.schlaf_minuten);
  prv_b25_bild(&s_ref.bild, NACHT_BILD_VERSION);
  prv_b25_liste(typ8);
  rtc_set_time(b + (u + 2) * 60);
  if (typ8) {
    prv_pebble_typ8_boot();
  }
  prv_v2_erster_start(u);
  int hrv = s_ref.hrv_anzahl;
  for (int m = u + 2; m < 20 * 60; m++) {
    prv_schritt(m, MINUTEN, &hrv);
  }
  const NachtListe n = prv_nacht_liste();
  SchlafAbschnitt t[NACHT_BILD_ABSCHNITTE];
  const int tief_bild = prv_tief_abschnitte(&s_ref.bild, t, NACHT_BILD_ABSCHNITTE);
  printf("\n%s %s Typ 8: %d Zyklen (%d min, Satz %d, Laeufe %d), %d Tief (Bild %d), "
         "DLS 84 %d, 0x4001 %d", name, typ8 ? "mit" : "ohne", n.zyklen, n.schlaf,
         s_ref.satz.schlaf_minuten, prv_laeufe(), n.tief, tief_bild, s_log_anzahl,
         s_nacht_anzahl);
  // Tiefschlaf je Abschnitt, ohne b25s Luecken, keiner doppelt (prv_nacht_liste): neu
  // hoechstens ZYKLEN_R_MAX; ohne Typ 8 bleiben b25s eigene dahinter, genau auf ihrem Abschnitt.
  const int tief_neu = (tief_bild < ZYKLEN_R_MAX) ? tief_bild : ZYKLEN_R_MAX;
  if (typ8) {
    cl_assert_equal_i(n.tief, tief_neu);
  } else {
    cl_assert(n.tief >= tief_neu && n.tief <= tief_bild);
  }
  // Die Nacht ist wieder da, ohne das Wachliegen; nichts geht noch einmal ans Telefon.
  cl_assert_equal_i(n.schlaf, s_ref.satz.schlaf_minuten);
  cl_assert(n.zyklen == prv_laeufe() || n.zyklen == ZYKLEN_C_MAX);
  cl_assert_equal_i(s_log_anzahl, 0);
  cl_assert_equal_i(s_nacht_anzahl, 0);
  NachtBild bild;
  cl_assert(boulder_nacht_letzte(&bild));
  cl_assert(!memcmp(&bild, &s_ref.bild, sizeof(bild)));
  // Ein weiterer Neustart am Abend: dieselben Sitzungen, keine dazu, nichts geloggt.
  prv_neustart(20 * 60, MINUTEN);
  const NachtListe n2 = prv_nacht_liste();
  cl_assert_equal_i(n2.schlaf, n.schlaf);
  cl_assert_equal_i(n2.sitzungen, n.sitzungen);
  cl_assert_equal_i(s_log_anzahl, 0);
}

// T8.2: Liste mit Typ 8 - ein WTF, zweiter Start sauber, die Zyklen und der Tiefschlaf der
// letzten b25-Nacht wieder da, nichts doppelt in DLS 84 (F1).
void test_boulder_zyklen__umzug_typ8(void) {
  for (size_t k = 0; k < ARRAY_LENGTH(s_naechte); k++) {
    prv_umzug_nach_der_nacht(s_naechte[k], true);
  }
  printf("\n");
}

// T8.2b: Liste ohne Typ 8 mit b25s Nachtblock - der Block wird ueber denselben Start der erste
// Zyklus, die Summe ohne Wachliegen; b25s Tiefschlaf wird je Abschnitt ersetzt, auch ueber
// dieselben Starts: keiner doppelt, keine Luecke als Tiefschlaf (26.9.: 9 Abschnitte, b25 7).
void test_boulder_zyklen__umzug_ohne_typ8(void) {
  for (size_t k = 0; k < ARRAY_LENGTH(s_naechte); k++) {
    prv_umzug_nach_der_nacht(s_naechte[k], false);
    const time_t block = s_ref.bild.beginn_utc + s_ref.bild.einschlafen * 60;
    int gefunden = 0;
    for (int i = 0; i < s_state.activity_sessions_count; i++) {
      const ActivitySession *s = &s_state.activity_sessions[i];
      gefunden += s->start_utc == block && s->type == ActivitySessionType_Sleep;
    }
    cl_assert_equal_i(gefunden, 1);
  }
  printf("\n");
}

// T8.2/T8.2b gestellt: Tiefschlaf ab dem Einschlafen. Der Tief-Abschnitt beginnt eine Minute
// spaeter (wie bei b25), sonst ersetzte er ueber denselben Start den ersten Zyklus - der Schlaf
// fehlte in der Summe.
void test_boulder_zyklen__umzug_tief_beim_einschlafen(void) {
  for (int typ8 = 0; typ8 < 2; typ8++) {
    s_tief_beim_einschlafen = true;
    prv_umzug_nach_der_nacht(s_naechte[0], typ8);
    const time_t start = s_ref.bild.beginn_utc + s_ref.bild.einschlafen * 60;
    int c1 = 0, r = 0;
    for (int i = 0; i < s_state.activity_sessions_count; i++) {
      const ActivitySession *s = &s_state.activity_sessions[i];
      c1 += s->start_utc == start && s->type == ActivitySessionType_Sleep;
      r += s->start_utc == start + 60 && s->type == ActivitySessionType_RestfulSleep;
    }
    cl_assert_equal_i(c1, 1);
    cl_assert_equal_i(r, 1);
  }
  printf("\n");
}

// T8.1: Bild von b24 ohne Zustand, und b26s Zustand nach "fertig", aber vor "endgueltig":
// V2 rechnet die Nacht selbst (gleicher beginn_utc), schickt sie nicht noch einmal (das
// Gesendete kommt aus dem Zustand bzw. dem Bild) und loggt ihre Zyklen genau einmal.
void test_boulder_zyklen__umzug_zustand(void) {
  for (int fall = 0; fall < 2; fall++) {
    const char *name = s_naechte[fall];
    test_boulder_zyklen__cleanup();
    test_boulder_zyklen__initialize();
    prv_lade(name);
    const uint32_t b = s_ref.beginn_utc;
    const int u = s_ref.bild.aufwachen + 40;
    if (fall == 0) {
      prv_b25_bild(&s_ref.bild, NACHT_BILD_VERSION_B24);
    } else {
      prv_b25_bild(&s_ref.bild, NACHT_BILD_VERSION);
      prv_b25_zustand(b, 0, s_ref.satz.beginn_utc, s_ref.satz.aufwachen,
                      s_ref.satz.schlaf_minuten);
    }
    rtc_set_time(b + (u + 2) * 60);
    prv_v2_erster_start(u);
    NachtBild bild;
    cl_assert(boulder_nacht_letzte(&bild));
    cl_assert_equal_i(bild.version, NACHT_BILD_VERSION);
    cl_assert(!memcmp(&bild, &s_ref.bild, sizeof(bild)));
    int hrv = s_ref.hrv_anzahl;
    for (int m = u + 2; m < 20 * 60; m++) {
      prv_schritt(m, MINUTEN, &hrv);
    }
    printf("\n%s, %s: 0x4001 %d, DLS 84 %d", name, fall ? "Zustand b26" : "Bild b24",
           s_nacht_anzahl, s_log_anzahl);
    cl_assert_equal_i(s_nacht_anzahl, 0);
    prv_pruefe_log(name);
    prv_pruefe_zyklen(name);
  }
  printf("\n");
}

// T8.3: Update mitten in der Nacht (100 min nach dem Einschlafen). Der Zustand kennt nur die
// Nacht davor; b25s HRV-Fenster dieser Nacht gehen in die Rechnung, die der Nacht davor nicht.
// Die Nacht kommt einmal, mit demselben beginn_utc, und ihre HRV-Saetze sind die von b26.
void test_boulder_zyklen__umzug_mitten_in_der_nacht(void) {
  int hrv_gleich = 0;
  for (size_t k = 0; k < ARRAY_LENGTH(s_naechte); k++) {
    test_boulder_zyklen__cleanup();
    test_boulder_zyklen__initialize();
    prv_lade(s_naechte[k]);
    const uint32_t b = s_ref.beginn_utc;
    const int u = s_ref.satz.einschlafen + 100;
    NachtBild vorher = s_ref.bild;
    vorher.beginn_utc -= PBL_SEC_PER_DAY;
    prv_b25_bild(&vorher, NACHT_BILD_VERSION);
    prv_b25_zustand(b - PBL_SEC_PER_DAY, b - PBL_SEC_PER_DAY + vorher.aufwachen * 60,
                    vorher.beginn_utc, vorher.aufwachen, vorher.schlaf_minuten);
    // b25 hat die Nacht davor geloggt; vor Mitternacht gehoert sie noch zu Pebbles Schlaftag,
    // der Umzug legt sie wieder an (ohne sie noch einmal zu senden).
    s_state.logged_sleep_activity_exit_at_utc = vorher.beginn_utc + vorher.aufwachen * 60;
    for (int i = 0; i < vorher.anzahl; i++) {
      if (vorher.abschnitte[i].phase == SchlafPhaseTief) {
        s_state.logged_restful_sleep_activity_exit_at_utc =
            vorher.beginn_utc + vorher.abschnitte[i].bis * 60;
      }
    }
    // Der Ring: drei Fenster der Nacht davor, dann die fertigen dieser Nacht.
    int platz = 0;
    for (int i = 0; i < 3 && i < s_ref.hrv_anzahl; i++) {
      prv_b25_hrv(platz++, b - PBL_SEC_PER_DAY + s_ref.hrv[i].minute * 60, s_ref.hrv[i].rmssd,
                  s_ref.hrv[i].puls);
    }
    int hrv = 0;
    while (hrv < s_ref.hrv_anzahl && s_ref.hrv[hrv].minute + 5 <= u) {
      prv_b25_hrv(platz++, b + s_ref.hrv[hrv].minute * 60, s_ref.hrv[hrv].rmssd,
                  s_ref.hrv[hrv].puls);
      hrv++;
    }
    rtc_set_time(b + (u + 2) * 60);
    prv_v2_erster_start(u);
    for (int m = u + 2; m < 20 * 60; m++) {
      prv_schritt(m, MINUTEN, &hrv);
    }
    prv_pruefe_log(s_naechte[k]);
    cl_assert(s_nacht_anzahl > 0);
    const NachtDlsRecord *letzter = &s_nacht[s_nacht_anzahl - 1];
    cl_assert_equal_i(letzter->beginn_utc, s_ref.satz.beginn_utc);
    int schlaf = 0;
    for (int i = 0; i < s_log_anzahl; i++) {
      cl_assert(s_log[i].start_utc >= b);  // von der Nacht davor nichts noch einmal
      schlaf += prv_ist_schlaf(s_log[i].activity) ? s_log[i].elapsed_sec / 60 : 0;
    }
    cl_assert_equal_i(schlaf, letzter->schlaf_minuten);
    // Die Saetze ohne das Protokoll (b25 hielt es nur im RAM: vor dem Update unbekannt).
    NachtDlsRecord soll = s_ref.satz;
    soll.akku_beginn = letzter->akku_beginn;
    soll.akku_ende = letzter->akku_ende;
    soll.flags = letzter->flags;
    soll.messstufe = letzter->messstufe;
    const bool satz_gleich = !memcmp(letzter, &soll, sizeof(soll));
    // DIE UEBERNAHME: Jedes Fenster, das b25 vor dem Update fertig hatte, steht in der letzten
    // Sendung wie bei b26 - die der Nacht davor nicht.
    const int teile = s_hrv_saetze_anzahl ? s_hrv_saetze[s_hrv_saetze_anzahl - 1].teile : 0;
    const int teile_ref = s_ref.hrv_saetze ? s_ref.hrv_satz[s_ref.hrv_saetze - 1].teile : 0;
    cl_assert((teile > 0) == (teile_ref > 0));  // ohne Fenster kein HRV-Satz, wie b26
    int vor_update = 0, gefunden = 0;
    for (int t = 0; t < teile_ref; t++) {
      const NachtHrvDlsRecord *r = &s_ref.hrv_satz[s_ref.hrv_saetze - 1 - t];
      for (int i = 0; i < r->anzahl; i++) {
        if (r->fenster[i].minute + 5 > u) {
          continue;
        }
        vor_update++;
        for (int v = 0; v < teile; v++) {
          const NachtHrvDlsRecord *x = &s_hrv_saetze[s_hrv_saetze_anzahl - 1 - v];
          for (int j = 0; j < x->anzahl; j++) {
            gefunden += !memcmp(&x->fenster[j], &r->fenster[i], sizeof(NachtHrvDlsFenster));
          }
        }
      }
    }
    bool hrv_saetze_gleich = teile_ref == teile;
    for (int t = 0; hrv_saetze_gleich && t < teile; t++) {
      hrv_saetze_gleich = !memcmp(&s_hrv_saetze[s_hrv_saetze_anzahl - teile + t],
                                  &s_ref.hrv_satz[s_ref.hrv_saetze - teile + t],
                                  sizeof(NachtHrvDlsRecord));
    }
    printf("\n%s, Update in Minute %d: %d Saetze, letzter %s b26 (Schlaf %d/%d), HRV %s b26, "
           "von b25 %d/%d Fenster", s_naechte[k], u, s_nacht_anzahl, satz_gleich ? "=" : "!=",
           letzter->schlaf_minuten, s_ref.satz.schlaf_minuten, hrv_saetze_gleich ? "=" : "!=",
           gefunden, vor_update);
    // Was der Boot verschluckt (b25s Minutendatei endet an der letzten Viertelstunde), wirkt
    // wie ein Neustart unter b25; die Toleranz wie bei den Neustarts (AP4).
    cl_assert(abs(letzter->schlaf_minuten - s_ref.satz.schlaf_minuten) <= 20);
    cl_assert_equal_i(gefunden, vor_update);
    hrv_gleich += vor_update;
  }
  printf("\n");
  // Mindestens eine Nacht hatte vor dem Update HRV-Fenster (sonst prueft das nichts).
  cl_assert(hrv_gleich > 0);
}
