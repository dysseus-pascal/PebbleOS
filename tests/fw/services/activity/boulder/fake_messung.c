/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2: Attrappe der Messung (messstufe.c, puls.c) fuer Tests, die den Dauerteil fuer
// Schlaf und Zyklen fahren. Die Messung selbst prueft test_boulder_messung gegen den echten
// HRM-Manager; hier wuerde sie nur Sensor und BlobDB ins Spiel bringen.

#include "pbl/services/activity/boulder/messstufe.h"
#include "pbl/services/activity/boulder/puls.h"

void boulder_messstufe_minute(void) {
}

void boulder_puls_minute(time_t jetzt_utc, bool nachtruhe) {
  (void)jetzt_utc;
  (void)nachtruhe;
}

void boulder_puls_tracking_aus(void) {
}
