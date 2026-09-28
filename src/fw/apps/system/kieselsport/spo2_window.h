/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "pebble_compat.h"

// SpO2: der juengste Wert aus Health Connect, vom Telefon geholt.
//
// KEINE MESSUNG. Die Firmware misst SpO2 im Hintergrund, gibt es aber keiner
// Watchapp heraus - weder als HealthMetric noch in den Minutendaten. Die
// Pebble-App traegt es in Health Connect ein, Kiesel-Helper liest es dort und
// schickt es auf Anfrage hierher.
void spo2_window_zeige(void);
