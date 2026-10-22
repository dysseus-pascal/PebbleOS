/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Nur mit der Variante: fw/apps/system/ baut jedes Board (siehe kieselsport.c).
#ifdef CONFIG_BOULDER_KIESELSPORT

#include "einstellungen.h"
#include "schluessel.h"

// Die App zaehlt die Schluessel des Rhythmus von ATEM_EIN an ab (AtemWert).
_Static_assert(MESSAGE_KEY_ATEM_HALTEN == MESSAGE_KEY_ATEM_EIN + AtemHalten &&
                   MESSAGE_KEY_ATEM_AUS == MESSAGE_KEY_ATEM_EIN + AtemAus &&
                   MESSAGE_KEY_ATEM_LEER == MESSAGE_KEY_ATEM_EIN + AtemLeer &&
                   MESSAGE_KEY_ATEM_RUNDEN == MESSAGE_KEY_ATEM_EIN + AtemRunden,
               "Schluessel des Rhythmus in der Folge von AtemWert");

void einstellungen_maxpuls(int32_t schlaege) {
  if (schlaege <= 100 || schlaege >= 240) return;
  kieselsport_maxpuls_alt((uint16_t)schlaege);
}

void einstellungen_maxpuls_vermerkt(bool gesetzt, int32_t schlaege) {
  if (!gesetzt) {
    kieselsport_maxpuls_nicht_setzen();
    return;
  }
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
  // 0 ist kein Pin, sonst Art + 1 (der Launch-Code, kieselsport.c): mit jeder Art eine mehr.
  if (art < 0 || art > ArtAnzahl) return;
  // NUR WAS SICH AENDERT, KOMMT IN DEN FLASH: Boulder schickt beim Speichern
  // alle Einstellungen mit, auch die gleich gebliebenen.
  if (persist_exists(PERSIST_PIN_ART) && persist_read_int(PERSIST_PIN_ART) == art) return;
  persist_write_int(PERSIST_PIN_ART, art);
}

void einstellungen_atem(AtemWert welcher, int32_t wert) {
  // Die Grenzen prueft der Dienst: dieselben gelten fuer das, was er aus dem Persist liest.
  kieselsport_atem_setze(welcher, wert);
}

void einstellungen_pin_zeit(const char *hhmm) {
  if (!hhmm) return;
  // Nur "H:MM" oder "HH:MM" - was sonst kommt, ist keine Uhrzeit.
  const size_t n = strlen(hhmm);
  if (n < 4 || n > 5 || hhmm[n - 3] != ':') return;
  char bisher[8] = "";
  if (persist_exists(PERSIST_PIN_ZEIT)) persist_read_string(PERSIST_PIN_ZEIT, bisher, sizeof(bisher));
  if (strcmp(bisher, hhmm) == 0) return;
  persist_write_string(PERSIST_PIN_ZEIT, hhmm);
}

void einstellungen_melden(DictionaryIterator *out) {
  // Der Wert, mit dem die Uhr rechnet - auch nach Alter, damit ein Telefon
  // ohne den Vermerk dieselben Zonen sieht.
  dict_write_int32(out, MESSAGE_KEY_MAXPULS, kieselsport_maxpuls());
  dict_write_int32(out, MESSAGE_KEY_MAXPULS_GESETZT, kieselsport_maxpuls_gesetzt() ? 1 : 0);
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
  // HINTEN ANGEHAENGT, damit alles davor bytegleich zu b25 bleibt. Mit dem
  // Rhythmus rechnet das Telefon die Runden einer Atemuebung aus ihrer Dauer.
  Atemrhythmus atem;
  kieselsport_atem(&atem);
  for (int i = 0; i < AtemWerte; i++) {
    dict_write_int32(out, MESSAGE_KEY_ATEM_EIN + i, atem.wert[i]);
  }
}

#endif  // CONFIG_BOULDER_KIESELSPORT
