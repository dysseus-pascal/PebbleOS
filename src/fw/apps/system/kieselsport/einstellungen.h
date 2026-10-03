/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "pebble_compat.h"

// Die Einstellungen aus der Konfigseite und aus Kiesel-Helper.
//
// SIE GEHEN AN DEN DIENST. Maximalpuls, Becken, Pause und Empfindlichkeit
// gebraucht der Trainingsdienst (services/kieselsport); er haelt sie im
// Persist und prueft die Grenzen. Nur der Timeline-Pin gehoert der App.

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

// ALLE EINSTELLUNGEN IN EINE NACHRICHT - fuer das Telefon.
//
// DIE UHR IST DIE EINE STELLE, AN DER SIE GELTEN. Geaendert werden sie auf
// der Konfigseite der Pebble-App oder in Kiesel-Helper; beide schicken an die
// Uhr, und die Uhr meldet danach, was gilt.
void einstellungen_melden(DictionaryIterator *out);
