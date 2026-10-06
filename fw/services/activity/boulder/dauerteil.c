/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2: der Dauerteil an der Naht zur Kraepelin-Kopie (variante.h), Stand AP1.
//
// STAND AP1 - bewusst neutral, damit die Kopie schon jetzt wie das Original zaehlt:
// keine Fahrt (AP6: kontext.c, nachfrage.c), kein Ring nach dem Boot (AP4: speicher.c),
// kein Abo "Aktiv" (AP6), Schlaf "verarbeitet bis zur letzten Minute", denn Kraepelins
// Schlafsitzungen verwirft die Kopie (D1) und die Zyklen der Variante kommen mit AP3.
// Spaetere Pakete ersetzen die Rumpfe hier; die Naht bleibt.

#include "pbl/services/activity/boulder/variante.h"

static time_t s_letzte_minute_utc;

void boulder_dauerteil_minute(const BoulderMinute *minute) {
  // Bis AP3/AP4 die Minute in die Nachtfelder und den Live-Automaten geben, zaehlt nur:
  // bis hier ist verarbeitet.
  s_letzte_minute_utc = minute->utc;
}

bool boulder_dauerteil_ring_fenster(time_t *von_utc, time_t *bis_utc) {
  (void)von_utc;
  (void)bis_utc;
  return false;
}

void boulder_dauerteil_ring_minute(const BoulderMinute *minute) {
  (void)minute;
}

time_t boulder_dauerteil_fahrt_offen_seit(void) {
  return 0;
}

bool boulder_dauerteil_fahrt_minute(time_t minute_utc) {
  (void)minute_utc;
  return false;
}

bool boulder_dauerteil_fahrt_laeuft(void) {
  return false;
}

bool boulder_dauerteil_fahrt_ueberdeckt(time_t start_utc, uint32_t laenge_s) {
  (void)start_utc;
  (void)laenge_s;
  return false;
}

time_t boulder_dauerteil_letzter_schlaf_utc(void) {
  // Ende der letzten uebergebenen Minute. Ohne Schlafsitzungen in der Liste rechnet
  // activity_sessions.c damit nur "jetzt wach" (activity_sessions.c:640).
  return s_letzte_minute_utc ? s_letzte_minute_utc + 60 : 0;
}

bool boulder_dauerteil_aktiv_abo_laeuft(void) {
  return false;
}

void boulder_dauerteil_aktiv_abo_pausieren(bool pausiert) {
  (void)pausiert;
}

void boulder_dauerteil_tracking_aus(void) {
}
