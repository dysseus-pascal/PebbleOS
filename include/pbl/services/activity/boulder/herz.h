/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Boulder V2, Schritt 3: Herzband der letzten 24 h und Zonenminuten von heute fuer die Health-App
// (Spezifikation Schritt 3, 3.8.2, Entscheid F8 = A; S6 in schnittstelle.h).
//
// WARUM IN DER VARIANTE: Die App soll ihre Karten sofort voll zeigen und nie auf KernelBG warten
// (F1). Der Minutenverlauf aus Pebbles Minutendatei wartet genau darauf (2.6.1). Die Variante
// sieht jede Minute ohnehin und fuehrt Band und Zonen in O(1) mit; die App kopiert 212 B.
//
// JE MINUTE (Minutenteil, boulder_herz_minute): Die Halbstunde der Minute wird zur juengsten, die
// uebersprungenen Plaetze werden leer. Hat die Minute einen Puls, aktualisiert er Minimum, Maximum
// und Mittel seiner Halbstunde und zaehlt in seiner Zone. Eine Minute eines neuen Ortstags setzt
// die Zonen auf 0 (Mitternacht). Die Grenzen sind Karvonen 40/60/80 % aus Ruhepuls-Median und
// Maximalpuls wie in Boulder (Gesundheitsrechnung.kt pulsZonen, pulsZone; ohne Median 60).
//
// NACHRECHNEN (Auftrag "Herz nachrechnen" auf KernelBG): nach dem Boot, sobald "Laden" die
// Ruhepulse und Kieselsport den Maximalpuls kennt, und wenn sich die Grenzen aendern. Ein
// Durchlauf durch Pebbles Minutendatei und den Puffer der Kopie (D18), ohne Heap fuer die Minuten.
// Er rechnet Band und Zonen aus denselben Minuten neu, die der laufende Betrieb gesehen hat, und
// kommt auf dasselbe Ergebnis. Bis er nach dem Boot gelaufen ist, gilt das Band als nicht da.
//
// MINUTEN IN FALSCHER REIHENFOLGE (Uhr zurueckgestellt, Pruefnaechte des Pruefbaus): Eine Minute
// einer aelteren Halbstunde, die noch im Band liegt, zaehlt mit; ihr Mittel ist dann gerundet aus
// dem alten Mittel, nicht genau (die genaue Summe kennt die Variante nur fuer die juengste
// Halbstunde). Das naechste Nachrechnen macht es wieder genau. Eine Minute eines aelteren
// Ortstags zaehlt in keiner Zone. Liegt das Band nach der Uhr in der Zukunft (Uhr um mehr als eine
// Halbstunde zurueck), stoesst der Takt das Nachrechnen an.

#include "pbl/services/activity/boulder/schnittstelle.h"

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

// Karvonen 40/60/80 % der Pulsreserve, die Reserve mindestens 20, ohne Ruhepuls-Median 60
// (Boulder: Gesundheitsrechnung.kt:475-478, Gesundheitsdaten.kt:529-531).
void boulder_herz_grenzen(uint8_t ruhepuls_median, uint16_t maxpuls,
                          uint8_t grenzen[HERZ_ZONEN - 1]);
// 0 leicht, 1 moderat, 2 kraeftig, 3 Spitze (Gesundheitsrechnung.kt:481).
uint8_t boulder_herz_zone(uint8_t bpm, const uint8_t grenzen[HERZ_ZONEN - 1]);

// Minutenteil (KernelBG): die eben fertige Minute ab `utc`, ihr Puls (0 = keiner). O(1), ohne Heap
// und ohne Datei; unter der Sperre nur Rechnen und Kopieren.
void boulder_herz_minute(time_t utc, uint8_t puls);

// Nach "Laden" (KernelBG): Band und Zonen nachrechnen lassen. Und der Takt jede Minute, auch am
// Kabel: Haben sich die Grenzen geaendert oder steht das Nachrechnen noch aus (verworfen, kein
// Tracking-Teil), reiht er es (wieder) ein.
void boulder_herz_nachrechnen(void);
void boulder_herz_takt(void);

// S6: Band und Zonen in den Puffer des Aufrufers, unter der Sperre nur die Kopie. false, solange
// das Nachrechnen seit dem Boot nicht gelaufen ist (dann leer).
bool boulder_herz_kopieren(BoulderHerz *aus);

// Pruefbefehl und Tests: der Stand im Kern, ohne Kopie. Nur auf KernelBG.
const BoulderHerz *boulder_herz_kern(void);
bool boulder_herz_bereit(void);

// Tests: wie nach einem Boot. Nicht in boulder_dauerteil_test_neustart: Der Pruefbau beginnt damit
// jede Pruefnacht neu und soll das Band dabei behalten.
void boulder_herz_test_neustart(void);
