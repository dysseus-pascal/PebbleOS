/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder: welche Ziffernschrift Stunden und Minuten bekommen - die Zuordnung
// aus TimeStyles update_fonts (clock_area.c), rein rechnend herausgeloest,
// damit ein Host-Test (test_timestyle) sie prueft. clock_area.c macht daraus
// die geladenen Schriften.
#pragma once

#include <stdint.h>

typedef enum {
  TS_SCHRIFT_SANS,      // Kumbh Sans Regular (Ersatz fuer Avenir Next Regular)
  TS_SCHRIFT_SANS_FETT, // Kumbh Sans SemiBold (Ersatz fuer Avenir Next Demi Bold)
  TS_SCHRIFT_LECO,      // LECO 1976
} TsSchrift;

// [font_id] = Byte 6 der Einstellungen (FONT_SETTING_*: 0 Standard, 1 LECO,
// 2 fett, 3 fette Stunde, 4 fette Minute). Eine unbekannte Nummer gibt LECO -
// im Original blieb die Schrift dann unbelegt, und Boulder faellt ohnehin auf
// LECO zurueck.
void ts_schriftwahl(uint8_t font_id, TsSchrift *stunden, TsSchrift *minuten);
