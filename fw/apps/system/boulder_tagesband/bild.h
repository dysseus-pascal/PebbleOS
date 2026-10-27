/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Das Bild des Zifferblatts Tagesband, fuer emery (200 x 228) gebaut wie die Health-App.

#include "applib/graphics/gtypes.h"
#include "apps/system/boulder_health/daten.h"

#include <stdbool.h>

// `verdeckt`: Pebbles Timeline-Vorschau liegt unten. Die Zeile mit Schritten, Ruhepuls und HRV
// entfaellt dann; sie laege unter der Vorschau, alles darueber bleibt frei.
void bt_zeichnen(GContext *ctx, GRect flaeche, bool verdeckt, const BhDaten *d);
