/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Boulder: ein steuerbarer Dauerteil fuer die Tests der Kraepelin-Kopie (variante.h).
// Die Kopie wird gegen die Naht geprueft; den echten Dauerteil bauen AP3-AP6.

#include "pbl/services/activity/boulder/variante.h"

#define FAKE_DAUERTEIL_MAX 2000

typedef struct {
  // Vorgaben
  bool training_ohne_schritte;  //< D17
  bool ring_an;                 //< D14
  time_t ring_von, ring_bis;
  // Mitschrift
  BoulderMinute minuten[FAKE_DAUERTEIL_MAX];
  int minuten_anzahl;
  BoulderMinute ring[FAKE_DAUERTEIL_MAX];
  int ring_anzahl;
  int tracking_aus;
  bool training;
} FakeDauerteil;

extern FakeDauerteil fake_dauerteil;

void fake_dauerteil_zuruecksetzen(void);
