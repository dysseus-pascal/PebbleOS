/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// DIE HAKEN, MIT DENEN BOULDER IN PEBBLES AKTIVITAETSDIENST GREIFT - je Schalter
// (boulder/Kconfig). Pebbles Dateien rufen nur diese Funktionen; ist ein Schalter
// aus, sind sie leer oder sagen "nicht zustaendig", und es gilt Pebbles Code
// unveraendert. So bleibt der Unterschied zu Upstream an jeder Stelle eine Zeile.
//
//  BOULDER_SCHLAF      Nacht und Schlafanalyse statt Kraepelins Schlafsitzungen
//  BOULDER_HRV         HRV-Fenster in der Nacht
//  BOULDER_MESSSTUFE   ein Regler fuer Puls-, SpO2- und HRV-Takt statt Pebbles Auswahl
//  BOULDER_HEALTH_APP  Boulders Health-App (emery), mit SpO2-Ring
//
// Nicht hier, weil nicht schaltbar: die Bewegung (Typ 8) und der Gehfilter. Sie
// sind mit Upstream geteilt und nur bewacht (boulder/vertraege.c).
//
// Auch Boulders Dienste untereinander gehen ueber diese Datei: die Nacht fragt
// nach HRV-Fenstern und der Messstufe, die HRV nach ihrem Takt. Ist der
// Schalter des anderen aus, kommt hier die Antwort ohne ihn.

#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/boulder/schlafanalyse.h"
#include "pbl/util/math.h"

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#if defined(CONFIG_BOULDER_SCHLAF)
#include "pbl/services/activity/boulder/nacht.h"
#endif
#if defined(CONFIG_BOULDER_HRV)
#include "pbl/services/activity/boulder/hrv.h"
#endif
#if defined(CONFIG_BOULDER_MESSSTUFE)
#include "pbl/services/activity/boulder/messstufe.h"
#endif
#if defined(CONFIG_BOULDER_HEALTH_APP)
#include "pbl/services/activity/boulder/spo2.h"
#endif
#if defined(CONFIG_BOULDER_MESSSTUFE) || defined(CONFIG_BOULDER_HEALTH_APP)
#include <pbl/drivers/rtc.h>
#endif

// --- Aus Pebbles Aktivitaetsdienst ---------------------------------------------------

// Jede Minute, ausserhalb der Sperre des Dienstes: HRV-Fenster, dann die Nacht.
// `spo2_misst`: der Sensor misst gerade SpO2 - dann beginnt kein HRV-Fenster.
static inline void boulder_minute(time_t utc, uint16_t letzte_bewegung, bool spo2_misst) {
#if defined(CONFIG_BOULDER_HRV)
  boulder_hrv_minute(utc, letzte_bewegung, spo2_misst);
#endif
#if defined(CONFIG_BOULDER_SCHLAF)
  boulder_nacht_minute(utc);
#endif
}

// Traegt die Nacht den Schlaf ein, fallen Kraepelins Schlafsitzungen weg - sonst
// stuende jede Nacht doppelt da. Seine Schlaferkennung selbst laeuft weiter.
static inline bool boulder_schlaf_ersetzt_kraepelin(ActivitySessionType art) {
#if defined(CONFIG_BOULDER_SCHLAF)
  return art == ActivitySessionType_Sleep || art == ActivitySessionType_RestfulSleep;
#else
  return false;
#endif
}

// Die Minuten einer Schlafsitzung, die zur Schlafsumme der Uhr zaehlen. Boulders
// Nachtblock traegt das Wachliegen darin mit (nicht_schlaf_min); es zaehlt nicht.
static inline uint16_t boulder_schlaf_minuten(const ActivitySession *sitzung) {
#if defined(CONFIG_BOULDER_SCHLAF)
  return sitzung->length_min - MIN(sitzung->nicht_schlaf_min, sitzung->length_min);
#else
  return sitzung->length_min;
#endif
}

// Den Pulstakt stellt die Messstufe. false: Pebbles Auswahl (10/30/60 min) gilt.
static inline bool boulder_puls_takt(uint16_t letzte_bewegung, uint32_t *takt_s) {
#if defined(CONFIG_BOULDER_MESSSTUFE)
  *takt_s = boulder_puls_takt_s(rtc_get_time(), letzte_bewegung);
  return true;
#else
  return false;
#endif
}

// Den SpO2-Takt ebenso. false: Pebbles Auswahl gilt.
static inline bool boulder_spo2_takt(uint32_t *takt_s) {
#if defined(CONFIG_BOULDER_MESSSTUFE)
  *takt_s = boulder_spo2_takt_s();
  return true;
#else
  return false;
#endif
}

// Misst die Uhr nach der Messstufe gar kein SpO2 (Stufe 1)?
static inline bool boulder_spo2_aus(void) {
#if defined(CONFIG_BOULDER_MESSSTUFE)
  return boulder_spo2_takt_s() == 0;
#else
  return false;
#endif
}

// Eine SpO2-Messung fuer den Ring der Health-App (0 = keine).
static inline void boulder_spo2_gemessen(uint8_t prozent) {
#if defined(CONFIG_BOULDER_HEALTH_APP)
  if (prozent) {
    boulder_spo2_merken(rtc_get_time(), prozent);
  }
#endif
}

// --- Zwischen Boulders Diensten -------------------------------------------------------

#if !defined(CONFIG_BOULDER_HRV)
// Ohne HRV keine Fenster: die Nacht wird ohne gerechnet und schickt keinen HRV-Satz.
static inline int boulder_hrv_fenster(time_t von, time_t bis, SchlafHrv *aus, int max) {
  return 0;
}
#endif

#if !defined(CONFIG_BOULDER_MESSSTUFE)
// Ohne Messstufe misst die Uhr nach Pebbles Auswahl; im Satz der Nacht steht 0
// ("unbekannt"), Boulder zeigt dann keine Stufe.
static inline uint8_t boulder_messstufe(void) {
  return 0;
}
// HRV-Fenster ohne Messstufe: alle 30 Minuten, wie bei der Vorgabe (Stufe 3).
#define BOULDER_HRV_TAKT_OHNE_MESSSTUFE_MIN 30
static inline uint16_t boulder_hrv_takt_min(void) {
  return BOULDER_HRV_TAKT_OHNE_MESSSTUFE_MIN;
}
#endif
