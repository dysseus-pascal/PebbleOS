/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Boulder: ein steuerbarer Dauerteil fuer die Tests der Kraepelin-Kopie (variante.h).
// Die Kopie wird gegen die Naht geprueft; den echten Dauerteil bauen AP3-AP6.

#include "pbl/services/activity/boulder/variante.h"

#define FAKE_DAUERTEIL_MAX 2000

typedef struct {
  // Vorgaben
  time_t fahrt_offen_seit;      //< D3
  time_t fahrt_von, fahrt_bis;  //< bestaetigte Fahrt [von, bis), D3
  bool fahrt_laeuft;            //< D5
  bool ueberdeckt;              //< D1
  bool ring_an;                 //< D14
  time_t ring_von, ring_bis;
  bool aktiv_abo;               //< D10
  // Mitschrift
  BoulderMinute minuten[FAKE_DAUERTEIL_MAX];
  int minuten_anzahl;
  BoulderMinute ring[FAKE_DAUERTEIL_MAX];
  int ring_anzahl;
  int pausiert_aufrufe;
  bool pausiert;
  int tracking_aus;
} FakeDauerteil;

extern FakeDauerteil fake_dauerteil;

void fake_dauerteil_zuruecksetzen(void);
