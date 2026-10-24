/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "art.h"

// Die Reihenfolge ist die Zahl, die ans Telefon geht. Das Menue ordnet die Arten selbst
// (apps/system/kieselsport/art_text.c).
static const ArtInfo s_arten[ArtAnzahl] = {
  // Schritte  Distanz  Reps   Bahnen  HRV
  { true,     true,    false, false,  false },   // Laufen
  // STRASSE UND GRAVEL SIND DASSELBE: am Lenker sind Schritte eine
  // Zufallszahl, und ohne GPS gibt es keine Strecke. Bleiben Zeit, Puls und
  // Kalorien - wenig, aber wahr.
  { false,    false,   false, false,  false },   // Strasse/Gravel
  { true,     true,    false, false,  false },   // Wandern
  // KRAFT: weder Schritte noch Strecke, dafuer Wiederholungen und die Pause
  // zwischen den Saetzen - und die Pause ist die Zahl, auf die man schaut.
  { false,    false,   true,  false,  false },   // Kraft
  { false,    false,   false, false,  false },   // MTB
  // YOGA: der Puls und die HRV sind die ganze Aussage.
  { false,    false,   false, false,  true  },   // Yoga
  // SCHWIMMEN: die Strecke entsteht aus gezaehlten Bahnen mal Beckenlaenge.
  { false,    false,   false, true,   false },   // Schwimmen
  // ATMEN: wie Yoga. Die Phasen und Runden zaehlt die App; hier zaehlt nur die Zeit.
  { false,    false,   false, false,  true  },   // Atmen
  // INDOOR wie Strasse/Gravel: auch auf der Rolle sind Schritte eine Zufallszahl.
  { false,    false,   false, false,  false },   // Indoor
  // FREIWASSER: keine Wende, also keine Bahnen, und die Uhr kennt die Strecke nicht. Ob das
  // Telefon sie aufzeichnet, sagt allein die Zahl der Art; die Uhr misst beide gleich.
  { false,    false,   false, false,  false },   // Freiwasser
  { false,    false,   false, false,  false },   // Freiwasser GPS
  // OFFEN wie Pebbles offenes Training: die Schritte zaehlen wie ohne Training, denn welche
  // Bewegung es ist, weiss niemand. Eine Strecke gibt es nur vom Telefon.
  { true,     false,   false, false,  false },   // Offen
  { true,     false,   false, false,  false },   // Offen GPS
};

const ArtInfo *art_info(Sportart art) {
  // Kein Test auf < 0: der Aufzaehlungstyp ist vorzeichenlos.
  if (art >= ArtAnzahl) {
    return &s_arten[0];
  }
  return &s_arten[art];
}
