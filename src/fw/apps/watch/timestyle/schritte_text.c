/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "schritte_text.h"

#include <stdio.h>

void schritte_text(char *puffer, size_t groesse, int32_t schritte, char trenner) {
  if (schritte < 0) {
    schritte = 0;
  }
  const int steps = (int)schritte;
  if (steps < 1000) {
    snprintf(puffer, groesse, "%i", steps);
  } else if (steps < 10000) {
    snprintf(puffer, groesse, "%i%c%ik", steps / 1000, trenner, steps / 100 % 10);
  } else {
    snprintf(puffer, groesse, "%ik", steps / 1000);
  }
}
