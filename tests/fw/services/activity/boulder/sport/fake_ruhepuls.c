/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Der Ruhepuls der letzten Nacht (nacht.c), den kalorien.c beim Start liest. Ohne die ganze
// Variante: Die Tests stellen ihn ein.

#include <stdint.h>

static uint8_t s_ruhepuls;

uint8_t boulder_nacht_ruhepuls(void) {
  return s_ruhepuls;
}

void fake_ruhepuls_setzen(uint8_t bpm) {
  s_ruhepuls = bpm;
}
