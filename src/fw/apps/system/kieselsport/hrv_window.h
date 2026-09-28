/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "pebble_compat.h"

// Eine HRV-Messung auf Knopfdruck: eine Minute ruhig sitzen, am Ende steht
// der RMSSD da und geht ans Telefon. Das war Herzintervall; es ist hier
// aufgegangen, zusammen mit der Nachtmessung (nacht.h).
void hrv_window_zeige(void);
