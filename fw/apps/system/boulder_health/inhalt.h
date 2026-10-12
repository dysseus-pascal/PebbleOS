/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Der Inhalt der fuenf Karten (Spezifikation Schritt 3, 2.2) unter dem Kopfbalken, und die
// Bausteine, die auch die Details zeichnen (Nachtblock, Pulsband).
//
// NUR AUS BhDaten: Zeichnen ruft keinen Syscall (L3); alles steht seit dem Laden im Heap der App.

#include "daten.h"
#include "karten.h"

#include "applib/graphics/gtypes.h"

void bh_inhalt_zeichnen(GContext *ctx, GRect flaeche, BhKarte karte, const BhDaten *d);

// --- Gemeinsam mit den Details ---

// Die Nacht als ein Block mit vier Bahnen (Awake, REM, Light, Deep), vom Beginn des ersten bis zum
// Ende des letzten Abschnitts ueber die ganze Breite von `r`.
void bh_nacht_block(GContext *ctx, GRect r, const NachtBild *n);

// Die Phasenfarbe (wie b25 und Boulder): wach warm, Schlaf in Blau.
GColor bh_phasenfarbe(uint8_t phase);

// Die Pulsgrenzen fuer Zonenfarben: aus S6, sonst aus der Lage gerechnet.
void bh_grenzen(const BhDaten *d, uint16_t grenzen[BH_ZONEN - 1]);

// Pulsband aus S6 ueber die UTC-Zeit [von, bis): je Halbstunde eine Saeule Minimum bis Maximum
// in Grau, darueber die Linie der Mittel in der Zonenfarbe. schatten_von/bis (UTC, 0 = keiner)
// markiert die Nacht als Streifen 2-4 px unter `r`.
void bh_pulsband(GContext *ctx, GRect r, const BhDaten *d, time_t von, time_t bis,
                 time_t schatten_von, time_t schatten_bis);
