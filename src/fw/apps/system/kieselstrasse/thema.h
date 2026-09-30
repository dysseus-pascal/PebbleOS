/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "pebble_compat.h"

// Timeline-Look wie die Schwesterapps: weisser Grund, schwarze Schrift.
// Auf einem Schirm, der spiegelt statt zu leuchten, ist Schwarz auf Weiss bei
// Sonne das Einzige, was traegt - und am Steuer schaut man bei Sonne.
//
// KST_ UND NICHT KS_: Kieselsport liegt im selben Baum und hat eigene KS_RAND
// und KS_BREIT mit anderen Werten. Zwei gleichnamige Makros mit verschiedener
// Bedeutung sind eine Falle, sobald je ein Kopf beide sieht.
#define KST_FARBE_GRUND      GColorWhite
#define KST_FARBE_TEXT       GColorBlack
#define KST_FARBE_ZART       PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack)
#define KST_FARBE_BALKEN     PBL_IF_COLOR_ELSE(GColorCobaltBlue, GColorBlack)
#define KST_FARBE_BALKEN_BG  PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite)

// Der Pfeil in OsmAnds Gruen, aber EINE STUFE DUNKLER: das helle Gruen des
// Telefons (00FF00) steht dort auf dunklem Grund; auf Weiss verliert es fast
// allen Kontrast. Islamgruen (00AA00) mit dunkelgruenem Rand bleibt als Gruen
// erkennbar und hebt sich trotzdem ab. Auf Schwarzweiss: schwarz.
#define KST_FARBE_PFEIL      PBL_IF_COLOR_ELSE(GColorIslamicGreen, GColorBlack)
#define KST_FARBE_PFEIL_RAND PBL_IF_COLOR_ELSE(GColorDarkGreen, GColorBlack)
// Der Teil des Kreisverkehrs, auf dem man nicht faehrt: da, aber leise.
#define KST_FARBE_RING       PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite)

// Breit heisst: emery (200) und gabbro (260). Flint ist mit 144 schmal, und
// dort muss jede Schrift eine Stufe kleiner sein.
#define KST_BREIT (PBL_DISPLAY_WIDTH >= 180)

// Der Rand. Auf der runden Uhr weiter, sonst schneidet der Kreis unten links
// den Text an.
#define KST_RAND PBL_IF_ROUND_ELSE(26, 8)
