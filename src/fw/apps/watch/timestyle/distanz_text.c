/* SPDX-FileCopyrightText: Dan Tilden (freakified) and the TimeStyle contributors */
/* SPDX-License-Identifier: MIT */
/* TimeStyle, https://github.com/freakified/TimeStylePebble (e2be725), LICENSE
 * in this folder. Boulder: the distance text of StepCounter_draw, moved here
 * unchanged (only the decimal separator in the km branch is Boulder's). */

#include "distanz_text.h"

#include <stdio.h>

bool distanz_text(char *puffer, size_t groesse, int32_t meter, bool metrisch, char trenner) {
  int distance = (int)meter;
  bool use_small_font = false;

  if (metrisch) {
    if (distance < 100) {
      snprintf(puffer, groesse, "%im", distance);
    } else if (distance < 1000) {
      distance /= 100; // convert to tenths of km
      // Boulder: the chosen decimal separator, like the miles branch below
      // (upstream always wrote ".").
      snprintf(puffer, groesse, "%c%ikm", trenner, distance);
    } else {
      distance /= 1000; // convert to km

      if (distance > 9) {
        use_small_font = true;
      }

      snprintf(puffer, groesse, "%ikm", distance);
    }
  } else {
    int miles_tenths = distance * 10 / 1609 % 10;
    // Boulder: integer rounding instead of roundf.
    int miles_whole = (distance + 1609 / 2) / 1609;

    if (miles_whole > 0) {
      snprintf(puffer, groesse, "%imi", miles_whole);
    } else {
      snprintf(puffer, groesse, "%c%imi", trenner, miles_tenths);
    }
  }
  return use_small_font;
}
