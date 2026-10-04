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
  bool puls_live;            //< der Sensor misst gerade, puls ist frisch
  time_t puls_zeit;
  uint8_t puls_min[DATEN_PULS_SPALTEN];   //< 0 = keine Messung in der halben Stunde
  uint8_t puls_max[DATEN_PULS_SPALTEN];
  uint8_t spo2;              //< letzter Wert, 0 = keiner
  bool spo2_aus;             //< die Uhr misst SpO2 gar nicht (Schalter oder Messstufe 1)
  time_t spo2_zeit;
  uint8_t spo2_nacht_tief;   //< tiefster Wert der letzten Nacht, 0 = keiner
} HealthDaten;

// DIE MINUTENHISTORIE STUECKWEISE. Ein Tag sind 24 Aufrufe in den
// Minutenspeicher im Flash - zusammen so lang, dass die App spuerbar spaet
// erschien und jede Minute kurz stand. Jetzt zeichnet sie sofort, und ein
// Timer liest ein Stueck nach dem anderen; die Kurven kommen, wenn alles da ist.
typedef struct {
  time_t kurve_beginn;
  time_t mitternacht;
  time_t jetzt;
  time_t ab;
  uint16_t schritte_stunde[24];
  uint8_t puls_min[DATEN_PULS_SPALTEN];
  uint8_t puls_max[DATEN_PULS_SPALTEN];
} MinutenLesen;

// Alles ausser der Minutenhistorie - schnell genug fuer den Start. Was die
// Minutenhistorie schon geliefert hat, bleibt stehen.
void daten_sammeln(HealthDaten *d);
void daten_minuten_beginnen(MinutenLesen *m);
// Ein Stueck lesen; true, wenn fertig - dann steht das Ergebnis in d.
bool daten_minuten_schritt(MinutenLesen *m, HealthDaten *d);
// Nur der Puls - fuer den Sekundentakt auf der Herz-Karte.
void daten_puls(HealthDaten *d);
