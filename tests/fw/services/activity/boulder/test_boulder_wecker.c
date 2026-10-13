/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2, AP7: die strenge Weckregel (Spezifikation 6, T4.2, Kriterium 4) mit dem ECHTEN
// alarm.c (Cron, Schlummer-Timer, nachgeholter Wecker nach dem Boot), der echten
// activity_sessions.c (Pebbles Schlafzustand) und dem echten Live-Zustand der Variante.
// Dazu der Wecker ueber einen Neustart (Entscheid 07.10.2026, wecklauf.h).
//
// alarm.c ist hier eingebunden (#include unten), nicht nachgebaut: So loescht ein simulierter
// Neustart auch sein RAM (Lauf, Zaehler, Schlummern) wie auf der Uhr; die Datei bleibt.
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
#include "pbl/services/activity/boulder/wecklauf.h"
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

#include "services/alarms/alarm.c"

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

// Ein Schlummern durch den Nutzer (alarm_set_snooze_alarm, 10 min) laeuft bis hier; 0 = keins.
// Der Timer-Fake kennt keine Uhr, deshalb feuert prv_minute_utc es zur rechten Minute.
static time_t s_schlummer_bis;

// Wann der Wecker zuerst klingelte (Ereignis von alarm.c; 0 = nicht) und wie oft.
static time_t s_geklingelt;
static int s_klingeln;
static void prv_ereignis(PebbleEvent *e) {
  if (e->type != PEBBLE_ALARM_CLOCK_EVENT) {
    return;
  }
  s_klingeln++;
  if (s_geklingelt == 0) {
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

// Auf der Uhr gibt event_loop.c die Wecker erst nach alarm_init frei (services_set_runlevel):
// hier `freigabe_s` danach. KernelBG startet einen verpassten Wecker noch einmal `bg_s` spaeter.
static void prv_wecker_boot_verzoegert(int freigabe_s, int bg_s) {
  pbl_cron_init();
  alarm_init();
  rtc_set_time(rtc_get_time() + freigabe_s);
  alarm_service_enable_alarms(true);
  rtc_set_time(rtc_get_time() + bg_s);
  fake_system_task_callbacks_invoke_pending();
}

static void prv_wecker_boot(void) {
  prv_wecker_boot_verzoegert(0, 0);
}

// Das RAM von alarm.c: alle Variablen mit statischer Dauer (test_boulder_wecker__alarm_ram_
// vollstaendig zaehlt sie in alarm.c nach). Abgelegt, bevor alarm.c das erste Mal laeuft, und
// bei jedem Neustart zurueckgeschrieben: alarm.c beginnt dann mit seinen eigenen
// Anfangswerten wie auf der Uhr, der Test schreibt sie nicht nach. Die Datei "alarms" bleibt.
// clang-format off
#define ALARM_RAM(X)                                                                         \
  X(s_alarms_enabled) X(s_next_alarm_time) X(s_next_alarm) X(s_next_alarm_cron)               \
  X(s_most_recent_alarm_id) X(s_most_recent_alarm_config) X(s_most_recent_alarm_recorded)     \
  X(s_snooze_timer_id) X(s_snooze_delay_m) X(s_user_snoozed) X(s_smart_snooze_counter)        \
  X(s_armed_record) X(s_missed_alarm_id) X(s_missed_alarm_time) X(s_mutex)                    \
  X(s_language_change_event_info) X(s_snooze_generation)
// clang-format on
#define ALARM_RAM_FELD(v) __typeof__(v) v;
#define ALARM_RAM_SICHERN(v) s_alarm_ram_anfang.v = v;
#define ALARM_RAM_ZURUECK(v) v = s_alarm_ram_anfang.v;
#define ALARM_RAM_EINS(v) +1
#define ALARM_RAM_ZAHL (0 ALARM_RAM(ALARM_RAM_EINS))
static struct {
  ALARM_RAM(ALARM_RAM_FELD)
} s_alarm_ram_anfang;
static bool s_alarm_ram_gesichert;

static void prv_alarm_ram_weg(void) {
  cl_assert(s_alarm_ram_gesichert);
  ALARM_RAM(ALARM_RAM_ZURUECK)
}

// Aus: Cron leer, laufende Timer (Schlummer) gestoppt, das RAM von alarm.c weg.
static void prv_wecker_aus(void) {
  pbl_cron_deinit();
  for (TimerID t = stub_new_timer_get_next(); t != TIMER_INVALID_ID;
       t = stub_new_timer_get_next()) {
    stub_new_timer_stop(t);
  }
  prv_alarm_ram_weg();
}

void test_boulder_wecker__initialize(void) {
  if (!s_alarm_ram_gesichert) {
    ALARM_RAM(ALARM_RAM_SICHERN)
    s_alarm_ram_gesichert = true;
  }
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
  s_klingeln = 0;
  s_schlummer_bis = 0;
  boulder_dauerteil_test_neustart();
  // Der Test feuert den naechsten Timer selbst (Schlummer); der Takt-Cron (AP4) stuende davor.
  boulder_dauerteil_test_ohne_cron(true);
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

// Die Minute ab `utc` wie auf der Uhr; danach steht die Uhr am Beginn der naechsten.
static void prv_minute_utc(time_t utc, bool gueltig, uint16_t vmc, uint8_t puls) {
  const BoulderMinute minute = {
    .utc = utc, .vmc = gueltig ? vmc : 0, .puls = gueltig ? puls : 0, .nicht_getragen = !gueltig,
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
  if (s_schlummer_bis && rtc_get_time() >= s_schlummer_bis) {
    s_schlummer_bis = 0;
    stub_new_timer_fire(s_snooze_timer_id);
  }
  pbl_cron_wakeup();
  fake_system_task_callbacks_invoke_pending();
}

// Minute m der Referenz wie auf der Uhr; danach steht die Uhr am Beginn von Minute m + 1.
static void prv_minute(int m) {
  prv_minute_utc(s_ref.beginn_utc + (time_t)m * PBL_SEC_PER_MIN, s_ref.gueltig[m], s_ref.vmc[m],
                 s_ref.puls[m]);
}

// Ein Neustart, der die Minuten [von, bis) verschluckt: nichts Statisches ueberlebt, Pebbles
// Liste ist weg (wie bei einem Boot ohne gespeicherte Sitzungen), der Wecker bootet neu und
// holt einen verpassten Fensterbeginn nach (alarm.c, ArmedAlarm).
static void prv_neustart_um(time_t an_utc, int freigabe_s, int bg_s) {
  fake_system_task_callbacks_invoke_pending();
  activity_algorithm_deinit();
  fake_system_task_callbacks_invoke_pending();
  prv_wecker_aus();
  boulder_dauerteil_test_neustart();
  memset(&s_state, 0, sizeof(s_state));
  s_letzte_vmc = 0;
  s_schlummer_bis = 0;
  rtc_set_time(an_utc);
  cl_assert(activity_algorithm_init(&s_rate));
  prv_wecker_boot_verzoegert(freigabe_s, bg_s);
}

static void prv_neustart(int von, int bis) {
  (void)von;
  prv_neustart_um(s_ref.beginn_utc + (time_t)bis * PBL_SEC_PER_MIN, 0, 0);
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
      if (!boulder_wecker_erlaubt(rtc_get_time() + 12 * PBL_SEC_PER_HOUR)) {
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
  (void)neustart_im_fenster;
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
#if STRENG
  // Seit dem Entscheid vom 07.10.2026 klingelt er auch nach einem Neustart mitten im Fenster
  // (wecklauf.h).
  cl_assert_equal_i(ohne, 0);
  cl_assert_equal_i(im_tief, 0);
  cl_assert_equal_i(zu_spaet, 0);
#else
  // PEBBLE-LUECKE: Nach einem Neustart MITTEN im Fenster klingelt der Smart Alarm an diesem Tag
  // nicht mehr. alarm.c holt nur einen Fensterbeginn nach, der hoechstens 5 min zurueckliegt
  // (alarm_init, ALARM_MISSED_MAX_DELAY_S); nach dem Fensterbeginn ist schon der naechste Tag
  // gespeichert. Jeder andere Fall klingelt.
  cl_assert_equal_i(ohne, neustart_im_fenster);
  // Ohne E4 weckt Pebble in mindestens einem Fall im Tief - dafuer gibt es E4.
  cl_assert(im_tief > 0);
#endif
}

// =============================================================================================
// Neustart (Entscheid 07.10.2026, wecklauf.h): vor, im und nach dem Fenster. Der Bau mit E4
// muss klingeln, wie entschieden; der Vergleichsbau zeigt, was Pebble allein tut.

#define FENSTER (SMART_ALARM_RANGE_S / PBL_SEC_PER_MIN)
// Das Nachholfenster in Minuten, wie entschieden (07.10.2026) - bewusst nicht aus wecker.h.
#define NACHHOLEN 30

void test_boulder_wecker__konstanten(void) {
  cl_assert_equal_i(WECKER_NACHHOLEN_S, NACHHOLEN * PBL_SEC_PER_MIN);
  cl_assert_equal_i(FENSTER, 30);
  // W erst, wenn der Live-Automat nach einer Luecke wieder einschlafen konnte: 10 + 2 Minuten.
  cl_assert_equal_i(WECKER_W_AB, 12);
  // wecklauf.c legt den Start eines nachgeholten Weckers nach Pebbles Grenze fuer verpasste.
  cl_assert_equal_i(WECKLAUF_PEBBLE_VERPASST_S, ALARM_MISSED_MAX_DELAY_S);
}

// Alle Variablen von alarm.c mit statischer Dauer muessen in ALARM_RAM stehen, sonst ueberlebte
// im Test etwas einen Neustart, das auf der Uhr weg ist. Gezaehlt werden die Zeilen, die mit
// "static " beginnen und keine Funktion sind (die zwei Tageslisten in einer Funktion sind
// const und eingerueckt).
void test_boulder_wecker__alarm_ram_vollstaendig(void) {
  char pfad[512];
  snprintf(pfad, sizeof(pfad), "%s/../../fw/services/alarms/alarm.c", CLAR_FIXTURE_PATH);
  FILE *f = fopen(pfad, "r");
  cl_assert(f);
  static char zeile[512];
  int zahl = 0;
  while (fgets(zeile, sizeof(zeile), f)) {
    zahl += !strncmp(zeile, "static ", 7) &&
            (!strchr(zeile, '(') || strstr(zeile, "PBL_MUTEX_DEFINE("));
  }
  fclose(f);
  cl_assert_equal_i(zahl, ALARM_RAM_ZAHL);
}

// Laesst die Minuten [von, bis) laufen. In Minute `aus` geht die Uhr aus und startet zu Beginn
// von Minute `an` neu (aus = -1: kein Neustart; an < aus: die Uhr geht danach nach).
static void prv_laufen(int von, int bis, int aus, int an) {
  for (int m = von; m < bis; m++) {
    if (m == aus) {
      prv_neustart(aus, an);
      aus = -1;
      m = an - 1;
      continue;
    }
    prv_minute(m);
  }
}

// Gestellt: Weckzeit 06:30 (Minute 630) in der Nacht vom 30.9., von 40 min davor bis eine
// Stunde danach ohne Regung - so weckt nur die Weckzeit selbst, ausser in Minute `bewegt`
// (>= 0) regt sich etwas.
#define T_STILL (10 * 60 + 30)
#define W_STILL (T_STILL - FENSTER)
static AlarmId prv_stiller_morgen(AlarmKind art, int bewegt) {
  prv_neu("nacht-2026-09-30");
  for (int m = W_STILL - 10; m < T_STILL + NACHHOLEN + 60; m++) {
    s_ref.vmc[m] = 0;
  }
  if (bewegt >= 0) {
    s_ref.gueltig[bewegt] = true;
    s_ref.vmc[bewegt] = 40;
  }
  return alarm_create(&(AlarmInfo){ .hour = (T_STILL / 60 + 20) % 24, .minute = T_STILL % 60,
                                    .kind = art, .is_smart = true });
}

// Ende jedes Neustart-Falls: so lange laufen lassen, dass ein zweites Klingeln auffiele.
#define BIS_STILL (T_STILL + NACHHOLEN + 10)

static void prv_bericht(const char *fall, int aus, int an) {
  printf("\n%s %s: aus %02d:%02d an %02d:%02d, klingelt ", STRENG ? "streng" : "Pebble", fall,
         UHR(aus), UHR(an));
  if (s_klingeln == 0) {
    printf("nicht");
  } else {
    printf("%02d:%02d (%dx)", UHR(prv_weckminute()), s_klingeln);
  }
}

// Vor dem Fenster: Pebble plant ihn wie immer, genau einmal. Streng dieselbe Weckminute wie
// ohne Neustart; Pebble allein klingelt nach einem Neustart sofort, sein Zustand ist "wach".
void test_boulder_wecker__neustart_vor_fenster(void) {
  int faelle = 0;
  for (size_t k = 0; k < ARRAY_LENGTH(s_naechte); k++) {
    prv_neu(s_naechte[k]);
    int laenge;
    const int lauf = prv_tief_lauf(&laenge);
    if (lauf < 0 || laenge < 8) {
      continue;
    }
    const int weckzeit = lauf + 1 + FENSTER;
    int w[2];
    for (int neustart = 0; neustart <= 1; neustart++) {
      prv_neu(s_naechte[k]);
      prv_smart_alarm(weckzeit);
      const int aus = neustart ? weckzeit - FENSTER - 10 : -1;
      prv_laufen(0, weckzeit + NACHHOLEN + 10, aus, weckzeit - FENSTER - 8);
      w[neustart] = prv_weckminute();
      cl_assert_equal_i(s_klingeln, 1);
    }
    printf("\n%s %s: vor dem Fenster neu gestartet, klingelt %02d:%02d (ohne Neustart "
           "%02d:%02d)", STRENG ? "streng" : "Pebble", s_naechte[k], UHR(w[1]), UHR(w[0]));
#if STRENG
    cl_assert_equal_i(w[1], w[0]);
#endif
    faelle++;
  }
  cl_assert(faelle > 0);
}

// Im Fenster, auf allen Naechten: Neustart, bevor der Wecker ohne Neustart klingeln wuerde
// (bis zu fuenf Minuten zwischen Fensterbeginn und dieser Weckminute), 1-2 min aus. Genau
// einmal, nie frueh im Tief, spaetestens zur Weckzeit. Weckzeiten: Fensterbeginn im laengsten
// Tief-Lauf (wo es einen gibt) und 04:30 bis 07:30 jede halbe Stunde.
void test_boulder_wecker__neustart_im_fenster(void) {
  int laeufe = 0, ohne = 0, doppelt = 0, im_tief = 0, zu_spaet = 0;
  for (size_t k = 0; k < ARRAY_LENGTH(s_naechte); k++) {
    prv_neu(s_naechte[k]);
    int laenge;
    const int lauf = prv_tief_lauf(&laenge);
    for (int z = -1; z <= 6; z++) {
      if (z < 0 && (lauf < 0 || laenge < 8)) {
        continue;
      }
      const int weckzeit = (z < 0) ? lauf + 1 + FENSTER : 8 * 60 + 30 + 30 * z;
      const int fensterbeginn = weckzeit - FENSTER;
      // Ohne Neustart: wann klingelt er?
      prv_neu(s_naechte[k]);
      prv_smart_alarm(weckzeit);
      prv_laufen(0, weckzeit + 1, -1, -1);
      const int w0 = prv_weckminute();
      cl_assert(w0 >= fensterbeginn && w0 <= weckzeit);
      for (int i = 0; i < 5; i++) {
        const int aus = fensterbeginn + ((w0 - fensterbeginn) * i) / 5;
        if (aus >= w0 || (i > 0 && aus == fensterbeginn + ((w0 - fensterbeginn) * (i - 1)) / 5)) {
          continue;
        }
        const int an = aus + 1 + i % 2;
        prv_neu(s_naechte[k]);
        prv_smart_alarm(weckzeit);
        prv_laufen(0, weckzeit + NACHHOLEN + 10, aus, an);
        const int w = prv_weckminute();
        laeufe++;
        ohne += w < 0;
        doppelt += s_klingeln > 1;
        im_tief += w >= 0 && w < weckzeit && prv_im_tief(w);
        zu_spaet += w > weckzeit;
        if (w < 0 || w > weckzeit || s_klingeln > 1 || (w < weckzeit && prv_im_tief(w))) {
          prv_bericht(s_naechte[k], aus, an);
        }
      }
    }
  }
  printf("\n%s: %d Neustarts im Fenster vor dem Klingeln, %d ohne Klingeln, %d doppelt, "
         "%d frueh im Tief, %d nach der Weckzeit\n", STRENG ? "streng" : "Pebble", laeufe, ohne,
         doppelt, im_tief, zu_spaet);
  cl_assert(laeufe >= 60);
  cl_assert_equal_i(doppelt, 0);
#if STRENG
  cl_assert_equal_i(ohne, 0);
  cl_assert_equal_i(im_tief, 0);
  cl_assert_equal_i(zu_spaet, 0);
#else
  // Pebble: Wer vor dem Klingeln neu startet, wird an diesem Tag nicht mehr geweckt.
  cl_assert_equal_i(ohne, laeufe);
#endif
}

// Eigenschaft wie oben, aber mit Neustarts: Nach einem Neustart alle 40 min (1-3 min aus)
// erlaubt die Regel in keiner Minute das Wecken im Tief. Die Weckzeit liegt weit weg.
void test_boulder_wecker__eigenschaft_mit_neustart(void) {
  int erlaubt = 0, verstoss = 0, neustarts = 0;
  for (size_t k = 0; k < ARRAY_LENGTH(s_naechte); k++) {
    prv_neu(s_naechte[k]);
    for (int m = 0; m < MINUTEN - 1; m++) {
      if (m > 0 && m % 40 == 0 && m + 3 < MINUTEN - 1) {
        const int an = m + 1 + (m / 40) % 3;
        prv_neustart(m, an);
        neustarts++;
        m = an - 1;
        continue;
      }
      prv_minute(m);
      if (!boulder_wecker_erlaubt(rtc_get_time() + 12 * PBL_SEC_PER_HOUR)) {
        continue;
      }
      erlaubt++;
      if (prv_im_tief(m + 1)) {
        verstoss++;
        printf("\n%s: erlaubt in %02d:%02d nach Neustart, Tief", s_naechte[k], UHR(m + 1));
      }
    }
  }
  printf("\nEigenschaft mit %d Neustarts: %d erlaubte Minuten, %d davon im Tief\n", neustarts,
         erlaubt, verstoss);
  cl_assert(erlaubt > 0);
  cl_assert_equal_i(verstoss, 0);
}

// Die Uhr war ueber den Fensterbeginn aus: bis 5 min holt Pebble nach, danach wecklauf.c.
void test_boulder_wecker__neustart_ueber_fensterbeginn(void) {
  static const int s_an[] = { 2, 5, 6, 10, 20, 29 };
  for (size_t k = 0; k < ARRAY_LENGTH(s_naechte); k++) {
    for (size_t i = 0; i < ARRAY_LENGTH(s_an); i++) {
      prv_neu(s_naechte[k]);
      int laenge;
      const int lauf = prv_tief_lauf(&laenge);
      if (lauf < 0 || laenge < 8) {
        break;
      }
      const int fensterbeginn = lauf + 1, weckzeit = fensterbeginn + FENSTER;
      prv_smart_alarm(weckzeit);
      const int an = fensterbeginn + s_an[i];
      prv_laufen(0, weckzeit + NACHHOLEN + 10, fensterbeginn - 1, an);
      prv_bericht(s_naechte[k], fensterbeginn - 1, an);
#if STRENG
      const int w = prv_weckminute();
      cl_assert_equal_i(s_klingeln, 1);
      cl_assert(w <= weckzeit);
      cl_assert(w == weckzeit || !prv_im_tief(w));
#else
      cl_assert_equal_i(s_klingeln, s_an[i] <= 5 ? 1 : 0);
#endif
    }
  }
}

// Nach der Weckzeit, noch nicht geklungen: sofort beim Start, aber nur im Nachholfenster -
// knapp davor und knapp danach. Der Lauf lief vor dem Neustart (`lief`) oder die Uhr war schon
// ueber den Fensterbeginn aus.
void test_boulder_wecker__neustart_nach_weckzeit(void) {
  static const int s_an[] = { 0, 1, 15, NACHHOLEN - 1, NACHHOLEN, NACHHOLEN + 1, NACHHOLEN + 20 };
  for (int lief = 0; lief <= 1; lief++) {
    for (size_t i = 0; i < ARRAY_LENGTH(s_an); i++) {
      prv_stiller_morgen(ALARM_KIND_EVERYDAY, -1);
      const int aus = lief ? T_STILL - 2 : W_STILL - 1, an = T_STILL + s_an[i];
      prv_laufen(0, an + NACHHOLEN + 10, aus, an);
      prv_bericht(lief ? "Lauf unterbrochen" : "ueber Fensterbeginn aus", aus, an);
#if STRENG
      if (s_an[i] < NACHHOLEN) {
        cl_assert_equal_i(s_klingeln, 1);
        cl_assert_equal_i(prv_weckminute(), an);
      } else {
        cl_assert_equal_i(s_klingeln, 0);
      }
#else
      cl_assert_equal_i(s_klingeln, 0);
#endif
    }
  }
}

// Schon geklungen (Regung in Minute W+10, er klingelt W+11): kein zweites Mal - weggedrueckt,
// liegen gelassen, Neustart im Fenster, zur Weckzeit oder danach. Am naechsten Morgen wieder.
void test_boulder_wecker__schon_geklungen(void) {
  static const struct {
    bool weggedrueckt;
    int an;
  } s_fall[] = {
    { true, W_STILL + 16 }, { false, W_STILL + 16 }, { true, T_STILL }, { false, T_STILL + 5 },
  };
  for (size_t i = 0; i < ARRAY_LENGTH(s_fall); i++) {
    prv_stiller_morgen(ALARM_KIND_EVERYDAY, W_STILL + 10);
    prv_laufen(0, W_STILL + 13, -1, -1);
    cl_assert_equal_i(prv_weckminute(), W_STILL + 11);
    if (s_fall[i].weggedrueckt) {
      alarm_dismiss_alarm();
    }
    prv_laufen(W_STILL + 13, BIS_STILL, W_STILL + 15, s_fall[i].an);
    prv_bericht(s_fall[i].weggedrueckt ? "geklungen, weggedrueckt" : "geklungen, liegen gelassen",
                W_STILL + 15, s_fall[i].an);
    cl_assert_equal_i(s_klingeln, 1);
  }

  // Der naechste Morgen: Der Lauf von gestern haelt ihn nicht auf, auch nicht mit Neustart.
  const time_t morgen = s_ref.beginn_utc + (time_t)(W_STILL + PBL_MIN_PER_DAY) * PBL_SEC_PER_MIN;
  for (time_t utc = s_ref.beginn_utc + (time_t)BIS_STILL * PBL_SEC_PER_MIN;
       utc < morgen + 10 * PBL_SEC_PER_MIN; utc += PBL_SEC_PER_MIN) {
    prv_minute_utc(utc, true, 0, 60);
  }
  prv_neustart(0, W_STILL + PBL_MIN_PER_DAY + 11);
  for (time_t utc = morgen + 11 * PBL_SEC_PER_MIN; utc < morgen + 40 * PBL_SEC_PER_MIN;
       utc += PBL_SEC_PER_MIN) {
    prv_minute_utc(utc, true, 0, 60);
  }
  printf("\n%s naechster Morgen mit Neustart im Fenster: %dx geklungen\n",
         STRENG ? "streng" : "Pebble", s_klingeln);
#if STRENG
  cl_assert_equal_i(s_klingeln, 2);
#else
  cl_assert_equal_i(s_klingeln, 1);
#endif
}

// Schlummern bleibt wie bei Pebble: nach 10 min noch einmal; ein Neustart im Schlummern
// verliert es, und nachgeholt wird dann nichts (er hat ja geklungen).
void test_boulder_wecker__schlummern(void) {
  for (int neustart = 0; neustart <= 1; neustart++) {
    prv_stiller_morgen(ALARM_KIND_EVERYDAY, W_STILL + 10);
    prv_laufen(0, W_STILL + 12, -1, -1);
    cl_assert_equal_i(s_klingeln, 1);
    alarm_set_snooze_alarm();
    s_schlummer_bis = rtc_get_time() + alarm_get_snooze_delay() * PBL_SEC_PER_MIN;
    prv_laufen(W_STILL + 12, BIS_STILL, neustart ? W_STILL + 15 : -1, W_STILL + 16);
    prv_bericht(neustart ? "geschlummert, Neustart" : "geschlummert", W_STILL + 15,
                W_STILL + 16);
    cl_assert_equal_i(s_klingeln, neustart ? 1 : 2);
  }
}

// Im Lauf ausgeschaltet, geloescht, umgestellt oder nicht mehr smart, dann Neustart: nichts
// nachholen. Ein gewoehnlicher Wecker klingelt zu seiner Zeit, wie bei Pebble.
typedef enum {
  AenderungAus,
  AenderungGeloescht,
  AenderungZeit,
  AenderungNichtSmart,
  AenderungAnzahl,
} Aenderung;

void test_boulder_wecker__ausgeschaltet(void) {
  static const char *s_name[] = { "ausgeschaltet", "geloescht", "auf 07:30 gestellt",
                                  "nicht mehr smart" };
  for (Aenderung a = 0; a < AenderungAnzahl; a++) {
    const AlarmId id = prv_stiller_morgen(ALARM_KIND_EVERYDAY, -1);
    prv_laufen(0, W_STILL + 5, -1, -1);
    switch (a) {
      case AenderungAus:
        alarm_set_enabled(id, false);
        break;
      case AenderungGeloescht:
        alarm_delete(id);
        break;
      case AenderungZeit:
        alarm_set_time(id, (T_STILL / 60 + 21) % 24, T_STILL % 60);
        break;
      case AenderungNichtSmart:
        alarm_set_smart(id, false);
        break;
      default:
        break;
    }
    fake_system_task_callbacks_invoke_pending();
    prv_laufen(W_STILL + 5, BIS_STILL, W_STILL + 8, W_STILL + 9);
    prv_bericht(s_name[a], W_STILL + 8, W_STILL + 9);
    if (a == AenderungNichtSmart) {
      cl_assert_equal_i(s_klingeln, 1);
      cl_assert_equal_i(prv_weckminute(), T_STILL);
    } else {
      cl_assert_equal_i(s_klingeln, 0);
    }
  }
}

// Einmalwecker: alarm.c schaltet ihn beim Fensterbeginn aus; nachgeholt wird er trotzdem, und
// danach bleibt er aus.
void test_boulder_wecker__einmalwecker(void) {
  // Neustart im Fenster: zur Weckzeit (still).
  AlarmId id = prv_stiller_morgen(ALARM_KIND_JUST_ONCE, -1);
  prv_laufen(0, BIS_STILL, W_STILL + 10, W_STILL + 11);
  prv_bericht("Einmalwecker, Neustart im Fenster", W_STILL + 10, W_STILL + 11);
#if STRENG
  cl_assert_equal_i(s_klingeln, 1);
  cl_assert_equal_i(prv_weckminute(), T_STILL);
#else
  cl_assert_equal_i(s_klingeln, 0);
#endif
  cl_assert(!alarm_get_enabled(id));

  // Geklungen, dann Neustart: nicht noch einmal.
  id = prv_stiller_morgen(ALARM_KIND_JUST_ONCE, W_STILL + 10);
  prv_laufen(0, BIS_STILL, W_STILL + 15, W_STILL + 16);
  prv_bericht("Einmalwecker geklungen", W_STILL + 15, W_STILL + 16);
  cl_assert_equal_i(s_klingeln, 1);
  cl_assert(!alarm_get_enabled(id));

  // Ueber den Fensterbeginn aus, an nach der Weckzeit: sofort, danach aus. Pebble allein
  // verschiebt ihn auf morgen.
  id = prv_stiller_morgen(ALARM_KIND_JUST_ONCE, -1);
  prv_laufen(0, BIS_STILL, W_STILL - 1, T_STILL + 5);
  prv_bericht("Einmalwecker, ueber Fensterbeginn aus", W_STILL - 1, T_STILL + 5);
#if STRENG
  cl_assert_equal_i(s_klingeln, 1);
  cl_assert_equal_i(prv_weckminute(), T_STILL + 5);
  cl_assert(!alarm_get_enabled(id));
#else
  cl_assert_equal_i(s_klingeln, 0);
  cl_assert(alarm_get_enabled(id));
#endif

  // Im Fenster geloescht, dann Neustart: nichts.
  id = prv_stiller_morgen(ALARM_KIND_JUST_ONCE, -1);
  prv_laufen(0, W_STILL + 5, -1, -1);
  alarm_delete(id);
  prv_laufen(W_STILL + 5, BIS_STILL, W_STILL + 8, W_STILL + 9);
  prv_bericht("Einmalwecker geloescht", W_STILL + 8, W_STILL + 9);
  cl_assert_equal_i(s_klingeln, 0);
}

// Zeitspruenge: das Telefon stellt nach dem Neustart die Uhr, die Uhr geht nach dem Neustart
// nach (vor dem Fenster), die Zeitzone wechselt im Lauf vor dem Neustart, die Uhr springt beim
// Neustart ueber das Nachholfenster.
void test_boulder_wecker__zeitsprung(void) {
  // Telefon stellt 20 s vor (wie event_loop.c bei > 15 s): einmal, zur Weckzeit.
  prv_stiller_morgen(ALARM_KIND_EVERYDAY, -1);
  prv_laufen(0, W_STILL + 12, W_STILL + 10, W_STILL + 11);
  rtc_set_time(rtc_get_time() + 20);
  alarm_handle_clock_change();
  fake_system_task_callbacks_invoke_pending();
  prv_laufen(W_STILL + 12, BIS_STILL, -1, -1);
  prv_bericht("Neustart, dann stellt das Telefon die Uhr", W_STILL + 10, W_STILL + 11);
#if STRENG
  cl_assert_equal_i(s_klingeln, 1);
  cl_assert_equal_i(prv_weckminute(), T_STILL);
#else
  cl_assert_equal_i(s_klingeln, 0);
#endif

  // Nach dem Neustart steht die Uhr vor dem Fenster (sie ging nach): kein Nachholen, Pebble
  // plant das Fenster neu - einmal, zur Weckzeit. Die Regung 5 min vor dem Fenster weckt nicht:
  // Vor dem Fenster laeuft kein Lauf, auch nicht der von vor dem Neustart.
  prv_stiller_morgen(ALARM_KIND_EVERYDAY, W_STILL - 5);
  prv_laufen(0, BIS_STILL, W_STILL + 5, W_STILL - 10);
  prv_bericht("Neustart, Uhr danach vor dem Fenster", W_STILL + 5, W_STILL - 10);
  cl_assert_equal_i(s_klingeln, 1);
  cl_assert(prv_weckminute() >= W_STILL);
#if STRENG
  cl_assert_equal_i(prv_weckminute(), T_STILL);
#endif

  // Die Zeitzone springt im Lauf eine Stunde vor (CEST -> UTC+3), dann Neustart: einmal,
  // spaetestens zur Weckzeit des Laufs (UTC), wie Pebbles Zaehler.
  prv_stiller_morgen(ALARM_KIND_EVERYDAY, -1);
  prv_laufen(0, W_STILL + 5, -1, -1);
  TimezoneInfo osten = { .tm_zone = "MSK", .tm_gmtoff = 3 * PBL_SEC_PER_HOUR };
  time_util_update_timezone(&osten);
  rtc_set_timezone(&osten);
  alarm_handle_clock_change();
  fake_system_task_callbacks_invoke_pending();
  prv_laufen(W_STILL + 5, BIS_STILL, W_STILL + 10, W_STILL + 11);
  prv_bericht("Zeitzone +1 h im Lauf, dann Neustart", W_STILL + 10, W_STILL + 11);
#if STRENG
  cl_assert_equal_i(s_klingeln, 1);
  cl_assert(prv_weckminute() <= T_STILL);
#else
  cl_assert_equal_i(s_klingeln, 0);
#endif

  // Beim Neustart springt die Uhr ueber das Nachholfenster: nichts mehr.
  prv_stiller_morgen(ALARM_KIND_EVERYDAY, -1);
  prv_laufen(0, BIS_STILL + 10, W_STILL + 10, T_STILL + NACHHOLEN + 3);
  prv_bericht("Neustart, Uhr danach nach dem Nachholfenster", W_STILL + 10,
              T_STILL + NACHHOLEN + 3);
  cl_assert_equal_i(s_klingeln, 0);
}

// Zwei Neustarts im Fenster und einer nach dem Klingeln: einmal, zur Weckzeit.
void test_boulder_wecker__zwei_neustarts(void) {
  prv_stiller_morgen(ALARM_KIND_EVERYDAY, -1);
  prv_laufen(0, W_STILL + 13, W_STILL + 5, W_STILL + 6);
  prv_laufen(W_STILL + 13, T_STILL + 3, W_STILL + 13, W_STILL + 14);
  prv_laufen(T_STILL + 3, BIS_STILL, T_STILL + 3, T_STILL + 4);
  prv_bericht("drei Neustarts", T_STILL + 3, T_STILL + 4);
#if STRENG
  cl_assert_equal_i(s_klingeln, 1);
  cl_assert_equal_i(prv_weckminute(), T_STILL);
#else
  cl_assert_equal_i(s_klingeln, 0);
#endif
}

// Pebble selbst erzwingt das Klingeln, wenn die Uhr im Lauf ueber die Weckzeit springt
// (alarm_handle_clock_change), an der Weckregel vorbei. Auch das zaehlt als geklungen: Ein
// Neustart im Nachholfenster danach weckt nicht noch einmal.
void test_boulder_wecker__zeitsprung_klingelt_dann_neustart(void) {
  prv_stiller_morgen(ALARM_KIND_EVERYDAY, -1);
  prv_laufen(0, W_STILL + 15, -1, -1);
  cl_assert_equal_i(s_klingeln, 0);
  rtc_set_time(s_ref.beginn_utc + (time_t)(T_STILL + 2) * PBL_SEC_PER_MIN);
  alarm_handle_clock_change();
  fake_system_task_callbacks_invoke_pending();
  cl_assert_equal_i(s_klingeln, 1);
  cl_assert_equal_i(prv_weckminute(), T_STILL + 2);
  prv_laufen(T_STILL + 2, BIS_STILL, T_STILL + 5, T_STILL + 6);
  prv_bericht("Uhr springt ueber die Weckzeit, dann Neustart", T_STILL + 5, T_STILL + 6);
  cl_assert_equal_i(s_klingeln, 1);
}

// Ein gewoehnlicher Wecker bleibt bei Pebble: Er klingelt nach dem Boot nur, wenn seine Zeit
// hoechstens 5 min her ist (ALARM_MISSED_MAX_DELAY_S), in beiden Bauten gleich.
void test_boulder_wecker__gewoehnlicher_wecker(void) {
  static const int s_an[] = { 3, 5, 6, 10 };
  for (size_t i = 0; i < ARRAY_LENGTH(s_an); i++) {
    prv_neu("nacht-2026-09-30");
    alarm_create(&(AlarmInfo){ .hour = (T_STILL / 60 + 20) % 24, .minute = T_STILL % 60,
                               .kind = ALARM_KIND_EVERYDAY, .is_smart = false });
    prv_laufen(0, T_STILL + 40, T_STILL - 1, T_STILL + s_an[i]);
    prv_bericht("gewoehnlicher Wecker", T_STILL - 1, T_STILL + s_an[i]);
    cl_assert_equal_i(s_klingeln, s_an[i] <= 5 ? 1 : 0);
  }
}

// Geklungen, dann geht die Uhr vor das Fenster zurueck, und sie ist ueber den neuen
// Fensterbeginn aus: Pebbles scharfer Wecker ist wieder dieser Morgen, aber er hat fuer diese
// Weckzeit schon geklungen - nicht noch einmal.
void test_boulder_wecker__geklungen_uhr_zurueck(void) {
  prv_stiller_morgen(ALARM_KIND_EVERYDAY, W_STILL + 10);
  prv_laufen(0, W_STILL + 13, -1, -1);
  cl_assert_equal_i(s_klingeln, 1);
  rtc_set_time(s_ref.beginn_utc + (time_t)(W_STILL - 5) * PBL_SEC_PER_MIN);
  alarm_handle_clock_change();
  fake_system_task_callbacks_invoke_pending();
  prv_laufen(W_STILL - 5, BIS_STILL, W_STILL - 1, W_STILL + 10);
  prv_bericht("geklungen, Uhr zurueck, ueber Fensterbeginn aus", W_STILL - 1, W_STILL + 10);
  cl_assert_equal_i(s_klingeln, 1);
}

// =============================================================================================
// Befunde der Gegenpruefung zum Wecker nach einem Neustart (07.10.2026)

// Minuten [von, bis) ab dem Fensterbeginn der Referenz ohne jede Regung, auch ueber ihr Ende
// hinaus (der naechste Morgen).
static void prv_still(int von, int bis) {
  for (int m = von; m < bis; m++) {
    prv_minute_utc(s_ref.beginn_utc + (time_t)m * PBL_SEC_PER_MIN, true, 0, 60);
  }
}

// Der naechste Morgen: still bis W + 10, Neustart, an W + 11, still bis W + 40. Wie oft klingelt
// er dabei? Klingelt er, dann zur Weckzeit (*minute, -1 = nicht).
static int prv_naechster_morgen_mit_neustart(int von, int *minute) {
  const int klingeln = s_klingeln;
  const time_t geklingelt = s_geklingelt;
  s_geklingelt = 0;
  const int w_morgen = W_STILL + PBL_MIN_PER_DAY;
  prv_still(von, w_morgen + 10);
  prv_neustart(w_morgen + 10, w_morgen + 11);
  prv_still(w_morgen + 11, w_morgen + 40);
  *minute = prv_weckminute();
  s_geklingelt = geklingelt;
  return s_klingeln - klingeln;
}

// Grenze des Nachholfensters: alarm_init 10 s vor seinem Ende, die Freigabe der Wecker
// (event_loop.c) erst 5 s danach. Pebble startet den nachgeholten Wecker dann nicht mehr: kein
// Klingeln, auch nicht spaeter mit der Weckzeit von morgen. Kommt die Freigabe noch im
// Nachholfenster und ist KernelBG erst danach dran: sofort. Am naechsten Morgen holt ein
// Neustart im Fenster ihn wie immer nach.
void test_boulder_wecker__nachholgrenze(void) {
  for (int lief = 0; lief <= 1; lief++) {
    for (int knapp = 0; knapp <= 1; knapp++) {
      prv_stiller_morgen(ALARM_KIND_EVERYDAY, -1);
      prv_laufen(0, lief ? T_STILL - 2 : W_STILL - 1, -1, -1);
      const time_t init = s_ref.beginn_utc + (time_t)(T_STILL + NACHHOLEN) * PBL_SEC_PER_MIN - 10;
      if (knapp) {
        prv_neustart_um(init, 5, 7);
      } else {
        prv_neustart_um(init, 15, 0);
      }
      prv_laufen(T_STILL + NACHHOLEN, T_STILL + NACHHOLEN + 60, -1, -1);
      printf("\n%s Nachholgrenze (%s, Freigabe %s): %dx", STRENG ? "streng" : "Pebble",
             lief ? "Lauf unterbrochen" : "ueber Fensterbeginn aus",
             knapp ? "knapp davor" : "danach", s_klingeln);
      if (s_klingeln) {
        printf(" um %02d:%02d", UHR(prv_weckminute()));
      }
#if STRENG
      cl_assert_equal_i(s_klingeln, knapp ? 1 : 0);
      if (knapp) {
        cl_assert_equal_i(prv_weckminute(), T_STILL + NACHHOLEN);
      }
#else
      cl_assert_equal_i(s_klingeln, 0);
#endif
      int minute;
      const int morgen = prv_naechster_morgen_mit_neustart(T_STILL + NACHHOLEN + 60, &minute);
      printf(", naechster Morgen mit Neustart im Fenster %dx", morgen);
      cl_assert_equal_i(morgen, STRENG ? 1 : 0);
#if STRENG
      // Zu seiner Weckzeit, nicht zu der von gestern.
      cl_assert_equal_i(minute, T_STILL + PBL_MIN_PER_DAY);
#endif
    }
  }
}

// Vor dem Fenster des Smart Alarms war ein anderer Wecker faellig, waehrend die Uhr aus war:
// Pebbles scharfer Wecker ist dann der andere. Der Smart Alarm wird trotzdem nachgeholt, sein
// Fensterbeginn fiel ja in die Zeit, in der die Uhr aus war.
void test_boulder_wecker__anderer_wecker_davor(void) {
  static const struct {
    bool smart;  //< der andere ist ein frueherer Smart Alarm (Weckzeit T - 60, sonst T - 40)
    int an;
  } s_fall[] = {
    { false, T_STILL - 10 }, { false, T_STILL + 5 }, { false, T_STILL + NACHHOLEN + 1 },
    { true, T_STILL - 10 },
  };
  for (size_t i = 0; i < ARRAY_LENGTH(s_fall); i++) {
    prv_stiller_morgen(ALARM_KIND_EVERYDAY, -1);
    const int andere = T_STILL - (s_fall[i].smart ? 60 : 40);
    alarm_create(&(AlarmInfo){ .hour = (andere / 60 + 20) % 24, .minute = andere % 60,
                               .kind = ALARM_KIND_EVERYDAY, .is_smart = s_fall[i].smart });
    const int aus = andere - (s_fall[i].smart ? FENSTER : 0) - 5;
    prv_laufen(0, BIS_STILL + 5, aus, s_fall[i].an);
    prv_bericht(s_fall[i].smart ? "frueherer Smart Alarm davor" : "gewoehnlicher Wecker davor",
                aus, s_fall[i].an);
#if STRENG
    if (s_fall[i].an < T_STILL + NACHHOLEN) {
      cl_assert_equal_i(s_klingeln, 1);
      cl_assert_equal_i(prv_weckminute(), MAX(T_STILL, s_fall[i].an));
    } else {
      cl_assert_equal_i(s_klingeln, 0);
    }
#else
    cl_assert_equal_i(s_klingeln, 0);
#endif
  }

  // Gegenfaelle, in beiden Bauten nichts: Pebble plante den Smart Alarm gar nicht fuer heute.
  // a) Erst im Fenster angelegt (Pebble plant morgen), dann ist die Uhr ueber einen anderen
  //    Wecker (W + 10) aus. b) Heute nicht dran (nur an den anderen Wochentagen), die Uhr ist
  //    ueber einen anderen Wecker (W - 10) und den Fensterbeginn aus.
  for (int fall = 0; fall <= 1; fall++) {
    prv_neu("nacht-2026-09-30");
    for (int m = W_STILL - 60; m < BIS_STILL + 5; m++) {
      s_ref.vmc[m] = 0;
    }
    const int andere = fall ? W_STILL - 10 : W_STILL + 10;
    alarm_create(&(AlarmInfo){ .hour = (andere / 60 + 20) % 24, .minute = andere % 60,
                               .kind = ALARM_KIND_EVERYDAY, .is_smart = false });
    struct tm ort;
    const time_t t_utc = s_ref.beginn_utc + (time_t)T_STILL * PBL_SEC_PER_MIN;
    localtime_r(&t_utc, &ort);
    bool tage[PBL_DAY_PER_WEEK];
    for (int t = 0; t < PBL_DAY_PER_WEEK; t++) {
      tage[t] = (t != ort.tm_wday);
    }
    const AlarmInfo smart = { .hour = (T_STILL / 60 + 20) % 24, .minute = T_STILL % 60,
                              .kind = fall ? ALARM_KIND_CUSTOM : ALARM_KIND_EVERYDAY,
                              .is_smart = true, .scheduled_days = fall ? &tage : NULL };
    if (fall) {
      alarm_create(&smart);
      prv_laufen(0, BIS_STILL + 5, andere - 2, W_STILL + 20);
    } else {
      prv_laufen(0, W_STILL + 5, -1, -1);
      alarm_create(&smart);
      fake_system_task_callbacks_invoke_pending();
      prv_laufen(W_STILL + 5, BIS_STILL + 5, andere - 2, W_STILL + 20);
    }
    prv_bericht(fall ? "heute nicht dran, anderer Wecker verpasst"
                     : "im Fenster angelegt, anderer Wecker verpasst", andere - 2, W_STILL + 20);
    cl_assert_equal_i(s_klingeln, 0);
  }
}

// Zwei Smart Alarme haben ihren Fensterbeginn verpasst (06:10 und 06:30, aus 05:30, an 06:25):
// alarm.c fuehrt nur einen Lauf. Der mit der fruehesten Weckzeit klingelt sofort.
void test_boulder_wecker__zwei_verpasst(void) {
  prv_stiller_morgen(ALARM_KIND_EVERYDAY, -1);
  const int frueher = T_STILL - 20;
  alarm_create(&(AlarmInfo){ .hour = (frueher / 60 + 20) % 24, .minute = frueher % 60,
                             .kind = ALARM_KIND_EVERYDAY, .is_smart = true });
  prv_laufen(0, BIS_STILL, W_STILL - 30, T_STILL - 5);
  prv_bericht("zwei verpasst", W_STILL - 30, T_STILL - 5);
#if STRENG
  cl_assert_equal_i(s_klingeln, 1);
  cl_assert_equal_i(prv_weckminute(), T_STILL - 5);
#else
  cl_assert_equal_i(s_klingeln, 0);
#endif
}

// Pebbles Lauf endet im Fenster ohne Klingeln: aus und gleich wieder an, geloescht und gleich
// neu angelegt (dieselbe Id), umgestellt und zurueck (seit v4.41.0 beendet jede Aenderung an
// Zeit, Art oder Tagen den Lauf, 62e8d0a8). Ohne Neustart klingelt er an diesem Morgen nicht
// mehr (Pebble plant morgen) - ein Neustart danach holt ihn dann auch nicht nach. Loest ein
// anderer Wecker ihn ab, klingelt der andere, mit und ohne Neustart.
typedef enum {
  EndeAusAn,
  EndeNeuAngelegt,
  EndeAndererWecker,
  EndeZeitZurueck,
  EndeAnzahl,
} Ende;

void test_boulder_wecker__lauf_endet(void) {
  static const char *s_name[] = { "aus und wieder an", "geloescht und neu angelegt",
                                  "anderer Wecker loest ab", "umgestellt und zurueck" };
  static const int s_ohne_neustart[] = {0, 0, 1, 0};
  for (Ende e = 0; e < EndeAnzahl; e++) {
    int klingeln[2];
    for (int neustart = 0; neustart <= 1; neustart++) {
      AlarmId id = prv_stiller_morgen(ALARM_KIND_EVERYDAY, -1);
      if (e == EndeAndererWecker) {
        const int andere = W_STILL + 10;
        alarm_create(&(AlarmInfo){ .hour = (andere / 60 + 20) % 24, .minute = andere % 60,
                                   .kind = ALARM_KIND_EVERYDAY, .is_smart = false });
      }
      prv_laufen(0, W_STILL + 5, -1, -1);
      const AlarmInfo info = { .hour = (T_STILL / 60 + 20) % 24, .minute = T_STILL % 60,
                               .kind = ALARM_KIND_EVERYDAY, .is_smart = true };
      switch (e) {
        case EndeAusAn:
          alarm_set_enabled(id, false);
          alarm_set_enabled(id, true);
          break;
        case EndeNeuAngelegt:
          alarm_delete(id);
          cl_assert_equal_i(alarm_create(&info), id);
          break;
        case EndeZeitZurueck:
          alarm_set_time(id, (T_STILL / 60 + 21) % 24, T_STILL % 60);
          alarm_set_time(id, info.hour, info.minute);
          break;
        default:
          break;
      }
      fake_system_task_callbacks_invoke_pending();
      prv_laufen(W_STILL + 5, BIS_STILL, neustart ? W_STILL + 15 : -1, W_STILL + 16);
      prv_bericht(s_name[e], neustart ? W_STILL + 15 : -1, neustart ? W_STILL + 16 : -1);
      klingeln[neustart] = s_klingeln;
    }
    cl_assert_equal_i(klingeln[0], s_ohne_neustart[e]);
#if STRENG
    cl_assert_equal_i(klingeln[1], klingeln[0]);
#else
    // Pebble allein holt nach einem Neustart im Fenster nie nach.
    cl_assert_equal_i(klingeln[1], e == EndeAndererWecker ? 1 : 0);
#endif
  }
}

// Ohne Neustart: geklungen, dann geht die Uhr vor das Fenster zurueck. Pebble plant das Fenster
// neu, und er klingelt noch einmal - in beiden Bauten gleich. "Nie zweimal fuer dieselbe
// Weckzeit" gilt ueber einen Neustart (geklungen_uhr_zurueck), nicht hier.
void test_boulder_wecker__geklungen_uhr_zurueck_ohne_neustart(void) {
  prv_stiller_morgen(ALARM_KIND_EVERYDAY, W_STILL + 10);
  prv_laufen(0, W_STILL + 13, -1, -1);
  cl_assert_equal_i(s_klingeln, 1);
  rtc_set_time(s_ref.beginn_utc + (time_t)(W_STILL - 5) * PBL_SEC_PER_MIN);
  alarm_handle_clock_change();
  fake_system_task_callbacks_invoke_pending();
  prv_laufen(W_STILL - 5, BIS_STILL, -1, -1);
  prv_bericht("geklungen, Uhr zurueck, ohne Neustart", -1, -1);
  cl_assert_equal_i(s_klingeln, 2);
}

// Gestellt: Der Flash ist voll. Erst PFS mit Fuelldateien, dann die Datei des Wecklaufs, bis kein
// Satz mehr hineinpasst. Name und Groesse wie in wecklauf.c, hier bewusst festgehalten.
#define WECKLAUF_DATEI_IM_TEST "boulder_wecker"
#define WECKLAUF_GROESSE_IM_TEST 1024
static int s_fuelldateien;

static void prv_flash_voll(void) {
  char name[16];
  s_fuelldateien = 0;
  for (size_t groesse = 1u << 20; groesse >= 256 && s_fuelldateien < 10000;) {
    snprintf(name, sizeof(name), "fuell%d", s_fuelldateien);
    const int fd = pfs_open(name, OP_FLAG_WRITE, FILE_TYPE_STATIC, groesse);
    if (fd < 0) {
      groesse /= 2;
      continue;
    }
    pfs_close(fd);
    s_fuelldateien++;
  }
  cl_assert(s_fuelldateien > 0);
  // Die Datei bis zum letzten Byte: zuletzt so kleine Saetze, dass kein Lauf mehr passt. Ohne
  // E4 gibt es sie nicht, und anlegen geht dann nicht mehr.
  SettingsFile datei;
  const status_t offen =
      settings_file_open(&datei, WECKLAUF_DATEI_IM_TEST, WECKLAUF_GROESSE_IM_TEST);
  cl_assert_equal_i(offen, STRENG ? S_SUCCESS : E_OUT_OF_STORAGE);
  if (offen != S_SUCCESS) {
    return;
  }
  static const uint8_t s_satz[32];
  int saetze = 0;
  for (size_t laenge = sizeof(s_satz); laenge > 0; laenge /= 2) {
    while (saetze < 2000 &&
           settings_file_set(&datei, "fuell", 5, s_satz, laenge) == S_SUCCESS) {
      saetze++;
    }
  }
  settings_file_close(&datei);
  cl_assert(saetze < 2000);
}

static void prv_flash_frei(void) {
  char name[16];
  for (int i = 0; i < s_fuelldateien; i++) {
    snprintf(name, sizeof(name), "fuell%d", i);
    pfs_remove(name);
  }
  s_fuelldateien = 0;
}

// Der Lauf steht in der Datei, dann ist der Flash voll, und er klingelt: "geklungen" muss
// trotzdem in die Datei - sonst klingelt ein Neustart im Nachholfenster ein zweites Mal.
void test_boulder_wecker__geklungen_bei_vollem_flash(void) {
  prv_stiller_morgen(ALARM_KIND_EVERYDAY, W_STILL + 10);
  prv_laufen(0, W_STILL + 5, -1, -1);
  prv_flash_voll();
  prv_laufen(W_STILL + 5, BIS_STILL, W_STILL + 15, W_STILL + 16);
  prv_bericht("geklungen bei vollem Flash, Neustart", W_STILL + 15, W_STILL + 16);
  prv_flash_frei();
  cl_assert_equal_i(s_klingeln, 1);
  cl_assert_equal_i(prv_weckminute(), W_STILL + 11);
}

// Der Flash ist voll, als das Fenster beginnt: Der Lauf kommt nicht in die Datei. Wird wieder
// Platz frei, holt eine spaetere Frage das Schreiben nach, und ein Neustart im Fenster findet
// den Lauf.
void test_boulder_wecker__lauf_spaeter_gespeichert(void) {
  prv_stiller_morgen(ALARM_KIND_EVERYDAY, -1);
  prv_laufen(0, W_STILL - 2, -1, -1);
  prv_flash_voll();
  prv_laufen(W_STILL - 2, W_STILL + 3, -1, -1);
  prv_flash_frei();
  prv_laufen(W_STILL + 3, BIS_STILL, W_STILL + 8, W_STILL + 9);
  prv_bericht("Lauf erst nach vollem Flash gespeichert, Neustart", W_STILL + 8, W_STILL + 9);
#if STRENG
  cl_assert_equal_i(s_klingeln, 1);
  cl_assert_equal_i(prv_weckminute(), T_STILL);
#else
  cl_assert_equal_i(s_klingeln, 0);
#endif
}
