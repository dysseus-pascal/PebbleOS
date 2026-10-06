/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Boulder V2: die strenge Weckregel (Spezifikation 6, Kriterium 4).
//
// WARUM EINE EIGENE REGEL: Pebbles Smart Alarm (alarm.c) klingelt, sobald Pebbles Zustand
// "wach" oder "unbekannt" sagt. Der ist "wach", sobald die Schlafsitzung fehlt oder nicht
// verlaengert wird - nach einem Neustart, bei voller Liste, nach einem Zeitsprung - dann
// klingelt er sofort, auch im Tiefschlaf. E4 in alarm.c fragt deshalb in jeder Lage hier.
//
// DIE REGEL: Geweckt wird nur kurz nach Bewegung. b25s Tiefschlaf verlangt eine
// Viertelstunde ohne jede Regung (schlafanalyse.h, PHASEN); eine Minute mit Bewegung oder
// kurz danach ist also nicht Tief. Spaetestens zur Weckzeit klingelt er immer.
//
// KernelBG, O(1), ohne Heap und ohne Datei; Minutenteil und alarm.c laufen auf demselben
// Task, deshalb ohne Sperre.

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

// Bewegungsbits je Minute: Bit k = Minute "letzte - k" hatte VMC > 0.
#define WECKER_S1 0x1u     //< S1: die letzte eigene Minute
#define WECKER_W 0x3Eu     //< W: m-5 ... m-1
// Die letzte eigene Minute ist hoechstens so alt (Ende der Minute bis jetzt), sonst ist der
// Dauerteil "nicht bereit" (Neustart, Tracking aus, Zeitsprung, Ladekabel).
#define WECKER_FRISCH_S (2 * 60)

// E4 (alarm.c, prv_should_smart_alarm_trigger): Darf der Smart Alarm jetzt klingeln?
// `stunde`/`minute` = seine Weckzeit (Ortszeit). Ab der Weckzeit immer: Pebbles Zaehler
// beginnt nach einem Neustart oder einem Zeitsprung neu und kaeme sonst zu spaet.
bool boulder_kraepelin_wecken_erlaubt(int stunde, int minute);

// Minutenteil (Dauerteil): die eben fertige eigene Minute.
void boulder_wecker_minute(time_t minute_utc, uint16_t vmc);

// Tracking aus (deinit): bis zur naechsten eigenen Minute nicht bereit.
void boulder_wecker_vergessen(void);
