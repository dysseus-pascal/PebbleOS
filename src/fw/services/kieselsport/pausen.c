/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pausen.h"

typedef struct {
  uint16_t aktiv_s;   //< aktive Sekunde, bei der die Pause begann
  uint16_t dauer_s;   //< Wandzeit der Pause
} Pause;

static Pause s_liste[KS_PAUSEN_MAX];
static int s_anzahl;
static uint16_t s_gesamt;
static uint32_t s_offen_aktiv;
static time_t s_offen_seit;

void pausen_leeren(void) {
  s_anzahl = 0;
  s_gesamt = 0;
  s_offen_aktiv = 0;
  s_offen_seit = 0;
}

void pausen_beginn(uint32_t aktiv_s, time_t jetzt) {
  s_offen_aktiv = aktiv_s;
  // Null hiesse "keine offene Pause" - eine Uhr am 1.1.1970 gibt es nicht.
  s_offen_seit = jetzt > 0 ? jetzt : 1;
}

void pausen_ende(time_t jetzt) {
  if (s_offen_seit == 0) return;
  // EINE ZURUECKGESTELLTE UHR macht keine negative Pause: dann war sie null.
  const uint32_t dauer = jetzt > s_offen_seit ? (uint32_t)(jetzt - s_offen_seit) : 0;
  const uint32_t aktiv = s_offen_aktiv;
  s_offen_seit = 0;
  if (s_gesamt < UINT16_MAX) s_gesamt++;
  // Was nicht in zwei Byte passt oder keinen Platz mehr hat, zaehlt nur mit.
  if (aktiv > UINT16_MAX || dauer > UINT16_MAX || s_anzahl >= KS_PAUSEN_MAX) return;
  s_liste[s_anzahl] = (Pause){ .aktiv_s = (uint16_t)aktiv, .dauer_s = (uint16_t)dauer };
  s_anzahl++;
}

time_t pausen_offen_seit(void) { return s_offen_seit; }

int pausen_anzahl(void) { return s_anzahl; }
uint16_t pausen_gesamt(void) { return s_gesamt; }

static void prv_u16(uint8_t *aus, uint16_t wert) {
  aus[0] = (uint8_t)(wert & 0xFF);
  aus[1] = (uint8_t)(wert >> 8);
}

size_t pausen_als_bytes(uint8_t *aus, size_t platz) {
  const size_t laenge = 2 + 4 * (size_t)s_anzahl;
  if (platz < laenge) return 0;
  aus[0] = KS_PAUSEN_FASSUNG;
  // Ueber 255 steht 255 da - das passt nie zur Liste und heisst: unvollstaendig.
  aus[1] = (uint8_t)(s_gesamt > 255 ? 255 : s_gesamt);
  for (int i = 0; i < s_anzahl; i++) {
    prv_u16(aus + 2 + 4 * i, s_liste[i].aktiv_s);
    prv_u16(aus + 4 + 4 * i, s_liste[i].dauer_s);
  }
  return laenge;
}
