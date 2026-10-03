/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "zweitzeit.h"

#include <stdio.h>

#define MINUTEN_TAG (24 * 60)

void zweitzeit_text(char *puffer, size_t groesse, int stunde, int minute, long gmtoff_s,
                    int versatz_h, bool h24, bool fuehrende_null) {
  // TimeStyle zog zusaetzlich tm_isdst ab und lag im Sommer eine Stunde
  // daneben: tm_gmtoff enthaelt die Sommerzeit schon. In Minuten gerechnet,
  // damit auch Orte mit halben Stunden Abstand (Indien) stimmen.
  const int utc = stunde * 60 + minute - (int)(gmtoff_s / 60);
  int ziel = (utc + versatz_h * 60) % MINUTEN_TAG;
  if (ziel < 0) {
    ziel += MINUTEN_TAG;
  }
  int h = ziel / 60;

  if (h24) {
    snprintf(puffer, groesse, fuehrende_null ? "%02i" : "%i", h);
    return;
  }
  const char am_pm = (h < 12) ? 'a' : 'p';
  h %= 12;
  if (h == 0) {
    h = 12;
  }
  snprintf(puffer, groesse, fuehrende_null ? "%02i%c" : "%i%c", h, am_pm);
}
