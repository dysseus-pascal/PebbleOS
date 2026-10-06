/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2: Attrappe der Nachfrage fuer Tests, die den Dauerteil fuer Schlaf, Zyklen und
// Wecker fahren. Dort entsteht hoechstens eine Kandidatin; gefragt wird sie nie beantwortet
// (= "Nein", nichts gestrichen). Den echten Dialog prueft test_boulder_fahrt.

#include "pbl/services/activity/boulder/kontext.h"

int g_fake_nachfragen;

void boulder_nachfrage_stellen(BoulderFahrtArt art, uint8_t nr) {
  (void)art;
  (void)nr;
  g_fake_nachfragen++;
}

// Ohne Pebbles Prefs: kein Ruhepuls eingestellt (die Variante nimmt dann 60).
uint8_t activity_prefs_heart_get_resting_hr(void) {
  return 0;
}
