/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2, Schritt 2 (AP7): Kieselsport im QEMU-Pruefbau. Nur mit CONFIG_BOULDER_PRUEFLAUF und
// CONFIG_BOULDER_KIESELSPORT gebaut (quellen.cmake), also nur in Testbauten von qemu_emery.
//
// WOZU: Kriterium 7 verlangt ein Training von zwei Stunden in qemu_emery - Heap vorher und nachher,
// Zeit je Callback, das Ende des 1-Hz-Abos - und Folgen von Start, Pause und Stopp. Die App
// liesse sich dafuer nur mit Tasten bedienen; die Shell ruft dieselben Befehle des Dienstes wie
// die App (kieselsport.h).
//
// DER PULS KOMMT WIE VOM SENSOR: qemu_emery hat nur den HRM-Stub, der nie einen Wert liefert. Der
// Geber hier spielt den Treiber: Jede Sekunde, und nur solange der HRM-Manager den Sensor
// eingeschaltet hat, gibt er einen Wert an hrm_manager_new_data_cb - wie gh3x2x.c. Von dort laeuft
// alles wie auf der Uhr: Kieselsports Abo, Activitys Abo und ueber deren Rundruf Pebbles
// Workout-Dienst mit den Pulskalorien (P1). Schaltet der Manager den Sensor ab (kein Abo mehr),
// kommt nichts mehr - so zeigt der Geber auch, wann das 1-Hz-Abo endet (T7.4).
//
//   kieselsport vorbereiten      Ballast (Heap wie obelix, T7.5) und Aktivitaetsaufzeichnung an
//   kieselsport start <art>      wie die App: 0 Laufen, 1 Velo, 2 Wandern, 3 Kraft, 4 MTB, 5 Yoga,
//                                6 Schwimmen
//   kieselsport pause|weiter|speichern|verwerfen
//   kieselsport puls <bpm>|profil|aus
//   kieselsport stand            Kieselsport, Pebbles Training, Zusammenfassung, Stopp, Geber
//   kieselsport abo              die Abos im HRM-Manager, Sensor an oder aus
//   kieselsport sitzungen        Pebbles Sitzungsliste ohne Naechte

#include "pbl/services/activity/boulder/prueflauf.h"

#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/boulder/kalorien.h"
#include "pbl/services/activity/boulder/sport/kieselsport.h"
#include "pbl/services/activity/workout_service.h"
#include "pbl/services/hrm/hrm_manager.h"
#include "pbl/services/new_timer/new_timer.h"
#include <pbl/drivers/hrm.h>
#include <pbl/logging/logging.h>
#include <pbl/shell/shell.h>

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

PBL_LOG_MODULE_DECLARE(service_activity, CONFIG_SERVICE_ACTIVITY_LOG_LEVEL);

// ---------------------------------------------------------------------------------------------
// Der Geber (NewTimers, jede Sekunde)

static TimerID s_geber = TIMER_INVALID_ID;
static bool s_profil;
static uint8_t s_fest_bpm;          //< fester Wert, wenn nicht das Profil laeuft
static uint32_t s_geber_s;          //< Sekunden seit dem Start des Gebers
static uint32_t s_geliefert;        //< Werte an den Manager
static uint32_t s_sensor_an_s;      //< Sekunden, in denen der Manager den Sensor an hatte
static uint32_t s_letzter_wert_s;   //< Gebersekunde des letzten gelieferten Werts
static volatile bool s_mit_hrv;     //< Yoga: auch Schlagabstaende (Kieselsports HRV)

// DAS PROFIL EINES LANGEN TRAININGS. Alle zehn Minuten von 100 auf 176 und zurueck: Das geht ueber
// alle fuenf Zonen hinauf und hinab, also brummt es (Hysterese) und die Pulskalorien aendern sich.
// Alle 25 Minuten 90 s nichts (eine Luecke ueber 60 s, kcal und Kurve muessen sie tragen), alle
// 40 Minuten 30 s "nicht am Arm". Rueckgabe false: in dieser Sekunde kommt kein Wert.
static bool prv_profil(uint32_t t, uint8_t *bpm, HRMQuality *guete) {
  if (t >= 1500 && t % 1500 < 90) {
    return false;
  }
  if (t >= 2400 && t % 2400 >= 200 && t % 2400 < 230) {
    *bpm = 0;
    *guete = HRMQuality_OffWrist;
    return true;
  }
  const uint32_t c = t % 600;
  uint32_t wert;
  if (c < 120) {
    wert = 100;
  } else if (c < 360) {
    wert = 100 + (c - 120) * 76 / 240;
  } else if (c < 420) {
    wert = 176;
  } else {
    wert = 176 - (c - 420) * 71 / 180;
  }
  wert = wert + (t * 7) % 5 - 2;
  *bpm = (uint8_t)wert;
  *guete = HRMQuality_Good;
  return true;
}

static void prv_geber_cb(void *daten) {
  const uint32_t t = s_geber_s++;
  // Wie der Treiber: Werte nur, solange der Manager den Sensor eingeschaltet hat.
  if (!hrm_is_enabled(HRM)) {
    return;
  }
  s_sensor_an_s++;
  uint8_t bpm = s_fest_bpm;
  HRMQuality guete = HRMQuality_Good;
  if (s_profil && !prv_profil(t, &bpm, &guete)) {
    return;
  }
  HRMData d = {.features = HRMFeature_BPM, .hrm_bpm = bpm, .hrm_quality = guete};
  if (s_mit_hrv && guete != HRMQuality_OffWrist && bpm > 0) {
    d.features |= HRMFeature_HRV;
    d.hrv_ppi_ms = (uint16_t)(60000 / bpm + (t * 13) % 41 - 20);
    d.hrv_quality = HRMQuality_Good;
  }
  hrm_manager_new_data_cb(&d);
  s_geliefert++;
  s_letzter_wert_s = t;
}

static void prv_geber(bool an) {
  if (s_geber == TIMER_INVALID_ID) {
    s_geber = new_timer_create();
  }
  if (an) {
    new_timer_start(s_geber, 1000, prv_geber_cb, NULL, TIMER_START_FLAG_REPEATING);
  } else {
    new_timer_stop(s_geber);
  }
}

// ---------------------------------------------------------------------------------------------
// Befehle

static int prv_vorbereiten(const struct pbl_shell *sh, size_t argc, char **argv) {
  const bool ballast = boulder_prueflauf_ballast();
  // Ein frisches QEMU-Abbild hat die Aufzeichnung aus. Ohne sie gibt es weder Activitys Rundruf
  // (Pebbles Workout-Dienst bekaeme keinen Puls) noch die Variante.
  if (!activity_tracking_on()) {
    activity_prefs_tracking_set_enabled(true);
    activity_start_tracking(false);
  }
  pbl_shell_print(sh, "Ballast %s, Aufzeichnung %s", ballast ? "da" : "FEHLT",
                  activity_tracking_on() ? "an" : "startet");
  return ballast ? 0 : -1;
}

static int prv_start(const struct pbl_shell *sh, size_t argc, char **argv) {
  const int art = atoi(argv[1]);
  if (art < 0 || art >= ArtAnzahl) {
    pbl_shell_print(sh, "Art %s unbekannt (0-%d)", argv[1], ArtAnzahl - 1);
    return -1;
  }
  s_mit_hrv = (art == ArtYoga);
  kieselsport_starte((Sportart)art, 0);
  pbl_shell_print(sh, "Start Art %d eingereiht", art);
  return 0;
}

// Pause und Weiter getrennt, damit ein Ablauf nicht aus Versehen umschaltet: Der Befehl wirkt
// nur aus dem passenden Zustand.
static int prv_umschalten(const struct pbl_shell *sh, Laufzustand von) {
  KsStand s;
  kieselsport_stand(&s);
  if (s.zustand != von) {
    pbl_shell_print(sh, "Zustand %d, nichts zu tun", s.zustand);
    return -1;
  }
  kieselsport_pause_umschalten();
  pbl_shell_print(sh, "%s eingereiht", von == LaufLaeuft ? "Pause" : "Weiter");
  return 0;
}

static int prv_pause(const struct pbl_shell *sh, size_t argc, char **argv) {
  return prv_umschalten(sh, LaufLaeuft);
}

static int prv_weiter(const struct pbl_shell *sh, size_t argc, char **argv) {
  return prv_umschalten(sh, LaufPause);
}

static int prv_speichern(const struct pbl_shell *sh, size_t argc, char **argv) {
  kieselsport_speichern();
  pbl_shell_print(sh, "Speichern eingereiht");
  return 0;
}

static int prv_verwerfen(const struct pbl_shell *sh, size_t argc, char **argv) {
  kieselsport_verwerfen();
  pbl_shell_print(sh, "Verwerfen eingereiht");
  return 0;
}

static int prv_puls(const struct pbl_shell *sh, size_t argc, char **argv) {
  if (!strcmp(argv[1], "aus")) {
    prv_geber(false);
    pbl_shell_print(sh, "Geber aus");
    return 0;
  }
  s_profil = !strcmp(argv[1], "profil");
  s_fest_bpm = s_profil ? 0 : (uint8_t)atoi(argv[1]);
  s_geber_s = 0;
  prv_geber(true);
  pbl_shell_print(sh, "Geber %s", s_profil ? "Profil" : argv[1]);
  return 0;
}

static int prv_stand(const struct pbl_shell *sh, size_t argc, char **argv) {
  KsStand s;
  kieselsport_stand(&s);
  pbl_shell_print(sh,
                  "KS zustand %d art %d dauer %" PRIu32 " kcal %" PRIu32 " puls %u frisch %d "
                  "zone %u mittel %u max %u ende %d nr %u maxpuls %u",
                  s.zustand, s.art, s.t.dauer_s, s.t.kcal, s.puls, s.frisch, s.zone,
                  s.t.puls_mittel, s.t.puls_max, s.ende, s.ende_nr, s.maxpuls);
  int32_t schritte = 0, dauer = 0, meter = 0, bpm = 0;
  HRZone zone = HRZone_Zone0;
  const bool pebble =
      workout_service_get_current_workout_info(&schritte, &dauer, &meter, &bpm, &zone);
  pbl_shell_print(sh,
                  "PEBBLE laeuft %d pause %d dauer %" PRId32 " schritte %" PRId32 " bpm %" PRId32
                  " cal %" PRId32,
                  pebble, pebble && workout_service_is_paused(), dauer, schritte, bpm,
                  pebble ? boulder_kalorien_bisher(dauer) : 0);
  Trainingsstand t;
  static char s_liste[KS_LISTE_MAX];
  const bool wartet = kieselsport_wartend_laden(&t, s_liste, sizeof(s_liste));
  uint32_t beginn = 0, zeit = 0;
  uint8_t art = 0;
  const bool stopp = kieselsport_stopp_laden(&beginn, &zeit, &art);
  pbl_shell_print(sh,
                  "PERSIST wartend %d beginn %" PRIu32 " dauer %" PRIu32 " kcal %" PRIu32
                  " stopp %d kurve %d/%u",
                  wartet, wartet ? t.beginn : 0, wartet ? t.dauer_s : 0, wartet ? t.kcal : 0,
                  stopp, kieselsport_kurve_wartet(), kieselsport_kurve_anzahl());
  pbl_shell_print(sh,
                  "GEBER s %" PRIu32 " sensor_an_s %" PRIu32 " geliefert %" PRIu32
                  " letzter %" PRIu32 " sensor %s",
                  s_geber_s, s_sensor_an_s, s_geliefert, s_letzter_wert_s,
                  hrm_is_enabled(HRM) ? "an" : "aus");
  return 0;
}

// Die Abos im Manager, wie T2.2 sie zaehlt: Kennungen beginnen bei 1 (hrm_manager.c:842). Ein
// abgelaufenes Abo bleibt in der Liste, bis der naechste Wert es entfernt; "rest 0" heisst das
// nur bei einem Abo mit Ablauf.
#define ABO_KENNUNGEN_MAX 4096

static int prv_abo(const struct pbl_shell *sh, size_t argc, char **argv) {
  int anzahl = 0;
  for (HRMSessionRef ref = 1; ref < ABO_KENNUNGEN_MAX; ref++) {
    AppInstallId app = INSTALL_ID_INVALID;
    uint32_t takt = 0;
    uint16_t rest = 0;
    HRMFeature merkmale = 0;
    if (!sys_hrm_manager_get_subscription_info(ref, &app, &takt, &rest, &merkmale)) {
      continue;
    }
    anzahl++;
    pbl_shell_print(sh, "ABO ref %" PRIu32 " app %" PRId32 " takt %" PRIu32 " rest %u merkmale 0x%x",
                    ref, (int32_t)app, takt, rest, merkmale);
  }
  pbl_shell_print(sh, "ABOS %d sensor %s", anzahl, hrm_is_enabled(HRM) ? "an" : "aus");
  return 0;
}

static int prv_sitzungen(const struct pbl_shell *sh, size_t argc, char **argv) {
  static ActivitySession s_liste[ACTIVITY_MAX_ACTIVITY_SESSIONS_COUNT];
  uint32_t n = ACTIVITY_MAX_ACTIVITY_SESSIONS_COUNT;
  if (!activity_get_sessions(&n, s_liste)) {
    pbl_shell_print(sh, "SITZUNGEN nicht lesbar");
    return -1;
  }
  int training = 0;
  for (uint32_t i = 0; i < n; i++) {
    const ActivitySession *a = &s_liste[i];
    if (a->type != ActivitySessionType_Run && a->type != ActivitySessionType_Walk &&
        a->type != ActivitySessionType_Open) {
      continue;
    }
    training++;
    pbl_shell_print(sh,
                    "SITZUNG typ %d start %" PRIu32 " min %u manual %d laufend %d schritte %u "
                    "kcal %u",
                    a->type, (uint32_t)a->start_utc, a->length_min, a->manual, a->ongoing,
                    (unsigned)a->step_data.steps, (unsigned)a->step_data.active_kcalories);
  }
  pbl_shell_print(sh, "SITZUNGEN %d von %" PRIu32, training, n);
  return 0;
}

static const struct pbl_shell_cmd sub_kieselsport[] = {
  PBL_SHELL_CMD(vorbereiten, NULL, "Ballast holen, Aufzeichnung an", prv_vorbereiten),
  PBL_SHELL_CMD_ARG(start, NULL, "Training starten <art>", prv_start, 2, 0),
  PBL_SHELL_CMD(pause, NULL, "Pause", prv_pause),
  PBL_SHELL_CMD(weiter, NULL, "Weiter", prv_weiter),
  PBL_SHELL_CMD(speichern, NULL, "Speichern", prv_speichern),
  PBL_SHELL_CMD(verwerfen, NULL, "Verwerfen", prv_verwerfen),
  PBL_SHELL_CMD_ARG(puls, NULL, "Puls einspeisen <bpm|profil|aus>", prv_puls, 2, 0),
  PBL_SHELL_CMD(stand, NULL, "Kieselsport und Pebbles Training", prv_stand),
  PBL_SHELL_CMD(abo, NULL, "HRM-Abos", prv_abo),
  PBL_SHELL_CMD(sitzungen, NULL, "Pebbles Sitzungen", prv_sitzungen),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(kieselsport, sub_kieselsport, "Kieselsport (Pruefbau)", NULL);
