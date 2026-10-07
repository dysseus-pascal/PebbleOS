/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Was der Trainingskern von der Uhr braucht - an EINER Stelle.
//
// WARUM EINE EIGENE SCHICHT: Der Kern kommt aus Kieselsport (b25), wo er Pebbles Tageszaehler
// las und die Sitzung selbst eintrug. Jetzt fuehrt Pebbles Workout-Dienst das Training (Weg 2):
// Start, Pause, Stopp, Dauer, Schritte, Strecke, Sitzung und Meldung kommen von dort, die kcal aus
// Boulders Pulsrechnung (kalorien.h). Diese Schicht bildet nur ab, was der Kern braucht;
// plattform.c tut es mit den Diensten der Firmware.
//
// SPERREN (Spezifikation 2.5, Regel 2): Die Sperre des Dienstes ist ein Blatt. Was hier mit
// "ohne Sperre" steht, ruft Pebbles Workout-Dienst, den HRM-Manager oder die Variante und darf
// nie unter ks_sperren() laufen: Pebble haelt beim Start und Stopp seine Sperre und nimmt darunter
// Activity, Kraepelin und HRM; der HRM-Manager haelt seine Sperre, waehrend er Kieselsports
// Pulsrueckruf ruft, und der nimmt die Sperre des Dienstes. Unter der Sperre des Dienstes nur
// Persist, Brummen, Akku, Beschleunigung und Kompass.

#include "pbl/services/activity/boulder/sport/kieselsport.h"

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

// --- Der Persist von Kieselsport (KIESELSPORT_UUID, speicher.c)

bool ks_persist_exists(uint32_t key);
int32_t ks_persist_read_int(uint32_t key);
void ks_persist_write_int(uint32_t key, int32_t wert);
// Rueckgabe: gelesene Bytes, <= 0 wenn nichts da ist.
int ks_persist_read_data(uint32_t key, void *puffer, size_t groesse);
void ks_persist_write_data(uint32_t key, const void *daten, size_t groesse);
void ks_persist_delete(uint32_t key);

// Solange offen, bleibt die Datei geoeffnet - sonst oeffnet jeder Zugriff sie neu. Um ein
// Training herum, weil es an seinem Ende viele sind.
void ks_persist_offen_halten(bool an);

// --- Profil

// Das Alter aus den Gesundheitseinstellungen der Uhr - fuer Tanaka.
uint8_t ks_alter(void);
// Ohne Sperre: der Ruhepuls fuer Boulders Pruefung des Maximalpulses (V2), 0 = keiner.
uint8_t ks_ruhepuls(void);

// --- Pebbles Workout-Dienst. Alles ohne Sperre, alles auf KernelBG.

// Was Pebble ueber das laufende Training weiss.
typedef struct {
  bool laeuft;        //< Pebble hat ein Training (auch pausiert)
  bool pause;
  int32_t dauer_s;    //< Pebbles duration_s: die aktive Zeit, eine Uhr fuer beide (Regel 2.5.3)
  int32_t schritte;
  int32_t meter;
  int32_t kcal_cal;   //< aktive Kalorien (1/1000 kcal) aus dem Puls, bis dauer_s
} KsWorkout;

// Startet Pebbles Training fuer diese Art (Laufen -> Run, Wandern -> Walk, sonst Open) mit
// [maxpuls] fuer die Pulskalorien. false: Pebble lehnt ab (es laeuft schon eines).
bool ks_workout_starten(Sportart art, uint16_t maxpuls);
// false: Pebble hat kein Training (mehr).
bool ks_workout_pause(bool an);
// false (und laeuft = false): Pebble hat kein Training.
bool ks_workout_lesen(KsWorkout *aus);
// Pebbles Stopp: ab 60 s eine Sitzung und Pebbles Meldung. Danach schiebt es das Datalogging an
// (V8).
void ks_workout_beenden(void);
// Pebbles Stopp ohne Sitzung und Meldung (F3 = A).
void ks_workout_verwerfen(void);

// --- Sensoren. Ihre Werte kommen ueber puls_*, reps_daten und bahnen_kompass zurueck, immer
// unter der Sperre des Dienstes.

// Ohne Sperre. Takt 0 heisst: Sensor fuer uns aus (F2 = A: Kieselsport haelt das 1-Hz-Abo).
void ks_puls_sensor(uint16_t takt_s, bool hrv);

typedef struct {
  int16_t x;
  int16_t y;
  int16_t z;
  bool did_vibrate;
} KsAccel;
void ks_accel(bool an);  //< 25 Hz, in Paketen zu zehn

void ks_kompass(bool an);

// --- Was sonst geschieht

void ks_brummen(const uint32_t *dauer_ms, uint32_t anzahl);
uint8_t ks_akku_prozent(bool *am_kabel);
// Die App nach vorn holen, damit sie eine wartende Zusammenfassung schickt - nur vom Zifferblatt
// aus, nie ueber eine andere App.
void ks_app_holen(void);
