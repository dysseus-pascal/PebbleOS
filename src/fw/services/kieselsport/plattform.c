/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "plattform.h"

#include "bahnen.h"
#include "dienst.h"
#include "puls.h"
#include "reps.h"

#include "applib/accel_service_private.h"
#include "applib/app_launch_reason.h"
#include "apps/system_app_ids.h"
#include "process_management/app_install_manager.h"
#include "process_management/app_manager.h"
#include "applib/persist.h"
#include "applib/ui/vibes.h"
#include "kernel/event_loop.h"
#include "kernel/pebble_tasks.h"
#include "pbl/drivers/rtc.h"
#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/activity_algorithm.h"
#include "pbl/services/activity/activity_calculators.h"
#include "pbl/services/activity/activity_insights.h"
#include "pbl/services/activity/activity_private.h"
#include "pbl/services/battery/battery_state.h"
#include "pbl/services/health_sync_endpoint.h"
#include "pbl/services/new_timer/new_timer.h"
#include "pbl/services/system_task.h"
#include "pbl/services/persist.h"
#include "pbl/services/settings/settings_file.h"
#include "pbl/util/math.h"
#include "pbl/util/trig.h"
#include "system/passert.h"
#include "util/time/time.h"

#ifdef CONFIG_HRM
#include "pbl/services/hrm/hrm_manager.h"
#include "pbl/services/hrm/hrm_manager_private.h"
#endif

#ifdef CONFIG_MAG
#include "applib/compass_service.h"
#endif

time_t ks_jetzt(void) {
  return rtc_get_time();
}

// --- Persist ---
//
// DER SPEICHER VON KIESELSPORT, NICHT DER DER LAUFENDEN APP. Jeder Zugriff
// meldet sich als Nutzer der Datei an und wieder ab; laeuft die App oder ein
// Training, bleibt sie dazwischen offen, und das An- und Abmelden kostet
// nur einen Zaehler.

static const Uuid s_uuid = KIESELSPORT_UUID_INIT;

static SettingsFile *prv_auf(void) {
  persist_service_client_open(&s_uuid);
  return persist_service_lock_and_get_store(&s_uuid);
}

static void prv_zu(SettingsFile *datei) {
  persist_service_unlock_store(datei);
  persist_service_client_close(&s_uuid);
}

void ks_persist_offen_halten(bool an) {
  if (an) {
    persist_service_client_open(&s_uuid);
  } else {
    persist_service_client_close(&s_uuid);
  }
}

bool ks_persist_exists(uint32_t key) {
  SettingsFile *datei = prv_auf();
  const bool da = settings_file_exists(datei, &key, sizeof(key));
  prv_zu(datei);
  return da;
}

int32_t ks_persist_read_int(uint32_t key) {
  int32_t wert = 0;
  SettingsFile *datei = prv_auf();
  settings_file_get(datei, &key, sizeof(key), &wert, sizeof(wert));
  prv_zu(datei);
  return wert;
}

void ks_persist_write_int(uint32_t key, int32_t wert) {
  SettingsFile *datei = prv_auf();
  settings_file_set(datei, &key, sizeof(key), &wert, sizeof(wert));
  prv_zu(datei);
}

int ks_persist_read_data(uint32_t key, void *puffer, size_t groesse) {
  SettingsFile *datei = prv_auf();
  int gelesen = settings_file_get_len(datei, &key, sizeof(key));
  if (gelesen > 0) {
    gelesen = MIN((int)groesse, gelesen);
    if (FAILED(settings_file_get(datei, &key, sizeof(key), puffer, gelesen))) {
      gelesen = 0;
    }
  }
  prv_zu(datei);
  return gelesen > 0 ? gelesen : 0;
}

void ks_persist_write_data(uint32_t key, const void *daten, size_t groesse) {
  SettingsFile *datei = prv_auf();
  settings_file_set(datei, &key, sizeof(key), daten, MIN(groesse, PERSIST_DATA_MAX_LENGTH));
  prv_zu(datei);
}

void ks_persist_delete(uint32_t key) {
  SettingsFile *datei = prv_auf();
  if (settings_file_exists(datei, &key, sizeof(key))) {
    settings_file_delete(datei, &key, sizeof(key));
  }
  prv_zu(datei);
}

// --- Tageszaehler ---

uint32_t ks_heute(KsZaehler zaehler) {
  static const ActivityMetric s_metrik[] = {
    [KsZaehlerSchritte] = ActivityMetricStepCount,
    [KsZaehlerMeter] = ActivityMetricDistanceMeters,
    [KsZaehlerKcal] = ActivityMetricActiveKCalories,
  };
  int32_t wert = 0;
  if (!activity_get_metric(s_metrik[zaehler], 1, &wert) || wert < 0) {
    return 0;
  }
  return (uint32_t)wert;
}

uint8_t ks_alter(void) {
  // DAS TELEFON GLEICHT ES AB (Gesundheitseinstellungen der Pebble-App); ohne
  // Abgleich steht hier die Vorgabe der Firmware.
  return activity_prefs_get_age_years();
}

// --- Puls ---
//
// EINE EIGENE ANMELDUNG AM SENSOR, mit Ablauf. puls_auffrischen erneuert
// sie jede Minute; bleibt das aus - ein Fehler, ein haengender Dienst -,
// gibt der Sensor nach ein paar Minuten von selbst auf, statt den Akku die
// ganze Nacht im Sekundentakt zu leeren.

#ifdef CONFIG_HRM
#define PULS_ABLAUF_S (5 * SECONDS_PER_MINUTE)

static HRMSessionRef s_puls = HRM_INVALID_SESSION_REF;

static void prv_puls_ereignis(PebbleHRMEvent *e, void *context) {
  ks_sperren();
  if (e->event_type == HRMEvent_BPM) {
    puls_ereignis_bpm(e->bpm.bpm, (int8_t)e->bpm.quality);
  } else if (e->event_type == HRMEvent_HRV) {
    puls_ereignis_ppi(e->hrv.ppi_ms);
  }
  ks_entsperren();
}
#endif

void ks_puls_sensor(uint16_t takt_s, bool hrv) {
#ifdef CONFIG_HRM
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
#endif
}

void ks_puls_szene(Sportart art, bool an) {
#ifdef CONFIG_HRM
  // DER PULSALGORITHMUS KENNT BEWEGUNG. Beim Laufen schwingt der Arm im
  // Takt, und ein Modell, das das weiss, verwechselt den Takt seltener mit
  // dem Herz. Yoga und Schwimmen bleiben beim allgemeinen Modell: das eine
  // ist Ruhe, fuer das andere gibt es keines.
  HRMActivityScene szene = HRMActivityScene_Default;
  if (an) {
    switch (art) {
      case ArtLaufen:
        szene = HRMActivityScene_Run;
        break;
      case ArtWandern:
        szene = HRMActivityScene_Walk;
        break;
      case ArtBike:
      case ArtMTB:
      case ArtKraft:
        szene = HRMActivityScene_HighIntensity;
        break;
      default:
        break;
    }
  }
  hrm_manager_set_activity_scene(szene);
#endif
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
  // Die Daten kommen in der Aufgabe an, die sich angemeldet hat - und das
  // soll KernelBG sein, wo auch der Takt laeuft.
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
// DER KOMPASS SPRICHT NUR MIT APP, WORKER UND KERNELMAIN. Also meldet sich
// der Dienst auf KernelMain an; die Richtung geht unter der Sperre direkt an
// den Bahnenzaehler, der nur zwei Zahlen mittelt.

#ifdef CONFIG_MAG
static void prv_kompass_daten(CompassHeadingData daten) {
  ks_sperren();
  bahnen_kompass(daten.magnetic_heading, daten.compass_status == CompassStatusCalibrated ||
                                             daten.compass_status == CompassStatusCalibrating);
  ks_entsperren();
}

static void prv_kompass_an(void *daten) {
  compass_service_subscribe(prv_kompass_daten);
  // Nicht bei jedem Grad aufwachen: fuenfzehn reichen, um eine Wende zu
  // sehen, und sparen Strom.
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
  const VibePattern muster = { .durations = dauer_ms, .num_segments = anzahl };
  vibes_enqueue_custom_pattern(muster);
}

uint8_t ks_akku_prozent(bool *am_kabel) {
  const BatteryChargeState akku = battery_get_charge_state();
  *am_kabel = akku.is_plugged;
  return akku.charge_percent;
}

void ks_bewegung_erkennen(bool an) {
  activity_algorithm_enable_activity_tracking(an);
}

#define MELDUNG_NACH_MS (30 * MS_PER_SECOND)
// DAS TRAINING GLEICH ANS TELEFON, nicht erst mit dem Datalogging-Takt. Aber
// nicht sofort: die Sitzung geht erst mit der naechsten Minute der Uhr in
// Datalogging (activity_sessions, prv_log_activities). Anderthalb Minuten
// liegen sicher danach - der Minutenlauf steht dann schon vor uns auf KernelBG.
#define SCHICKEN_NACH_MS (90 * MS_PER_SECOND)
static TimerID s_schicken_takt = TIMER_INVALID_ID;

static void prv_schicken_faellig(void *daten) {
  health_sync_schicken();
}

static ActivitySession s_meldung;
static uint16_t s_meldung_puls;
static TimerID s_meldung_takt = TIMER_INVALID_ID;

static void prv_meldung_schicken(void *daten) {
  activity_insights_push_activity_session_notification(rtc_get_time(), &s_meldung, s_meldung_puls,
                                                       NULL);
}

static void prv_meldung_faellig(void *daten) {
  // Der NewTimer hat einen kleinen Stapel; die Meldung baut KernelBG.
  system_task_add_callback(prv_meldung_schicken, NULL);
}

void ks_sitzung_melden(const Trainingsstand *t) {
  // WIE DIE WORKOUT-APP: eine von Hand gestartete Sitzung im Verlauf der
  // Uhr. Laufen und Wandern haben dort ihre Art, alles andere ist ein
  // offenes Training.
  ActivitySessionType typ = ActivitySessionType_Open;
  if (t->art == ArtLaufen) {
    typ = ActivitySessionType_Run;
  } else if (t->art == ArtWandern) {
    typ = ActivitySessionType_Walk;
  }
  const uint32_t minuten = MIN(ACTIVITY_SESSION_MAX_LENGTH_MIN, t->dauer_s / SECONDS_PER_MINUTE);
  ActivitySession sitzung = {
    .type = typ,
    .start_utc = t->beginn,
    .length_min = minuten,
    .ongoing = false,
    .manual = true,
    // Die Sitzung zaehlt in uint16: ein langer Tag ueberschritte 65535 Schritte
    // und wuerde abgeschnitten statt begrenzt.
    .step_data.steps = MIN(t->schritte, UINT16_MAX),
    .step_data.distance_meters = MIN(t->meter, UINT16_MAX),
    .step_data.active_kcalories = MIN(t->kcal, UINT16_MAX),
    .step_data.resting_kcalories =
        ROUND(activity_private_compute_resting_calories(minuten), ACTIVITY_CALORIES_PER_KCAL),
  };
  activity_sessions_prv_add_activity_session(&sitzung);

  // DIE MELDUNG DER WORKOUT-APP ("You're a machine!") gehoert zum Charme der
  // Pebble - aber nicht sofort: da legte sie sich ueber den Schirm mit den
  // Zonen, den Kieselsport nach dem Speichern zeigt. Also eine halbe Minute
  // spaeter, mit unseren Zahlen. Die drei Zonen der Meldung sind Pebbles,
  // nicht unsere fuenf - deshalb ohne sie.
  s_meldung = sitzung;
  s_meldung_puls = t->puls_mittel;
  if (s_meldung_takt == TIMER_INVALID_ID) {
    s_meldung_takt = new_timer_create();
  }
  new_timer_start(s_meldung_takt, MELDUNG_NACH_MS, prv_meldung_faellig, NULL, 0);

  if (s_schicken_takt == TIMER_INVALID_ID) {
    s_schicken_takt = new_timer_create();
  }
  new_timer_start(s_schicken_takt, SCHICKEN_NACH_MS, prv_schicken_faellig, NULL, 0);
}

void ks_app_holen(void) {
  // WIE FRUEHER DER WORKER, aber hoeflicher: der holte die App auch ueber
  // eine andere hinweg. Hier nur vom Zifferblatt aus - wer gerade etwas
  // anderes tut, bekommt die Zusammenfassung beim naechsten Versuch.
  if (!app_manager_is_watchface_running()) {
    return;
  }
  app_manager_put_launch_app_event(&(AppLaunchEventConfig){
    .id = APP_ID_KIESELSPORT,
    .common.reason = APP_LAUNCH_WORKER,
  });
}

void ks_startmenue_oben(bool laeuft) {
  // Wie die Workout-App: waehrend des Trainings ganz oben, danach rutscht
  // sie nach der ueblichen Frist an ihren Platz zurueck.
  app_install_mark_prioritized(APP_ID_KIESELSPORT, !laeuft);
}
