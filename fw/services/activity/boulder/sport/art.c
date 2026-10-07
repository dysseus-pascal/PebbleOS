/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "art.h"

// Die Reihenfolge ist die des Menues UND die Zahl, die ans Telefon geht.
static const ArtInfo s_arten[ArtAnzahl] = {
  // Schritte  Distanz  Reps   Bahnen
  { true,     true,    false, false },   // Laufen
  // STRASSE UND GRAVEL SIND DASSELBE: am Lenker sind Schritte eine
  // Zufallszahl, und ohne GPS gibt es keine Strecke. Bleiben Zeit, Puls und
  // Kalorien - wenig, aber wahr.
  { false,    false,   false, false },   // Strasse/Gravel
  { true,     true,    false, false },   // Wandern
  // KRAFT: weder Schritte noch Strecke, dafuer Wiederholungen und die Pause
  // zwischen den Saetzen - und die Pause ist die Zahl, auf die man schaut.
  { false,    false,   true,  false },   // Kraft
  { false,    false,   false, false },   // MTB
  // YOGA: der Puls und die HRV sind die ganze Aussage.
  { false,    false,   false, false },   // Yoga
  // SCHWIMMEN: die Strecke entsteht aus gezaehlten Bahnen mal Beckenlaenge.
  { false,    false,   false, true  },   // Schwimmen
};

const ArtInfo *art_info(Sportart art) {
  // Kein Test auf < 0: der Aufzaehlungstyp ist vorzeichenlos.
  if (art >= ArtAnzahl) {
    return &s_arten[0];
  }
  return &s_arten[art];
}
