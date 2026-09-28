/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "pebble_compat.h"
#include "art_text.h"

// Die Symbole der Sportarten - im Menue klein, auf dem Startschirm gross.
//
// WIE IN DER WORKOUT-APP DER PEBBLE steht vor jeder Art ein Bild, und vor
// dem Start steht es gross da. Dort sind es Bitmaps; hier sind es Striche und
// Kreise, gerechnet aus einem Raster von 28 Punkten. So passt dasselbe
// Symbol in eine Menuezeile auf flint und auf den halben Schirm von emery,
// ohne je Groesse ein Bild mitzuschleppen.
//
// `mitte` ist der Mittelpunkt, `groesse` die Kantenlaenge in Punkten.
void symbol_sport(GContext *ctx, Sportart art, GPoint mitte, int16_t groesse, GColor farbe);

// Ein Herz - fuer den Menuepunkt "HRV", in derselben Groesse wie die Arten.
void symbol_herz(GContext *ctx, GPoint mitte, int16_t groesse, GColor farbe);
