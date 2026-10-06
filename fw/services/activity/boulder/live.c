/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/activity/boulder/live.h"

#include "pbl/services/activity/boulder/eingang.h"

#include <string.h>

// Dieselben Gewichte wie schlafanalyse.c (s_gewichte, Minute-4..Minute+2). Die Datei ist
// bytegleich zu b26 und gibt sie nicht heraus; deshalb hier noch einmal.
static const uint16_t s_gewichte[7] = { 106, 54, 58, 76, 230, 74, 67 };

// Pulse der Schlafminuten: ein Histogramm statt einer Liste, damit der Median O(1) im
// Speicher bleibt (512 B) und keine Minute verloren geht.
static uint16_t s_histogramm[256];
static uint16_t s_pulse;
static bool s_schlaeft;
static uint8_t s_folge_schlaf;
static uint8_t s_folge_wach;
static int16_t s_lauf_beginn;

void boulder_live_neu(void) {
  memset(s_histogramm, 0, sizeof(s_histogramm));
  s_pulse = 0;
  s_schlaeft = false;
  s_folge_schlaf = 0;
  s_folge_wach = 0;
  s_lauf_beginn = -1;
}

// Cole-Kripke fuer die Minute `i` mit den Minuten bis `bis` (einschliesslich): die
// fehlende Zukunft und Luecken werden neu gewichtet, wie in schlafanalyse.c.
static bool prv_bewegung_wach(const BoulderEingang *e, int i, int bis) {
  if (!e->gueltig[i]) {
    return true;
  }
  uint32_t summe = 0;
  uint32_t gewicht = 0;
  for (int k = 0; k < 7; k++) {
    const int j = i + k - 4;
    if (j < 0 || j > bis || !e->gueltig[j]) {
      continue;
    }
    summe += (uint32_t)s_gewichte[k] * e->bewegung[j];
    gewicht += s_gewichte[k];
  }
  return gewicht == 0 || summe > SCHLAF_SCHWELLE_VMC * gewicht;
}

static uint8_t prv_median(void) {
  uint32_t gezaehlt = 0;
  for (int b = 0; b < 256; b++) {
    gezaehlt += s_histogramm[b];
    if (2 * gezaehlt >= s_pulse) {
      return (uint8_t)b;
    }
  }
  return 255;
}

void boulder_live_minute(int minute) {
  const int m = minute - LIVE_NACHLAUF;
  if (m < 0 || minute >= SCHLAF_MINUTEN_MAX) {
    return;
  }
  const BoulderEingang *e = boulder_eingang();
  bool wach = prv_bewegung_wach(e, m, minute);
  const uint8_t puls = e->gueltig[m] ? e->puls[m] : 0;
  if (!wach && puls && s_pulse < UINT16_MAX) {
    s_histogramm[puls]++;
    s_pulse++;
  }
  // Wach ohne Bewegung: ein Herz deutlich ueber dem Schlafpuls dieser Nacht.
  if (puls && s_pulse >= LIVE_PULS_AB && puls > prv_median() + LIVE_PULS_UEBER) {
    wach = true;
  }
  if (wach) {
    s_folge_wach = (s_folge_wach < UINT8_MAX) ? s_folge_wach + 1 : s_folge_wach;
    s_folge_schlaf = 0;
  } else {
    s_folge_schlaf = (s_folge_schlaf < UINT8_MAX) ? s_folge_schlaf + 1 : s_folge_schlaf;
    s_folge_wach = 0;
  }
  if (!s_schlaeft && s_folge_schlaf >= LIVE_SCHLAF_FOLGE) {
    s_schlaeft = true;
    s_lauf_beginn = (int16_t)(m - LIVE_SCHLAF_FOLGE + 1);
  } else if (s_schlaeft && s_folge_wach >= LIVE_WACH_FOLGE) {
    s_schlaeft = false;
  }
}

bool boulder_live_schlaeft(void) {
  return s_schlaeft;
}

int boulder_live_lauf_beginn(void) {
  return s_lauf_beginn;
}
