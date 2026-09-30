/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "pebble_compat.h"

void nav_window_push(void);
/** Neu zeichnen, weil eine Nachricht kam. */
void nav_window_refresh(void);
/** Den Zustand dieser Datei auf Anfang - beim Start der App (kieselstrasse.c). */
void nav_window_zuruecksetzen(void);
