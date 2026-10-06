/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2: das eine Puls-Abo der Variante (puls.h).

#include "pbl/services/activity/boulder/puls.h"

#include "pbl/services/activity/boulder/hrv.h"
#include "pbl/services/activity/boulder/messstufe.h"
#include "pbl/services/activity/boulder/takt.h"
#include "pbl/util/units.h"
#include <pbl/logging/logging.h>

#ifdef CONFIG_HRM
#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/activity_private.h"
#include "pbl/services/hrm/hrm_manager.h"
#include "pbl/services/hrm/hrm_manager_private.h"
#include <pbl/drivers/rtc.h>
#endif

#include <inttypes.h>
#include <string.h>

PBL_LOG_MODULE_DECLARE(service_activity, CONFIG_SERVICE_ACTIVITY_LOG_LEVEL);

#ifdef CONFIG_HRM

// Ruhend bleibt das Intervall stehen; neu angemeldet wird mit Pebbles "Aus"-Takt.
#define PULS_RUHEND_S ACTIVITY_HRM_SUBSCRIPTION_OFF_PERIOD_SEC

static HRMSessionRef s_abo = HRM_INVALID_SESSION_REF;
static HRMFeature s_ist_merkmale;
static uint32_t s_ist_takt_s;

// Eingaben des Minutenteils, gelesen im Callback (beides KernelBG).
static time_t s_jetzt;
static bool s_nachtruhe;
static bool s_tracking;

static struct {
  bool laeuft;
  time_t beginn;
  HrvRechnung rechnung;
  RtcTicks letzter_um;  //< wann der vorige Abstand kam
} s_hrv;

static void prv_puls_cb(void *data);
static BoulderAufgabe s_aufgabe = BOULDER_AUFGABE("Boulder Puls", prv_puls_cb);

static bool prv_platz(void) {
  return system_task_get_available_space() >= BOULDER_SCHLANGE_FREI;
}

static void prv_soll(HRMFeature *merkmale, uint32_t *takt_s) {
  *merkmale = 0;
  *takt_s = s_ist_takt_s;
  const uint8_t stufe = boulder_messstufe_geladen();
  if (!stufe || !s_tracking) {
    return;
  }
  if (s_hrv.laeuft) {
    *merkmale = HRMFeature_BPM | HRMFeature_HRV;
    *takt_s = 1;
    return;
  }
  // Nur dichter als Pebbles eigener Takt: seltener machen kann die Variante Pebbles
  // Messung nicht (5.3), und gleich oft waere ein zweites Abo fuer dieselben Werte.
  const BoulderStufe *w = boulder_messstufe_werte(stufe);
  if (s_nachtruhe && w->puls_nacht_s < w->puls_tag_min * PBL_SEC_PER_MIN) {
    *merkmale = HRMFeature_BPM;
    *takt_s = w->puls_nacht_s;
  }
}

static bool prv_hrv_im_takt(void) {
#ifndef CONFIG_HRM_HRV
  // Ohne HRV im Treiber (qemu_emery: Stub) kaemen keine Abstaende - das Fenster hielte den
  // Sensor nur fuenf Minuten umsonst an.
  return false;
#endif
  const uint8_t stufe = boulder_messstufe_geladen();
  if (!stufe || !s_tracking || !s_nachtruhe) {
    return false;
  }
  const uint8_t takt = boulder_messstufe_werte(stufe)->hrv_takt_min;
  return takt && ((s_jetzt / PBL_SEC_PER_MIN) % takt) == 0;
}

static bool prv_hrv_ende_faellig(void) {
  // Eine zurueckgestellte Uhr bricht das Fenster ab: sonst wartete es, bis die neue Zeit
  // den alten Beginn einholt, und so lange gaebe es keine Fenster (b25 N4).
  return s_hrv.laeuft && (s_jetzt < s_hrv.beginn ||
                          s_jetzt - s_hrv.beginn >= HRV_FENSTER_MIN * PBL_SEC_PER_MIN);
}

// SpO2 braucht denselben Lichtweg: ein Fenster bekaeme kaum Abstaende (b25 M8), und
// Pebble wiche ihm nicht aus - es weicht nur einem schon laufenden Dauer-Gruen.
static bool prv_spo2_misst(void) {
  const ActivityState *a = activity_private_state();
  return a->spo2.currently_sampling ||
         a->activity_spo2.phase == ActivitySpO2ActPhase_Sampling;
}

// NUR SICHERE ABSTAENDE, UND KEINER UEBER EINE LUECKE (b25 M8). Unter "Acceptable" zaehlt
// ein Abstand nicht, und auch der danach nicht gegen den davor. O(1): das laeuft im
// HRM-Rueckruf, unter der Sperre des Managers - hier wird nichts an- oder abgemeldet.
static void prv_rueckruf(PebbleHRMEvent *e, void *context) {
  if (!s_hrv.laeuft || e->event_type != HRMEvent_HRV) {
    return;
  }
  const RtcTicks jetzt = rtc_get_ticks();
  const bool luecke = s_hrv.rechnung.vorher != 0 &&
                      (jetzt - s_hrv.letzter_um) * 1000 / RTC_TICKS_HZ > HRV_LUECKE_MS;
  if (e->hrv.quality < HRMQuality_Acceptable || e->hrv.ppi_ms == 0 || luecke) {
    boulder_hrv_unterbrechen(&s_hrv.rechnung);
  }
  s_hrv.letzter_um = jetzt;
  if (e->hrv.quality >= HRMQuality_Acceptable && e->hrv.ppi_ms) {
    boulder_hrv_intervall(&s_hrv.rechnung, e->hrv.ppi_ms);
  }
}

static void prv_hrv_beenden(bool behalten) {
  s_hrv.laeuft = false;
  const uint16_t rmssd = boulder_hrv_rmssd(&s_hrv.rechnung);
  PBL_LOG_INFO("Boulder: HRV-Fenster %s, RMSSD %u aus %u Intervallen",
               behalten ? "fertig" : "abgebrochen", rmssd, s_hrv.rechnung.anzahl);
  if (behalten && rmssd) {
    boulder_dauerteil_hrv_fenster(s_hrv.beginn, rmssd, boulder_hrv_puls(&s_hrv.rechnung));
  }
}

// Setzt das Abo auf den Sollzustand; ohne Platz bleibt der Rest fuer den naechsten Takt.
static void prv_soll_setzen(void) {
  HRMFeature merkmale;
  uint32_t takt_s;
  prv_soll(&merkmale, &takt_s);
  // Erst das Intervall, dann die Merkmale: so wird nie mit altem Takt neu gemessen.
  if (takt_s != s_ist_takt_s && merkmale) {
    if (!prv_platz()) {
      return;
    }
    if (!sys_hrm_manager_set_update_interval(s_abo, takt_s, 0 /*expire_s*/)) {
      PBL_LOG_ERR("Boulder: Puls-Abo %" PRIu32 " fehlt", (uint32_t)s_abo);
      return;
    }
    s_ist_takt_s = takt_s;
  }
  if (merkmale != s_ist_merkmale) {
    if (!prv_platz()) {
      return;
    }
    if (!sys_hrm_manager_set_features(s_abo, merkmale)) {
      PBL_LOG_ERR("Boulder: Puls-Abo %" PRIu32 " fehlt", (uint32_t)s_abo);
      return;
    }
    s_ist_merkmale = merkmale;
  }
}

static bool prv_soll_erreicht(void) {
  HRMFeature merkmale;
  uint32_t takt_s;
  prv_soll(&merkmale, &takt_s);
  return merkmale == s_ist_merkmale && (!merkmale || takt_s == s_ist_takt_s);
}

static void prv_puls_cb(void *data) {
  boulder_aufgabe_laeuft(&s_aufgabe);
  if (s_abo == HRM_INVALID_SESSION_REF) {
    if (!prv_platz()) {
      return;
    }
    // Einmal anmelden, ruhend (Merkmale 0: der Manager schaltet dafuer keinen Sensor).
    s_abo = hrm_manager_subscribe_with_callback(INSTALL_ID_INVALID, PULS_RUHEND_S,
                                                0 /*expire_s*/, 0 /*features*/,
                                                false /*low_latency*/, prv_rueckruf, NULL);
    s_ist_merkmale = 0;
    s_ist_takt_s = PULS_RUHEND_S;
  }
  if (prv_hrv_ende_faellig()) {
    prv_hrv_beenden(s_jetzt >= s_hrv.beginn);
  }
  // Nicht bei "Puls aus": der Manager sperrte HRV ohnehin, das Fenster bliebe leer.
  if (!s_hrv.laeuft && prv_hrv_im_takt() && s_jetzt != s_hrv.beginn &&
      activity_prefs_heart_rate_is_enabled() && !prv_spo2_misst() && prv_platz()) {
    s_hrv.laeuft = true;
    s_hrv.beginn = s_jetzt;
    s_hrv.letzter_um = 0;
    boulder_hrv_neu(&s_hrv.rechnung);
  }
  prv_soll_setzen();
}

void boulder_puls_minute(time_t jetzt_utc, bool nachtruhe) {
  s_jetzt = jetzt_utc;
  s_nachtruhe = nachtruhe;
  s_tracking = true;
  if (s_abo == HRM_INVALID_SESSION_REF || prv_hrv_ende_faellig() ||
      (!s_hrv.laeuft && prv_hrv_im_takt()) || !prv_soll_erreicht()) {
    boulder_aufgabe_einreihen(&s_aufgabe);
  }
}

void boulder_puls_stufe_neu(void) {
  boulder_aufgabe_einreihen(&s_aufgabe);
}

void boulder_puls_tracking_aus(void) {
  s_tracking = false;
  if (s_hrv.laeuft) {
    prv_hrv_beenden(false);
  }
  if (s_abo == HRM_INVALID_SESSION_REF) {
    return;
  }
  // Ohne Minuten kaeme kein Takt mehr, der das Abo ruhen liesse: direkt, sonst einmal
  // verwerfbar nachholen.
  prv_soll_setzen();
  if (!prv_soll_erreicht() && !boulder_aufgabe_einreihen(&s_aufgabe)) {
    PBL_LOG_WRN("Boulder: Puls-Abo bleibt bis zum naechsten Takt aktiv");
  }
}

bool boulder_puls_hrv_laeuft(void) {
  return s_hrv.laeuft;
}

uint32_t boulder_puls_abo(void) {
  return s_abo;
}

#ifdef UNITTEST
void boulder_puls_test_zuruecksetzen(void) {
  s_abo = HRM_INVALID_SESSION_REF;
  s_ist_merkmale = 0;
  s_ist_takt_s = 0;
  s_jetzt = 0;
  s_nachtruhe = false;
  s_tracking = false;
  memset(&s_hrv, 0, sizeof(s_hrv));
  s_aufgabe.steht_an = false;
}
#endif

#else  // !CONFIG_HRM: ohne Sensor gibt es nichts zu messen.

void boulder_puls_minute(time_t jetzt_utc, bool nachtruhe) {
  (void)jetzt_utc;
  (void)nachtruhe;
}

void boulder_puls_stufe_neu(void) {
}

void boulder_puls_tracking_aus(void) {
}

bool boulder_puls_hrv_laeuft(void) {
  return false;
}

uint32_t boulder_puls_abo(void) {
  return 0;
}

#endif
