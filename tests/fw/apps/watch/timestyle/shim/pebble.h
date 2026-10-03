/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder: steht im Host-Test fuer das <pebble.h> des Zifferblatts
// (src/fw/apps/watch/timestyle/pebble.h). Das echte zieht die ganze App-
// Umgebung samt Ressourcen nach; settings.h und sidebar_widgets.h brauchen
// nur die Typen.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "applib/graphics/gtypes.h"
