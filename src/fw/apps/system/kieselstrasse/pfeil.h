/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "pebble_compat.h"

/**
 * Der Abbiegepfeil - gezeichnet nach OsmAnds Kennzahl, nicht nach seinem Text.
 *
 * DAS IST DER GANZE UNTERSCHIED ZUR VORGAENGERIN. Dort kam nur der Satz
 * "80 m • Turn right and go" an, und die Uhr musste die Richtung aus Woertern
 * erraten: in zwei Sprachen, mit Wortgrenzen, damit "Rechtsweg" keinen
 * Rechtspfeil ausloest - und Kreisverkehre standen in keiner Liste, weil
 * niemand alle Formulierungen kennt.
 *
 * Hier kommt eine Zahl. Sie stammt aus TurnType in OsmAnds Router und ist
 * damit dieselbe, die OsmAnd selbst zum Zeichnen benutzt:
 *
 *      1 C     geradeaus          8 KL    links halten
 *      2 TL    links              9 KR    rechts halten
 *      3 TSLL  leicht links      10 TU    wenden
 *      4 TSHL  scharf links      11 TRU   wenden (rechtsherum)
 *      5 TR    rechts            12 OFFR  abseits der Route
 *      6 TSLR  leicht rechts     13 RNDB  Kreisverkehr
 *      7 TSHR  scharf rechts     14 RNLB  Kreisverkehr (linksherum)
 *
 * 0 heisst "nichts bekannt" und zeichnet nichts. Eine geratene Richtung waere
 * schlimmer als keine.
 */

/** Den Pfad einmal bauen. Muss vor dem ersten Zeichnen laufen. */
void pfeil_init(void);
void pfeil_deinit(void);

/**
 * Den Zustand dieser Datei auf Anfang - beim Start der App.
 *
 * DER PFAD LAG IM APP-SPEICHER DES VORIGEN LAUFS. Die Variable ueberlebt das
 * Ende einer System-App, der Speicher nicht; ohne das hier zeichnete
 * pfeil_zeichne mit einem Zeiger ins Leere (wie in Kieselsport, 580ba98f8).
 */
void pfeil_zuruecksetzen(void);

/** Wie breit der Pfeil auf dieser Uhr ist - fuer die Aufteilung der Zeile. */
int16_t pfeil_breite(void);

/** Wie hoch er ist - fuer die Anzeige ohne Zahl, wo er ueber dem Text steht. */
int16_t pfeil_hoehe(void);

/**
 * Zeichnet das Zeichen fuer `art` mittig um `mitte`.
 *
 * Kreisverkehr und "abseits der Route" sind KEINE gedrehten Pfeile, sondern
 * eigene Zeichen - ein Kreisverkehr als schraeger Pfeil waere eine Luege ueber
 * die Kreuzung, die gleich kommt.
 */
void pfeil_zeichne(GContext *ctx, GPoint mitte, int art);

/** Gibt es fuer diese Kennzahl ueberhaupt ein Zeichen? */
bool pfeil_kennt(int art);
