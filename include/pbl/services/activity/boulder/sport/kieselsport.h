/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Kieselsport auf Pebbles Workout-Dienst (Schritt 2, aus b25 include/pbl/services/kieselsport/).
//
// DIESELBE KENNUNG WIE IN b25 UND IM STORE. Boulder erkennt die App an der UUID
// (eingebauter Begleiter, AppMessages), und der Persist liegt in der Datei "ps" + UUID.
// Mit einer anderen UUID verloere der Wechsel von b25 die Einstellungen und alles, was
// noch beim Telefon ankommen soll. Die Registry-Nummer (-103) speichert die Uhr nicht.

#include "pbl/util/uuid.h"

// 6c386250-a6d7-4a19-a485-52541b5d7c0d
#define KIESELSPORT_UUID_INIT                                                             \
  {0x6c, 0x38, 0x62, 0x50, 0xa6, 0xd7, 0x4a, 0x19, 0xa4, 0x85, 0x52, 0x54, 0x1b, 0x5d, \
   0x7c, 0x0d}
#define KIESELSPORT_UUID ((Uuid)KIESELSPORT_UUID_INIT)
