/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "pebble_compat.h"

// Das Menue "Messen": HRV (eine Minute, auf der Uhr gemessen) und SpO2 (der
// letzte Wert, den Kiesel-Helper aus Health Connect schickt).
void messen_window_zeige(void);

// Der Zustand dieser Datei auf Anfang - beim Start der App (siehe kieselsport.c).
void messen_window_zuruecksetzen(void);
