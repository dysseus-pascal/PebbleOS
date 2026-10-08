/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// DAS UMFELD DER DIENST-TESTS (test_kieselsport, test_kieselsport_folgen). Es steht in einem Kopf,
// weil T7.3 (Zufallsfolgen) als eigener Test ohne DUMA laeuft: DUMA legt jede Allokation auf
// eigene Seiten, und der Sekundentakt reiht jede Sekunde einen Callback ein - die Folgen haben
// ueber hundert Stunden. Jeder Test bindet den Kopf genau einmal ein; nicht jeder braucht alles.
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"

#include "clar.h"

#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/activity_private.h"
#include "pbl/services/activity/boulder/kalorien.h"
#include "pbl/services/activity/boulder/sport/kieselsport.h"
#include "pbl/services/activity/boulder/takt.h"
#include "pbl/services/activity/workout_service.h"
#include "pbl/services/hrm/hrm_manager.h"
#include "pbl/services/hrm/hrm_manager_private.h"
#include "pbl/services/new_timer/new_timer.h"
#include "pbl/services/regular_timer.h"
#include "pbl/util/trig.h"
#include "services/activity/boulder/sport/schluessel.h"
#include "services/activity/boulder/sport/wartend.h"

#include "applib/accel_service_private.h"
#include "applib/compass_service.h"
#include "applib/ui/vibes.h"
#include "kernel/events.h"
#include "process_management/app_install_manager.h"
#include "process_management/app_manager.h"

#include "stubs_evented_timer.h"
#include "stubs_hexdump.h"
#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_settings_file.h"

#include "fake_events.h"
#include "fake_pebble_tasks.h"
#include "fake_rtc.h"
#include "fake_system_task.h"

#include <errno.h>
#include <math.h>
#include <setjmp.h>

// KIESELSPORT AUF PEBBLES WORKOUT-DIENST (Schritt 2, AP2). Die Tests des b25-Dienstes
// (main: tests/fw/services/kieselsport/test_kieselsport.c), portiert auf den Weg 2: Der Dienst
// laeuft mit seiner echten Plattform (plattform.c) gegen Pebbles echten Workout-Dienst
// (workout_service.c mit P1 und kalorien.c) und die echte Sitzungsliste (activity_sessions.c).
// Nachgebaut ist nur das Umfeld, ohne Pebble-Logik: der HRM-Manager mit seiner Sperre (er gibt
// jeden Messwert auf KernelBG unter seiner Sperre an das Abo), Activitys Rundruf an den
// Workout-Dienst (activity.c:185-245: gueltige Werte und "nicht am Arm"), die Zeitgeber, die
// KernelBG-Schlange, Sensoren, Akku, Persist und Profil.
//
// WAS SICH GEGENUEBER b25 AENDERT, steht am Test: Sitzung und Meldung kommen von Pebble, 30 s
// nach dem Speichern (V4); die kcal aus dem Puls statt aus Pebbles Tageszaehler; das Archiv
// entfaellt (V7); Kieselsport rueckt nicht im Startmenue nach oben (F6 = B).
//
// SPERRFOLGE (T7.2): Die Sperren-Attrappe zeichnet jede Sperre auf, die unter einer anderen
// genommen wird. cleanup prueft in JEDEM Test die Regel 2.5.2.

extern struct pbl_mutex s_kieselsport_sperre;
extern struct pbl_mutex s_kalorien_sperre;
extern void prv_workout_timer_cb(void *data);
extern void workout_service_reset(void);
extern void kieselsport_test_neustart(void);
extern void fake_ruhepuls_setzen(uint8_t bpm);
extern int g_fake_ohne_schritte_aufrufe;
extern bool g_fake_ohne_schritte;
extern void (*g_fake_ohne_schritte_beim_aufruf)(bool ohne);

// =============================================================================================
// Aufgaben und Sperren
//
// Der Test laeuft in einem Faden; die Task der Attrappe sagt, wer dran ist (App, KernelBG,
// KernelMain, NewTimer). Die App hat die hoechste Prioritaet: Sie kann KernelBG mitten im
// Speichern unterbrechen. Greift sie dann zu einer Sperre, die KernelBG haelt, muesste sie
// warten: hier springt ihr Aufruf zurueck (longjmp, vor jeder Wirkung) und laeuft, sobald
// KernelBG die Sperre freigibt, von vorn - wie nach dem Warten. Sperren sind rekursiv.

typedef enum {
  SperreAndere,
  SperreDienst,    //< Kieselsport
  SperreWorkout,   //< Pebbles Workout-Dienst
  SperreActivity,  //< Pebbles Activity (Sitzungsliste)
  SperreHrm,       //< der HRM-Manager (nachgebildet)
  SperreKalorien,  //< der Zaehler des Trainings (kalorien.c)
  SperreArten,
} SperrArt;

typedef struct {
  struct pbl_mutex *m;
  PebbleTask halter;
  int tiefe;
} Sperre;

static Sperre s_sperren[16];
static int s_sperren_anzahl;
static struct pbl_mutex *s_workout_sperre;  //< gelernt beim workout_service_init
static struct pbl_mutex s_hrm_sperre;
static ActivityState s_state;
static int s_kanten[SperreArten][SperreArten];

static jmp_buf s_warten;
static Sperre *s_app_wartet_auf;
static void (*s_app_aktion)(void);
static uint32_t s_unterbrich_vor;

static SperrArt prv_art(const struct pbl_mutex *m) {
  if (m == &s_kieselsport_sperre) return SperreDienst;
  if (m == s_workout_sperre) return SperreWorkout;
  if (m == &s_state.mutex) return SperreActivity;
  if (m == &s_hrm_sperre) return SperreHrm;
  if (m == &s_kalorien_sperre) return SperreKalorien;
  return SperreAndere;
}

static Sperre *prv_sperre(struct pbl_mutex *m) {
  for (int i = 0; i < s_sperren_anzahl; i++) {
    if (s_sperren[i].m == m) return &s_sperren[i];
  }
  cl_assert(s_sperren_anzahl < (int)ARRAY_LENGTH(s_sperren));
  s_sperren[s_sperren_anzahl] = (Sperre){.m = m};
  return &s_sperren[s_sperren_anzahl++];
}

static void prv_app_laeuft(void) {
  const PebbleTask vorher = pebble_task_get_current();
  stub_pebble_tasks_set_current(PebbleTask_App);
  if (setjmp(s_warten) == 0) {
    s_app_aktion();
    s_app_aktion = NULL;
  }
  stub_pebble_tasks_set_current(vorher);
}

void pbl_mutex_init(struct pbl_mutex *m) {
  // workout_service_init legt die Sperre des Workout-Dienstes an - so kennt der Test sie.
  s_workout_sperre = m;
}
void pbl_mutex_deinit(struct pbl_mutex *m) {}
int pbl_mutex_lock_lr(struct pbl_mutex *m, pbl_timeout_t timeout, uintptr_t lr) {
  Sperre *s = prv_sperre(m);
  const PebbleTask task = pebble_task_get_current();
  if (s->tiefe > 0 && s->halter != task) {
    if (pbl_timeout_is_no_wait(timeout)) {
      return -EBUSY;
    }
    cl_assert(task == PebbleTask_App && s_app_aktion);
    s_app_wartet_auf = s;
    longjmp(s_warten, 1);
  }
  if (s->tiefe == 0) {
    for (int i = 0; i < s_sperren_anzahl; i++) {
      if (&s_sperren[i] != s && s_sperren[i].tiefe > 0 && s_sperren[i].halter == task) {
        s_kanten[prv_art(s_sperren[i].m)][prv_art(m)]++;
      }
    }
    s->halter = task;
  }
  s->tiefe++;
  return 0;
}
void pbl_mutex_unlock(struct pbl_mutex *m) {
  Sperre *s = prv_sperre(m);
  cl_assert(s->tiefe > 0 && s->halter == pebble_task_get_current());
  if (--s->tiefe == 0 && s_app_wartet_auf == s) {
    s_app_wartet_auf = NULL;
    prv_app_laeuft();
  }
}
bool pbl_mutex_is_owner(const struct pbl_mutex *m) {
  for (int i = 0; i < s_sperren_anzahl; i++) {
    if (s_sperren[i].m == m) {
      return s_sperren[i].tiefe > 0 && s_sperren[i].halter == pebble_task_get_current();
    }
  }
  return false;
}
void pbl_mutex_assert_held(const struct pbl_mutex *m, bool held) {}

// =============================================================================================
// Zeitgeber: Sekunden und Minuten (regular_timer), einmalige (new_timer)

static RegularTimerInfo *s_sekunden_takte[4];
static RegularTimerInfo *s_minuten_takte[4];

static bool prv_eintragen(RegularTimerInfo **liste, RegularTimerInfo *cb) {
  for (int i = 0; i < 4; i++) {
    if (liste[i] == cb) return true;
  }
  for (int i = 0; i < 4; i++) {
    if (!liste[i]) {
      liste[i] = cb;
      return true;
    }
  }
  return false;
}
void regular_timer_add_seconds_callback(RegularTimerInfo *cb) {
  cl_assert(prv_eintragen(s_sekunden_takte, cb));
}
void regular_timer_add_minutes_callback(RegularTimerInfo *cb) {
  cl_assert(prv_eintragen(s_minuten_takte, cb));
}
bool regular_timer_is_scheduled(RegularTimerInfo *cb) {
  for (int i = 0; i < 4; i++) {
    if (s_sekunden_takte[i] == cb || s_minuten_takte[i] == cb) return true;
  }
  return false;
}
bool regular_timer_remove_callback(RegularTimerInfo *cb) {
  bool weg = false;
  for (int i = 0; i < 4; i++) {
    if (s_sekunden_takte[i] == cb) s_sekunden_takte[i] = NULL, weg = true;
    if (s_minuten_takte[i] == cb) s_minuten_takte[i] = NULL, weg = true;
  }
  return weg;
}

typedef struct {
  bool laeuft;
  time_t faellig;
  NewTimerCallback cb;
  void *daten;
} Wecker;
static Wecker s_wecker[4];
static int s_wecker_anzahl;  //< nie zurueckgesetzt: Kennungen bleiben gueltig
TimerID new_timer_create(void) {
  cl_assert(s_wecker_anzahl < 4);
  return (TimerID)(s_wecker_anzahl++ + 1);
}
bool new_timer_start(TimerID t, uint32_t ms, NewTimerCallback cb, void *daten, uint32_t flags) {
  s_wecker[t - 1] = (Wecker){.laeuft = true, .faellig = rtc_get_time() + ms / 1000, .cb = cb,
                             .daten = daten};
  return true;
}
bool new_timer_stop(TimerID t) {
  s_wecker[t - 1].laeuft = false;
  return true;
}
void new_timer_delete(TimerID t) {}

// =============================================================================================
// Pebbles Umfeld: Profil, Tageszaehler, Sitzungsliste, Meldung, Auto-Erkennung

ActivityState *activity_private_state(void) {
  return &s_state;
}

// Activitys Getter ueber die Liste der echten activity_sessions.c (activity.c:1511-1529).
bool activity_get_sessions(uint32_t *session_entries, ActivitySession *sessions) {
  pbl_mutex_lock(&s_state.mutex, PBL_FOREVER);
  const uint32_t n = MIN(*session_entries, s_state.activity_sessions_count);
  memcpy(sessions, s_state.activity_sessions, n * sizeof(ActivitySession));
  *session_entries = n;
  pbl_mutex_unlock(&s_state.mutex);
  return true;
}

static int32_t s_schritte_tag;
bool activity_get_metric(ActivityMetric metric, uint32_t history_len, int32_t *history) {
  *history = s_schritte_tag;
  return metric == ActivityMetricStepCount;
}

static char s_erkennung[64];
void activity_algorithm_enable_activity_tracking(bool enable) {
  const size_t n = strlen(s_erkennung);
  cl_assert(n + 1 < sizeof(s_erkennung));
  s_erkennung[n] = enable ? '1' : '0';
}

static int s_meldungen;
static ActivitySession s_meldung;
// Der Beginn jeder gemeldeten Sitzung (T7.3): Pebbles Liste fasst eine zweite Sitzung mit
// gleichem Typ und Beginn still mit der ersten zusammen (activity_sessions.c:170-177), die
// Meldungen zaehlen jedes Speichern.
#define GEMELDET_MAX 64
static time_t s_gemeldet_beginn[GEMELDET_MAX];
void activity_insights_push_activity_session_notification(time_t notif_time,
                                                          ActivitySession *session, int32_t avg_hr,
                                                          int32_t *hr_zone_time_s) {
  if (s_meldungen < GEMELDET_MAX) {
    s_gemeldet_beginn[s_meldungen] = session->start_utc;
  }
  s_meldungen++;
  s_meldung = *session;
}

void workout_utils_send_abandoned_workout_notification(void) {}

static uint8_t s_alter;
static uint16_t s_gewicht_dag;
uint8_t activity_prefs_get_age_years(void) { return s_alter; }
ActivityGender activity_prefs_get_gender(void) { return ActivityGenderMale; }
uint16_t activity_prefs_get_weight_dag(void) { return s_gewicht_dag; }
uint16_t activity_prefs_get_height_mm(void) { return 1800; }
uint8_t activity_prefs_heart_get_elevated_hr(void) { return 100; }
uint8_t activity_prefs_heart_get_zone1_threshold(void) { return 130; }
uint8_t activity_prefs_heart_get_zone2_threshold(void) { return 154; }
uint8_t activity_prefs_heart_get_zone3_threshold(void) { return 172; }
HRMonitoringInterval activity_prefs_get_hrm_measurement_interval(void) {
  return HRMonitoringInterval_10Min;
}
AppInstallId app_get_app_id(void) { return 0; }
uint32_t time_get_uptime_seconds(void) { return PBL_SEC_PER_DAY + rtc_get_time(); }

// activity_sessions.c fuer das, was hier nicht laeuft (Minutenlauf, Datalogging, Datei): Die
// Tests rufen nur das Eintragen. Liefe etwas anderes, waere der Test falsch gebaut.
time_t time_utc_to_local(time_t utc_time) {
  cl_fail("activity_sessions: Datalogging im Test");
  return 0;
}
time_t time_util_get_midnight_of(time_t ts) {
  cl_fail("activity_sessions: Tagesgrenze im Test");
  return 0;
}
int time_util_get_minute_of_day(time_t utc_sec) {
  cl_fail("activity_sessions: Schlaf im Test");
  return 0;
}
time_t activity_algorithm_get_last_sleep_utc(void) { return 0; }
void activity_algorithm_post_process_sleep_sessions(uint16_t num_sessions,
                                                    ActivitySession *sessions) {}
SettingsFile *activity_private_settings_open(void) { return NULL; }
void activity_private_settings_close(SettingsFile *file) {}
DataLoggingSession *dls_create(uint32_t tag, DataLoggingItemType item_type, uint16_t item_size,
                               bool buffered, bool resume, const Uuid *uuid) {
  return NULL;
}
DataLoggingResult dls_log(DataLoggingSession *s, const void *data, uint32_t num_items) {
  return DATA_LOGGING_SUCCESS;
}
static int s_dls_schub;
void dls_send_all_sessions(void) { s_dls_schub++; }

// Die Sitzungen der Uhr, die keine Nacht sind (Run/Walk/Open), ab [ab].
static int prv_sitzungen(void) {
  int n = 0;
  for (int i = 0; i < s_state.activity_sessions_count; i++) {
    n += !activity_sessions_prv_is_sleep_activity(s_state.activity_sessions[i].type);
  }
  return n;
}

static const ActivitySession *prv_sitzung(int i) {
  cl_assert(i < s_state.activity_sessions_count);
  return &s_state.activity_sessions[i];
}

// =============================================================================================
// HRM-Manager (nachgebildet): ein KernelBG-Abo mit Rueckruf, unter der Sperre des Managers

typedef struct {
  bool belegt;
  HRMSessionRef ref;
  uint32_t takt;
  uint16_t ablauf;
  HRMFeature merkmale;
  HRMSubscriberCallback cb;
  void *ctx;
} Abo;
static Abo s_abo;
static HRMSessionRef s_naechste_ref;
static int s_abo_anmeldungen;
static int s_app_abos;  //< Pebbles frontend_opened (F2 = A: nie)
static HRMActivityScene s_szene;

HRMSessionRef hrm_manager_subscribe_with_callback(AppInstallId app_id, uint32_t update_interval_s,
                                                  uint16_t expire_s, HRMFeature features,
                                                  bool low_latency, HRMSubscriberCallback callback,
                                                  void *context) {
  cl_assert(pebble_task_get_current() == PebbleTask_KernelBackground && callback);
  pbl_mutex_lock(&s_hrm_sperre, PBL_FOREVER);
  cl_assert(!s_abo.belegt);
  s_abo = (Abo){.belegt = true, .ref = ++s_naechste_ref, .takt = update_interval_s,
                          .ablauf = expire_s, .merkmale = features, .cb = callback,
                          .ctx = context};
  s_abo_anmeldungen++;
  pbl_mutex_unlock(&s_hrm_sperre);
  return s_abo.ref;
}
bool sys_hrm_manager_unsubscribe(HRMSessionRef ref) {
  pbl_mutex_lock(&s_hrm_sperre, PBL_FOREVER);
  const bool da = s_abo.belegt && s_abo.ref == ref;
  if (da) s_abo.belegt = false;
  pbl_mutex_unlock(&s_hrm_sperre);
  return da;
}
bool sys_hrm_manager_set_update_interval(HRMSessionRef ref, uint32_t takt, uint16_t ablauf) {
  pbl_mutex_lock(&s_hrm_sperre, PBL_FOREVER);
  const bool da = s_abo.belegt && s_abo.ref == ref;
  if (da) s_abo.takt = takt, s_abo.ablauf = ablauf;
  pbl_mutex_unlock(&s_hrm_sperre);
  return da;
}
bool sys_hrm_manager_set_features(HRMSessionRef ref, HRMFeature features) {
  pbl_mutex_lock(&s_hrm_sperre, PBL_FOREVER);
  const bool da = s_abo.belegt && s_abo.ref == ref;
  if (da) s_abo.merkmale = features;
  pbl_mutex_unlock(&s_hrm_sperre);
  return da;
}
HRMSessionRef sys_hrm_manager_app_subscribe(AppInstallId app_id, uint32_t update_interval_s,
                                            uint16_t expire_s, HRMFeature features) {
  s_app_abos++;
  return 99;
}
void hrm_manager_set_activity_scene(HRMActivityScene scene) {
  pbl_mutex_lock(&s_hrm_sperre, PBL_FOREVER);
  s_szene = scene;
  pbl_mutex_unlock(&s_hrm_sperre);
}

// Der Takt des Abos, 0 = keines.
static uint32_t prv_sensor_takt(void) {
  return s_abo.belegt ? s_abo.takt : 0;
}
static bool prv_sensor_hrv(void) {
  return s_abo.belegt && (s_abo.merkmale & HRMFeature_HRV);
}

static void prv_hrm_ereignis(const PebbleHRMEvent *e, HRMFeature merkmal) {
  if (!s_abo.belegt || !(s_abo.merkmale & merkmal)) return;
  const PebbleTask vorher = pebble_task_get_current();
  stub_pebble_tasks_set_current(PebbleTask_KernelBackground);
  pbl_mutex_lock(&s_hrm_sperre, PBL_FOREVER);
  s_abo.cb((PebbleHRMEvent *)e, s_abo.ctx);
  pbl_mutex_unlock(&s_hrm_sperre);
  stub_pebble_tasks_set_current(vorher);
}

// Ein Messwert des Sensors: an Kieselsports Abo (KernelBG) und ueber Activitys Rundruf als
// Health-Ereignis an Pebbles Workout-Dienst (KernelMain), wie activity.c:207-232 ihn weiterreicht.
static void prv_messwert(uint8_t bpm, HRMQuality q) {
  const PebbleHRMEvent e = {.event_type = HRMEvent_BPM, .bpm = {.bpm = bpm, .quality = q}};
  prv_hrm_ereignis(&e, HRMFeature_BPM);
  const bool gueltig = bpm >= ACTIVITY_DEFAULT_MIN_HR && bpm <= ACTIVITY_DEFAULT_MAX_HR;
  if (gueltig || q == HRMQuality_OffWrist) {
    const PebbleTask vorher = pebble_task_get_current();
    stub_pebble_tasks_set_current(PebbleTask_KernelMain);
    PebbleHealthEvent h = {
      .type = HealthEventHeartRateUpdate,
      .data.heart_rate_update = {.current_bpm = q == HRMQuality_OffWrist ? 0 : bpm,
                                 .quality = q},
    };
    workout_service_health_event_handler(&h);
    stub_pebble_tasks_set_current(vorher);
  }
}

static void prv_abstand(uint16_t ppi_ms) {
  const PebbleHRMEvent e = {.event_type = HRMEvent_HRV, .hrv = {.ppi_ms = ppi_ms}};
  prv_hrm_ereignis(&e, HRMFeature_HRV);
}

// Activitys Schrittmeldung (activity.c:1146-1159) mit der Tagessumme.
static void prv_schritte(int n) {
  s_schritte_tag += n;
  const PebbleTask vorher = pebble_task_get_current();
  stub_pebble_tasks_set_current(PebbleTask_KernelMain);
  PebbleHealthEvent e = {
    .type = HealthEventMovementUpdate,
    .data.movement_update.steps = s_schritte_tag,
  };
  workout_service_health_event_handler(&e);
  stub_pebble_tasks_set_current(vorher);
}

// =============================================================================================
// Sensoren, Akku, Brummen, App

static AccelDataHandler s_accel_handler;
static AccelServiceState s_accel_sitzung;
AccelServiceState *accel_session_create(void) { return &s_accel_sitzung; }
void accel_session_delete(AccelServiceState *session) {}
void accel_session_data_subscribe(AccelServiceState *session, uint32_t samples_per_update,
                                  AccelDataHandler handler) {
  cl_assert(pebble_task_get_current() == PebbleTask_KernelBackground);
  s_accel_handler = handler;
}
void accel_session_data_unsubscribe(AccelServiceState *session) { s_accel_handler = NULL; }

static CompassHeadingHandler s_kompass_handler;
void compass_service_subscribe(CompassHeadingHandler handler) {
  cl_assert(pebble_task_get_current() == PebbleTask_KernelMain);
  s_kompass_handler = handler;
}
void compass_service_unsubscribe(void) { s_kompass_handler = NULL; }
int compass_service_set_heading_filter(CompassHeading filter) { return 0; }
void launcher_task_add_callback(CallbackEventCallback cb, void *daten) {
  const PebbleTask vorher = pebble_task_get_current();
  stub_pebble_tasks_set_current(PebbleTask_KernelMain);
  cb(daten);
  stub_pebble_tasks_set_current(vorher);
}

static int s_brumm_zahl;
static uint32_t s_brumm_letztes[4];
static uint32_t s_brumm_laenge;
void vibes_enqueue_custom_pattern(VibePattern muster) {
  s_brumm_zahl++;
  s_brumm_laenge = muster.num_segments;
  memcpy(s_brumm_letztes, muster.durations, muster.num_segments * sizeof(uint32_t));
}

static uint8_t s_akku = 80;
BatteryChargeState battery_get_charge_state(void) {
  return (BatteryChargeState){.charge_percent = s_akku};
}

static int s_geholt;
bool app_manager_is_watchface_running(void) { return true; }
void app_manager_put_launch_app_event(const AppLaunchEventConfig *config) {
  cl_assert_equal_i(config->id, 4242);
  s_geholt++;
}
AppInstallId app_install_get_id_for_uuid(const Uuid *uuid) {
  const Uuid kieselsport = KIESELSPORT_UUID;
  return uuid_equal(uuid, &kieselsport) ? 4242 : INSTALL_ID_INVALID;
}

// P5 (Entscheid 3 vom 08.10.2026): Kieselsport im Startmenue oben. Was Pebble daraus macht, prueft
// test_kieselsport_startmenue am echten app_install_manager.c; hier zaehlt, wann der Dienst es
// meldet: welche App, ob ohne Ablauf (im Training) oder mit (danach), und nie unter seiner Sperre.
static int s_oben_aufrufe;
static AppInstallId s_oben_id;
static bool s_oben_ohne_ablauf;
static bool s_oben_unter_sperre;
void app_install_mark_prioritized(AppInstallId install_id, bool can_expire) {
  s_oben_aufrufe++;
  s_oben_id = install_id;
  s_oben_ohne_ablauf = !can_expire;
  s_oben_unter_sperre |= pbl_mutex_is_owner(&s_kieselsport_sperre);
}

// =============================================================================================
// Persist von Kieselsport (speicher.c hat seinen eigenen Test): im Speicher

#define EINTRAEGE_MAX 64
typedef struct {
  bool belegt;
  uint32_t key;
  size_t laenge;
  uint8_t daten[256];
} Eintrag;
static Eintrag s_persist[EINTRAEGE_MAX];
static int s_offen;
static bool s_jemals[256];  //< je Schluessel: je geschrieben?

static Eintrag *prv_finde(uint32_t key) {
  for (int i = 0; i < EINTRAEGE_MAX; i++) {
    if (s_persist[i].belegt && s_persist[i].key == key) return &s_persist[i];
  }
  return NULL;
}

bool ks_persist_exists(uint32_t key) { return prv_finde(key) != NULL; }

int ks_persist_read_data(uint32_t key, void *puffer, size_t groesse) {
  Eintrag *e = prv_finde(key);
  if (!e) return 0;
  const size_t n = e->laenge < groesse ? e->laenge : groesse;
  memcpy(puffer, e->daten, n);
  return (int)n;
}

static int s_schreibvorgaenge;  //< jeder ist ein neuer Satz im Flash

void ks_persist_write_data(uint32_t key, const void *daten, size_t groesse) {
  if (s_app_aktion && !s_app_wartet_auf && key == s_unterbrich_vor) {
    prv_app_laeuft();
  }
  s_schreibvorgaenge++;
  if (key < ARRAY_LENGTH(s_jemals)) s_jemals[key] = true;
  Eintrag *e = prv_finde(key);
  for (int i = 0; !e && i < EINTRAEGE_MAX; i++) {
    if (!s_persist[i].belegt) e = &s_persist[i];
  }
  cl_assert(e);
  e->belegt = true;
  e->key = key;
  e->laenge = groesse > 256 ? 256 : groesse;
  memcpy(e->daten, daten, e->laenge);
}

int32_t ks_persist_read_int(uint32_t key) {
  int32_t wert = 0;
  ks_persist_read_data(key, &wert, sizeof(wert));
  return wert;
}

void ks_persist_write_int(uint32_t key, int32_t wert) {
  ks_persist_write_data(key, &wert, sizeof(wert));
}

void ks_persist_delete(uint32_t key) {
  Eintrag *e = prv_finde(key);
  if (e) e->belegt = false;
}

void ks_persist_offen_halten(bool an) { s_offen += an ? 1 : -1; }

// V7: Das Archiv (10, 11, 100-119) wird nie mehr geschrieben.
static bool prv_archiv_beschrieben(void) {
  bool ja = s_jemals[PERSIST_ARCHIV_ANZAHL] || s_jemals[PERSIST_ARCHIV_NAECHST];
  for (int i = 0; i < PERSIST_ARCHIV_PLAETZE; i++) {
    ja |= s_jemals[PERSIST_ARCHIV_BASIS + i];
  }
  return ja;
}

// =============================================================================================
// Ablauf

#define BEGINN 1790445600  // 26.9.2026, 18:00 UTC

static void prv_kernel_bg(void) {
  fake_system_task_callbacks_invoke_pending();
}

static void prv_als_newtimer(void (*cb)(void *), void *daten) {
  const PebbleTask vorher = pebble_task_get_current();
  stub_pebble_tasks_set_current(PebbleTask_NewTimers);
  cb(daten);
  stub_pebble_tasks_set_current(vorher);
}

// Eine Sekunde wie auf der Uhr: die RTC laeuft, die Sekunden-Zeitgeber (Pebbles Workout-Timer
// und Kieselsports Takt, in der Reihenfolge ihrer Anmeldung) und faellige Wecker feuern auf dem
// NewTimer, dann arbeitet KernelBG seine Schlange ab.
static void prv_sekunde(void) {
  fake_rtc_increment_time(1);
  for (int i = 0; i < 4; i++) {
    RegularTimerInfo *t = s_sekunden_takte[i];
    if (t) prv_als_newtimer(t->cb, t->cb_data);
  }
  for (int i = 0; i < s_wecker_anzahl; i++) {
    if (s_wecker[i].laeuft && rtc_get_time() >= s_wecker[i].faellig) {
      s_wecker[i].laeuft = false;
      prv_als_newtimer(s_wecker[i].cb, s_wecker[i].daten);
    }
  }
  prv_kernel_bg();
}

static void prv_sekunden_leer(int n) {
  for (int i = 0; i < n; i++) prv_sekunde();
}

// Eine Sekunde Training mit frischem Puls und zwei Schritten.
static void prv_sekunde_mit_puls(uint8_t bpm) {
  prv_schritte(2);
  prv_messwert(bpm, HRMQuality_Excellent);
  prv_sekunde();
}

// Der Minutentakt (Nachsenden), wie in b25s Test von Hand.
static bool prv_minutentakt_an(void) {
  for (int i = 0; i < 4; i++) {
    if (s_minuten_takte[i]) return true;
  }
  return false;
}
static void prv_minute(void) {
  for (int i = 0; i < 4; i++) {
    RegularTimerInfo *t = s_minuten_takte[i];
    if (t) prv_als_newtimer(t->cb, t->cb_data);
  }
  prv_kernel_bg();
}

static bool prv_takt_an(void) {
  for (int i = 0; i < 4; i++) {
    if (s_sekunden_takte[i]) return true;
  }
  return false;
}

// Die Befehle der App: eingereiht, KernelBG arbeitet sie ab.
static void prv_starte(Sportart art, uint32_t beginn) {
  kieselsport_starte(art, beginn);
  prv_kernel_bg();
}
static void prv_pause(void) {
  kieselsport_pause_umschalten();
  prv_kernel_bg();
}
static void prv_speichern(void) {
  kieselsport_speichern();
  prv_kernel_bg();
}
static void prv_verwerfen(void) {
  kieselsport_verwerfen();
  prv_kernel_bg();
}

static KsStand prv_stand(void) {
  KsStand s;
  kieselsport_stand(&s);
  return s;
}

static int32_t prv_pebble_dauer(void) {
  int32_t dauer = -1;
  workout_service_get_current_workout_info(NULL, &dauer, NULL, NULL, NULL);
  return dauer;
}

static void prv_neustart(void) {
  kieselsport_test_neustart();
  stub_pebble_tasks_set_current(PebbleTask_KernelBackground);
  kieselsport_init();
  stub_pebble_tasks_set_current(PebbleTask_App);
}

static void umfeld_initialize(void) {
  fake_rtc_init(0, BEGINN);
  fake_event_init();
  fake_system_task_callbacks_cleanup();
  system_task_set_available_space(100);
  stub_pebble_tasks_set_current(PebbleTask_App);
  memset(s_sperren, 0, sizeof(s_sperren));
  s_sperren_anzahl = 0;
  memset(s_kanten, 0, sizeof(s_kanten));
  s_app_wartet_auf = NULL;
  s_app_aktion = NULL;
  memset(s_sekunden_takte, 0, sizeof(s_sekunden_takte));
  memset(s_minuten_takte, 0, sizeof(s_minuten_takte));
  for (int i = 0; i < 4; i++) s_wecker[i].laeuft = false;
  memset(&s_state, 0, sizeof(s_state));
  workout_service_reset();
  workout_service_init();
  memset(s_persist, 0, sizeof(s_persist));
  memset(s_jemals, 0, sizeof(s_jemals));
  s_schreibvorgaenge = 0;
  s_offen = 0;
  s_schritte_tag = 5000;
  memset(s_erkennung, 0, sizeof(s_erkennung));
  s_meldungen = 0;
  s_dls_schub = 0;
  s_alter = 0;
  s_gewicht_dag = 7000;
  fake_ruhepuls_setzen(0);
  g_fake_ohne_schritte_aufrufe = 0;
  g_fake_ohne_schritte = false;
  g_fake_ohne_schritte_beim_aufruf = NULL;
  memset(&s_abo, 0, sizeof(s_abo));
  s_abo_anmeldungen = 0;
  s_app_abos = 0;
  s_szene = HRMActivityScene_Default;
  s_accel_handler = NULL;
  s_kompass_handler = NULL;
  s_brumm_zahl = 0;
  s_akku = 80;
  s_geholt = 0;
  s_oben_aufrufe = 0;
  s_oben_id = INSTALL_ID_INVALID;
  s_oben_ohne_ablauf = false;
  s_oben_unter_sperre = false;
  prv_neustart();
}

static void umfeld_cleanup(void) {
  if (kieselsport_laeuft()) {
    prv_verwerfen();
  }
  prv_kernel_bg();
  // F2 = A: Pebbles Workout-Dienst legt nie ein eigenes Puls-Abo an (frontend_opened).
  cl_assert_equal_i(s_app_abos, 0);
  // T7.2, Regel 2.5.2: Die Sperre des Dienstes ist ein Blatt - unter ihr weder Pebbles
  // Workout-Dienst noch Activity (Variante) noch der HRM-Manager. Der Pulsrueckruf (unter der
  // HRM-Sperre) ruft kein workout_service_*.
  cl_assert_equal_i(s_kanten[SperreDienst][SperreWorkout], 0);
  cl_assert_equal_i(s_kanten[SperreDienst][SperreActivity], 0);
  cl_assert_equal_i(s_kanten[SperreDienst][SperreHrm], 0);
  cl_assert_equal_i(s_kanten[SperreHrm][SperreWorkout], 0);
  // P5: das Startmenue nie unter der Sperre des Dienstes (sie bleibt ein Blatt).
  cl_assert(!s_oben_unter_sperre);
  // Die Sperre des Kalorienzaehlers nimmt Pebble unter seiner Workout-Sperre: Sie ist ein Blatt,
  // unter ihr wird keine andere genommen.
  for (int b = 0; b < SperreArten; b++) {
    cl_assert_equal_i(s_kanten[SperreKalorien][b], 0);
  }
  // Und keine Sperre ueber Kreuz: was unter A genommen wird, nimmt nie A unter sich.
  for (int a = 1; a < SperreArten; a++) {
    for (int b = 1; b < SperreArten; b++) {
      cl_assert(!(s_kanten[a][b] && s_kanten[b][a]));
    }
  }
  workout_service_reset();
}
