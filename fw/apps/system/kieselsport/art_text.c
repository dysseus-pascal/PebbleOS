/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Nur mit der Variante: fw/apps/system/ baut jedes Board (siehe kieselsport.c).
#ifdef CONFIG_BOULDER_KIESELSPORT

#include "art_text.h"
#include "texte.h"

#include "pbl/util/size.h"

// Dieselbe Reihenfolge wie Sportart - die Zahl, die ans Telefon geht.
static const StringId s_namen[ArtAnzahl] = {
  STR_ART_LAUFEN, STR_ART_BIKE, STR_ART_WANDERN, STR_ART_KRAFT,
  STR_ART_MTB, STR_ART_YOGA, STR_ART_SCHWIMMEN, STR_ART_ATEM,
  STR_ART_INDOOR, STR_ART_FREIWASSER, STR_ART_FREIWASSER_GPS, STR_ART_OFFEN, STR_ART_OFFEN_GPS,
};

// DAS MENUE ORDNET SELBST, die Zahl einer Art darf sich nie verschieben. Raeder und Schwimmen
// stehen darum hinter je einer Zeile in einem eigenen Menue; jede Gruppe beginnt mit der Art, die
// vorher allein im Hauptmenue stand. Das offene Training gehoert zu keiner Sportart und steht
// darum am Ende, mit und ohne GPS.
static const uint8_t s_haupt[] = {
  ArtLaufen, MENUE_BIKE, ArtWandern, ArtKraft, ArtYoga, MENUE_SCHWIMMEN, ArtAtem,
  ArtOffen, ArtOffenGps,
};

typedef struct {
  StringId name;
  const Sportart *arten;
  uint8_t anzahl;
} Gruppe;

static const Sportart s_raeder[] = { ArtBike, ArtMTB, ArtIndoor };
static const Sportart s_baeder[] = { ArtSchwimmen, ArtFreiwasser, ArtFreiwasserGps };
static const Gruppe s_gruppen[] = {
  [MENUE_BIKE - ArtAnzahl] = { STR_GRUPPE_BIKE, s_raeder, ARRAY_LENGTH(s_raeder) },
  [MENUE_SCHWIMMEN - ArtAnzahl] = { STR_GRUPPE_SCHWIMMEN, s_baeder, ARRAY_LENGTH(s_baeder) },
};

bool menue_ist_gruppe(int eintrag) {
  return eintrag >= MENUE_BIKE && eintrag < MENUE_BIKE + (int)ARRAY_LENGTH(s_gruppen);
}

// Eine unbekannte Gruppe ist die erste: wie ausserhalb des Hauptmenues (menue_eintrag) oeffnet
// sie ein Untermenue, statt eine fremde Art zu starten.
static const Gruppe *prv_gruppe(int gruppe) {
  return &s_gruppen[menue_ist_gruppe(gruppe) ? gruppe - MENUE_BIKE : 0];
}

const char *art_name(Sportart art) {
  if (art >= ArtAnzahl) {
    return "?";
  }
  return S(s_namen[art]);
}

const char *art_gruppe(Sportart art) {
  for (size_t g = 0; g < ARRAY_LENGTH(s_gruppen); g++) {
    for (size_t i = 0; i < s_gruppen[g].anzahl; i++) {
      if (s_gruppen[g].arten[i] == art) {
        return S(s_gruppen[g].name);
      }
    }
  }
  return NULL;
}

uint16_t menue_zeilen(void) {
  return ARRAY_LENGTH(s_haupt);
}

int menue_eintrag(uint16_t zeile) {
  return zeile < ARRAY_LENGTH(s_haupt) ? s_haupt[zeile] : MENUE_BIKE;
}

const char *menue_gruppe_name(int gruppe) {
  return S(prv_gruppe(gruppe)->name);
}

uint16_t menue_gruppe_zeilen(int gruppe) {
  return prv_gruppe(gruppe)->anzahl;
}

Sportart menue_gruppe_art(int gruppe, uint16_t zeile) {
  const Gruppe *g = prv_gruppe(gruppe);
  return zeile < g->anzahl ? g->arten[zeile] : g->arten[0];
}

#endif  // CONFIG_BOULDER_KIESELSPORT
