/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder: das <pebble.h> des Zifferblatts fuer test_timestyle_ziffern. Es ist
// das echte (src/fw/apps/watch/timestyle/pebble.h); nur die Kennungen der
// TimeStyle-Ressourcen fehlen in den Ressourcen des Host-Tests und stehen
// darum hier. Der Test laedt die Schriften selbst aus resources/.
#pragma once

#define RESOURCE_ID_TIMESTYLE_LECO_REGULAR_FFONT 0x7501
#define RESOURCE_ID_TIMESTYLE_SANS_REGULAR_FFONT 0x7502
#define RESOURCE_ID_TIMESTYLE_SANS_BOLD_FFONT    0x7503
#define RESOURCE_ID_TIMESTYLE_WEATHER_GENERIC             0
#define RESOURCE_ID_TIMESTYLE_WEATHER_THUNDERSTORM        0
#define RESOURCE_ID_TIMESTYLE_WEATHER_RAINING_AND_SNOWING 0
#define RESOURCE_ID_TIMESTYLE_WEATHER_PARTLY_CLOUDY_NIGHT 0
#define RESOURCE_ID_TIMESTYLE_WEATHER_PARTLY_CLOUDY       0
#define RESOURCE_ID_TIMESTYLE_WEATHER_LIGHT_SNOW          0
#define RESOURCE_ID_TIMESTYLE_WEATHER_LIGHT_RAIN          0
#define RESOURCE_ID_TIMESTYLE_WEATHER_HEAVY_SNOW          0
#define RESOURCE_ID_TIMESTYLE_WEATHER_HEAVY_RAIN          0
#define RESOURCE_ID_TIMESTYLE_WEATHER_CLOUDY              0
#define RESOURCE_ID_TIMESTYLE_WEATHER_CLEAR_NIGHT         0
#define RESOURCE_ID_TIMESTYLE_WEATHER_CLEAR_DAY           0
#define RESOURCE_ID_TIMESTYLE_DISCONNECTED   0
#define RESOURCE_ID_TIMESTYLE_DATE_BG_LG     0
#define RESOURCE_ID_TIMESTYLE_BATTERY_CHARGE 0
#define RESOURCE_ID_TIMESTYLE_BATTERY_BG     0
#define RESOURCE_ID_TIMESTYLE_HEALTH_SLEEP   0
#define RESOURCE_ID_TIMESTYLE_HEALTH_STEPS   0
#define RESOURCE_ID_TIMESTYLE_HEALTH_HEART   0

#include "apps/watch/timestyle/pebble.h"
