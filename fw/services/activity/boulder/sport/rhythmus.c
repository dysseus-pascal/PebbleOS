/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "rhythmus.h"

#include "schluessel.h"

// Ein Wert mehr griffe nach 18: erst in schluessel.h nachsehen, 20 und 21 sind fuer immer weg.
_Static_assert(PERSIST_ATEM_BASIS + AtemWerte - 1 == 17, "Persist 13-17");

#define PHASE_MAX_S 60
#define RUNDEN_MAX  99
#define VORGABE     {.wert = {4, 4, 4, 4, 10}}

static Atemrhythmus s_rhythmus = VORGABE;

static bool prv_gueltig(AtemWert welcher, int32_t wert) {
  switch (welcher) {
    case AtemEin:
    case AtemAus:
      // Ohne Ein- und Ausatmen ist es kein Atemzug.
      return wert >= 1 && wert <= PHASE_MAX_S;
    case AtemHalten:
    case AtemLeer:
      return wert >= 0 && wert <= PHASE_MAX_S;
    case AtemRunden:
      return wert >= 1 && wert <= RUNDEN_MAX;
    default:
      return false;
  }
}

void rhythmus_laden(void) {
  for (int i = 0; i < AtemWerte; i++) {
    const uint32_t key = PERSIST_ATEM_BASIS + i;
    if (!ks_persist_exists(key)) {
      continue;
    }
    const int32_t wert = ks_persist_read_int(key);
    if (prv_gueltig((AtemWert)i, wert)) {
      s_rhythmus.wert[i] = (uint8_t)wert;
    }
  }
}

Atemrhythmus rhythmus(void) {
  return s_rhythmus;
}

void rhythmus_setze(AtemWert welcher, int32_t wert) {
  // Boulder schickt beim Speichern alle Einstellungen, auch die gleich gebliebenen (reps.c).
  if (!prv_gueltig(welcher, wert) || s_rhythmus.wert[welcher] == wert) {
    return;
  }
  s_rhythmus.wert[welcher] = (uint8_t)wert;
  ks_persist_write_int(PERSIST_ATEM_BASIS + welcher, wert);
}

#if UNITTEST
void rhythmus_test_neustart(void) {
  s_rhythmus = (Atemrhythmus)VORGABE;
}
#endif
