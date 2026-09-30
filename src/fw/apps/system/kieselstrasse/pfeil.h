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
 *
 * SO, WIE OSMAND SIE ZEICHNET: ein Schaft, der unten ins Bild kommt, eine
 * Kurve in die Richtung und eine grosse Spitze - auf Farbschirmen gruen mit
 * dunklem Rand. Wer den Pfeil auf dem Telefon gesehen hat, erkennt ihn am
 * Handgelenk wieder, ohne ihn neu lesen zu lernen.
 */

/**
 * Den Zustand dieser Datei auf Anfang - beim Start der App.
 *
 * Eine System-App behaelt ihre statischen Variablen ueber ihr Ende hinaus
 * (Kieselsport, 580ba98f8). Der Bauplatz des Pfeils liegt nur waehrend des
 * Zeichnens auf dem App-Heap; sein Zeiger darf nie aus einem vorigen Lauf
 * stammen.
 */
void pfeil_zuruecksetzen(void);

/**
 * Zeichnet das Zeichen fuer `art` mittig in das Quadrat `kasten`.
 *
 * `ausfahrt` (1, 2, 3 ... oder 0) steht im Kreisverkehr als Zahl im Ring.
 * `winkel` ist OsmAnds turn_angle in Grad (0 geradeaus, + rechts) oder
 * WEG_KEIN_WINKEL - ohne ihn zeigt der Kreisverkehr keine Ausfahrt statt
 * einer geratenen.
 */
void pfeil_zeichne(GContext *ctx, GRect kasten, int art, int ausfahrt, int winkel);

/** Gibt es fuer diese Kennzahl ueberhaupt ein Zeichen? */
bool pfeil_kennt(int art);

/** Steht bei dieser Art die Nummer der Ausfahrt im Ring? */
bool pfeil_zeigt_nummer(int art, int ausfahrt);
