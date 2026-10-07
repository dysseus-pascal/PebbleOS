/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2, AP6: Velo und Auto (Spezifikation 3.3-3.7, Kriterium 5) - mit der echten
// Glue-Kopie und dem echten Kern (Zaehler, Puffer, DLS 81, Minutendatei), dem echten Dauerteil
// (kontext.c), der echten Nachfrage (nachfrage.c, nur Pebbles Dialog-Bibliothek ist eine
// Attrappe) und der echten activity_sessions.c. Spezifikation 9: T5.1, T5.2, T5.3, T2.3 und
// V5-V8; T5.4 (Abo am echten HRM-Manager) steht in test_boulder_messung.
//
// ECHTE FAHRTEN LAGEN NICHT VOR. Alle Fahrten hier sind gestellt, nach dem, was die
// Merkmale beschreiben (wenige Pedal-Schritte, viel Bewegung, ruhige Hand am Lenker). Sie
// pruefen die Ablaeufe, nicht die Schwellen; die gehoeren an echten Fahrten eingestellt.
//
// Minutenwerte: --wrap ersetzt nur die beiden Messfunktionen des Kerns (Schritte, VMC/Lage);
// Kraepelins Geh- und Schlafdetektor rechnen echt. Der Puls ist ausgeduennt wie auf Stufe 1
// (ein Wert alle zehn Minuten) und erst dicht, wenn die Variante ihr Abo "Aktiv" will.
//
// SCHRITT 2, AP4 (Spezifikation 5, 6.1: T4.1-T4.4, T2.3 mit Training, T5.2 Fahrt): Trainings
// laufen ueber Pebbles echten Workout-Dienst (workout_service.c mit P1 und kalorien.c, wie auf
// obelix): Sein Start schliesst laufende Sitzungen und erreicht die Variante ueber D15, sein Stopp
// speichert das Training in der echten Sitzungsliste. Den Merker "ohne Schritte" setzt der Test
// dort, wo Kieselsport ihn setzt (nach dem Start); welche Art ihn bekommt, prueft
// test_kieselsport mit Kieselsports echter Plattform.

#include "applib/data_logging.h"
#include "applib/ui/dialogs/actionable_dialog.h"
#include "applib/ui/dialogs/actionable_dialog_private.h"
#include "applib/ui/dialogs/dialog.h"
#include "applib/ui/window.h"
#include "kernel/event_loop.h"
#include "kernel/ui/modals/modal_manager.h"
#include <pbl/drivers/ambient_light.h>
#include <pbl/drivers/rtc.h>
#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/activity_algorithm.h"
#include "pbl/services/activity/activity_private.h"
#include <pbl/cron/cron.h>
#include "pbl/services/activity/boulder/kalorien.h"
#include "pbl/services/activity/boulder/kontext.h"
#include "pbl/services/activity/boulder/kraepelin/activity_algorithm_kraepelin.h"
#include "pbl/services/activity/boulder/kraepelin/kraepelin_algorithm.h"
#include "pbl/services/activity/boulder/takt.h"
#include "pbl/services/activity/boulder/variante.h"
#include "pbl/services/activity/workout_service.h"
#include "pbl/services/battery/battery_state.h"
#include "pbl/services/data_logging/data_logging_service.h"
#include "pbl/services/filesystem/pfs.h"
#include "pbl/services/hrm/hrm_manager_private.h"
#include "pbl/services/settings/settings_file.h"
#include "pbl/services/time.h"
#include "pbl/util/math.h"
#include "pbl/util/size.h"
#include "pbl/util/units.h"
#include "resource/resource_ids.auto.h"

#include <stdio.h>
#include <string.h>

#include "clar.h"

#include "stubs_activity_insights.h"
#include "stubs_alarm.h"
// Der Wecklauf (wecklauf.c) prueft test_boulder_wecker mit dem echten alarm.c; hier laeuft
// kein Smart Alarm, die Minute hat nichts zu tun.
void boulder_wecklauf_minute(void) {
}
#include "stubs_analytics.h"
#include "stubs_evented_timer.h"
#include "stubs_hexdump.h"
// hr_util.c laeuft echt (Pebbles Workout-Dienst, Zonen).
#include "stubs_irq.h"
#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_sleep.h"
#include "stubs_task_wdt.h"

#include "fake_accel_service.h"
#include "fake_new_timer.h"
#include "fake_rtc.h"
#include "fake_spi_flash.h"
#include "fake_system_task.h"

extern bool g_fake_puls_aktiv;

// =============================================================================================
// Minutenteil: kein Heap, keine DLS-Anlage (T7.1, T7.2 fuer kontext.c)
static bool s_im_minutenteil;
static int s_heap_im_minutenteil;

void *__real_kernel_malloc(size_t bytes);
void *__wrap_kernel_malloc(size_t bytes) {
  s_heap_im_minutenteil += s_im_minutenteil;
  return __real_kernel_malloc(bytes);
}
void *__real_kernel_zalloc(size_t bytes);
void *__wrap_kernel_zalloc(size_t bytes) {
  s_heap_im_minutenteil += s_im_minutenteil;
  return __real_kernel_zalloc(bytes);
}
void *__real_kernel_malloc_check(size_t bytes);
void *__wrap_kernel_malloc_check(size_t bytes) {
  s_heap_im_minutenteil += s_im_minutenteil;
  return __real_kernel_malloc_check(bytes);
}
void *__real_kernel_zalloc_check(size_t bytes);
void *__wrap_kernel_zalloc_check(size_t bytes) {
  s_heap_im_minutenteil += s_im_minutenteil;
  return __real_kernel_zalloc_check(bytes);
}

// Kern: Vorgabe je Minute statt Messung
static uint8_t s_vorgabe_schritte;
static uint16_t s_vorgabe_vmc;
static uint8_t s_vorgabe_lage;
uint32_t __wrap_boulder_kalg_analyze_samples(KAlgState *state, AccelRawData *data,
                                             uint32_t num_samples, uint32_t *consumed_samples) {
  *consumed_samples = num_samples;
  const uint32_t schritte = s_vorgabe_schritte;
  s_vorgabe_schritte = 0;
  return schritte;
}
void __wrap_boulder_kalg_minute_stats(KAlgState *state, uint16_t *vmc, uint8_t *orientation,
                                      bool *still) {
  *vmc = s_vorgabe_vmc;
  *orientation = s_vorgabe_lage;
  *still = false;
}

// =============================================================================================
// Pebble rund um activity_sessions.c und die Glue-Kopie

static ActivityState s_state;
ActivityState *activity_private_state(void) {
  return &s_state;
}

// Sperren: die Activity-Sperre zaehlt mit (Gegenpruefung B3). kontext.c prueft in jedem Weg,
// der die Fahrt aendert, dass sie gehalten ist - so prueft jeder Test hier die Regel mit.
static int s_activity_sperre;
void pbl_mutex_init(struct pbl_mutex *m) {
}
void pbl_mutex_deinit(struct pbl_mutex *m) {
}
int pbl_mutex_lock_lr(struct pbl_mutex *m, pbl_timeout_t timeout, uintptr_t lr) {
  if (m == &s_state.mutex) {
    s_activity_sperre++;
  }
  return 0;
}
void pbl_mutex_unlock(struct pbl_mutex *m) {
  if (m == &s_state.mutex) {
    cl_assert(s_activity_sperre > 0);
    s_activity_sperre--;
  }
}
bool pbl_mutex_is_owner(const struct pbl_mutex *m) {
  return true;
}
void pbl_mutex_assert_held(const struct pbl_mutex *m, bool held) {
  if (m == &s_state.mutex) {
    cl_assert_equal_b(s_activity_sperre > 0, held);
  }
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
  return (BatteryChargeState){ .charge_percent = 70 };
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
static uint8_t s_puls;  //< Minutenmedian, 0 = kein Wert
void activity_metrics_prv_get_median_hr_bpm(int32_t *median, int32_t *total_weight) {
  if (median) {
    *median = s_puls;
  }
  if (total_weight) {
    *total_weight = s_puls ? 100 : 0;
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
static HRMActivityScene s_szene;
static int s_szene_aufrufe;
void hrm_manager_set_activity_scene(HRMActivityScene scene) {
  s_szene = scene;
  s_szene_aufrufe++;
}
bool activity_prefs_hrm_activity_tracking_is_enabled(void) {
  return true;
}
uint8_t activity_prefs_heart_get_resting_hr(void) {
  return 0;  // nicht eingestellt: die Variante nimmt 60
}

// =============================================================================================
// Pebbles Workout-Dienst (echt) braucht sein Umfeld: Activitys Getter, Insights, Profil, HRM.

extern void prv_workout_timer_cb(void *data);
extern void workout_service_reset(void);

// Activitys Getter ueber die Liste der echten activity_sessions.c (activity.c:1511-1529).
bool activity_get_sessions(uint32_t *session_entries, ActivitySession *sessions) {
  pbl_mutex_lock(&s_state.mutex, PBL_FOREVER);
  const uint32_t n = MIN(*session_entries, s_state.activity_sessions_count);
  memcpy(sessions, s_state.activity_sessions, n * sizeof(ActivitySession));
  *session_entries = n;
  pbl_mutex_unlock(&s_state.mutex);
  return true;
}
bool activity_get_metric(ActivityMetric metric, uint32_t history_len, int32_t *history) {
  uint32_t schritte = 0;
  activity_algorithm_get_steps(&schritte);
  *history = (int32_t)schritte;
  return metric == ActivityMetricStepCount;
}
void workout_utils_send_abandoned_workout_notification(void) {
}
// Das Profil fuer Pebbles Rechnungen und die Pulskalorien (F7): 70 kg, 41 Jahre, Mann - wie
// Boulders Testfahrt (TrainingsanalyseTest.kt, Orakel unten).
uint8_t activity_prefs_get_age_years(void) {
  return 41;
}
ActivityGender activity_prefs_get_gender(void) {
  return ActivityGenderMale;
}
uint16_t activity_prefs_get_weight_dag(void) {
  return 7000;
}
uint16_t activity_prefs_get_height_mm(void) {
  return 1800;
}
uint8_t activity_prefs_heart_get_elevated_hr(void) {
  return 100;
}
uint8_t activity_prefs_heart_get_zone1_threshold(void) {
  return 130;
}
uint8_t activity_prefs_heart_get_zone2_threshold(void) {
  return 154;
}
uint8_t activity_prefs_heart_get_zone3_threshold(void) {
  return 172;
}
HRMonitoringInterval activity_prefs_get_hrm_measurement_interval(void) {
  return HRMonitoringInterval_10Min;
}
AppInstallId app_get_app_id(void) {
  return 0;
}
HRMSessionRef sys_hrm_manager_app_subscribe(AppInstallId app_id, uint32_t update_interval_s,
                                            uint16_t expire_s, HRMFeature features) {
  return 2;
}
bool sys_hrm_manager_set_features(HRMSessionRef session, HRMFeature features) {
  return true;
}
// Kieselsport selbst laeuft hier nicht; der erste Lauf der Variante stoesst ihn nur an.
void kieselsport_init(void) {
}

// DLS: 81 (Minuten der Kopie) und 84 (Sitzungen) werden mitgeschrieben.
#define DLS_MAX 64
static AlgMinuteDLSRecord s_dls[DLS_MAX];
static int s_dls_anzahl;
static bool s_dls_scheitert;
static int s_dls_im_minutenteil;
static ActivitySessionDataLoggingRecord s_dls84[DLS_MAX];
static int s_dls84_anzahl;
DataLoggingSession *dls_create(uint32_t tag, DataLoggingItemType item_type, uint16_t item_size,
                               bool buffered, bool resume, const Uuid *uuid) {
  s_dls_im_minutenteil += s_im_minutenteil;
  if (tag == DlsSystemTagActivityMinuteData && s_dls_scheitert) {
    return NULL;
  }
  return (DataLoggingSession *)(uintptr_t)tag;
}
DataLoggingResult dls_log(DataLoggingSession *logging_session, const void *data,
                          uint32_t num_items) {
  if ((uintptr_t)logging_session == DlsSystemTagActivityMinuteData) {
    cl_assert(s_dls_anzahl < DLS_MAX);
    s_dls[s_dls_anzahl++] = *(const AlgMinuteDLSRecord *)data;
  } else if ((uintptr_t)logging_session == DlsSystemTagActivitySession) {
    cl_assert(s_dls84_anzahl < DLS_MAX);
    s_dls84[s_dls84_anzahl++] = *(const ActivitySessionDataLoggingRecord *)data;
  }
  return DATA_LOGGING_SUCCESS;
}
void dls_send_all_sessions(void) {
}

// =============================================================================================
// KernelMain und Pebbles Dialog-Bibliothek (Attrappe): nachfrage.c laeuft echt dagegen.

static struct {
  CallbackEventCallback cb[8];
  void *daten[8];
  int anzahl;
} s_main;
void launcher_task_add_callback(CallbackEventCallback callback, void *data) {
  cl_assert(s_main.anzahl < 8);
  s_main.cb[s_main.anzahl] = callback;
  s_main.daten[s_main.anzahl++] = data;
}

static struct {
  int dialoge;        //< erstellt
  int gleichzeitig;   //< hoechstens so viele standen zugleich
  bool offen;
  time_t offen_seit;
  char text[16];
  uint32_t symbol;
  uint32_t ablauf_ms;
  DialogCallbacks rueckrufe;
  void *kontext;
  ClickConfigProvider tasten;
  ClickHandler taste[NUM_BUTTONS];
  int vibrationen;
} s_ui;
static ActionableDialog s_dialog;
static bool s_ruhezeit;

ActionableDialog *actionable_dialog_create(const char *dialog_name) {
  s_ui.dialoge++;
  memset(&s_dialog, 0, sizeof(s_dialog));
  return &s_dialog;
}
Dialog *actionable_dialog_get_dialog(ActionableDialog *actionable_dialog) {
  return &actionable_dialog->dialog;
}
void actionable_dialog_set_action_bar_type(ActionableDialog *actionable_dialog,
                                           DialogActionBarType action_bar_type,
                                           ActionBarLayer *action_bar) {
  cl_assert_equal_i(action_bar_type, DialogActionBarConfirmDecline);
}
void actionable_dialog_set_click_config_provider(ActionableDialog *actionable_dialog,
                                                 ClickConfigProvider click_config_provider) {
  s_ui.tasten = click_config_provider;
}
void window_single_click_subscribe(ButtonId button_id, ClickHandler handler) {
  s_ui.taste[button_id] = handler;
}
void actionable_dialog_push(ActionableDialog *actionable_dialog, WindowStack *window_stack) {
  cl_assert(!s_ui.offen);
  s_ui.offen = true;
  s_ui.offen_seit = rtc_get_time();
  s_ui.gleichzeitig = 1;
  s_ui.tasten(actionable_dialog);
}
// Wie das Fenster: vom Stapel genommen, wird es entladen (dialog_unload -> unload).
void actionable_dialog_pop(ActionableDialog *actionable_dialog) {
  if (!s_ui.offen) {
    return;
  }
  s_ui.offen = false;
  if (s_ui.rueckrufe.unload) {
    s_ui.rueckrufe.unload(s_ui.kontext ? s_ui.kontext : &actionable_dialog->dialog);
  }
}
void dialog_set_text(Dialog *dialog, const char *text) {
  strncpy(s_ui.text, text, sizeof(s_ui.text) - 1);
}
void dialog_set_icon(Dialog *dialog, uint32_t icon_id) {
  s_ui.symbol = icon_id;
}
void dialog_set_background_color(Dialog *dialog, GColor background_color) {
}
void dialog_set_text_color(Dialog *dialog, GColor text_color) {
}
void dialog_set_timeout(Dialog *dialog, uint32_t timeout) {
  s_ui.ablauf_ms = timeout;
}
void dialog_set_callbacks(Dialog *dialog, const DialogCallbacks *callbacks,
                          void *callback_context) {
  s_ui.rueckrufe = *callbacks;
  s_ui.kontext = callback_context;
}
WindowStack *modal_manager_get_window_stack(ModalPriority priority) {
  cl_assert_equal_i(priority, ModalPriorityGeneric);
  return NULL;
}
bool do_not_disturb_is_active(void) {
  return s_ruhezeit;
}
void vibes_double_pulse(void) {
  s_ui.vibrationen++;
}

// KernelMain: eingereihte Callbacks, dann der eigene Ablauf des Dialogs (dialog_set_timeout).
static void prv_kernel_main(void) {
  for (int i = 0; i < s_main.anzahl; i++) {
    s_main.cb[i](s_main.daten[i]);
  }
  s_main.anzahl = 0;
  if (s_ui.offen && (rtc_get_time() - s_ui.offen_seit) * 1000 >= (time_t)s_ui.ablauf_ms) {
    actionable_dialog_pop(&s_dialog);
  }
}

static void prv_taste(ButtonId taste) {
  cl_assert(s_ui.offen);
  s_ui.taste[taste](NULL, &s_dialog);
}

// =============================================================================================
// Eine Minute wie auf der Uhr, mit Buchfuehrung: was jede Minute gezaehlt hat (roh) und was
// in DLS 81 und der Minutendatei stehen soll (nach dem Streichen).

#define MINUTEN_MAX 600
static time_t s_utc[MINUTEN_MAX];
static uint8_t s_roh[MINUTEN_MAX];
static bool s_fahrt[MINUTEN_MAX];  //< soll gestrichen sein
static int s_n;
static int s_minute_nr;
static time_t s_t0;
static AccelSamplingRate s_rate;

// Ein Beschleunigungsblock mit so vielen Schritten (Kraepelin zaehlt sie, die Kopie bucht sie).
static void prv_schritte(uint8_t schritte) {
  s_vorgabe_schritte = schritte;
  AccelRawData leer[KALG_SAMPLE_HZ] = {};
  activity_algorithm_handle_accel(leer, ARRAY_LENGTH(leer), 0);
}

// Schritt 2: Der Test erwartet, dass die Variante Schritte gerade nirgends bucht (Training ohne
// Schritte). Eine Erwartung des Tests, nicht aus dem Code gelesen.
static bool s_erwartet_ohne_schritte;
// F7: Sekundenwerte des Abos "Aktiv" (NULL = keine, 0 = diese Sekunde keiner): der Test gibt
// sie der Variante, wie puls.c es im HRM-Rueckruf tut.
static uint8_t (*s_sekundenpuls)(time_t utc);
static bool s_ohne[MINUTEN_MAX];  //< ganze Minute im Training ohne Schritte

// Eine Minute wie auf der Uhr. Mit `mitte` laeuft nach 30 s etwas (Start, Stopp), danach kommen
// `nach` weitere Schritte; `gezaehlt` = was die Minute in Tagessumme und DLS 81 haben soll.
static void prv_minute_geteilt(uint8_t vor, void (*mitte)(void), uint8_t nach, uint8_t gezaehlt,
                               uint16_t vmc, uint8_t lage, uint8_t puls) {
  const time_t beginn = rtc_get_time();
  s_vorgabe_vmc = vmc;
  s_vorgabe_lage = lage;
  prv_schritte(vor);
  if (mitte) {
    fake_rtc_increment_time(PBL_SEC_PER_MIN / 2);
    mitte();
    prv_schritte(nach);
    fake_rtc_increment_time(PBL_SEC_PER_MIN / 2);
  } else {
    fake_rtc_increment_time(PBL_SEC_PER_MIN);
  }
  if (s_sekundenpuls) {
    for (int i = 0; i < PBL_SEC_PER_MIN; i++) {
      const uint8_t bpm = s_sekundenpuls(beginn + i);
      if (bpm) {
        boulder_kontext_puls(bpm, beginn + i);
      }
    }
  }
  if (workout_service_is_workout_ongoing()) {
    prv_workout_timer_cb(NULL);  // Pebbles Sekundentakt: die Dauer aus der RTC
  }
  s_puls = puls;
  cl_assert(s_n < MINUTEN_MAX);
  s_utc[s_n] = beginn;
  s_roh[s_n] = gezaehlt;  // s_fahrt[s_n] setzt der Aufrufer vorher (sonst false)
  s_ohne[s_n] = s_erwartet_ohne_schritte && !mitte;
  s_n++;
  s_minute_nr++;
  const time_t jetzt = rtc_get_time();
  AlgMinuteRecord satz = {};
  s_im_minutenteil = true;
  activity_algorithm_minute_handler(jetzt, &satz);
  s_im_minutenteil = false;
  activity_sessions_prv_minute_handler(jetzt);
  fake_system_task_callbacks_invoke_pending();
  boulder_dauerteil_takt(jetzt + 10);
  fake_system_task_callbacks_invoke_pending();
  prv_kernel_main();
  fake_system_task_callbacks_invoke_pending();
}

static void prv_minute(uint8_t schritte, uint16_t vmc, uint8_t lage, uint8_t puls) {
  prv_minute_geteilt(schritte, NULL, 0, s_erwartet_ohne_schritte ? 0 : schritte, vmc, lage, puls);
}

// Puls wie auf Stufe 1 (alle zehn Minuten ein Wert), ab dem Abo "Aktiv" jede Minute.
static uint8_t prv_puls(uint8_t wert) {
  return (g_fake_puls_aktiv || (s_minute_nr % 10) == 0) ? wert : 0;
}

static void prv_gehen(int minuten) {
  for (int i = 0; i < minuten; i++) {
    prv_minute(95 + (i % 7), 2600 + (i % 5) * 90, (uint8_t)(0x41 + (i % 4)), prv_puls(92));
  }
}

// Velo: wenige Pedal-Schritte, viel Bewegung, die Hand ruhig am Lenker (eine Lage, jede
// fuenfte Minute die Nachbarlage), Puls deutlich ueber dem Ruhepuls.
static void prv_velo(int minuten, bool *gestrichen) {
  for (int i = 0; i < minuten; i++) {
    const int k = s_minute_nr;
    if (gestrichen) {
      s_fahrt[s_n] = *gestrichen;
    }
    prv_minute((uint8_t)(3 + (k * 7) % 16), (uint16_t)(700 + (k * 37) % 600),
               (k % 5 == 4) ? 0x38 : 0x37, prv_puls(104));
  }
}

// Auto (und Tram, Zug): kaum Schritte, wenig Bewegung, gleiche Lage, Puls in Ruhe.
static void prv_auto(int minuten, bool gestrichen) {
  for (int i = 0; i < minuten; i++) {
    const int k = s_minute_nr;
    s_fahrt[s_n] = gestrichen;
    prv_minute((uint8_t)(1 + k % 3), (uint16_t)(60 + (k * 13) % 120), 0x26, prv_puls(66));
  }
}

static uint32_t prv_zaehler(void) {
  uint32_t schritte;
  cl_assert(activity_algorithm_get_steps(&schritte));
  return schritte;
}

static uint32_t prv_soll_summe(void) {
  uint32_t summe = 0;
  for (int i = 0; i < s_n; i++) {
    summe += s_fahrt[i] ? 0 : s_roh[i];
  }
  return summe;
}

static const AlgMinuteDLSSample *prv_dls_minute(time_t utc) {
  for (int j = 0; j < s_dls_anzahl; j++) {
    const time_t erste = s_dls[j].hdr.time_utc;
    const int i = (utc - erste) / PBL_SEC_PER_MIN;
    if (utc >= erste && i < s_dls[j].hdr.num_samples) {
      return &s_dls[j].samples[i];
    }
  }
  return NULL;
}

// DLS 81 und Minutendatei Minute fuer Minute: Fahrtminuten genullt (auch aktiv, Strecke,
// kcal), alle anderen mit ihren Schritten. Geprueft wird, was sicher geschrieben ist (aelter als
// 35 min); Rueckgabe: so viele Minuten aus der Datei.
static int prv_pruefe_minuten(void) {
  static HealthMinuteData datei[MINUTEN_MAX];
  uint32_t n_datei = ARRAY_LENGTH(datei);
  time_t ab = s_t0;
  cl_assert(activity_algorithm_get_minute_history(datei, &n_datei, &ab));
  const time_t geschrieben = rtc_get_time() - 35 * PBL_SEC_PER_MIN;
  int geprueft = 0;
  for (int i = 0; i < s_n && s_utc[i] < geschrieben; i++) {
    const int soll = s_fahrt[i] ? 0 : s_roh[i];
    const AlgMinuteDLSSample *m = prv_dls_minute(s_utc[i]);
    if (m) {
      if (m->base.steps != soll) {
        printf("Minute %d: DLS %d, soll %d (roh %d)\n", i, m->base.steps, soll, s_roh[i]);
      }
      cl_assert_equal_i(m->base.steps, soll);
      if (s_fahrt[i] || s_ohne[i]) {
        cl_assert(!m->base.active);
        cl_assert_equal_i(m->distance_cm, 0);
        cl_assert_equal_i(m->active_calories, 0);
      }
    }
    const int d = (int)((s_utc[i] - ab) / PBL_SEC_PER_MIN);
    cl_assert(d >= 0 && d < (int)n_datei);
    cl_assert(!datei[d].is_invalid);
    cl_assert_equal_i(datei[d].steps, soll);
    geprueft++;
  }
  return geprueft;
}

// Ueberdeckt ein Spaziergang in Pebbles Liste [von, bis)?
static bool prv_gehen_in(time_t von, time_t bis) {
  for (int i = 0; i < s_state.activity_sessions_count; i++) {
    const ActivitySession *a = &s_state.activity_sessions[i];
    if ((a->type == ActivitySessionType_Walk || a->type == ActivitySessionType_Run) &&
        a->start_utc < bis && a->start_utc + a->length_min * PBL_SEC_PER_MIN > von) {
      return true;
    }
  }
  return false;
}

static uint32_t prv_fahrt_summe(void) {
  uint32_t summe = 0;
  for (int i = 0; i < s_n; i++) {
    summe += s_fahrt[i] ? s_roh[i] : 0;
  }
  return summe;
}

static const ActivitySession *prv_sitzung(ActivitySessionType typ) {
  const ActivitySession *gefunden = NULL;
  for (int i = 0; i < s_state.activity_sessions_count; i++) {
    if (s_state.activity_sessions[i].type == typ) {
      cl_assert(!gefunden);  // hoechstens eine
      gefunden = &s_state.activity_sessions[i];
    }
  }
  return gefunden;
}

// Laeuft, bis die Kandidatin steht; Rueckgabe: nach so vielen Fahrtminuten.
static int prv_velo_bis_kandidatin(int hoechstens, bool gestrichen) {
  for (int i = 1; i <= hoechstens; i++) {
    prv_velo(1, &gestrichen);
    if (boulder_kontext_zustand() == FahrtKandidatin) {
      return i;
    }
  }
  return -1;
}

// =============================================================================================

void test_boulder_fahrt__initialize(void) {
  TimezoneInfo tz = { .tm_zone = "CEST", .tm_gmtoff = 2 * PBL_SEC_PER_HOUR };
  time_util_update_timezone(&tz);
  s_t0 = 1759302000;  // 1.10.2026, 09:00 CEST
  fake_rtc_init(100, s_t0);
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  pfs_format(false);
  memset(&s_state, 0, sizeof(s_state));
  s_activity_sperre = 0;
  memset(&s_ui, 0, sizeof(s_ui));
  memset(&s_main, 0, sizeof(s_main));
  s_ruhezeit = false;
  s_dls_anzahl = 0;
  s_dls_scheitert = false;
  s_dls_im_minutenteil = 0;
  s_heap_im_minutenteil = 0;
  s_n = 0;
  memset(s_fahrt, 0, sizeof(s_fahrt));
  s_minute_nr = 1;
  s_puls = 0;
  s_szene = HRMActivityScene_Default;
  s_szene_aufrufe = 0;
  g_fake_puls_aktiv = false;
  memset(s_ohne, 0, sizeof(s_ohne));
  s_erwartet_ohne_schritte = false;
  s_sekundenpuls = NULL;
  s_dls84_anzahl = 0;
  workout_service_reset();
  workout_service_init();
  // Kieselsports geprueftes Maximum (V2), wie der Dienst es nach dem Laden meldet (F7).
  boulder_kalorien_maxpuls(190);
  pbl_cron_init();
  boulder_dauerteil_test_neustart();
  boulder_szene_vergessen();
  cl_assert(activity_algorithm_init(&s_rate));
  fake_system_task_callbacks_invoke_pending();
}

void test_boulder_fahrt__cleanup(void) {
  // Ein offener Dialog (die Nachfrage haelt ihn statisch) gehoert nicht in den naechsten Test.
  if (s_ui.offen) {
    actionable_dialog_pop(&s_dialog);
  }
  fake_system_task_callbacks_invoke_pending();
  workout_service_reset();
  activity_algorithm_deinit();
  fake_system_task_callbacks_invoke_pending();
  pbl_cron_deinit();
  cl_assert_equal_i(s_activity_sperre, 0);
}

// T5.1, T5.3 "Ja", T2.3, F5, F7: 14 min gehen, 25 min Velo, 30 min gehen. Die Fahrt beginnt
// in der letzten Minute einer 15-min-Gruppe: ohne Zurueckhalten ab der ersten fahrtartigen
// Minute laege sie schon ungenullt in DLS 81, wenn die Kandidatin entsteht.
void test_boulder_fahrt__velo_ja_streicht_genau_die_fahrt(void) {
  prv_gehen(14);
  const time_t beginn = rtc_get_time();
  const int nach = prv_velo_bis_kandidatin(12, true);
  printf("\nVelo: Kandidatin nach %d min\n", nach);
  cl_assert(nach > 0 && nach <= 12);
  cl_assert_equal_i(boulder_kontext_art(), FahrtArtVelo);
  cl_assert_equal_i(boulder_kontext_beginn(), beginn);
  // Die Vorstufe hat das Abo "Aktiv" an und die Szene gesetzt (3.7).
  cl_assert(g_fake_puls_aktiv);
  cl_assert_equal_i(s_szene, HRMActivityScene_HighIntensity);
  // Ein Dialog, englisch, vorhandenes Symbol, 2 min, eine Vibration.
  cl_assert_equal_i(s_ui.dialoge, 1);
  cl_assert(s_ui.offen);
  cl_assert_equal_s(s_ui.text, "Cycling?");
  cl_assert_equal_i(s_ui.symbol, RESOURCE_ID_WORKOUT_APP_DETECTED);
  cl_assert_equal_i(s_ui.ablauf_ms, 2 * 60 * 1000);
  cl_assert_equal_i(s_ui.vibrationen, 1);
  // Bis zum "Ja" zaehlen die Fahrtschritte (sichtbar wie bei Pebbles Schlafnullung).
  const uint32_t vor_ja = prv_zaehler();
  bool ja = true;
  prv_velo(1, &ja);
  prv_taste(BUTTON_ID_UP);
  fake_system_task_callbacks_invoke_pending();
  cl_assert_equal_i(boulder_kontext_zustand(), FahrtBestaetigt);
  cl_assert(prv_zaehler() < vor_ja);
  const ActivitySession *s = prv_sitzung(ActivitySessionType_Open);
  cl_assert(s);
  cl_assert_equal_i(s->start_utc, beginn);
  cl_assert(s->ongoing);
  cl_assert(!s->manual);
  prv_velo(25 - nach - 1, &ja);
  // Absteigen: die ersten Gehminuten sind der "Schwanz" - ihre Schritte kommen zurueck.
  prv_gehen(45);
  cl_assert_equal_i(boulder_kontext_zustand(), FahrtKeine);
  s = prv_sitzung(ActivitySessionType_Open);
  cl_assert(!s->ongoing);
  cl_assert_equal_i(s->length_min, 25);
  cl_assert_equal_i(s->step_data.steps, 0);
  // F7 (Schritt 2): die Pulskalorien der Fahrt; die Werte prueft t5_2_* gegen Boulder.
  cl_assert(s->step_data.active_kcalories > 0);
  cl_assert_equal_i(s->step_data.active_kcalories, ROUND(boulder_kontext_kcal(), 1000));
  cl_assert(!prv_gehen_in(beginn, beginn + 25 * PBL_SEC_PER_MIN));
  cl_assert(!g_fake_puls_aktiv);
  // Die eigene Szene ist zurueckgenommen; beim Gehen setzt Kraepelin seine (Walk).
  cl_assert(s_szene != HRMActivityScene_HighIntensity);
  // Tagessumme, DLS 81 und Minutendatei: genau ohne die Fahrtschritte (T2.3).
  cl_assert_equal_i(prv_zaehler(), prv_soll_summe());
  const int geprueft = prv_pruefe_minuten();
  cl_assert(geprueft >= 14 + 25);
  cl_assert_equal_i(boulder_kraepelin_gestrichen(), prv_fahrt_summe());
  // K7: kein Heap und keine DLS-Anlage im Minutenteil.
  cl_assert_equal_i(s_heap_im_minutenteil, 0);
  cl_assert_equal_i(s_dls_im_minutenteil, 0);
}

// T5.3: 90 min Velo, die erste Frage bleibt ohne Antwort. Die zweite kommt, bevor das
// Zurueckhalten 45 min erreicht - so liegt beim "Ja" noch jede Fahrtminute im Puffer.
void test_boulder_fahrt__lange_fahrt_ja_erst_bei_der_zweiten_frage(void) {
  s_dls_scheitert = true;  // und ohne DLS-Sitzung (Pool voll): der Zaehler stimmt trotzdem
  prv_gehen(20);
  const time_t beginn = rtc_get_time();
  bool ja = true;
  const int nach = prv_velo_bis_kandidatin(12, true);
  cl_assert(nach > 0);
  int minuten = nach;
  while (s_ui.dialoge < 2) {
    prv_velo(1, &ja);
    minuten++;
    cl_assert(minuten <= 60);
  }
  printf("\nZweite Frage nach %d min Fahrt\n", minuten);
  cl_assert(minuten <= KONTEXT_ZWEITE_FRAGE_MIN);
  cl_assert_equal_i(s_ui.vibrationen, 2);
  prv_velo(1, &ja);
  prv_taste(BUTTON_ID_UP);
  fake_system_task_callbacks_invoke_pending();
  minuten++;
  cl_assert_equal_i(boulder_kontext_zustand(), FahrtBestaetigt);
  prv_velo(90 - minuten, &ja);
  prv_gehen(40);
  cl_assert_equal_i(s_ui.dialoge, 2);
  cl_assert_equal_i(prv_sitzung(ActivitySessionType_Open)->start_utc, beginn);
  cl_assert_equal_i(prv_sitzung(ActivitySessionType_Open)->length_min, 90);
  cl_assert_equal_i(prv_zaehler(), prv_soll_summe());
  cl_assert_equal_i(s_dls_anzahl, 0);
  cl_assert(prv_pruefe_minuten() >= 20 + 90);
}

// T5.3 "Nein" und F4 ohne Antwort: nichts aendert sich, keine weitere Frage in dieser Fahrt.
void test_boulder_fahrt__nein_und_keine_antwort_aendern_nichts(void) {
  prv_gehen(16);
  cl_assert(prv_velo_bis_kandidatin(12, false) > 0);
  prv_velo(1, NULL);
  prv_taste(BUTTON_ID_DOWN);
  fake_system_task_callbacks_invoke_pending();
  cl_assert_equal_i(boulder_kontext_zustand(), FahrtAbgelehnt);
  cl_assert(!g_fake_puls_aktiv);  // aus nach "Nein" (3.7)
  prv_velo(30, NULL);
  cl_assert_equal_i(s_ui.dialoge, 1);
  prv_gehen(20);
  cl_assert_equal_i(boulder_kontext_zustand(), FahrtKeine);

  // Zweite Fahrt, beide Fragen ohne Antwort: genau zwei Dialoge, dann "Nein".
  s_ruhezeit = true;  // in der Ruhezeit ohne Vibration, der Dialog kommt trotzdem
  cl_assert(prv_velo_bis_kandidatin(12, false) > 0);
  prv_velo(10, NULL);
  prv_gehen(25);
  cl_assert_equal_i(s_ui.dialoge, 3);
  cl_assert_equal_i(s_ui.vibrationen, 1);
  cl_assert_equal_i(boulder_kontext_zustand(), FahrtKeine);
  cl_assert(prv_sitzung(ActivitySessionType_Open) == NULL);
  cl_assert_equal_i(boulder_kraepelin_gestrichen(), 0);
  cl_assert_equal_i(prv_zaehler(), prv_soll_summe());
  prv_pruefe_minuten();
}

// V5, V6: "Driving?" auch in Tram und Zug; "Ja" streicht die Schritte, ohne Sitzung.
void test_boulder_fahrt__tram_driving_ohne_sitzung(void) {
  prv_gehen(15);
  int nach = 0;
  while (boulder_kontext_zustand() != FahrtKandidatin) {
    prv_auto(1, true);
    cl_assert(++nach <= 12);
  }
  cl_assert_equal_i(boulder_kontext_art(), FahrtArtAuto);
  cl_assert_equal_s(s_ui.text, "Driving?");
  cl_assert(!g_fake_puls_aktiv);  // kein dichter Puls fuer Auto
  prv_auto(1, true);
  prv_taste(BUTTON_ID_UP);
  fake_system_task_callbacks_invoke_pending();
  prv_auto(20, true);
  prv_gehen(30);
  cl_assert(prv_sitzung(ActivitySessionType_Open) == NULL);
  cl_assert_equal_i(prv_zaehler(), prv_soll_summe());
  prv_pruefe_minuten();
}

// T5.2 (gestellt): kein Gehen, keine Ruhe, kein Schreibtisch mit wechselnder Hand wird zur
// Kandidatin. Echte Tage (Buero, Tram, Zug) fehlen noch.
void test_boulder_fahrt__keine_kandidatin_ohne_fahrt(void) {
  prv_gehen(40);
  for (int i = 0; i < 40; i++) {
    prv_minute(0, (uint16_t)(i % 7), 0x30, prv_puls(58));  // still sitzen, lesen
  }
  for (int i = 0; i < 60; i++) {
    // Schreibtisch: Tippen, Maus, Telefon - die Lage wechselt.
    prv_minute(0, (uint16_t)(40 + (i * 29) % 400), (uint8_t)(0x20 + (i % 5) * 3), prv_puls(68));
  }
  prv_gehen(30);
  cl_assert_equal_i(s_ui.dialoge, 0);
  cl_assert_equal_i(boulder_kontext_zustand(), FahrtKeine);
  cl_assert_equal_i(prv_zaehler(), prv_soll_summe());
}

// V7: Fahrt ueber Mitternacht. Abgezogen wird nur der Anteil von heute; beim Telefon sind alle
// Fahrtminuten genullt.
void test_boulder_fahrt__mitternacht(void) {
  const time_t mitternacht = time_util_get_midnight_of(s_t0) + PBL_SEC_PER_DAY;
  fake_rtc_increment_time(mitternacht - 24 * PBL_SEC_PER_MIN - rtc_get_time());
  s_t0 = rtc_get_time();
  prv_gehen(16);
  const int erste = s_n;
  bool ja = true;
  cl_assert(prv_velo_bis_kandidatin(12, true) > 0);
  prv_velo(1, &ja);
  cl_assert(rtc_get_time() > mitternacht);
  uint32_t heute = 0, gestern = 0;
  for (int i = erste; i < s_n; i++) {
    *(s_utc[i] >= mitternacht ? &heute : &gestern) += s_roh[i];
  }
  const uint32_t vorher = prv_zaehler();
  prv_taste(BUTTON_ID_UP);
  fake_system_task_callbacks_invoke_pending();
  cl_assert_equal_i(vorher - prv_zaehler(), heute);
  cl_assert(gestern > 0);
  prv_velo(10, &ja);
  prv_gehen(40);
  cl_assert_equal_i(prv_zaehler(), prv_soll_summe() + gestern);
  prv_pruefe_minuten();
}

// N1, 2.6: Ein Training beendet die bestaetigte Fahrt; ihre Sitzung bleibt zu, die Szene des
// Trainings bleibt stehen, die Trainingsminuten zaehlen.
void test_boulder_fahrt__training_beendet_die_fahrt(void) {
  prv_gehen(16);
  bool ja = true;
  cl_assert(prv_velo_bis_kandidatin(12, true) > 0);
  prv_velo(1, &ja);
  prv_taste(BUTTON_ID_UP);
  fake_system_task_callbacks_invoke_pending();
  prv_velo(5, &ja);
  const time_t start = prv_sitzung(ActivitySessionType_Open)->start_utc;
  activity_algorithm_enable_activity_tracking(false);
  hrm_manager_set_activity_scene(HRMActivityScene_Run);
  const int aufrufe = s_szene_aufrufe;
  prv_velo(20, NULL);  // im Training: keine Fahrt, die Schritte zaehlen
  cl_assert_equal_i(boulder_kontext_zustand(), FahrtKeine);
  const ActivitySession *s = prv_sitzung(ActivitySessionType_Open);
  cl_assert(!s->ongoing);
  cl_assert_equal_i(s->start_utc, start);
  cl_assert_equal_i(s_szene_aufrufe, aufrufe);
  cl_assert_equal_i(s_szene, HRMActivityScene_Run);
  cl_assert(!g_fake_puls_aktiv);
  activity_algorithm_enable_activity_tracking(true);
  prv_gehen(30);
  cl_assert_equal_i(s_ui.dialoge, 1);
  cl_assert_equal_i(prv_zaehler(), prv_soll_summe());
  prv_pruefe_minuten();
}

// 7.4: Nach einem Neustart geht eine bestaetigte Fahrt mit gleichem Start weiter (eine
// Sitzung, kein zweiter Tag-84-Satz); nach einer Pause ueber 20 min endet sie dort, wo die
// Ablage sie zuletzt sah.
void test_boulder_fahrt__neustart_setzt_die_fahrt_fort(void) {
  prv_gehen(16);
  bool ja = true;
  cl_assert(prv_velo_bis_kandidatin(12, true) > 0);
  prv_velo(1, &ja);
  prv_taste(BUTTON_ID_UP);
  fake_system_task_callbacks_invoke_pending();
  prv_velo(4, &ja);
  BoulderFahrtAblage ablage;
  boulder_kontext_ablage(&ablage);
  const time_t beginn = boulder_kontext_beginn();
  // F7: Die Kalorien bis zur Ablage gehen mit; nach dem Boot zaehlt der Zaehler weiter.
  cl_assert(ablage.kcal_cal > 0);
  cl_assert_equal_i(ablage.kcal_cal, boulder_kontext_kcal());
  boulder_kontext_test_zuruecksetzen();
  cl_assert_equal_i(boulder_kontext_zustand(), FahrtKeine);
  boulder_kontext_laden(&ablage);
  cl_assert_equal_i(boulder_kontext_zustand(), FahrtBestaetigt);
  cl_assert_equal_i(boulder_kontext_kcal(), ablage.kcal_cal);
  fake_rtc_increment_time(2 * PBL_SEC_PER_MIN);  // der Neustart
  prv_velo(10, &ja);
  cl_assert(boulder_kontext_kcal() > ablage.kcal_cal);
  cl_assert_equal_i(prv_sitzung(ActivitySessionType_Open)->step_data.active_kcalories,
                    ROUND(boulder_kontext_kcal(), 1000));
  cl_assert_equal_i(prv_sitzung(ActivitySessionType_Open)->start_utc, beginn);
  cl_assert(prv_sitzung(ActivitySessionType_Open)->ongoing);
  boulder_kontext_ablage(&ablage);
  boulder_kontext_test_zuruecksetzen();
  boulder_kontext_laden(&ablage);
  fake_rtc_increment_time(30 * PBL_SEC_PER_MIN);
  prv_gehen(5);
  cl_assert_equal_i(boulder_kontext_zustand(), FahrtKeine);
  const ActivitySession *s = prv_sitzung(ActivitySessionType_Open);
  cl_assert(!s->ongoing);
  cl_assert_equal_i(s->start_utc + s->length_min * PBL_SEC_PER_MIN,
                    ablage.letzte_artige + PBL_SEC_PER_MIN);
}

// Gegenpruefung B3: Das Einstecken beendet eine bestaetigte Fahrt auf KernelMain, unter der
// Activity-Sperre (activity.c:1323 -> early_deinit -> D15), die der Minutenteil jetzt auch
// nimmt. Einmal beendet: Sitzung zu, kein zweites Ende, wenn danach noch eine Minute laeuft.
void test_boulder_fahrt__einstecken_beendet_die_fahrt_einmal(void) {
  prv_gehen(16);
  bool ja = true;
  cl_assert(prv_velo_bis_kandidatin(12, true) > 0);
  prv_velo(1, &ja);
  prv_taste(BUTTON_ID_UP);
  fake_system_task_callbacks_invoke_pending();
  prv_velo(5, &ja);
  cl_assert(boulder_kontext_fahrt_laeuft());
  const time_t start = prv_sitzung(ActivitySessionType_Open)->start_utc;
  pbl_mutex_lock(&s_state.mutex, PBL_FOREVER);
  activity_algorithm_early_deinit();
  pbl_mutex_unlock(&s_state.mutex);
  cl_assert_equal_i(boulder_kontext_zustand(), FahrtKeine);
  const ActivitySession *s = prv_sitzung(ActivitySessionType_Open);
  cl_assert(!s->ongoing);
  cl_assert_equal_i(s->start_utc, start);
  const uint16_t laenge = s->length_min;
  const uint32_t zaehler = prv_zaehler();
  // Ein zweites Beenden (zweites Ereignis vom Ladegeraet) aendert nichts mehr.
  boulder_kontext_sitzung_beenden();
  cl_assert_equal_i(prv_sitzung(ActivitySessionType_Open)->length_min, laenge);
  cl_assert_equal_i(prv_zaehler(), zaehler);
  cl_assert_equal_i(s_activity_sperre, 0);
}

// =============================================================================================
// Schritt 2, AP4: Training ohne Schritte (D17, V1), Fahrt und Training, Pulskalorien (F7)

// Ein Training ueber Pebbles echten Workout-Dienst, wie Kieselsport es startet (plattform.c,
// ks_workout_starten): erst Pebbles Start, dann - ohne Sperre - der Merker der Variante.
static ActivitySessionType s_start_typ;
static bool s_start_ohne_schritte;
static bool s_loggen_nach_start;  //< T4.3: Pebbles Logging laeuft zwischen den Schliessungen
static time_t s_start_um;
static void prv_training_start(void) {
  s_start_um = rtc_get_time();
  cl_assert(workout_service_start_workout(s_start_typ));
  boulder_variante_training_ohne_schritte(s_start_ohne_schritte);
  cl_assert_equal_i(s_activity_sperre, 0);
  if (s_loggen_nach_start) {
    activity_sessions_prv_minute_handler(rtc_get_time());
  }
}
static time_t s_stopp_um;
static void prv_training_stopp(void) {
  s_stopp_um = rtc_get_time();
  prv_workout_timer_cb(NULL);  // Pebbles Sekundentakt lief bis jetzt
  cl_assert(workout_service_stop_workout());
}

static uint16_t prv_rate(void) {
  uint16_t schritte;
  uint32_t ms;
  time_t ende;
  cl_assert(activity_algorithm_get_step_rate(&schritte, &ms, &ende));
  return schritte;
}

// Pebbles Sitzungen eines Typs: die erkannte (manual = 0) und die des Trainings (manual = 1).
static const ActivitySession *prv_sitzung_manuell(ActivitySessionType typ, bool manuell) {
  const ActivitySession *gefunden = NULL;
  for (int i = 0; i < s_state.activity_sessions_count; i++) {
    const ActivitySession *a = &s_state.activity_sessions[i];
    if (a->type == typ && a->manual == manuell) {
      cl_assert(!gefunden);  // hoechstens eine
      gefunden = a;
    }
  }
  return gefunden;
}

static time_t prv_ende(const ActivitySession *a) {
  return a->start_utc + (time_t)a->length_min * PBL_SEC_PER_MIN;
}

// T4.1 (Kriterium 4, V1 vom Nutzer geaendert): Ein Training ohne Schritte (Velo, MTB, Kraft, Yoga,
// Schwimmen) zaehlt vom Start bis zum Stopp keine Schritte - nicht in der Tagessumme, nicht in der
// Minute (DLS 81, Minutendatei), Rate 0. Auch viele Schritte (Kraft, Schwimmen: Kraepelin sieht
// Gehen) nicht. Was vor dem Start und nach dem Stopp in derselben Minute kommt, zaehlt.
void test_boulder_fahrt__t4_1_training_ohne_schritte_zaehlt_nichts(void) {
  // Der Merker ohne Training wirkt nicht.
  boulder_variante_training_ohne_schritte(true);
  prv_gehen(10);
  cl_assert(prv_rate() > 0);
  s_start_typ = ActivitySessionType_Open;
  s_start_ohne_schritte = true;
  prv_minute_geteilt(60, prv_training_start, 35, 60, 2600, 0x41, 0);
  const uint32_t summe = prv_zaehler();
  s_erwartet_ohne_schritte = true;
  for (int i = 0; i < 20; i++) {
    prv_minute((i % 3) ? 14 : 118, (uint16_t)(700 + i * 40), 0x37, prv_puls(120));
    cl_assert_equal_i(prv_zaehler(), summe);
    cl_assert_equal_i(prv_rate(), 0);
  }
  s_erwartet_ohne_schritte = false;
  prv_minute_geteilt(50, prv_training_stopp, 45, 45, 2600, 0x41, 0);
  cl_assert(prv_rate() > 0);
  // Pebble hat das Training gespeichert, die Variante zaehlt wieder.
  const ActivitySession *t = prv_sitzung_manuell(ActivitySessionType_Open, true);
  cl_assert(t);
  cl_assert_equal_i(t->start_utc, s_start_um);
  cl_assert_equal_i(t->length_min, 21);
  prv_gehen(40);
  cl_assert_equal_i(prv_zaehler(), prv_soll_summe());
  cl_assert(prv_pruefe_minuten() >= 10 + 22);
  // Kein Schritt der Trainingsminuten ging an der Summe vorbei: es sind keine Fahrtschritte.
  cl_assert_equal_i(boulder_kraepelin_gestrichen(), 0);
}

// T4.1, die andere Seite: Laufen und Wandern (Run, Walk) zaehlen Schritte wie Pebble. Ein Training
// ohne Schritte davor laesst nichts zurueck: Der Merker endet mit seinem Training - auch fuer ein
// Training aus Pebbles eigener Workout-App, die den Merker nie anfasst (das erste unten).
void test_boulder_fahrt__t4_1_laufen_zaehlt_schritte(void) {
  prv_gehen(5);
  s_start_typ = ActivitySessionType_Open;
  s_start_ohne_schritte = true;
  prv_training_start();
  s_erwartet_ohne_schritte = true;
  prv_velo(5, NULL);
  s_erwartet_ohne_schritte = false;
  prv_training_stopp();
  const ActivitySessionType arten[] = {ActivitySessionType_Run, ActivitySessionType_Walk};
  for (size_t k = 0; k < ARRAY_LENGTH(arten); k++) {
    if (k == 0) {
      cl_assert(workout_service_start_workout(arten[k]));  // Pebbles Workout-App
    } else {
      s_start_typ = arten[k];
      s_start_ohne_schritte = false;
      prv_training_start();  // Kieselsport (Wandern)
    }
    const uint32_t vorher = prv_zaehler();
    for (int i = 0; i < 12; i++) {
      prv_minute(150, 3200, 0x42, prv_puls(140));
      cl_assert(prv_rate() > 0);
    }
    cl_assert_equal_i(prv_zaehler(), vorher + 12 * 150);
    prv_training_stopp();
  }
  prv_gehen(40);
  cl_assert_equal_i(prv_zaehler(), prv_soll_summe());
  cl_assert(prv_pruefe_minuten() >= 5 + 5 + 24);
}

// T4.2, T2.3 mit Training (Kriterium 4): Eine bestaetigte Fahrt laeuft, zwei Minuten an der Ampel
// (weder velo- noch auto-artig), dann startet ein Velo-Training mitten in der Minute. Die Fahrt
// endet am Trainingsstart (auf die Minute), mit der Ampel, und ueberdeckt das Training nicht. Auf
// der Uhr je ein Eintrag; Tagessumme, DLS 81 und Minutendatei um genau die Fahrt- und
// Trainingsschritte kleiner. T4.3 laesst Pebbles Logging zwischen den beiden Schliessungen der
// Fahrt laufen (Pebbles beim Start, die der Variante in der naechsten Minute).
static void prv_fahrt_dann_training(bool loggen_dazwischen) {
  prv_gehen(14);
  const time_t beginn = rtc_get_time();
  bool ja = true;
  cl_assert(prv_velo_bis_kandidatin(12, true) > 0);
  prv_velo(1, &ja);
  prv_taste(BUTTON_ID_UP);
  fake_system_task_callbacks_invoke_pending();
  prv_velo(8, &ja);
  const int32_t kcal_bis_ampel = boulder_kontext_kcal();
  // Die Ampel: kaum Bewegung, gehoert zur Fahrt, solange sie nicht zu Ende ist.
  s_fahrt[s_n] = true;
  prv_minute(4, 12, 0x37, prv_puls(98));
  s_fahrt[s_n] = true;
  prv_minute(3, 9, 0x37, prv_puls(97));
  const ActivitySession *f = prv_sitzung_manuell(ActivitySessionType_Open, false);
  cl_assert(f->ongoing);
  const uint16_t vorher_min = f->length_min;  // bis zur letzten fahrtartigen Minute
  const time_t start_minute = rtc_get_time();
  s_start_typ = ActivitySessionType_Open;
  s_start_ohne_schritte = true;
  s_loggen_nach_start = loggen_dazwischen;
  // Vor dem Start 6 Schritte (noch Fahrt, zaehlen am Ende wieder), danach 9 (Training: keine).
  prv_minute_geteilt(6, prv_training_start, 9, 6, 800, 0x37, prv_puls(104));
  s_loggen_nach_start = false;
  // Pebbles Start hat die Fahrt geschlossen, die Variante in dieser Minute auf den Start gesetzt.
  f = prv_sitzung_manuell(ActivitySessionType_Open, false);
  cl_assert(!f->ongoing);
  cl_assert_equal_i(f->start_utc, beginn);
  cl_assert_equal_i(prv_ende(f), start_minute);
  cl_assert_equal_i(f->length_min, vorher_min + 2);
  cl_assert_equal_i(boulder_kontext_zustand(), FahrtKeine);
  s_erwartet_ohne_schritte = true;
  prv_velo(15, NULL);
  s_erwartet_ohne_schritte = false;
  prv_minute_geteilt(5, prv_training_stopp, 20, 20, 2600, 0x41, prv_puls(92));
  prv_gehen(40);
  f = prv_sitzung_manuell(ActivitySessionType_Open, false);
  const ActivitySession *t = prv_sitzung_manuell(ActivitySessionType_Open, true);
  cl_assert(f && t);
  cl_assert_equal_i(t->start_utc, s_start_um);
  cl_assert(prv_ende(f) <= t->start_utc);
  cl_assert(prv_ende(f) > t->start_utc - PBL_SEC_PER_MIN);
  cl_assert_equal_i(t->length_min, 16);
  cl_assert(!prv_gehen_in(beginn, prv_ende(t)));
  // F7: Die Fahrt traegt ihre Pulskalorien bis zum Trainingsstart, mit der Ampel.
  cl_assert(boulder_kontext_kcal() > kcal_bis_ampel);
  cl_assert(f->step_data.active_kcalories > 0);
  cl_assert_equal_i(f->step_data.active_kcalories, ROUND(boulder_kontext_kcal(), 1000));
  cl_assert_equal_i(prv_zaehler(), prv_soll_summe());
  cl_assert(prv_pruefe_minuten() >= 14 + 25);
  cl_assert_equal_i(boulder_kraepelin_gestrichen(), prv_fahrt_summe());
  cl_assert_equal_i(s_heap_im_minutenteil, 0);
}

void test_boulder_fahrt__t4_2_fahrt_endet_am_trainingsstart(void) {
  prv_fahrt_dann_training(false);
  // Ohne Logging dazwischen geht jede Sitzung genau einmal ins Datalogging (DLS 84).
  int fahrt = 0, training = 0;
  const ActivitySession *f = prv_sitzung_manuell(ActivitySessionType_Open, false);
  for (int i = 0; i < s_dls84_anzahl; i++) {
    fahrt += s_dls84[i].start_utc == (uint32_t)f->start_utc;
    training += s_dls84[i].start_utc == (uint32_t)s_start_um;
  }
  cl_assert_equal_i(fahrt, 1);
  cl_assert_equal_i(training, 1);
}

// T4.3: Mit Logging zwischen den beiden Schliessungen gehen von der Fahrt zwei Saetze hinaus,
// gleicher Start und Typ, der spaetere laenger (bis zum Trainingsstart). Boulder speichert unter
// (Start, Typ) und behaelt den laengeren (B: HealthDaoExt.kt:45-57): ein Eintrag je Sitzung. Das
// prueft Boulders eigener Code mit diesen Saetzen (Scratchpad, Berichte AP4 und AP5: Parser, DAO
// und das Ueberspringen von `manual`); mit BOULDER_DLS84_AUSGABE schreibt der Test sie dafuer als
// Hex in eine Datei.
void test_boulder_fahrt__t4_3_dls84_mit_logging_zwischen_den_schliessungen(void) {
  prv_fahrt_dann_training(true);
  const ActivitySession *f = prv_sitzung_manuell(ActivitySessionType_Open, false);
  int fahrt = 0, training = 0;
  uint32_t erste = 0, letzte = 0;
  for (int i = 0; i < s_dls84_anzahl; i++) {
    const ActivitySessionDataLoggingRecord *r = &s_dls84[i];
    // P4 (F4 = A): Fassung 7, 27 Byte; `manual` traegt nur das Training. Boulder 0.52 traegt
    // es damit nicht noch einmal als Workout ein; Fahrt und Spaziergaenge bleiben erkannt.
    const uint8_t *bytes = (const uint8_t *)r;
    cl_assert_equal_i(r->version, 7);
    cl_assert_equal_i(r->size, 27);
    cl_assert_equal_i(bytes[26], r->start_utc == (uint32_t)s_start_um);
    if (r->activity != ActivitySessionType_Open) {
      continue;  // Kraepelins Spaziergaenge davor und danach
    }
    if (r->start_utc == (uint32_t)f->start_utc) {
      erste = fahrt++ ? erste : r->elapsed_sec;
      letzte = r->elapsed_sec;
    }
    training += r->start_utc == (uint32_t)s_start_um;
  }
  cl_assert_equal_i(fahrt, 2);
  cl_assert(erste < letzte);
  cl_assert_equal_i(letzte, f->length_min * PBL_SEC_PER_MIN);
  cl_assert_equal_i(training, 1);
  const char *ausgabe = getenv("BOULDER_DLS84_AUSGABE");
  if (ausgabe) {
    FILE *d = fopen(ausgabe, "w");
    cl_assert(d);
    for (int i = 0; i < s_dls84_anzahl; i++) {
      for (size_t b = 0; b < sizeof(s_dls84[i]); b++) {
        fprintf(d, "%02x", ((const uint8_t *)&s_dls84[i])[b]);
      }
      fprintf(d, "\n");
    }
    fclose(d);
  }
}

// T4.4 (Kriterium 4, Spezifikation G10): Ein Velo-Training von 30 Minuten endet mitten in der
// Minute, der Nutzer faehrt weiter. Die Kandidatin danach beginnt nicht vor dem Trainingsende -
// auch nicht in der Minute des Stopps, die bis dahin Training war.
void test_boulder_fahrt__t4_4_nach_dem_training_keine_kandidatin_davor(void) {
  prv_gehen(14);
  s_start_typ = ActivitySessionType_Open;
  s_start_ohne_schritte = true;
  prv_training_start();
  s_erwartet_ohne_schritte = true;
  prv_velo(30, NULL);
  s_erwartet_ohne_schritte = false;
  cl_assert_equal_i(s_ui.dialoge, 0);
  // Stopp nach 30 s, dann weiter im Sattel: velo-artig.
  prv_minute_geteilt(9, prv_training_stopp, 6, 6, 820, 0x37, prv_puls(104));
  const int nach = prv_velo_bis_kandidatin(15, false);
  printf("\nNach dem Training: Kandidatin nach %d min, ab %+d s nach dem Stopp\n", nach,
         (int)(boulder_kontext_beginn() - s_stopp_um));
  cl_assert(nach > 0);
  cl_assert(boulder_kontext_beginn() >= s_stopp_um);
}

// T4.4, der Ring: Neun Velo-Minuten (noch keine Kandidatin: Mindestdauer 10 min), ein kurzes
// Training, weiter velo-artig. Die Minuten vor dem Training zaehlen nicht mehr mit - sonst
// begaenne die Kandidatin vor ihm und ueberdeckte es.
void test_boulder_fahrt__t4_4_kurzes_training_leert_den_ring(void) {
  prv_gehen(14);
  prv_velo(9, NULL);
  cl_assert_equal_i(boulder_kontext_zustand(), FahrtKeine);
  s_start_typ = ActivitySessionType_Open;
  s_start_ohne_schritte = true;
  prv_training_start();
  s_erwartet_ohne_schritte = true;
  prv_velo(2, NULL);
  s_erwartet_ohne_schritte = false;
  prv_training_stopp();
  const int nach = prv_velo_bis_kandidatin(15, false);
  cl_assert(nach >= 10);
  cl_assert(boulder_kontext_beginn() >= s_stopp_um);
}

// =============================================================================================
// T5.2 (Kriterium 5, F7): Pulskalorien der bestaetigten Velofahrt - derselbe Zaehler wie im
// Training, gegen Boulders eigenen Code. Die Sollwerte stammen aus Trainingsanalyse.kt (Boulder
// 34fc13cb), ausgefuehrt mit dem Orakel aus AP1 (Scratchpad a4-orakel/Main.kt): Profil 70 kg,
// 41 Jahre, Mann, Maximum 190, kein Ruhepuls (keine Nacht: 60). Boulder zaehlt den letzten Punkt
// 10 s; ein Punkt am Fahrtende unter dem Ruhepuls zaehlt dort 0, so endet der letzte echte Punkt
// wie auf der Uhr am Fahrtende.

// Boulders Testfahrt vom 29.9.2026 (TrainingsanalyseTest.kt:65-68), je Minute.
static const uint8_t FAHRT_PULS[] = {
  95, 94, 126, 121, 120, 122, 121, 128, 130, 132, 128, 122,
  129, 125, 140, 139, 137, 140, 135, 125, 129, 125, 143, 145,
};

static void prv_velo_mit_puls(const uint8_t *puls, int minuten, bool gestrichen) {
  for (int i = 0; i < minuten; i++) {
    const int k = s_minute_nr;
    s_fahrt[s_n] = gestrichen;
    prv_minute((uint8_t)(3 + (k * 7) % 16), (uint16_t)(700 + (k * 37) % 600),
               (k % 5 == 4) ? 0x38 : 0x37, puls[i]);
  }
}

static void prv_wie_boulder(int32_t cal, double boulder_kcal) {
  const double soll = boulder_kcal * 1000.0;
  printf("\n  %d cal, Boulder %.3f cal\n", (int)cal, soll);
  cl_assert(cal >= (int32_t)soll - 2 && cal <= (int32_t)soll + 3);
}

// Nur Minutenpulse (Pebbles Messung jede Minute, kein Sekundenwert): jede Minute gilt 60 s, von
// der ersten fahrtartigen Minute an - auch die vor der Kandidatin (Orakel A1: 156,750300 kcal).
void test_boulder_fahrt__t5_2_fahrt_mit_minutenpuls_wie_boulder(void) {
  prv_gehen(14);
  const time_t beginn = rtc_get_time();
  int i = 0;
  for (; i < 12 && boulder_kontext_zustand() != FahrtKandidatin; i++) {
    prv_velo_mit_puls(&FAHRT_PULS[i], 1, true);
  }
  cl_assert_equal_i(boulder_kontext_zustand(), FahrtKandidatin);
  prv_taste(BUTTON_ID_UP);
  fake_system_task_callbacks_invoke_pending();
  prv_velo_mit_puls(&FAHRT_PULS[i], (int)ARRAY_LENGTH(FAHRT_PULS) - i, true);
  prv_gehen(10);
  const ActivitySession *f = prv_sitzung_manuell(ActivitySessionType_Open, false);
  cl_assert_equal_i(f->start_utc, beginn);
  cl_assert_equal_i(f->length_min, 24);
  prv_wie_boulder(boulder_kontext_kcal(), 156.750300);
  cl_assert_equal_i(f->step_data.active_kcalories, 157);
}

// Fahrtbeginn: Sekunde 0 der ersten Velominute (Orakel A2).
static time_t s_fahrt_beginn;
static uint8_t prv_sekundenpuls_a2(time_t utc) {
  const int s = (int)(utc - s_fahrt_beginn);
  return (s % 89 == 7) ? 0 : (uint8_t)(110 + (s / 20) % 40 + (((s / 120) % 2) ? 15 : 0));
}

// Ab Minute 13 Sekundenwerte des Abos "Aktiv" (einzelne fehlen); ihr Minutenpuls (100) zaehlt dann
// nicht. Bis dahin der Minutenpuls, auch nach der Kandidatin. Nach der Fahrt kommen weiter
// Sekundenwerte, bis das Ende feststeht: Sie gehoeren nicht zur Fahrt (Orakel A2: 199,074178).
void test_boulder_fahrt__t5_2_fahrt_mit_sekundenpuls_wie_boulder(void) {
  prv_gehen(14);
  s_fahrt_beginn = rtc_get_time();
  int i = 0;
  for (; i < 12 && boulder_kontext_zustand() != FahrtKandidatin; i++) {
    prv_velo_mit_puls(&FAHRT_PULS[i], 1, true);
  }
  cl_assert_equal_i(boulder_kontext_zustand(), FahrtKandidatin);
  prv_taste(BUTTON_ID_UP);
  fake_system_task_callbacks_invoke_pending();
  prv_velo_mit_puls(&FAHRT_PULS[i], 13 - i, true);
  cl_assert_equal_i(rtc_get_time(), s_fahrt_beginn + 13 * PBL_SEC_PER_MIN);
  s_sekundenpuls = prv_sekundenpuls_a2;
  uint8_t hundert[17];
  memset(hundert, 100, sizeof(hundert));
  prv_velo_mit_puls(hundert, 17, true);
  prv_gehen(10);
  s_sekundenpuls = NULL;
  const ActivitySession *f = prv_sitzung_manuell(ActivitySessionType_Open, false);
  cl_assert_equal_i(f->length_min, 30);
  prv_wie_boulder(boulder_kontext_kcal(), 199.074178);
  cl_assert_equal_i(f->step_data.active_kcalories, 199);
}

// T5.2, "Nein": Der Zaehler faellt weg - weder Minuten- noch Sekundenwerte zaehlen danach, und es
// gibt keine Sitzung.
void test_boulder_fahrt__t5_2_nein_ohne_kalorien(void) {
  prv_gehen(14);
  int i = 0;
  for (; i < 12 && boulder_kontext_zustand() != FahrtKandidatin; i++) {
    prv_velo_mit_puls(&FAHRT_PULS[i], 1, false);
  }
  cl_assert(boulder_kontext_kcal() > 0);  // der Zaehler lief ab der Kandidatin
  prv_taste(BUTTON_ID_DOWN);
  fake_system_task_callbacks_invoke_pending();
  cl_assert_equal_i(boulder_kontext_zustand(), FahrtAbgelehnt);
  s_fahrt_beginn = rtc_get_time();
  s_sekundenpuls = prv_sekundenpuls_a2;
  prv_velo_mit_puls(&FAHRT_PULS[i], 6, false);
  s_sekundenpuls = NULL;
  cl_assert_equal_i(boulder_kontext_kcal(), 0);
  prv_gehen(10);
  cl_assert(prv_sitzung(ActivitySessionType_Open) == NULL);
}
