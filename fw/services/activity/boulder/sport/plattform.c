/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "plattform.h"

#include "art.h"
#include "bahnen.h"
#include "dienst.h"
#include "puls.h"
#include "reps.h"

#include "applib/accel_service_private.h"
#include "applib/app_launch_reason.h"
#include "applib/ui/vibes.h"
#include "kernel/event_loop.h"
#include "kernel/pebble_tasks.h"
#include "pbl/drivers/rtc.h"
#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/boulder/kalorien.h"
#include "pbl/services/activity/boulder/nacht.h"
#include "pbl/services/activity/boulder/takt.h"
#include "pbl/services/activity/boulder/variante.h"
#include "pbl/services/activity/workout_service.h"
#include "pbl/services/battery/battery_state.h"
#include "pbl/services/data_logging/data_logging_service.h"
#include "pbl/services/hrm/hrm_manager.h"
#include "pbl/services/hrm/hrm_manager_private.h"
#include "pbl/services/new_timer/new_timer.h"
#include "pbl/services/system_task.h"
#include "pbl/util/math.h"
#include "pbl/util/trig.h"
#include "pbl/util/units.h"
#include "process_management/app_install_manager.h"
#include "process_management/app_manager.h"
#include "system/passert.h"

#ifdef CONFIG_MAG
#include "applib/compass_service.h"
#endif

time_t ks_jetzt(void) {
  return rtc_get_time();
}

// --- Profil ---

uint8_t ks_alter(void) {
  // DAS TELEFON GLEICHT ES AB (Gesundheitseinstellungen); ohne Abgleich steht hier die Vorgabe
  // der Firmware.
  return activity_prefs_get_age_years();
}

uint8_t ks_ruhepuls(void) {
  // Derselbe Ruhepuls wie fuer die Pulskalorien (boulder_kalorien_profil_jetzt): der Median der
  // letzten sieben Naechte (V3), wie Boulder ihn fuer Maximalpuls und Zonen nimmt.
  return boulder_nacht_ruhepuls_median();
}

// --- Pebbles Workout-Dienst ---
//
// PEBBLE KENNT NUR RUN, WALK UND OPEN (workout_service.c:273-276). Laufen und Wandern haben dort
// ihre Art, alles andere ist ein offenes Training - so stand es auch in b25s Sitzung.
static ActivitySessionType prv_typ(Sportart art) {
  switch (art) {
    case ArtLaufen:
      return ActivitySessionType_Run;
    case ArtWandern:
      return ActivitySessionType_Walk;
    default:
      return ActivitySessionType_Open;
  }
}

bool ks_workout_starten(Sportart art, uint16_t maxpuls) {
  // Die Pulskalorien rechnen mit Boulders geprueftem Maximum (V2); boulder_kalorien_start liest
  // es beim Start unter Pebbles Sperre.
  ks_kalorien_maxpuls(maxpuls);
  if (!workout_service_start_workout(prv_typ(art))) {
    return false;
  }
  // DIE SZENE (V5): Pebble setzt fuer Open das Modell fuer hohe Belastung. Yoga ist Ruhe, und fuer
  // Schwimmen gibt es kein Modell - beide rechnen wie in b25 mit dem allgemeinen. Den Stopp setzt
  // Pebble selbst zurueck.
  if (art == ArtYoga || art == ArtSchwimmen) {
    hrm_manager_set_activity_scene(HRMActivityScene_Default);
  }
  // SCHRITTE NUR BEIM LAUFEN UND WANDERN (V1, vom Nutzer geaendert): Am Lenker, an der Hantel, auf
  // der Matte und im Wasser zaehlt der Schrittzaehler Bewegung, die keine Schritte sind. Die
  // Variante laesst sie bis zum Stopp weg - aus Tagessumme, Minute und Rate (D17). Erst nach dem
  // Start, ohne Sperre (Regel 2.5.2): der Start hat den Merker der Variante frisch gesetzt.
  boulder_variante_training_ohne_schritte(!art_info(art)->schritte);
  return true;
}

void ks_kalorien_maxpuls(uint16_t maxpuls) {
  boulder_kalorien_maxpuls((uint8_t)MIN(maxpuls, UINT8_MAX));
}

bool ks_workout_pause(bool an) {
  return workout_service_pause_workout(an);
}

bool ks_workout_lesen(KsWorkout *aus) {
  *aus = (KsWorkout){0};
  // ERST DIE PAUSE, DANN DIE DAUER: Pausiert Pebble dazwischen (Laden, KernelMain), ist die Dauer
  // schon die angehaltene, und der naechste Takt traegt die Pause bei genau dieser Sekunde ein.
  const bool pause = workout_service_is_paused();
  int32_t schritte = 0;
  int32_t dauer = 0;
  int32_t meter = 0;
  if (!workout_service_get_current_workout_info(&schritte, &dauer, &meter, NULL, NULL)) {
    return false;
  }
  *aus = (KsWorkout){
    .laeuft = true,
    .pause = pause,
    .dauer_s = dauer,
    .schritte = schritte,
    .meter = meter,
    // Dieselbe Summe, die beim Stopp in Pebbles Sitzung kommt (P1, boulder_kalorien_ende).
    .kcal_cal = boulder_kalorien_bisher(dauer),
  };
  return true;
}

// V8: DAS TRAINING GLEICH ANS TELEFON, nicht erst mit dem Datalogging-Takt. Aber nicht sofort:
// die Sitzung geht erst mit der naechsten Minute der Uhr ins Datalogging (activity_sessions.c,
// prv_log_activities). Anderthalb Minuten liegen sicher danach.
#define SCHICKEN_NACH_MS (90 * PBL_MSEC_PER_SEC)
static TimerID s_schicken_takt = TIMER_INVALID_ID;

static void prv_schicken_faellig(void *daten) {
  // dls_send_all_sessions reiht einen nicht verwerfbaren Callback ein (dls_main.c:343-350): nur
  // bei Platz (Schritt 1, Regel 2). Sonst geht die Sitzung mit Pebbles eigenem Takt hinaus.
  if (system_task_get_available_space() < BOULDER_SCHLANGE_FREI) {
    return;
  }
  dls_send_all_sessions();
}

void ks_workout_beenden(void) {
  if (!workout_service_stop_workout()) {
    return;
  }
  if (s_schicken_takt == TIMER_INVALID_ID) {
    s_schicken_takt = new_timer_create();
  }
  new_timer_start(s_schicken_takt, SCHICKEN_NACH_MS, prv_schicken_faellig, NULL, 0);
}

void ks_workout_verwerfen(void) {
  // F3 = A: Pebbles Stopp mit dem Merker aus kalorien.c - keine Sitzung, keine Meldung.
  boulder_training_verwerfen();
}

// --- Puls ---
//
// EINE EIGENE ANMELDUNG AM SENSOR, mit Ablauf (F2 = A). puls_auffrischen erneuert sie jede
// Minute; bleibt das aus - ein Fehler, ein haengender Dienst -, gibt der Sensor nach ein paar
// Minuten von selbst auf, statt den Akku die ganze Nacht im Sekundentakt zu leeren. Pebbles
// Workout-Dienst bekommt dieselben Werte ueber Activitys Rundruf (activity.c:185-245).

#define PULS_ABLAUF_S (5 * PBL_SEC_PER_MIN)

static HRMSessionRef s_puls = HRM_INVALID_SESSION_REF;

// KernelBG, unter der Sperre des HRM-Managers (hrm_manager.c:458-534). Darum hier nur die Sperre
// des Dienstes und kein workout_service_*, und kein An- oder Abmelden.
static void prv_puls_ereignis(PebbleHRMEvent *e, void *context) {
  ks_sperren();
  if (e->event_type == HRMEvent_BPM) {
    puls_ereignis_bpm(e->bpm.bpm, (int8_t)e->bpm.quality);
  } else if (e->event_type == HRMEvent_HRV) {
    puls_ereignis_ppi(e->hrv.ppi_ms);
  }
  ks_entsperren();
}

void ks_puls_sensor(uint16_t takt_s, bool hrv) {
  if (takt_s == 0) {
    if (s_puls != HRM_INVALID_SESSION_REF) {
      sys_hrm_manager_unsubscribe(s_puls);
      s_puls = HRM_INVALID_SESSION_REF;
    }
    return;
  }
  const HRMFeature merkmale = HRMFeature_BPM | (hrv ? HRMFeature_HRV : 0);
  // Ist die Anmeldung abgelaufen, schlaegt das Nachstellen fehl - dann neu.
  if (s_puls != HRM_INVALID_SESSION_REF &&
      sys_hrm_manager_set_update_interval(s_puls, takt_s, PULS_ABLAUF_S) &&
      sys_hrm_manager_set_features(s_puls, merkmale)) {
    return;
  }
  s_puls = hrm_manager_subscribe_with_callback(INSTALL_ID_INVALID, takt_s, PULS_ABLAUF_S,
                                               merkmale, false /* low_latency */,
                                               prv_puls_ereignis, NULL);
}

// --- Beschleunigung ---

static AccelServiceState *s_accel;

static void prv_accel_daten(AccelData *daten, uint32_t anzahl) {
  KsAccel stueck[10];
  while (anzahl > 0) {
    const uint32_t n = MIN(anzahl, ARRAY_LENGTH(stueck));
    for (uint32_t i = 0; i < n; i++) {
      stueck[i] = (KsAccel){
        .x = daten[i].x,
        .y = daten[i].y,
        .z = daten[i].z,
        .did_vibrate = daten[i].did_vibrate,
      };
    }
    ks_sperren();
    reps_daten(stueck, n);
    ks_entsperren();
    daten += n;
    anzahl -= n;
  }
}

void ks_accel(bool an) {
  // Die Daten kommen in der Aufgabe an, die sich angemeldet hat - und das soll KernelBG sein, wo
  // auch der Takt laeuft.
  PBL_ASSERTN(pebble_task_get_current() == PebbleTask_KernelBackground);
  if (an && !s_accel) {
    s_accel = accel_session_create();
    accel_session_data_subscribe(s_accel, 10, prv_accel_daten);
  } else if (!an && s_accel) {
    accel_session_data_unsubscribe(s_accel);
    accel_session_delete(s_accel);
    s_accel = NULL;
  }
}

// --- Kompass ---
//
// DER KOMPASS SPRICHT NUR MIT APP, WORKER UND KERNELMAIN. Also meldet sich der Dienst auf
// KernelMain an; die Richtung geht unter der Sperre direkt an den Bahnenzaehler, der nur zwei
// Zahlen mittelt.

#ifdef CONFIG_MAG
static void prv_kompass_daten(CompassHeadingData daten) {
  ks_sperren();
  bahnen_kompass(daten.magnetic_heading, daten.compass_status == CompassStatusCalibrated ||
                                             daten.compass_status == CompassStatusCalibrating);
  ks_entsperren();
}

static void prv_kompass_an(void *daten) {
  compass_service_subscribe(prv_kompass_daten);
  // Nicht bei jedem Grad aufwachen: fuenfzehn reichen, um eine Wende zu sehen, und sparen Strom.
  compass_service_set_heading_filter(TRIG_MAX_ANGLE / 24);
}

static void prv_kompass_aus(void *daten) {
  compass_service_unsubscribe();
}
#endif

void ks_kompass(bool an) {
#ifdef CONFIG_MAG
  launcher_task_add_callback(an ? prv_kompass_an : prv_kompass_aus, NULL);
#endif
}

// --- Was sonst geschieht ---

void ks_brummen(const uint32_t *dauer_ms, uint32_t anzahl) {
  const VibePattern muster = {.durations = dauer_ms, .num_segments = anzahl};
  vibes_enqueue_custom_pattern(muster);
}

uint8_t ks_akku_prozent(bool *am_kabel) {
  const BatteryChargeState akku = battery_get_charge_state();
  *am_kabel = akku.is_plugged;
  return akku.charge_percent;
}

void ks_app_holen(void) {
  // WIE FRUEHER DER WORKER, aber hoeflicher: der holte die App auch ueber eine andere hinweg.
  // Hier nur vom Zifferblatt aus - wer gerade etwas anderes tut, bekommt die Zusammenfassung beim
  // naechsten Versuch.
  if (!app_manager_is_watchface_running()) {
    return;
  }
  static const Uuid s_uuid = KIESELSPORT_UUID_INIT;
  app_manager_put_launch_app_event(&(AppLaunchEventConfig){
    .id = app_install_get_id_for_uuid(&s_uuid),
    .common.reason = APP_LAUNCH_WORKER,
  });
}
