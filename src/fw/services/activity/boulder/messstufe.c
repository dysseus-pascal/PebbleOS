/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/activity/boulder/messstufe.h"

#include "pbl/services/activity/boulder/nacht.h"
#include "pbl/services/activity/boulder/schlafanalyse.h"
#include "util/time/time.h"

typedef struct {
  uint16_t puls_nacht_s;
  uint16_t puls_tag_s;
  uint8_t hrv_nacht_min;
  uint16_t spo2_s;
} Stufe;

// Die Tabelle aus messstufe.h, Stufe 1 zuerst.
static const Stufe s_stufen[MESSSTUFE_MAX] = {
  { 10 * 60, 30 * 60, 0, 0 },
  { 10 * 60, 10 * 60, 60, 60 * 60 },
  { 5 * 60, 10 * 60, 30, 30 * 60 },
  { 2 * 60, 10 * 60, 15, 10 * 60 },
  { 1 * 60, 5 * 60, 10, 10 * 60 },
};

static const Stufe *prv_stufe(void) {
  uint8_t s = boulder_messstufe();
  if (s < MESSSTUFE_MIN || s > MESSSTUFE_MAX) {
    s = MESSSTUFE_VORGABE;
  }
  return &s_stufen[s - 1];
}

bool boulder_nachtruhe(time_t utc, uint16_t letzte_bewegung) {
  // Dieselbe Schwelle wie im Schlaf: was darueber liegt, ist wach.
  return boulder_nacht_im_fenster(utc) && letzte_bewegung <= SCHLAF_SCHWELLE_VMC;
}

uint32_t boulder_puls_takt_s(time_t utc, uint16_t letzte_bewegung) {
  const Stufe *s = prv_stufe();
  return boulder_nachtruhe(utc, letzte_bewegung) ? s->puls_nacht_s : s->puls_tag_s;
}

uint32_t boulder_spo2_takt_s(void) {
  return prv_stufe()->spo2_s;
}

uint16_t boulder_hrv_takt_min(void) {
  return prv_stufe()->hrv_nacht_min;
}
