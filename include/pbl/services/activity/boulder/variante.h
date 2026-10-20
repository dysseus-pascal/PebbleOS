/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Boulder V2: die Naht zwischen der Kraepelin-Kopie und dem Dauerteil der Variante
// (Spezifikation 2.3 D1-D14, 2.4).
//
// ZWEI TEILE: Die Kopie (Tracking-Teil) lebt auf dem Heap von activity_algorithm_init
// bis deinit; am Ladekabel ist sie weg. Der Dauerteil (Nacht, Live-Zustand, Training,
// Abos) ist statisch und lebt ab dem ersten init. Deshalb fragt die Kopie den Dauerteil
// und nicht umgekehrt; der Dauerteil ruft die Kopie nur ueber boulder_kraepelin_*.
//
// SPERREN (2.4, Regel 3): Reihenfolge immer Activity -> Kraepelin -> Variante. Die Kopie
// ruft die boulder_dauerteil_* auch unter ihrer Sperre; der Dauerteil nimmt dabei
// hoechstens seine eigene, innerste Sperre und ruft nichts aus Pebble auf. Alles hier ist
// O(1), ohne Heap und ohne Datei: es laeuft im Minutenteil (K7).

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

// Ein fertiger Minutensatz (D2) oder eine Minute aus Pebbles Minutendatei (D14).
typedef struct {
  time_t utc;            //< Beginn der Minute
  uint16_t vmc;          //< Bewegung (Kraepelins VMC)
  uint8_t puls;          //< Minutenmedian, 0 = keiner
  uint8_t spo2;          //< Probe dieser Minute in Prozent, 0 = keine (Datei: immer 0)
  bool geladen;          //< am Ladekabel
  bool nicht_getragen;   //< Ladekabel oder Puls-Sensor "nicht am Arm" (Datei: = geladen)
} BoulderMinute;

// ---------------------------------------------------------------------------------------
// Dauerteil, von der Kopie gerufen

// D2: der fertige Satz der eben abgeschlossenen Minute, nach der Kraepelin-Sperre.
void boulder_dauerteil_minute(const BoulderMinute *minute);

// D14: Soll init beim Zaehlen der Minutendatei den Ring fuellen, und fuer welche Zeit?
// Falsch = nein (nur nach einem Boot ja). Danach kommt jede Minute der Datei in
// [von, bis) einmal ueber boulder_dauerteil_ring_minute, in Dateireihenfolge.
bool boulder_dauerteil_ring_fenster(time_t *von_utc, time_t *bis_utc);
void boulder_dauerteil_ring_minute(const BoulderMinute *minute);

// D17 (Schritt 2, V1): Laeuft ein Training ohne Schritte? Dann zaehlen neue Schritte weder in
// der Tagessumme noch in der Minute, und die Rate ist 0.
bool boulder_dauerteil_training_ohne_schritte(void);

// D6: bis wohin der Schlaf verarbeitet ist (activity_algorithm_get_last_sleep_utc).
time_t boulder_dauerteil_letzter_schlaf_utc(void);

// deinit: Der Tracking-Teil geht (D13).
void boulder_dauerteil_tracking_aus(void);

// D15: Ein Training laeuft (enable_activity_tracking(false) unter Pebbles Workout-Sperre; von
// der App-Task mit Pebbles Workout-App, von KernelBG mit Kieselsport): nur ein Merker.
void boulder_dauerteil_training(bool laeuft);

// Tests: der Dauerteil wie nach einem Boot (alles Statische auf null; Dateien bleiben). Der
// naechste init fuellt den Ring und reiht "Laden" ein (AP4).
void boulder_dauerteil_test_neustart(void);
// Tests, die Pebbles Timer selbst der Reihe nach feuern: den eigenen Takt nicht als Cron planen
// (sein Weck-Timer kaeme vor den eigenen des Tests). Der Test ruft boulder_dauerteil_takt.
void boulder_dauerteil_test_ohne_cron(bool ohne);

// ---------------------------------------------------------------------------------------
// Kieselsport an die Variante (Schritt 2, Spezifikation 2.6)

// V1, vom Nutzer geaendert: Das eben gestartete Training zaehlt keine Schritte (alle Arten ausser
// Laufen und Wandern). Kieselsport ruft es nach workout_service_start_workout, ohne Sperre
// (Regel 2.5.2). Es gilt bis zum Ende genau dieses Trainings; ohne laufendes Training wirkt es
// nicht.
void boulder_variante_training_ohne_schritte(bool ohne);

// ---------------------------------------------------------------------------------------
// Kopie, vom Dauerteil gerufen (KernelBG, ohne die Sperre der Variante)

// D18 (Schritt 3, "Herz nachrechnen"): jede Minute in [von, bis) aus Pebbles Minutendatei und dem
// Puffer der Kopie, der noch nicht in der Datei steht - in Dateireihenfolge, die nicht die
// Zeitfolge sein muss. Ein Durchlauf mit settings_file_each, ohne Heap fuer die Minuten. Laeuft
// unter der Kraepelin-Sperre: `je_minute` rechnet nur und nimmt keine Sperre. Rueckgabe false ohne
// Tracking-Teil oder ohne Minutendatei (dann kam keine Minute).
typedef void (*BoulderJeMinute)(const BoulderMinute *minute, void *kontext);
bool boulder_kraepelin_minuten(time_t von_utc, time_t bis_utc, BoulderJeMinute je_minute,
                               void *kontext);

