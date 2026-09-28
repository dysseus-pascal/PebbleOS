/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "plattform.h"

// Die Pulskurve eines Trainings - fuer das Telefon, ueber die App.
//
// WARUM UEBER DIE APP: die Pebble-App reicht Data Logging nicht an
// klassische Companion-Apps weiter - die Kurve kam so nie an. Was ankommt,
// ist AppMessage. Also merkt sich der Dienst die Kurve, legt sie beim
// Speichern in den Persist, und die App schickt sie stueckweise hinterher,
// sobald die Zusammenfassung bestaetigt ist.
//
// ALLE ZEHN SEKUNDEN EIN WERT, als ein Byte. Der Persist fasst je Schluessel
// 256 Byte; sechs Schluessel zu 200 Byte sind 1200 Werte, also gut drei
// Stunden. Ein Puls aendert sich nicht so schnell, dass zehn Sekunden eine
// Kurve verfaelschten.

#define KS_KURVE_MAX 1200
#define KS_KURVE_STUECK 200   //< Byte je Persist-Schluessel

// --- Dienst ---
void kurve_start(void);
// Jede Sekunde: der frische Puls oder 0. Nimmt alle KS_KURVE_TAKT_S einen.
void kurve_tick(uint32_t sekunde, uint16_t puls);
// Beim Speichern: in den Persist, mit dem Beginn des Trainings.
void kurve_merken(uint32_t beginn);

// --- App ---
bool kurve_wartet(void);
uint32_t kurve_beginn(void);
uint16_t kurve_anzahl(void);
// Ab welchem Wert das naechste Stueck beginnt.
uint16_t kurve_ab(void);
// Das naechste Stueck, hoechstens `max` Byte. Rueckgabe: wie viele.
uint16_t kurve_stueck(uint8_t *aus, uint16_t max);
// Das Telefon hat `n` Werte bestaetigt.
void kurve_bestaetigt(uint16_t n);
void kurve_vergessen(void);

// DIE ZEIT JE ZONE, aus der gemerkten Kurve - fuer den Schirm nach dem
// Speichern. Gelesen wird der ganze Persist, nicht nur was noch aufs
// Telefon wartet. Werte ohne Puls zaehlen nicht.
bool kurve_zeiten(uint32_t sekunden[KS_ZONEN + 1]);
