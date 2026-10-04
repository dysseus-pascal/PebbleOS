/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "daten.h"

// Die drei Karten der Health-App: Heute, Schlaf, Herz & SpO2.
typedef enum {
  KarteHeute = 0,
  KarteSchlaf,
  KarteHerz,
  KartenAnzahl,
} Karte;

// Die Sprache der Uhr einmal lesen - Deutsch oder, sonst, Englisch.
void karten_sprache(void);

// herz: 0 = keines (der Puls ist nicht live), 1 = klein, 2 = gross im Schlag.
void karten_zeichnen(GContext *ctx, GRect b, Karte karte, const HealthDaten *d, int herz);
