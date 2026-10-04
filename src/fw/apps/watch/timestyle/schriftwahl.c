/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "schriftwahl.h"

// Dieselben Nummern wie settings.h (FONT_SETTING_*); hier ohne settings.h,
// das <pebble.h> braucht. test_timestyle nagelt beide Seiten fest.
#define SCHRIFT_STANDARD 0
#define SCHRIFT_LECO 1
#define SCHRIFT_FETT 2
#define SCHRIFT_FETTE_STUNDE 3
#define SCHRIFT_FETTE_MINUTE 4

void ts_schriftwahl(uint8_t font_id, TsSchrift *stunden, TsSchrift *minuten) {
  switch (font_id) {
    case SCHRIFT_STANDARD:
      *stunden = TS_SCHRIFT_SANS;
      *minuten = TS_SCHRIFT_SANS;
      return;
    case SCHRIFT_FETT:
      *stunden = TS_SCHRIFT_SANS_FETT;
      *minuten = TS_SCHRIFT_SANS_FETT;
      return;
    case SCHRIFT_FETTE_STUNDE:
      *stunden = TS_SCHRIFT_SANS_FETT;
      *minuten = TS_SCHRIFT_SANS;
      return;
    case SCHRIFT_FETTE_MINUTE:
      *stunden = TS_SCHRIFT_SANS;
      *minuten = TS_SCHRIFT_SANS_FETT;
      return;
    case SCHRIFT_LECO:
    default:
      *stunden = TS_SCHRIFT_LECO;
      *minuten = TS_SCHRIFT_LECO;
      return;
  }
}
