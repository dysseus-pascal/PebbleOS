/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Was der Trainingskern von der Uhr braucht - an EINER Stelle.
//
// WARUM EINE EIGENE SCHICHT: der Kern kommt aus Kieselsport, wo er als App
// und Worker ueber das SDK lief. In der Firmware meinen persist_* den
// Speicher der gerade laufenden App und health_service_* die Sicht einer App
// - beides passt nicht zu einem Dienst auf KernelBG. Diese Schicht bildet nur
// ab, was der Kern wirklich braucht; plattform.c tut es mit den Diensten der
// Firmware, die Tests setzen ihre eigene darunter.

#include "pbl/services/kieselsport/kieselsport.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifndef ARRAY_LENGTH
#define ARRAY_LENGTH(a) (sizeof(a) / sizeof((a)[0]))
#endif

time_t ks_jetzt(void);

// --- Der Persist von Kieselsport (KIESELSPORT_UUID)

bool ks_persist_exists(uint32_t key);
int32_t ks_persist_read_int(uint32_t key);
void ks_persist_write_int(uint32_t key, int32_t wert);
// Rueckgabe: gelesene Bytes, <= 0 wenn nichts da ist.
int ks_persist_read_data(uint32_t key, void *puffer, size_t groesse);
void ks_persist_write_data(uint32_t key, const void *daten, size_t groesse);
void ks_persist_delete(uint32_t key);

// Solange offen, bleibt die Datei geoeffnet - sonst oeffnet jeder Zugriff
// sie neu. Um ein Training herum, weil es an seinem Ende viele sind.
void ks_persist_offen_halten(bool an);

// --- Die Tageszaehler der Uhr

typedef enum {
  KsZaehlerSchritte,
  KsZaehlerMeter,
  KsZaehlerKcal,
} KsZaehler;
uint32_t ks_heute(KsZaehler zaehler);

// --- Sensoren. Ihre Werte kommen ueber puls_*, reps_daten und
// bahnen_kompass zurueck, immer unter der Sperre des Dienstes.

// Takt 0 heisst: Sensor fuer uns aus.
void ks_puls_sensor(uint16_t takt_s, bool hrv);
// Wie der Pulsalgorithmus rechnen soll - bewegt oder in Ruhe.
void ks_puls_szene(Sportart art, bool an);

typedef struct {
  int16_t x;
  int16_t y;
  int16_t z;
  bool did_vibrate;
} KsAccel;
void ks_accel(bool an);   //< 25 Hz, in Paketen zu zehn

void ks_kompass(bool an);

// --- Was sonst geschieht

void ks_brummen(const uint32_t *dauer_ms, uint32_t anzahl);
uint8_t ks_akku_prozent(bool *am_kabel);
// Die automatische Erkennung von Bewegung ruht, solange ein Training
// laeuft - sonst stuende dieselbe Stunde zweimal im Verlauf.
void ks_bewegung_erkennen(bool an);
// Das fertige Training als Sitzung in den Verlauf der Uhr.
void ks_sitzung_melden(const Trainingsstand *t);
// Die App nach vorn holen, damit sie eine wartende Zusammenfassung schickt -
// nur vom Zifferblatt aus, nie ueber eine andere App.
void ks_app_holen(void);
// Solange ein Training laeuft, steht Kieselsport oben im Startmenue.
void ks_startmenue_oben(bool laeuft);
