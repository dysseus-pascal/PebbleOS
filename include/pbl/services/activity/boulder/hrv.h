/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Boulder V2: die HRV-Rechnung eines Fensters (Spezifikation 4.9), aus b25 hrv.c
// (M: :59-126) unveraendert uebernommen, damit alte und neue Werte vergleichbar bleiben:
// RMSSD ueber Schlag-zu-Schlag-Differenzen, Grenzen 300-2000 ms, ein Abstand, der um
// mehr als ein Drittel vom vorigen abweicht, zaehlt nicht, mindestens 30 Intervalle,
// ganzzahlige Wurzel. Jeder Abstand kostet O(1), ohne Heap - er kommt im HRM-Rueckruf.

#include <stdint.h>

#define HRV_MIN_INTERVALLE 30

typedef struct {
  uint16_t vorher;     //< voriger Abstand, 0 = keiner (Neubeginn nach Luecke)
  uint32_t summe;      //< Summe der quadrierten Differenzen
  uint16_t anzahl;     //< gezaehlte Differenzen
  uint32_t ppi_summe;  //< Summe der gezaehlten Abstaende, fuer den Puls
} HrvRechnung;

void boulder_hrv_neu(HrvRechnung *r);
// Ein Abstand in ms, in der Reihenfolge des Sensors.
void boulder_hrv_intervall(HrvRechnung *r, uint16_t ppi_ms);
// Der naechste Abstand wird nicht gegen den vorigen gerechnet (Luecke, schlechte Qualitaet).
void boulder_hrv_unterbrechen(HrvRechnung *r);
// RMSSD in ms, 0 unter HRV_MIN_INTERVALLE.
uint16_t boulder_hrv_rmssd(const HrvRechnung *r);
// Der Puls aus denselben Abstaenden, gerundet; 0 unter HRV_MIN_INTERVALLE.
uint8_t boulder_hrv_puls(const HrvRechnung *r);
