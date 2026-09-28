/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "pebble_compat.h"

// Die Namen der Sportarten. Was eine Art kann, weiss der Dienst
// (kieselsport_art_info); wie sie heisst, nur die App - der Dienst zeigt
// nichts an.

// Der Name in der Sprache der Uhr, auch fuer eine Art ausserhalb des
// Bereichs (dann "?").
const char *art_name(Sportart art);

// Der Obertitel im Menue - "Bike" bei den beiden, sonst NULL.
const char *art_gruppe(Sportart art);
