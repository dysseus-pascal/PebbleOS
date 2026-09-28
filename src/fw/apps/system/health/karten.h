/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "daten.h"

// Die vier Karten der Health-App: Heute, Schlaf, Herz & SpO2, Woche.
typedef enum {
  KarteHeute = 0,
  KarteSchlaf,
  KarteHerz,
  KarteWoche,
  KartenAnzahl,
} Karte;

// Die Sprache der Uhr einmal lesen - Deutsch oder, sonst, Englisch.
void karten_sprache(void);

void karten_zeichnen(GContext *ctx, GRect b, Karte karte, const HealthDaten *d);
