/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Die Rechnung des Zifferblatts Tagesband: wo im Band was liegt, und die Texte.
//
// DAS BAND zeigt [jetzt - 18 h, jetzt + 6 h] wie Boulders Tagesband beim Oeffnen (drei Viertel
// Vergangenheit). REIN: nur Argumente hinein, Ergebnis heraus, kein Syscall und keine Grafik; die
// Daten kommen aus der Datenschicht der Health-App (daten.h), damit Zifferblatt und App dieselben
// Werte zeigen.

#include "applib/graphics/gtypes.h"
#include "apps/system/boulder_health/daten.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#define BT_VERGANGEN_S (18 * 60 * 60)
#define BT_SPANNE_S    (24 * 60 * 60)

typedef struct {
  time_t von; //!< linker Rand
  time_t jetzt;
  int16_t x0;
  int16_t breite;
} BtBand;

BtBand bt_band(time_t jetzt, int16_t x0, int16_t breite);

// x eines Zeitpunkts, auch ausserhalb des Bands.
int16_t bt_x(const BtBand *b, time_t t);

// Der sichtbare Teil von [von, bis) in der Vergangenheit des Bands als [*links, *rechts). false,
// wenn nichts davon sichtbar ist.
bool bt_strecke(const BtBand *b, time_t von, time_t bis, int16_t *links, int16_t *rechts);

// Farbe einer Phase der Nacht (NachtDlsAbschnitt.phase). Wie die Health-App, nur tief heller:
// Deren OxfordBlue verschwindet auf dem dunklen Band.
GColor bt_phasenfarbe(uint8_t phase);

// Farbe einer Sitzung von Pebble im Band; GColorClear fuer die, die das Band nicht zeigt (die
// Nacht kommt aus S1, nicht aus Pebbles Schlafsitzungen).
GColor bt_sitzungsfarbe(const ActivitySession *s);

// Der Puls je Halbstunde (S6) in der Vergangenheit des Bands: x in der Mitte der Halbstunde, die
// laufende beim Jetzt. `neu`: Die Linie setzt hier neu an (davor fehlt eine Halbstunde).
typedef struct {
  int16_t x;
  uint8_t bpm;
  bool neu;
} BtPulspunkt;

// Rueckgabe: wie viele Punkte; *unten und *oben: Skala mit 5 bpm Rand wie bh_pulsband.
uint8_t bt_pulspunkte(const BtBand *b, const BhDaten *d, BtPulspunkt aus[HERZ_HALBSTUNDEN],
                      uint8_t *unten, uint8_t *oben);

// Die Stunden 0, 6, 12 und 18 Ortszeit im Band.
#define BT_MARKEN_MAX 5
typedef struct {
  int16_t x;
  uint8_t stunde;
} BtMarke;

uint8_t bt_marken(const BtBand *b, BtMarke aus[BT_MARKEN_MAX]);

// "0", "6", "12", "18"; mit 12 h "12a", "6a", "12p", "6p".
void bt_marke_text(uint8_t stunde, bool h24, char *aus, size_t platz);

// "TUE 27 OCT" zum Tag `heute` (Tage seit 1.1.2000).
void bt_datum(uint16_t heute, char *aus, size_t platz);

// Die Uhrzeit zur Minute des Tages: "14:05", mit 12 h "2:05" und *ampm "PM" (sonst NULL).
void bt_zeit(uint16_t minute, bool h24, char *aus, size_t platz, const char **ampm);

// Ruhepuls und HRV des heutigen Wachtags wie Night im Schlafmodus und Today (bh_daten_nachttage):
// eine alte Nacht zeigt keinen Wert. 0 = keiner.
void bt_heute(const BhDaten *d, uint8_t *ruhepuls, uint16_t *hrv);
