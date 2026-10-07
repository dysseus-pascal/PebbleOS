/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Boulder V2: die strenge Weckregel (Spezifikation 6, Kriterium 4).
//
// WARUM EINE EIGENE REGEL: Pebbles Smart Alarm (alarm.c) klingelt, sobald Pebbles Zustand
// "wach" oder "unbekannt" sagt. Der ist "wach", sobald die Schlafsitzung fehlt oder nicht
// verlaengert wird - nach einem Neustart, bei voller Liste, nach einem Zeitsprung - dann
// klingelt er sofort, auch im Tiefschlaf. E4 in alarm.c fragt deshalb in jeder Lage hier
// (ueber wecklauf.h, das den Lauf ueber einen Neustart traegt).
//
// DIE REGEL: Geweckt wird nur kurz nach Bewegung. b25s Tiefschlaf verlangt eine
// Viertelstunde ohne jede Regung (schlafanalyse.h, PHASEN); eine Minute mit Bewegung oder
// kurz danach ist also nicht Tief. Spaetestens zur Weckzeit klingelt er immer.
//
// KernelBG, O(1), ohne Heap und ohne Datei; Minutenteil und alarm.c laufen auf demselben
// Task, deshalb ohne Sperre.

#include "pbl/services/activity/boulder/live.h"

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

// Bewegungsbits je Minute: Bit k = Minute "letzte - k" hatte VMC > 0.
#define WECKER_S1 0x1u     //< S1: die letzte eigene Minute
#define WECKER_W 0x3Eu     //< W: m-5 ... m-1
// W gilt erst nach so vielen eigenen Minuten in Folge (seit Boot, Tracking aus, Luecke).
// Fehlende Minuten (der Neustart selbst, nicht gespeicherte Minuten) sind fuer den
// Live-Automaten wach; so lange braucht er, um danach wieder "schlaeft" sagen zu koennen
// (live.h). Vorher heisst sein "wach" nichts, und vor der Weckzeit weckt nur S1. Gefunden mit
// einem Neustart kurz vor dem Fenster (test_boulder_wecker, neustart_vor_fenster und
// eigenschaft_mit_neustart): W weckte im geglaetteten Tief.
#define WECKER_W_AB (LIVE_SCHLAF_FOLGE + LIVE_NACHLAUF)
// Die letzte eigene Minute ist hoechstens so alt (Ende der Minute bis jetzt), sonst ist der
// Dauerteil "nicht bereit" (Neustart, Tracking aus, Zeitsprung, Ladekabel).
#define WECKER_FRISCH_S (2 * 60)
// Nachholfenster (Entscheid 07.10.2026): So lange nach der Weckzeit klingelt ein Smart Alarm,
// der noch nicht geklungen hat, sofort - im Lauf und nach einem Neustart (wecklauf.h). Spaeter
// nicht mehr: Dann ist die Weckzeit so lange vorbei, dass der Wecker eher ueberrascht als weckt.
#define WECKER_NACHHOLEN_S (30 * 60)

// Die Weckzeit (UTC) zu `stunde`:`minute` Ortszeit, deren Fenster oder Nachholfenster `jetzt`
// enthaelt: die naechste, ausser sie liegt weniger als WECKER_NACHHOLEN_S zurueck.
time_t boulder_wecker_weckzeit(time_t jetzt, int stunde, int minute);

// Darf der Smart Alarm mit dieser Weckzeit (UTC) jetzt klingeln? Ab der Weckzeit immer (bis
// WECKER_NACHHOLEN_S danach): Pebbles Zaehler beginnt nach einem Neustart oder einem
// Zeitsprung neu und kaeme sonst zu spaet. Davor nur kurz nach Bewegung. E4 fragt das ueber
// wecklauf.h mit der Weckzeit des laufenden Fensters.
bool boulder_wecker_erlaubt(time_t weckzeit);

// Minutenteil (Dauerteil): die eben fertige eigene Minute.
void boulder_wecker_minute(time_t minute_utc, uint16_t vmc);

// Tracking aus (deinit): bis zur naechsten eigenen Minute nicht bereit.
void boulder_wecker_vergessen(void);
