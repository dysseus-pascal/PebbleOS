/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2: der Schlafmodus von Hand (schlafmodus.h).

#include "pbl/services/activity/boulder/schlafmodus.h"

#include "kernel/pebble_tasks.h"
#include "pbl/services/activity/boulder/takt.h"
#include "pbl/services/activity/boulder/variante.h"
#include "pbl/util/units.h"
#include <pbl/logging/logging.h>

#include <string.h>

PBL_LOG_MODULE_DECLARE(service_activity, CONFIG_SERVICE_ACTIVITY_LOG_LEVEL);

static BoulderSchlafmodus s_zustand;

time_t boulder_schlafmodus_fenster(time_t utc, time_t regulaer) {
  const time_t start = (time_t)s_zustand.start;
  if (!start || utc < start) {
    return regulaer;
  }
  if (s_zustand.aktiv) {
    return start;
  }
  // NACH DEM ENDE BIS ZUM NAECHSTEN FENSTER AB 20 UHR: Beginnt das Fenster, in dem `utc` liegt,
  // erst mit oder nach dem Ende, gilt es. Sonst laeuft das Fenster von Hand weiter - so gehoeren
  // spaete Minuten (Luecken, HRV) noch zu ihm, und Schlaf danach ist eine Folgenacht.
  if (regulaer >= (time_t)s_zustand.ende || utc - start >= SCHLAFMODUS_FENSTER_MAX_S) {
    return regulaer;
  }
  return start;
}

bool boulder_schlafmodus_aktiv(void) {
  return s_zustand.aktiv;
}

time_t boulder_schlafmodus_seit(void) {
  return s_zustand.aktiv ? (time_t)s_zustand.start : 0;
}

bool boulder_schlafmodus_nacht_von_hand(time_t satz_beginn, time_t eingeschlafen) {
  if (!s_zustand.start || satz_beginn != (time_t)s_zustand.start) {
    return false;
  }
  return s_zustand.aktiv || eingeschlafen < (time_t)s_zustand.ende;
}

bool boulder_schlafmodus_starten(time_t minute) {
  if (s_zustand.aktiv) {
    return false;
  }
  s_zustand = (BoulderSchlafmodus){
    .fassung = SCHLAFMODUS_FASSUNG,
    .aktiv = 1,
    .start = (uint32_t)minute,
  };
  PBL_LOG_INFO("Boulder: Schlafmodus an");
  return true;
}

bool boulder_schlafmodus_beenden(time_t jetzt) {
  if (!s_zustand.aktiv) {
    return false;
  }
  s_zustand.aktiv = 0;
  s_zustand.ende = (uint32_t)jetzt;
  PBL_LOG_INFO("Boulder: Schlafmodus aus nach %d min",
               (int)((jetzt - (time_t)s_zustand.start) / PBL_SEC_PER_MIN));
  return true;
}

bool boulder_schlafmodus_minute(time_t minute_utc, bool schlaeft) {
  if (!s_zustand.aktiv) {
    return false;
  }
  s_zustand.geschlafen |= schlaeft;
  const time_t ende = minute_utc + PBL_SEC_PER_MIN;
  if (s_zustand.geschlafen ||
      ende - (time_t)s_zustand.start < SCHLAFMODUS_OHNE_SCHLAF_MIN * PBL_SEC_PER_MIN) {
    return false;
  }
  // Vergessen oder aus Versehen gestartet: ohne Schlaf haette die Nacht von Hand die naechste
  // gewoehnliche Nacht nach 15 Stunden abgeschnitten.
  return boulder_schlafmodus_beenden(ende);
}

bool boulder_schlafmodus_nacht_endgueltig(time_t fenster, time_t jetzt) {
  if (!s_zustand.aktiv || fenster != (time_t)s_zustand.start) {
    return false;
  }
  return boulder_schlafmodus_beenden(jetzt);
}

// ---------------------------------------------------------------------------------------------
// Schalten von der App (Pebbles Quick Launch, Health-App)

static void prv_schalten_cb(void *data);
static BoulderAufgabe s_aufgabe_schalten = BOULDER_AUFGABE("Boulder Schlafmodus", prv_schalten_cb);
// Der letzte Wunsch gilt: Ein zweiter vor dem Lauf ersetzt den ersten.
static volatile bool s_wunsch;

static void prv_schalten_cb(void *data) {
  BOULDER_AUFGABE_LAEUFT(&s_aufgabe_schalten);
  boulder_dauerteil_schlafmodus(s_wunsch);
}

void boulder_schlafmodus_schalten(bool an) {
  s_wunsch = an;
  if (pebble_task_get_current() != PebbleTask_App) {
    boulder_aufgabe_einreihen(&s_aufgabe_schalten);
    return;
  }
  // AUS DER APP NICHT VERWERFBAR, wie die Auftraege der Health-App (schnittstelle.c): Ein
  // verworfener Tastendruck hiesse, die Nacht faengt nicht an. Erst der Merker, dann das Einreihen.
  if (s_aufgabe_schalten.steht_an) {
    return;
  }
  s_aufgabe_schalten.steht_an = true;
  if (!system_task_add_callback(s_aufgabe_schalten.lauf, NULL)) {
    s_aufgabe_schalten.steht_an = false;
    PBL_LOG_WRN("Boulder: Schlafmodus nicht eingereiht");
  }
}

// ---------------------------------------------------------------------------------------------

void boulder_schlafmodus_ablage(BoulderSchlafmodus *aus) {
  *aus = s_zustand;
}

void boulder_schlafmodus_laden(const BoulderSchlafmodus *ein) {
  if (ein->fassung != SCHLAFMODUS_FASSUNG) {
    return;
  }
  s_zustand = *ein;
}

void boulder_schlafmodus_test_neustart(void) {
  memset(&s_zustand, 0, sizeof(s_zustand));
  s_wunsch = false;
  s_aufgabe_schalten.steht_an = false;
}
