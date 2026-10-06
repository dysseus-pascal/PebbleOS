/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2, AP7: die strenge Weckregel (Spezifikation 6, T4.2, Kriterium 4) mit dem ECHTEN
// alarm.c (Cron, Schlummer-Timer, nachgeholter Wecker nach dem Boot), der echten
// activity_sessions.c (Pebbles Schlafzustand) und dem echten Live-Zustand der Variante.
//
// Dieselbe Quelle laeuft zweimal (CMakeLists.txt):
// - test_boulder_wecker: alarm.c mit E4 (CONFIG_SERVICE_ACTIVITY_ALGORITHM_BOULDER).
// - test_boulder_wecker_vergleich: alarm.c unveraendert. Er MUSS in einem der Sonderfaelle im
//   Tief wecken - sonst nuetzt E4 nichts und wird dem Nutzer neu vorgelegt (Spezifikation 9).
//
// Jede Minute wie auf der Uhr: Minutenteil der Variante, Pebbles Sitzungs-Minute, KernelBG,
// dann der Wecker zur vollen Minute (Cron oder sein 60-s-Schlummer). Bezug fuer "Tief" sind
// die Endphasen der Referenznaechte (b26 = V2-Endfassung, AP-G).

#include "applib/data_logging.h"
#include <pbl/cron/cron.h>
#include <pbl/drivers/ambient_light.h>
#include <pbl/drivers/rtc.h>
#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/activity_algorithm.h"
#include "pbl/services/activity/activity_private.h"
#include "pbl/services/activity/boulder/nacht.h"
#include "pbl/services/activity/boulder/schlafanalyse.h"
#include "pbl/services/activity/boulder/variante.h"
#include "pbl/services/activity/boulder/wecker.h"
#include "pbl/services/alarms/alarm.h"
#include "pbl/services/battery/battery_state.h"
#include "pbl/services/data_logging/data_logging_service.h"
#include "pbl/services/filesystem/pfs.h"
#include "pbl/services/hrm/hrm_manager_private.h"
#include "pbl/services/time.h"
#include "pbl/util/size.h"
#include "pbl/util/units.h"
#include "process_management/pebble_process_md.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clar.h"

#include "stubs_activity_insights.h"
#include "stubs_alarm_pin.h"
#include "stubs_analytics.h"
#include "stubs_app_install_manager.h"
#include "stubs_hexdump.h"
#include "stubs_hr_util.h"
#include "stubs_i18n.h"
#include "stubs_irq.h"
// Nur Warnungen und Fehler: 258 Naechte mit Info-Logs je Minute waeren ~80 MB Ausgabe.
#define CUSTOM_LOG_INTERNAL
#include "stubs_logging.h"
static void log_internal(uint8_t log_level, const char *src_filename, int src_line_number,
                         const char *fmt, va_list args) {
  if (log_level <= LOG_LEVEL_WARNING) {
    printf("%s:%d> ", GET_FILE_NAME(src_filename), src_line_number);
    vprintf(fmt, args);
    printf("\n");
  }
}
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_sleep.h"
#include "stubs_task_wdt.h"

#include "fake_accel_service.h"
#include "fake_events.h"
#include "fake_new_timer.h"
#include "fake_rtc.h"
#include "fake_spi_flash.h"
#include "fake_system_task.h"

#ifdef CONFIG_SERVICE_ACTIVITY_ALGORITHM_BOULDER
#define STRENG 1
#else
#define STRENG 0
#endif

// =============================================================================================
// Pebble rund um activity_sessions.c, die Glue-Kopie und alarm.c

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
DataLoggingSession *dls_create(uint32_t tag, DataLoggingItemType item_type, uint16_t item_size,
                               bool buffered, bool resume, const Uuid *uuid) {
  return (DataLoggingSession *)(uintptr_t)tag;
}
DataLoggingResult dls_log(DataLoggingSession *logging_session, const void *data,
                          uint32_t num_items) {
  return DATA_LOGGING_SUCCESS;
}
void dls_send_all_sessions(void) {
}

// alarm.c: Pebbles Schlafzustand (activity_metrics.c liest ihn aus dem Zustand des Dienstes)
// und die Bewegung der letzten Minute.
static uint16_t s_letzte_vmc;
bool activity_get_metric(ActivityMetric metric, uint32_t history_len, int32_t *history) {
  switch (metric) {
    case ActivityMetricSleepState:
      *history = s_state.sleep_data.cur_state;
      return true;
    case ActivityMetricSleepStateSeconds:
      *history = s_state.sleep_data.cur_state_elapsed_minutes * PBL_SEC_PER_MIN;
      return true;
    case ActivityMetricLastVMC:
      *history = s_letzte_vmc;
      return true;
    default:
      cl_fail("unerwartete Metrik");
      return false;
  }
}
const PebbleProcessMd *alarms_app_get_info(void) {
  static const PebbleProcessMdSystem s_info = { .common = { .uuid = { 0x67, 0xa3 } } };
  return (const PebbleProcessMd *)&s_info;
}
void event_service_client_subscribe(EventServiceInfo *service_info) {
}
status_t reminder_db_delete_with_parent(const TimelineItemId *id) {
  return S_SUCCESS;
}
void timeline_event_refresh(void) {
}

// Wann der Wecker klingelte (Ereignis von alarm.c); 0 = nicht.
static time_t s_geklingelt;
static void prv_ereignis(PebbleEvent *e) {
  if (e->type == PEBBLE_ALARM_CLOCK_EVENT && s_geklingelt == 0) {
    s_geklingelt = rtc_get_time();
  }
}

// =============================================================================================
// Referenznacht (AP-G): Eingabe und Endphasen

#define MINUTEN SCHLAF_MINUTEN_MAX
#define TIEF '2'

typedef struct {
  uint32_t beginn_utc;
  bool gueltig[MINUTEN];
  uint16_t vmc[MINUTEN];
  uint8_t puls[MINUTEN];
  char phasen[MINUTEN + 1];
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
    } else if (!strcmp(zeile, "phasen")) {
      strcpy(s_ref.phasen, wert);
    }
  }
  fclose(f);
  cl_assert(s_ref.beginn_utc && strlen(s_ref.phasen) == MINUTEN);
}

static bool prv_tief(int m) {
  return m >= 0 && m < MINUTEN && s_ref.phasen[m] == TIEF;
}

static const char *s_naechte[] = {
  "nacht-2026-09-26", "nacht-2026-09-29", "nacht-2026-09-30",
  "nacht-2026-10-03", "nacht-2026-10-05", "nacht-2026-10-05-morgen",
};

// Minute des Fensters (ab 20:00 Ortszeit) als Uhrzeit.
#define UHR(m) (((m) / 60 + 20) % 24), ((m) % 60)

// =============================================================================================
// Uhr: Boot, Minute, Neustart

static AccelSamplingRate s_rate;

static void prv_wecker_boot(void) {
  pbl_cron_init();
  alarm_init();
  alarm_service_enable_alarms(true);
  fake_system_task_callbacks_invoke_pending();
}

// Aus: Cron leer, laufende Timer (Schlummer) gestoppt. Cron behaelt seinen Timer ueber init.
static void prv_wecker_aus(void) {
  pbl_cron_deinit();
  for (TimerID t = stub_new_timer_get_next(); t != TIMER_INVALID_ID;
       t = stub_new_timer_get_next()) {
    stub_new_timer_stop(t);
  }
}

void test_boulder_wecker__initialize(void) {
  TimezoneInfo tz = { .tm_zone = "CEST", .tm_gmtoff = 2 * PBL_SEC_PER_HOUR };
  time_util_update_timezone(&tz);
  fake_rtc_init(100, 1758000000);
  rtc_set_timezone(&tz);
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  pfs_format(false);
  fake_event_init();
  fake_event_set_callback(prv_ereignis);
  memset(&s_state, 0, sizeof(s_state));
  s_letzte_vmc = 0;
  s_geklingelt = 0;
  boulder_dauerteil_test_neustart();
  cl_assert(activity_algorithm_init(&s_rate));
  prv_wecker_boot();
}

void test_boulder_wecker__cleanup(void) {
  fake_system_task_callbacks_invoke_pending();
  activity_algorithm_deinit();
  fake_system_task_callbacks_invoke_pending();
  prv_wecker_aus();
}

static void prv_neu(const char *nacht) {
  test_boulder_wecker__cleanup();
  test_boulder_wecker__initialize();
  prv_lade(nacht);
  // Die Uhr steht am Fensterbeginn, bevor ein Wecker geplant wird.
  rtc_set_time(s_ref.beginn_utc);
}

// Minute m der Referenz wie auf der Uhr; danach steht die Uhr am Beginn von Minute m + 1.
static void prv_minute(int m) {
  const bool gueltig = s_ref.gueltig[m];
  const time_t utc = s_ref.beginn_utc + (time_t)m * PBL_SEC_PER_MIN;
  const BoulderMinute minute = {
    .utc = utc, .vmc = gueltig ? s_ref.vmc[m] : 0, .puls = gueltig ? s_ref.puls[m] : 0,
    .nicht_getragen = !gueltig,
  };
  rtc_set_time(utc + PBL_SEC_PER_MIN);
  boulder_dauerteil_minute(&minute);
  activity_sessions_prv_minute_handler(utc + PBL_SEC_PER_MIN);
  fake_system_task_callbacks_invoke_pending();
  s_letzte_vmc = minute.vmc;
  // Der Wecker: sein Schlummer-Timer (60 s) und der Cron zur vollen Minute, auf KernelBG.
  const TimerID t = stub_new_timer_get_next();
  if (t != TIMER_INVALID_ID && stub_new_timer_timeout(t) <= PBL_SEC_PER_MIN * 1000) {
    stub_new_timer_invoke(1);
  }
  pbl_cron_wakeup();
  fake_system_task_callbacks_invoke_pending();
}

// Ein Neustart, der die Minuten [von, bis) verschluckt: nichts Statisches ueberlebt, Pebbles
// Liste ist weg (wie bei einem Boot ohne gespeicherte Sitzungen), der Wecker bootet neu und
// holt einen verpassten Fensterbeginn nach (alarm.c, ArmedAlarm).
static void prv_neustart(int von, int bis) {
  (void)von;
  fake_system_task_callbacks_invoke_pending();
  activity_algorithm_deinit();
  fake_system_task_callbacks_invoke_pending();
  prv_wecker_aus();
  boulder_dauerteil_test_neustart();
  memset(&s_state, 0, sizeof(s_state));
  s_letzte_vmc = 0;
  rtc_set_time(s_ref.beginn_utc + (time_t)bis * PBL_SEC_PER_MIN);
  cl_assert(activity_algorithm_init(&s_rate));
  prv_wecker_boot();
}

static void prv_smart_alarm(int weckzeit) {
  alarm_create(&(AlarmInfo){ .hour = (weckzeit / 60 + 20) % 24, .minute = weckzeit % 60,
                             .kind = ALARM_KIND_EVERYDAY, .is_smart = true });
}

// Die Minute des Fensters, in der der Wecker klingelte; -1 = nicht.
static int prv_weckminute(void) {
  return s_geklingelt ? (int)((s_geklingelt - s_ref.beginn_utc) / PBL_SEC_PER_MIN) : -1;
}

// Klingeln in Minute k weckt im Tief, wenn k oder die eben fertige Minute k - 1 Tief ist.
static bool prv_im_tief(int k) {
  return prv_tief(k) || prv_tief(k - 1);
}

// =============================================================================================
// T4.2: Weckzeiten 04:00-07:30 alle 5 min auf allen Naechten

void test_boulder_wecker__weckzeiten(void) {
  int laeufe = 0, im_tief = 0, zur_zeit_tief = 0, zu_spaet = 0, vor_zeit = 0;
  for (size_t k = 0; k < ARRAY_LENGTH(s_naechte); k++) {
    for (int weckzeit = 8 * 60; weckzeit <= 11 * 60 + 30; weckzeit += 5) {
      prv_neu(s_naechte[k]);
      prv_smart_alarm(weckzeit);
      for (int m = 0; m <= weckzeit && s_geklingelt == 0; m++) {
        prv_minute(m);
      }
      const int w = prv_weckminute();
      laeufe++;
      cl_assert(w >= 0);
      zu_spaet += w > weckzeit;
      vor_zeit += weckzeit - w;
      // Zur Weckzeit klingelt er immer (Spezifikation 6, "spaetestens"); "nie im Tief" gilt
      // fuer das fruehe Wecken. Wer das ganze Fenster ohne Regung im Tief liegt, wird zur
      // Weckzeit im Tief geweckt - wie von einem gewoehnlichen Wecker.
      if (prv_im_tief(w)) {
        im_tief += w < weckzeit;
        zur_zeit_tief += w == weckzeit;
        printf("\n%s: Wecker %02d:%02d klingelt %02d:%02d im Tief", s_naechte[k], UHR(weckzeit),
               UHR(w));
      }
    }
  }
  printf("\n%s: %d Weckzeiten, %d frueh im Tief, %d zur Weckzeit im Tief, %d nach der "
         "Weckzeit, im Mittel %.1f min davor\n", STRENG ? "streng" : "Pebble", laeufe, im_tief,
         zur_zeit_tief, zu_spaet, (double)vor_zeit / laeufe);
  cl_assert_equal_i(zu_spaet, 0);
#if STRENG
  cl_assert_equal_i(im_tief, 0);
#endif
}

// T4.2 Eigenschaft (Spezifikation 6): in keiner Minute einer Nacht erlaubt die Regel das Wecken
// im Tief. Die Weckzeit liegt weit weg, damit nur S1/W entscheiden. Prueft wecker.c allein und
// laeuft deshalb in beiden Bauten gleich.
void test_boulder_wecker__eigenschaft(void) {
  int erlaubt = 0, verstoss = 0;
  for (size_t k = 0; k < ARRAY_LENGTH(s_naechte); k++) {
    prv_neu(s_naechte[k]);
    for (int m = 0; m < MINUTEN - 1; m++) {
      prv_minute(m);
      const int fern = (m + 12 * 60) % PBL_MIN_PER_DAY;
      if (!boulder_kraepelin_wecken_erlaubt((fern / 60 + 20) % 24, fern % 60)) {
        continue;
      }
      erlaubt++;
      if (prv_im_tief(m + 1)) {
        verstoss++;
        printf("\n%s: erlaubt in %02d:%02d, Tief", s_naechte[k], UHR(m + 1));
      }
    }
  }
  printf("\nEigenschaft: %d erlaubte Minuten, %d davon im Tief\n", erlaubt, verstoss);
  cl_assert(erlaubt > 0);
  cl_assert_equal_i(verstoss, 0);
}

// Gestellt: ein Morgen ohne jede Regung - der Wecker wartet bis genau zur Weckzeit, auch wenn
// ein Neustart ueber den Fensterbeginn Pebbles Zaehler erst 2 min spaeter beginnen laesst.
void test_boulder_wecker__stiller_morgen(void) {
  for (int neustart = 0; neustart <= 1; neustart++) {
    prv_neu("nacht-2026-09-30");
    for (int m = 9 * 60; m < 11 * 60; m++) {
      s_ref.vmc[m] = 0;
    }
    const int weckzeit = 10 * 60 + 30;
    const int fensterbeginn = weckzeit - SMART_ALARM_RANGE_S / PBL_SEC_PER_MIN;
    prv_smart_alarm(weckzeit);
    for (int m = 0; m <= weckzeit + 5 && s_geklingelt == 0; m++) {
      if (neustart && m == fensterbeginn - 1) {
        prv_neustart(m, fensterbeginn + 2);
        m = fensterbeginn + 1;
        continue;
      }
      prv_minute(m);
    }
    printf("\n%s: stiller Morgen%s, Wecker %02d:%02d klingelt %02d:%02d\n",
           STRENG ? "streng" : "Pebble", neustart ? " mit Neustart" : "", UHR(weckzeit),
           UHR(prv_weckminute()));
#if STRENG
    cl_assert_equal_i(prv_weckminute(), weckzeit);
#endif
  }
}

// =============================================================================================
// T4.2 Sonderfaelle: Neustart, Liste voll, Zeitsprung. Jeder Fall legt den Fensterbeginn in den
// laengsten Tiefschlaf-Lauf ab 04:00 (wo es einen gibt).

// Laengster Tief-Lauf in [04:00, 07:30]; Beginn oder -1.
static int prv_tief_lauf(int *laenge) {
  int bester = -1;
  *laenge = 0;
  for (int m = 8 * 60; m <= 11 * 60 + 30; m++) {
    int l = 0;
    while (prv_tief(m + l)) {
      l++;
    }
    if (l > *laenge && (m == 8 * 60 || !prv_tief(m - 1))) {
      bester = m;
      *laenge = l;
    }
  }
  return bester;
}

typedef enum {
  FallNeustartFensterbeginn,  //< aus vor dem Fensterbeginn, an 2 min danach (nachgeholt)
  FallNeustartImFenster,      //< aus und an mitten im Fenster
  FallListeVoll,              //< Pebbles Sitzungsliste ist voll, keine Schlafsitzung
  FallZeitsprung,             //< die Uhr springt im Fenster 10 min vor
  FallAnzahl,
} Fall;

static const char *s_fall_name[] = { "Neustart am Fensterbeginn", "Neustart im Fenster",
                                     "Liste voll", "Zeitsprung" };

// Gibt die Weckminute zurueck (-1 = nicht geklingelt); `weckzeit` aus dem Tief-Lauf.
static int prv_fall(Fall fall, int lauf, int *weckzeit) {
  const int fensterbeginn = lauf + 1;
  *weckzeit = fensterbeginn + SMART_ALARM_RANGE_S / PBL_SEC_PER_MIN;
  prv_smart_alarm(*weckzeit);
  if (fall == FallListeVoll) {
    // Gestellt: 32 kurze Spaziergaenge am Vorabend fuellen Pebbles Liste.
    for (int i = 0; i < ACTIVITY_MAX_ACTIVITY_SESSIONS_COUNT; i++) {
      s_state.activity_sessions[i] = (ActivitySession){
        .start_utc = s_ref.beginn_utc - (time_t)(60 - i) * PBL_SEC_PER_MIN,
        .length_min = 1, .type = ActivitySessionType_Walk,
      };
    }
    s_state.activity_sessions_count = ACTIVITY_MAX_ACTIVITY_SESSIONS_COUNT;
  }
  for (int m = 0; m <= *weckzeit + 5 && s_geklingelt == 0; m++) {
    if (fall == FallNeustartFensterbeginn && m == fensterbeginn - 1) {
      prv_neustart(m, fensterbeginn + 2);
      m = fensterbeginn + 1;
      continue;
    }
    if (fall == FallNeustartImFenster && m == fensterbeginn + 3) {
      prv_neustart(m, fensterbeginn + 5);
      m = fensterbeginn + 4;
      continue;
    }
    if (fall == FallZeitsprung && m == fensterbeginn + 3) {
      // Die Minuten bis +13 fehlen; die Uhr meldet die Aenderung wie clock.c.
      rtc_set_time(s_ref.beginn_utc + (time_t)(fensterbeginn + 13) * PBL_SEC_PER_MIN);
      alarm_handle_clock_change();
      fake_system_task_callbacks_invoke_pending();
      m = fensterbeginn + 12;
      continue;
    }
    prv_minute(m);
  }
  return prv_weckminute();
}

void test_boulder_wecker__sonderfaelle(void) {
  int im_tief = 0, ohne = 0, zu_spaet = 0, neustart_im_fenster = 0;
  for (size_t k = 0; k < ARRAY_LENGTH(s_naechte); k++) {
    for (Fall fall = 0; fall < FallAnzahl; fall++) {
      prv_neu(s_naechte[k]);
      int laenge;
      const int lauf = prv_tief_lauf(&laenge);
      if (lauf < 0 || laenge < 8) {
        break;
      }
      int weckzeit;
      const int w = prv_fall(fall, lauf, &weckzeit);
      const bool tief = w >= 0 && prv_im_tief(w);
      printf("\n%s %s: Tief ab %02d:%02d (%d min), Wecker %02d:%02d, klingelt ", STRENG ?
             "streng" : "Pebble", s_naechte[k], UHR(lauf), laenge, UHR(weckzeit));
      if (w < 0) {
        printf("nicht");
      } else {
        printf("%02d:%02d%s", UHR(w), tief ? " IM TIEF" : "");
      }
      printf(" - %s", s_fall_name[fall]);
      im_tief += tief && w < weckzeit;
      ohne += w < 0;
      zu_spaet += w > weckzeit;
      neustart_im_fenster += fall == FallNeustartImFenster;
    }
  }
  printf("\n%s: %d im Tief, %d ohne Klingeln, %d nach der Weckzeit\n",
         STRENG ? "streng" : "Pebble", im_tief, ohne, zu_spaet);
  // PEBBLE-LUECKE (offen, mit und ohne E4): Nach einem Neustart MITTEN im Fenster klingelt der
  // Smart Alarm an diesem Tag nicht mehr. alarm.c holt nur einen Fensterbeginn nach, der
  // hoechstens 5 min zurueckliegt (alarm_init, ALARM_MISSED_MAX_DELAY_S); nach dem
  // Fensterbeginn ist schon der naechste Tag gespeichert. E4 wird gar nicht gefragt. Jeder
  // andere Fall muss klingeln.
  cl_assert_equal_i(ohne, neustart_im_fenster);
#if STRENG
  cl_assert_equal_i(im_tief, 0);
  cl_assert_equal_i(zu_spaet, 0);
#else
  // Ohne E4 weckt Pebble in mindestens einem Fall im Tief - dafuer gibt es E4.
  cl_assert(im_tief > 0);
#endif
}
