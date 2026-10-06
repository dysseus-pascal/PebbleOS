/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Boulder V2: die Naht zwischen der Kraepelin-Kopie und dem Dauerteil der Variante
// (Spezifikation 2.3 D1-D14, 2.4).
//
// ZWEI TEILE: Die Kopie (Tracking-Teil) lebt auf dem Heap von activity_algorithm_init
// bis deinit; am Ladekabel ist sie weg. Der Dauerteil (Nacht, Live-Zustand, Velo/Auto,
// Abos) ist statisch und lebt ab dem ersten init. Deshalb fragt die Kopie den Dauerteil
// und nicht umgekehrt; der Dauerteil ruft die Kopie nur ueber boulder_kraepelin_*.
//
// SPERREN (2.4, Regel 3): Reihenfolge immer Activity -> Kraepelin -> Variante. Die Kopie
// ruft die boulder_dauerteil_* auch unter ihrer Sperre; der Dauerteil nimmt dabei
// hoechstens seine eigene, innerste Sperre und ruft nichts aus Pebble auf. Alles hier ist
// O(1), ohne Heap und ohne Datei: es laeuft im Minutenteil (K7).

#include "pbl/services/hrm/hrm_activity_scene.h"

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

// Ein fertiger Minutensatz (D2) oder eine Minute aus Pebbles Minutendatei (D14).
typedef struct {
  time_t utc;            //< Beginn der Minute
  uint16_t vmc;          //< Bewegung (Kraepelins VMC)
  uint8_t lage;          //< Kraepelins orientation
  uint8_t schritte;      //< gezaehlt; in der Datei nach Kraepelins Schlaf-Nullung
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

// D3: Seit wann ist eine Velo-/Autofahrt offen (Beginn der ersten fahrtartigen Minute,
// 0 = nichts offen)? Die Kopie haelt ihre Minuten ab da zurueck, hoechstens
// BOULDER_FAHRT_ZURUECK_MAX. Endet "offen" mit einem "Ja", ruft der Dauerteil VORHER
// boulder_kraepelin_fahrt_streichen, damit keine Fahrtminute ungestrichen hinausgeht.
time_t boulder_dauerteil_fahrt_offen_seit(void);
#define BOULDER_FAHRT_ZURUECK_MAX 60

// D3: Gehoert die Minute zu einer bestaetigten Fahrt? Dann schreibt die Kopie sie mit
// Schritten, aktiv, Strecke und aktiven kcal = 0 (wie Kraepelins Schlafminuten).
bool boulder_dauerteil_fahrt_minute(time_t minute_utc);

// D5: Laeuft gerade eine bestaetigte Fahrt? Dann zaehlen neue Schritte nicht.
bool boulder_dauerteil_fahrt_laeuft(void);

// D1: Ueberdeckt eine Velo-/Autokandidatin diese Zeit? Dann ist es kein Spaziergang.
bool boulder_dauerteil_fahrt_ueberdeckt(time_t start_utc, uint32_t laenge_s);

// D6: bis wohin der Schlaf verarbeitet ist (activity_algorithm_get_last_sleep_utc).
time_t boulder_dauerteil_letzter_schlaf_utc(void);

// D10: das Puls-Abo "Aktiv" der Variante (Velo), fuer activity.c wie Kraepelins Abo.
bool boulder_dauerteil_aktiv_abo_laeuft(void);
void boulder_dauerteil_aktiv_abo_pausieren(bool pausiert);

// deinit: Der Tracking-Teil geht; ein offener Fahrt-Entscheid gilt als "Nein" (D13).
void boulder_dauerteil_tracking_aus(void);

// Tests: der Dauerteil wie nach einem Boot (alles Statische auf null). Das Laden nach dem
// Boot (Ring, Nacht) kommt mit AP4.
void boulder_dauerteil_test_neustart(void);

// ---------------------------------------------------------------------------------------
// Kopie, vom Dauerteil gerufen (KernelBG, ohne die Sperre der Variante)

// D4: Bei "Ja" die Schritte der Fahrtminuten [von, bis) am Kraepelin-Zaehler abziehen,
// soweit sie noch im Puffer oder in der laufenden Minute liegen und zum heutigen Tag
// gehoeren. activity.c uebernimmt den Zaehler beim naechsten Beschleunigungsblock.
void boulder_kraepelin_fahrt_streichen(time_t von_utc, time_t bis_utc);

// D5: Schritte, die seit init an der Summe vorbeigingen (Diagnose, Tests).
uint32_t boulder_kraepelin_gestrichen(void);

// ---------------------------------------------------------------------------------------
// Szene des Puls-Algorithmus (szene.c, D10): eine Stelle fuer Kraepelins Walk/Run und
// das Abo "Aktiv". Kraepelin geht vor; zurueckgesetzt wird nur die eigene Szene.
void boulder_szene_kraepelin(HRMActivityScene szene);
// Nur ohne Training aufrufen (das Training setzt seine eigene Szene).
void boulder_szene_aktiv(bool an);
