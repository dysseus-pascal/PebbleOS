/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kurve.h"
#include "puls.h"
#include "schluessel.h"

static uint8_t s_werte[KS_KURVE_MAX];
static uint16_t s_anzahl;

void kurve_start(void) {
  // Leeren, nicht nur zuruecksetzen: der erste Wert faellt auf Sekunde null,
  // die der Takt nie erreicht - dort stuende sonst der vom letzten Training.
  memset(s_werte, 0, sizeof(s_werte));
  s_anzahl = 0;
}

void kurve_tick(uint32_t sekunde, uint16_t puls) {
  if (sekunde % KS_KURVE_TAKT_S != 0) return;
  const uint32_t i = sekunde / KS_KURVE_TAKT_S;
  if (i >= KS_KURVE_MAX) return;
  s_werte[i] = puls > 255 ? 255 : (uint8_t)puls;
  if (i + 1 > s_anzahl) s_anzahl = (uint16_t)(i + 1);
}

void kurve_merken(uint32_t beginn) {
  if (s_anzahl < 2) return;
  for (uint16_t ab = 0, k = 0; ab < s_anzahl; ab += KS_KURVE_STUECK, k++) {
    const uint16_t n = (uint16_t)(s_anzahl - ab > KS_KURVE_STUECK ? KS_KURVE_STUECK : s_anzahl - ab);
    ks_persist_write_data(PERSIST_KURVE_BASIS + k, s_werte + ab, n);
  }
  ks_persist_write_int(PERSIST_KURVE_ANZAHL, s_anzahl);
  ks_persist_write_int(PERSIST_KURVE_BEGINN, (int32_t)beginn);
  ks_persist_write_int(PERSIST_KURVE_AB, 0);
}

bool kurve_wartet(void) {
  return ks_persist_exists(PERSIST_KURVE_ANZAHL) && kurve_ab() < kurve_anzahl();
}

uint32_t kurve_beginn(void) {
  return ks_persist_exists(PERSIST_KURVE_BEGINN) ? (uint32_t)ks_persist_read_int(PERSIST_KURVE_BEGINN) : 0;
}

uint16_t kurve_anzahl(void) {
  return ks_persist_exists(PERSIST_KURVE_ANZAHL) ? (uint16_t)ks_persist_read_int(PERSIST_KURVE_ANZAHL) : 0;
}

uint16_t kurve_ab(void) {
  return ks_persist_exists(PERSIST_KURVE_AB) ? (uint16_t)ks_persist_read_int(PERSIST_KURVE_AB) : 0;
}

uint16_t kurve_stueck(uint8_t *aus, uint16_t max) {
  const uint16_t anzahl = kurve_anzahl();
  uint16_t ab = kurve_ab();
  uint16_t n = 0;
  while (ab < anzahl && n < max) {
    const uint16_t k = ab / KS_KURVE_STUECK;
    const uint16_t in = ab % KS_KURVE_STUECK;
    uint8_t puffer[KS_KURVE_STUECK];
    const int gelesen = ks_persist_read_data(PERSIST_KURVE_BASIS + k, puffer, sizeof(puffer));
    if (gelesen <= (int)in) break;
    uint16_t hier = (uint16_t)(gelesen - in);
    if (hier > max - n) hier = (uint16_t)(max - n);
    if (ab + hier > anzahl) hier = (uint16_t)(anzahl - ab);
    memcpy(aus + n, puffer + in, hier);
    n = (uint16_t)(n + hier);
    ab = (uint16_t)(ab + hier);
  }
  return n;
}

void kurve_bestaetigt(uint16_t n) {
  const uint16_t neu = (uint16_t)(kurve_ab() + n);
  if (neu >= kurve_anzahl()) {
    kurve_vergessen();
  } else {
    ks_persist_write_int(PERSIST_KURVE_AB, neu);
  }
}

void kurve_vergessen(void) {
  ks_persist_delete(PERSIST_KURVE_ANZAHL);
  ks_persist_delete(PERSIST_KURVE_BEGINN);
  ks_persist_delete(PERSIST_KURVE_AB);
  for (int k = 0; k < (KS_KURVE_MAX + KS_KURVE_STUECK - 1) / KS_KURVE_STUECK; k++) {
    ks_persist_delete(PERSIST_KURVE_BASIS + k);
  }
}

bool kurve_zeiten(uint32_t sekunden[KS_ZONEN + 1]) {
  for (int z = 0; z <= KS_ZONEN; z++) sekunden[z] = 0;
  const uint16_t anzahl = kurve_anzahl();
  if (anzahl == 0) return false;
  bool etwas = false;
  uint8_t puffer[KS_KURVE_STUECK];
  for (uint16_t ab = 0, k = 0; ab < anzahl; ab += KS_KURVE_STUECK, k++) {
    const int gelesen = ks_persist_read_data(PERSIST_KURVE_BASIS + k, puffer, sizeof(puffer));
    for (int i = 0; i < gelesen && ab + i < anzahl; i++) {
      if (puffer[i] == 0) continue;
      sekunden[puls_zone(puffer[i])] += KS_KURVE_TAKT_S;
      etwas = true;
    }
  }
  return etwas;
}
