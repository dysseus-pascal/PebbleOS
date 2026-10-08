/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Boulder V2, Schritt 3: die Schritte der sechs Vortage fuer die Health-App (Spezifikation
// Schritt 3, 3.8.3, Entscheid F9 = A; S7 in schnittstelle.h).
//
// WARUM EINE EIGENE WOCHE: Pebbles Tageshistorie der Tage 1-6 schreibt das Telefon ueber die
// BlobDB Health neu, und fehlen Boulder Daten, mit 0 (health_db.c, prv_notify_health_listeners).
// Die Uhr zeigt nur, was sie selbst gesammelt hat (Grundsatz). Darum merkt sich die Variante
// Pebbles Schritte von heute, so wie die Uhr sie am Tagesende hatte.
//
// SPIEGEL (je Minute, boulder_woche_minute): Pebbles Schritte von heute aus dem RAM, mit dem
// Ortsdatum der eben fertigen Minute. Das ist der Tag, zu dem Pebbles Wert gehoert: Pebble setzt
// ihn erst NACH dem Minutenteil der ersten Minute eines Tags auf 0 (activity.c,
// prv_process_minute_data_tail), und der Minutenteil um 0:00 meldet die Minute 23:59 - so landet
// der volle Wert des Tags, mit der letzten Minute, im Spiegel.
//
// TAGESWECHSEL: Die erste Minute eines neuen Ortsdatums schiebt den Spiegel als juengsten Vortag
// in die Woche. Fehlen Tage (Uhr aus), bleiben sie ohne Wert (WOCHE_KEIN_WERT). Geht die Uhr
// zurueck auf einen frueheren Tag, beginnt der Spiegel dort neu, die Woche bleibt.
//
// ABLAGE als "woche" in boulder_v2 (Woche und Spiegel, 36 B): beim Tageswechsel mit der naechsten
// Ablage, sonst mit der Ablage (alle 15 min), wenn sich etwas geaendert hat und der letzte Satz
// mindestens 60 min alt ist. So sind es hoechstens 25 Saetze am Tag statt 96. Folge: Ist die Uhr
// ueber Mitternacht aus, fehlen dem Vortag hoechstens die Schritte der letzten Stunde.
//
// WECHSEL b26 -> V2: Die Woche beginnt leer; Pebbles Historie kann Telefonwerte enthalten.

#include "pbl/services/activity/boulder/schnittstelle.h"
#include "pbl/services/settings/settings_file.h"

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

// Pebbles Schritte von heute, wie die Variante sie zuletzt sah.
typedef struct {
  uint16_t tag;      //!< Tage seit 1.1.2000 (Ortszeit), 0 = noch keiner
  uint16_t reserve;
  uint32_t schritte;
} BoulderSchrittSpiegel;

_Static_assert(sizeof(BoulderSchrittSpiegel) == 8, "BoulderSchrittSpiegel: 8 B (3.5)");

// So oft hoechstens schreibt die Ablage "woche" ohne Tageswechsel (3.8.3).
#define WOCHE_ABLAGE_ABSTAND_S (60 * 60)

// Minutenteil (KernelBG): die eben fertige Minute ab `minute_utc`. Liest Pebbles Schrittzahl von
// heute (Activity-Sperre, ausserhalb jeder Sperre der Variante); O(1), ohne Heap und ohne Datei.
void boulder_woche_minute(time_t minute_utc);

// S7: die Woche in den Puffer des Aufrufers, unter der Sperre nur die Kopie. false, solange
// "Laden" sie seit dem Boot nicht gelesen hat (dann leer).
bool boulder_woche_kopieren(BoulderWoche *aus);

// "Laden": Woche und Spiegel aus der Datei (NULL: keine Datei, die Woche beginnt leer).
void boulder_woche_laden(SettingsFile *datei);

// Ablage (alle 15 min und nach einem Tageswechsel): "woche" schreiben, wenn faellig (oben).
void boulder_woche_ablegen(SettingsFile *datei, time_t jetzt);

// Pruefbefehl und Tests: der Stand im Kern, ohne Kopie. Nur auf KernelBG.
const BoulderWoche *boulder_woche_kern(void);
const BoulderSchrittSpiegel *boulder_woche_spiegel(void);

// Tests: wie nach einem Boot (nicht gelesen).
void boulder_woche_test_neustart(void);
