/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2: SpO2-Ring (spo2.h), aus b25 spo2.c; neu ist nur die Kopie mit memcpy.

#include "pbl/services/activity/boulder/spo2.h"

#include "pbl/kernel/mutex.h"

#include <string.h>

// Geschrieben auf KernelBG, gelesen aus der App - daher die Sperre.
static PBL_MUTEX_DEFINE(s_sperre);
static Spo2Wert s_ring[SPO2_WERTE_MAX];
static int s_naechster;
static int s_anzahl;

void boulder_spo2_merken(time_t utc, uint8_t prozent) {
  if (prozent == 0 || prozent > 100) {
    return;
  }
  pbl_mutex_lock(&s_sperre, PBL_FOREVER);
  s_ring[s_naechster] = (Spo2Wert){ .zeit = utc, .prozent = prozent };
  s_naechster = (s_naechster + 1) % SPO2_WERTE_MAX;
  if (s_anzahl < SPO2_WERTE_MAX) {
    s_anzahl++;
  }
  pbl_mutex_unlock(&s_sperre);
}

int boulder_spo2_werte(time_t von, time_t bis, Spo2Wert *aus, int max) {
  int n = 0;
  pbl_mutex_lock(&s_sperre, PBL_FOREVER);
  const int erster = (s_naechster - s_anzahl + SPO2_WERTE_MAX) % SPO2_WERTE_MAX;
  for (int i = 0; i < s_anzahl && n < max; i++) {
    const Spo2Wert *w = &s_ring[(erster + i) % SPO2_WERTE_MAX];
    if (w->zeit >= von && w->zeit < bis) {
      // memcpy: `aus` kann der schiefe Puffer einer App sein (S3, schnittstelle.c).
      memcpy(&aus[n++], w, sizeof(*w));
    }
  }
  pbl_mutex_unlock(&s_sperre);
  return n;
}
