/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Boulder: liest Pebbles Fixture-Dateien (tests/fixtures/activity/...) zur Laufzeit, wie
// test_kraepelin_algorithm.c: Minutenzeilen "{ steps, 0xNN, vmc, light[, plugged]}" und
// Beschleunigungszeilen "{ x, y, z}". Kein Nachbau einer Rechnung, nur Eingabedaten.

#include "applib/accel_service.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
  uint8_t schritte;
  uint8_t lage;
  uint16_t vmc;
  uint8_t licht;
  bool geladen;
} FixMinute;

// Haengt die Minuten der Datei an `aus` (ab `anzahl`), Rueckgabe: neue Anzahl.
static int fixture_minuten_lesen(const char *pfad, FixMinute *aus, int anzahl, int max) {
  FILE *f = fopen(pfad, "r");
  if (!f) {
    printf("Fixture fehlt: %s\n", pfad);
    return anzahl;
  }
  char zeile[256];
  while (anzahl < max && fgets(zeile, sizeof(zeile), f)) {
    int w[5] = {0};
    const int n = sscanf(zeile, " { %i , %i , %i , %i , %i", &w[0], &w[1], &w[2], &w[3], &w[4]);
    if (n >= 4) {
      aus[anzahl++] = (FixMinute){.schritte = w[0], .lage = w[1], .vmc = w[2], .licht = w[3],
                                  .geladen = (n == 5) && w[4]};
    }
  }
  fclose(f);
  return anzahl;
}

// Haengt die Beschleunigungswerte der Datei an `aus` (ab `anzahl`), Rueckgabe: neue Anzahl.
[[maybe_unused]] static int fixture_accel_lesen(const char *pfad, AccelRawData *aus, int anzahl,
                                                int max) {
  FILE *f = fopen(pfad, "r");
  if (!f) {
    printf("Fixture fehlt: %s\n", pfad);
    return anzahl;
  }
  char zeile[256];
  while (anzahl < max && fgets(zeile, sizeof(zeile), f)) {
    int w[4];
    if (sscanf(zeile, " { %i , %i , %i %n", &w[0], &w[1], &w[2], &w[3]) == 3 &&
        zeile[w[3]] == '}') {
      aus[anzahl++] = (AccelRawData){.x = w[0], .y = w[1], .z = w[2]};
    }
  }
  fclose(f);
  return anzahl;
}
