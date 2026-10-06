/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2: Attrappe der Messung (messstufe.c, puls.c) fuer Tests, die den Dauerteil fuer
// Schlaf und Zyklen fahren. Die Messung selbst prueft test_boulder_messung gegen den echten
// HRM-Manager; hier wuerde sie nur Sensor und BlobDB ins Spiel bringen.

#include "pbl/services/activity/boulder/messstufe.h"
#include "pbl/services/activity/boulder/puls.h"

void boulder_messstufe_minute(void) {
}

// Die Stufe der Referenznaechte (AP-G: 3 am 26.9., sonst 4); der Test setzt sie.
uint8_t g_fake_messstufe = 4;
uint8_t boulder_messstufe(void) {
  return g_fake_messstufe;
}

void boulder_messstufe_nachlesen(void) {
}

void boulder_puls_minute(time_t jetzt_utc, bool nachtruhe) {
  (void)jetzt_utc;
  (void)nachtruhe;
}

void boulder_puls_tracking_aus(void) {
}
