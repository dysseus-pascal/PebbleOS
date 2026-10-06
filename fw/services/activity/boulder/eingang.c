/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/activity/boulder/eingang.h"

#include <string.h>

static BoulderEingang s_eingang;

void boulder_eingang_neu(void) {
  memset(&s_eingang, 0, sizeof(s_eingang));
  s_eingang.letzte = -1;
}

bool boulder_eingang_minute(int minute, uint16_t vmc, uint8_t puls, bool gueltig) {
  if (minute < 0 || minute >= SCHLAF_MINUTEN_MAX || minute <= s_eingang.letzte) {
    return false;
  }
  s_eingang.letzte = (int16_t)minute;
  // Wie b25/b26 (M: nacht.c:245-251): ohne Satz oder mit VMC 0xFFFF kein Schlaf.
  if (!gueltig || vmc == 0xFFFF) {
    return true;
  }
  s_eingang.bewegung[minute] = vmc;
  s_eingang.puls[minute] = (puls == 0xFF) ? 0 : puls;
  s_eingang.gueltig[minute] = true;
  s_eingang.ende = (int16_t)(minute + 1);
  return true;
}

bool boulder_eingang_hrv(int minute, uint16_t rmssd, uint8_t puls) {
  const int n = s_eingang.hrv_anzahl;
  if (minute < 0 || minute >= SCHLAF_MINUTEN_MAX || n >= SCHLAF_HRV_MAX ||
      (n > 0 && minute <= s_eingang.hrv[n - 1].minute)) {
    return false;
  }
  s_eingang.hrv[n] = (SchlafHrv){ .minute = (uint16_t)minute, .rmssd = rmssd, .puls = puls };
  s_eingang.hrv_anzahl = n + 1;
  return true;
}

const BoulderEingang *boulder_eingang(void) {
  return &s_eingang;
}
