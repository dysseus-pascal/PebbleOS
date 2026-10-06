/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2: HRV-Rechnung (hrv.h). Rechnung aus b25 hrv.c (M: :59-126), nur der Zustand
// liegt jetzt in einer Struktur statt in Dateivariablen.

#include "pbl/services/activity/boulder/hrv.h"

#include <string.h>

void boulder_hrv_neu(HrvRechnung *r) {
  memset(r, 0, sizeof(*r));
}

// EIN INTERVALL, DAS UM MEHR ALS EIN DRITTEL VOM VORIGEN ABWEICHT, ist ein verpasster
// Schlag, kein Herz - sonst zoege ein einziger 1800er den Wert ins Absurde.
void boulder_hrv_intervall(HrvRechnung *r, uint16_t ppi) {
  if (ppi < 300 || ppi > 2000) {
    return;
  }
  if (r->vorher != 0) {
    const int d = (int)ppi - (int)r->vorher;
    const int grenze = (int)r->vorher / 3;
    if (d > -grenze && d < grenze && r->summe < 0xF0000000u && r->anzahl < 0xFFFF) {
      r->summe += (uint32_t)(d * d);
      r->ppi_summe += ppi;
      r->anzahl++;
    }
  }
  r->vorher = ppi;
}

void boulder_hrv_unterbrechen(HrvRechnung *r) {
  r->vorher = 0;
}

static uint32_t prv_wurzel(uint32_t x) {
  uint32_t w = 0;
  uint32_t bit = 1u << 30;
  while (bit > x) {
    bit >>= 2;
  }
  while (bit) {
    if (x >= w + bit) {
      x -= w + bit;
      w = (w >> 1) + bit;
    } else {
      w >>= 1;
    }
    bit >>= 2;
  }
  return w;
}

uint16_t boulder_hrv_rmssd(const HrvRechnung *r) {
  return (r->anzahl < HRV_MIN_INTERVALLE) ? 0 : (uint16_t)prv_wurzel(r->summe / r->anzahl);
}

// DER PULS DES FENSTERS aus denselben Abstaenden wie der RMSSD: eine HRV sagt erst neben
// ihrem Puls etwas (Satz 0x4002).
uint8_t boulder_hrv_puls(const HrvRechnung *r) {
  if (r->anzahl < HRV_MIN_INTERVALLE || r->ppi_summe == 0) {
    return 0;
  }
  const uint32_t bpm = (60000u * r->anzahl + r->ppi_summe / 2) / r->ppi_summe;
  return (bpm > 0xFF) ? 0xFF : (uint8_t)bpm;
}
