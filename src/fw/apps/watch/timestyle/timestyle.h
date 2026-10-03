/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder: TimeStyle als System-Zifferblatt (Quelle: freakified/TimeStylePebble,
// MIT, siehe LICENSE in diesem Ordner).
#pragma once

#include "process_management/pebble_process_md.h"

// Eigene Kennung, NICHT die der Store-App: so laesst sich TimeStyle aus dem
// Store weiter daneben installieren, und beide stoeren sich nicht. Der Name
// "TimeStyle Bedrock" unterscheidet es in der Liste von der Store-Fassung.
// d8af11d3-1e92-4f6d-b494-c5572f7bb7df
#define TIMESTYLE_UUID_INIT \
  {0xd8, 0xaf, 0x11, 0xd3, 0x1e, 0x92, 0x4f, 0x6d, 0xb4, 0x94, 0xc5, 0x57, 0x2f, 0x7b, 0xb7, 0xdf}

// Die Kennung des eingebauten Zifferblatts in b21, bevor es seinen eigenen
// Namen bekam. Wer es dort als Standard hatte, wird einmal umgestellt.
// e5c8eb9b-fd6a-4b3e-b795-d38ae5083063
#define TIMESTYLE_B21_UUID_INIT \
  {0xe5, 0xc8, 0xeb, 0x9b, 0xfd, 0x6a, 0x4b, 0x3e, 0xb7, 0x95, 0xd3, 0x8a, 0xe5, 0x08, 0x30, 0x63}

// Die Store-App (mit PebbleKit JS). Nur, um sie als Standard abzuloesen.
// 4368ffa4-f0fb-4823-90be-f754b076bdaa
#define TIMESTYLE_STORE_UUID_INIT \
  {0x43, 0x68, 0xff, 0xa4, 0xf0, 0xfb, 0x48, 0x23, 0x90, 0xbe, 0xf7, 0x54, 0xb0, 0x76, 0xbd, 0xaa}

const PebbleProcessMd *timestyle_get_app_info(void);
