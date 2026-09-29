/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "pebble_compat.h"

// Der Schirm, wenn der Dienst die App geholt hat: ein gespeichertes Training
// kam nie beim Telefon an. Er sagt, was geschieht, und geht von selbst
// wieder zu, sobald das Telefon alles hat - oder nach einer Minute.
void sende_window_zeige(void);

// Der Zustand dieser Datei auf Anfang - beim Start der App (siehe kieselsport.c).
void sende_window_zuruecksetzen(void);
