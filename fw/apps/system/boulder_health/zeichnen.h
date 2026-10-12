/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Kleine Zeichenhelfer fuer Karten und Details der Health-App.
//
// WARUM EIGENE HELFER: Die App laeuft ohne Systemrechte und darf nur rufen, was SDK-Apps rufen
// (pruefe_ohne_rechte.py). Einige SDK-Namen sind Makros auf andere Funktionen (graphics_fill_rect
// ist im SDK graphics_fill_round_rect_by_value). Die Helfer rufen die erlaubten Namen an einer
// Stelle, Karten und Details bleiben lesbar.

#include "applib/graphics/gtypes.h"
#include "applib/graphics/text.h"

#include <stdint.h>

// Farben (Spezifikation Schritt 3, 2.1; Kontrast prueft test_bh_karten)
#define BH_GRAU       GColorDarkGray   //!< Labels
#define BH_HELLGRAU   GColorLightGray  //!< Spuren, Achsen, Nachtschatten
#define BH_GUT        GColorDarkGreen  //!< "as usual", "Well rested", "in range"
#define BH_AUFFAELLIG GColorWindsorTan //!< alles andere (Bernstein)

#define BH_KOPF_H 28 //!< Kopfbalken 0-27 px
#define BH_RAND   8

typedef enum {
  BhSchriftG14,
  BhSchriftG14Fett,
  BhSchriftG18,
  BhSchriftG18Fett,
  BhSchriftG24Fett,
  BhSchriftL26, //!< LECO 26 fett mit AM/PM
  BhSchriftL38, //!< LECO 38 fett
} BhSchriftart;

GFont bh_schrift(BhSchriftart art);

// Text in einem Rahmen.
void bh_schreiben(GContext *ctx, const char *text, BhSchriftart art, GRect rahmen,
                  GTextAlignment ausrichtung, GColor farbe);

// Breite eines Texts in einer Schrift (einzeilig).
int16_t bh_textbreite(const char *text, BhSchriftart art);

// Zahl in grosser Schrift, die Einheit klein in GOTHIC dahinter auf der Grundlinie. Rueckgabe: die
// x-Koordinate rechts von der Einheit; ohne Einheit ort.x (die Zahl wird dann nicht vermessen).
int16_t bh_zahl_mit_einheit(GContext *ctx, const char *zahl, const char *einheit, BhSchriftart art,
                            GPoint ort, GColor farbe);

// Ein Urteil rechts neben einer Zahl mit Einheit (Today, Night, Heart), rechtsbuendig bis
// `rechts`: in GOTHIC 18 fett in einer Zeile ab `oben`, wenn es rechts von `frei_ab` Platz hat,
// sonst umbrochen, die zweite Zeile dort, wo die eine stuende; braucht es dann mehr als zwei
// Zeilen, in GOTHIC 14 fett. Eine lange Dauer ("10:59 h") neben "Very short night" passt nicht in
// eine Zeile.
GRect bh_urteil_rahmen(const char *text, int16_t frei_ab, int16_t rechts, int16_t oben,
                       BhSchriftart *art);
void bh_urteil(GContext *ctx, const char *text, int16_t frei_ab, int16_t rechts, int16_t oben,
               GColor farbe);

void bh_rechteck(GContext *ctx, GRect r, GColor farbe);
void bh_linie(GContext *ctx, GPoint a, GPoint b, GColor farbe);
void bh_punkt(GContext *ctx, GPoint mitte, uint16_t radius, GColor farbe);
void bh_ring(GContext *ctx, GPoint mitte, uint16_t radius, GColor farbe);

// Kopfbalken eines Details: Farbe der Karte, Titel weiss, ohne Punkte.
void bh_kopf_detail(GContext *ctx, int16_t breite, const char *titel, GColor farbe);
