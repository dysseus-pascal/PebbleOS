/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "../kieselsport/pebble_compat.h"
#include "pbl/services/activity/boulder/nacht.h"

// Was die Health-App zeigt, einmal gesammelt.
//
// ALLES KOMMT VON DER UHR: Tageszaehler und Minutenhistorie aus dem
// Aktivitaetsdienst, die Nacht aus der Boulder-Nachtrechnung, SpO2 aus ihrem
// Ring. Das Telefon braucht es fuer keine dieser Zahlen.

#define DATEN_TAGE 7
#define DATEN_PULS_SPALTEN 48   //< halbe Stunden der letzten 24 Stunden

typedef struct {
  // Heute
  int32_t schritte;
  int32_t meter;
  int32_t kcal;
  int32_t aktiv_min;
  uint16_t schritte_stunde[24];
  int stunde_jetzt;

  // Die letzte Nacht
  bool nacht_da;
  NachtBild nacht;

  // Herz
  uint8_t puls;              //< 0 = keiner
  time_t puls_zeit;
  uint8_t puls_min[DATEN_PULS_SPALTEN];   //< 0 = keine Messung in der halben Stunde
  uint8_t puls_max[DATEN_PULS_SPALTEN];
  uint8_t spo2;              //< letzter Wert, 0 = keiner
  time_t spo2_zeit;
  uint8_t spo2_nacht_tief;   //< tiefster Wert der letzten Nacht, 0 = keiner

  // Woche, [0] ist heute
  int32_t woche_schritte[DATEN_TAGE];
  int32_t woche_schlaf_min[DATEN_TAGE];
  int wochentag_heute;       //< 0 = Sonntag, wie tm_wday
} HealthDaten;

void daten_sammeln(HealthDaten *d);
// Nur der Puls - fuer den Sekundentakt auf der Herz-Karte.
void daten_puls(HealthDaten *d);
