/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "pebble_compat.h"

// Die Einstellungen vom Telefon (Boulder, frueher Kiesel-Helper und die Konfigseite).
//
// SIE GEHEN AN DEN DIENST. Maximalpuls, Becken, Pause, Empfindlichkeit und den
// Rhythmus der Atemuebung haelt der Trainingsdienst (services/activity/boulder/sport)
// im Persist und prueft die Grenzen. Nur der Timeline-Pin gehoert der App.

// Ohne Vermerk, von einem Telefon, das "nicht gesetzt" nicht kennt
// (kieselsport_maxpuls_alt).
void einstellungen_maxpuls(int32_t schlaege);
// Mit Vermerk: [gesetzt] false heisst "nach Alter", die Zahl zaehlt dann nicht.
void einstellungen_maxpuls_vermerkt(bool gesetzt, int32_t schlaege);
void einstellungen_becken(int32_t meter);
void einstellungen_pausenziel(int32_t sekunden);
void einstellungen_empfindlichkeit(int32_t stufe);
void einstellungen_pin_art(int32_t art);
void einstellungen_pin_zeit(const char *hhmm);
void einstellungen_atem(AtemWert welcher, int32_t wert);

// ALLE EINSTELLUNGEN IN EINE NACHRICHT - fuer das Telefon.
//
// DIE UHR IST DIE EINE STELLE, AN DER SIE GELTEN. Geaendert werden sie auf
// der Konfigseite der Pebble-App oder in Kiesel-Helper; beide schicken an die
// Uhr, und die Uhr meldet danach, was gilt.
void einstellungen_melden(DictionaryIterator *out);
