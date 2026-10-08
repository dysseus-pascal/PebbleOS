/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "pebble_compat.h"
#include "kachel.h"

// Die Namen der Sportarten. Was eine Art kann, weiss der Dienst
// (kieselsport_art_info); wie sie heisst, nur die App - der Dienst zeigt
// nichts an.

// art_name() steht in kachel.h - die Kachel im Startmenue braucht ihn auch.

// Der Obertitel im Menue - "Bike" bei den beiden, sonst NULL.
const char *art_gruppe(Sportart art);
