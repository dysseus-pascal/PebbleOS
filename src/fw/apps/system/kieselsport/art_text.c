/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "art_text.h"
#include "texte.h"

// Dieselbe Reihenfolge wie Sportart - die Zahl, die ans Telefon geht.
static const StringId s_namen[ArtAnzahl] = {
  STR_ART_LAUFEN, STR_ART_BIKE, STR_ART_WANDERN, STR_ART_KRAFT,
  STR_ART_MTB, STR_ART_YOGA, STR_ART_SCHWIMMEN,
};

const char *art_name(Sportart art) {
  if (art >= ArtAnzahl) {
    return "?";
  }
  return S(s_namen[art]);
}

const char *art_gruppe(Sportart art) {
  // Strasse/Gravel und MTB sind beide Velo - das sagt der Obertitel.
  if (art == ArtBike || art == ArtMTB) {
    return S(STR_GRUPPE_BIKE);
  }
  return NULL;
}
