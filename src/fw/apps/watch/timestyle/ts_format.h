/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder: die Einstellungen von TimeStyle Bedrock, wie Boulder sie schickt.
// Ein Wert von genau 32 Byte unter "tsBedrock" (timestyle.h). Derselbe Aufbau
// und dieselben Leseregeln stehen in Boulder (TsBedrockFormat.kt); beide
// Seiten testen mit denselben Testvektoren. Rein, ohne Firmware-Dienste,
// damit es auf dem Host testbar ist.
//
//  0     Fassung (1; 0 = nicht gesetzt, dann alle Vorgaben)
//  1-4   Farben GColor8: Zeit, Hintergrund, Leiste, Leistenschrift
//  5     Sprache          6  Schrift (immer LECO)   7  Stundenvibration 0-2
//  8-10  Widgets oben, Mitte, unten                 11 Zeitzone-Versatz int8
//  12    Dezimaltrenner '.' oder ','
//  13    Flags A: 0 fuehrende Null, 1 Vibration bei Trennung, 2 Leiste links,
//        3 grosse Schrift, 4 Trennungssymbol, 5 frei, 6 Akku in Prozent,
//        7 kein automatischer Akku
//  14    Flags B: 0 Distanz statt Schritte, 1 nur Tiefschlaf
//  16-23 Name der Zeitzone, UTF-8, Byte 23 immer 0
//  15, 24-31 frei (0)
#pragma once

#include "settings.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TS_FORMAT_LAENGE 32

// Die Werte vom Foto des Nutzers (03.10.2026). Die einzige Stelle dafuer.
void ts_format_vorgaben(Settings *s);

// Liest einen Wert beliebiger Laenge und liefert immer einen gueltigen Satz:
// falsche Laenge oder Fassung 0 ergibt die Vorgaben, ein ungueltiges Feld die
// Vorgabe nur dieses Feldes. Hoehere Fassungen werden wie Fassung 1 gelesen.
void ts_format_dekodieren(const uint8_t *b, size_t len, Settings *s);

// Die 29 Sprachen, deren Zeichen die eingebauten Schriften kennen.
bool ts_format_sprache_erlaubt(uint8_t id);
