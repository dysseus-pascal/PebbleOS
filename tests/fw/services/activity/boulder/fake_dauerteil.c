/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_dauerteil.h"

#include <string.h>

FakeDauerteil fake_dauerteil;

void fake_dauerteil_zuruecksetzen(void) {
  memset(&fake_dauerteil, 0, sizeof(fake_dauerteil));
}

void boulder_dauerteil_minute(const BoulderMinute *minute) {
  if (fake_dauerteil.minuten_anzahl < FAKE_DAUERTEIL_MAX) {
    fake_dauerteil.minuten[fake_dauerteil.minuten_anzahl++] = *minute;
  }
}

bool boulder_dauerteil_ring_fenster(time_t *von_utc, time_t *bis_utc) {
  *von_utc = fake_dauerteil.ring_von;
  *bis_utc = fake_dauerteil.ring_bis;
  return fake_dauerteil.ring_an;
}

void boulder_dauerteil_ring_minute(const BoulderMinute *minute) {
  if (fake_dauerteil.ring_anzahl < FAKE_DAUERTEIL_MAX) {
    fake_dauerteil.ring[fake_dauerteil.ring_anzahl++] = *minute;
  }
}

bool boulder_dauerteil_training_ohne_schritte(void) {
  return fake_dauerteil.training_ohne_schritte;
}

time_t boulder_dauerteil_letzter_schlaf_utc(void) {
  return 4242;
}

void boulder_dauerteil_tracking_aus(void) {
  fake_dauerteil.tracking_aus++;
}

void boulder_dauerteil_training(bool laeuft) {
  fake_dauerteil.training = laeuft;
}

