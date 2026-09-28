/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "einstellungen.h"
#include "schluessel.h"

void einstellungen_maxpuls(int32_t schlaege) {
  if (schlaege <= 100 || schlaege >= 240) return;
  kieselsport_maxpuls_setze((uint16_t)schlaege);
}

void einstellungen_becken(int32_t meter) {
  if (meter < 5 || meter > 100) return;
  kieselsport_becken_setze((uint16_t)meter);
}

void einstellungen_pausenziel(int32_t sekunden) {
  if (sekunden < 0 || sekunden > 3600) return;
  kieselsport_pausenziel_setze((uint16_t)sekunden);
}

void einstellungen_empfindlichkeit(int32_t stufe) {
  if (stufe < 1 || stufe > 3) return;
  kieselsport_empfindlichkeit_setze((uint8_t)stufe);
}

void einstellungen_pin_art(int32_t art) {
  if (art < 0 || art > 7) return;
  persist_write_int(PERSIST_PIN_ART, art);
}

void einstellungen_pin_zeit(const char *hhmm) {
  if (!hhmm) return;
  // Nur "H:MM" oder "HH:MM" - was sonst kommt, ist keine Uhrzeit.
  const size_t n = strlen(hhmm);
  if (n < 4 || n > 5 || hhmm[n - 3] != ':') return;
  persist_write_string(PERSIST_PIN_ZEIT, hhmm);
}

void einstellungen_melden(DictionaryIterator *out) {
  dict_write_int32(out, MESSAGE_KEY_MAXPULS, kieselsport_maxpuls());
  dict_write_int32(out, MESSAGE_KEY_BECKEN, kieselsport_becken());
  dict_write_int32(out, MESSAGE_KEY_PAUSENZIEL, kieselsport_pausenziel());
  dict_write_int32(out, MESSAGE_KEY_EMPFIND, kieselsport_empfindlichkeit());
  dict_write_int32(out, MESSAGE_KEY_PIN_ART,
                   persist_exists(PERSIST_PIN_ART) ? persist_read_int(PERSIST_PIN_ART) : 0);
  char zeit[8] = "18:00";
  if (persist_exists(PERSIST_PIN_ZEIT)) persist_read_string(PERSIST_PIN_ZEIT, zeit, sizeof(zeit));
  dict_write_cstring(out, MESSAGE_KEY_PIN_ZEIT, zeit);
  // DIE NACHT MISST JETZT DIE FIRMWARE (services/activity/boulder), nicht
  // mehr Kieselsport. Aus heisst fuer Kiesel-Helper: keine zweite Messung
  // daneben.
  dict_write_int32(out, MESSAGE_KEY_NACHT_AN, 0);
}
