/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Die Variante, wie Kieselsport und kalorien.c sie sehen - ohne die ganze Variante:
// - der Ruhepuls fuer Kalorien und Maximalpuls (V3: Median der letzten sieben Naechte, nacht.c),
//   den die Tests einstellen;
// - der Merker "Training ohne Schritte" (V1, D17): aufgezeichnet, mit dem, was beim Aufruf galt.
//   Seine Wirkung prueft test_boulder_fahrt an der echten Variante.

#include <stdbool.h>
#include <stdint.h>

static uint8_t s_ruhepuls;

uint8_t boulder_nacht_ruhepuls_median(void) {
  return s_ruhepuls;
}

void fake_ruhepuls_setzen(uint8_t bpm) {
  s_ruhepuls = bpm;
}

// Was der Test beim Aufruf festhalten will (Sperre, Task, Pebbles Training), fragt er ueber
// diesen Rueckruf ab.
int g_fake_ohne_schritte_aufrufe;
bool g_fake_ohne_schritte;
void (*g_fake_ohne_schritte_beim_aufruf)(bool ohne);

void boulder_variante_training_ohne_schritte(bool ohne) {
  g_fake_ohne_schritte_aufrufe++;
  g_fake_ohne_schritte = ohne;
  if (g_fake_ohne_schritte_beim_aufruf) {
    g_fake_ohne_schritte_beim_aufruf(ohne);
  }
}
