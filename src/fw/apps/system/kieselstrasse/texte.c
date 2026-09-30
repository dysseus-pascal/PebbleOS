/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pebble_compat.h"
#include "texte.h"

// Tabelle aus texte_tabelle.h. Sie liegt im Firmware-Abbild und belegt
// keinen Heap.
static const char *const s_tabelle[StrasseTexte][StrasseSprachen] = {
#define STR(id, maxbytes, en, de, fr, it, es) { en, de, fr, it, es },
#include "texte_tabelle.h"
#undef STR
};

static StrasseSprache s_sprache = StrasseSpracheEN;

// Der Locale-String ist "de_DE", "fr_FR", "en_US" ... Ein Sprachpaket darf
// aber auch nur den Zwei-Buchstaben-Code liefern. Deshalb wird NIE auf "de_DE"
// verglichen, sondern auf die ersten zwei Zeichen - "de_CH" und "fr_CH" sind
// damit gleich mit erledigt.
static StrasseSprache prv_sprache_waehlen(const char *locale) {
  if (!locale) return StrasseSpracheEN;
  if (strncmp(locale, "de", 2) == 0) return StrasseSpracheDE;
  if (strncmp(locale, "fr", 2) == 0) return StrasseSpracheFR;
  if (strncmp(locale, "it", 2) == 0) return StrasseSpracheIT;
  if (strncmp(locale, "es", 2) == 0) return StrasseSpracheES;
  return StrasseSpracheEN;
}

void strasse_texte_lesen(void) {
  s_sprache = prv_sprache_waehlen(i18n_get_system_locale());
}

const char *ST(StrasseText id) {
  // Kein Test auf id < 0: der Aufzaehlungstyp ist vorzeichenlos, der Vergleich
  // waere immer falsch und -Werror=type-limits bricht daran ab.
  if (id >= StrasseTexte) return "";
  const char *text = s_tabelle[id][s_sprache];
  if (!text) text = s_tabelle[id][StrasseSpracheEN];
  return text ? text : "";
}
