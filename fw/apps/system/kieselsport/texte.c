/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Nur mit der Variante: fw/apps/system/ baut jedes Board (siehe kieselsport.c).
#ifdef CONFIG_BOULDER_KIESELSPORT

#include "pebble_compat.h"
#include "texte.h"

// Tabelle aus strings_table.h. Sie liegt im App-Abbild und zaehlt damit zum
// Speicherabdruck, belegt aber keinen Heap.
static const char *const s_table[STR_COUNT][STRINGS_LANG_COUNT] = {
#define STR(id, maxbytes, en, de, fr, it, es) { en, de, fr, it, es },
#include "texte_tabelle.h"
#undef STR
};

static StringLang s_lang = STRINGS_EN;

// Der Locale-String ist "de_DE", "fr_FR", "en_US" ... Ein Sprachpaket darf
// aber auch nur den Zwei-Buchstaben-Code liefern. Deshalb wird NIE auf
// "de_DE" verglichen, sondern auf die ersten zwei Zeichen - so wird auch eine
// Schweizer Uhr (de_CH, fr_CH, it_CH) richtig erkannt.
static StringLang prv_pick_language(const char *locale) {
  if (!locale) return STRINGS_EN;
  if (strncmp(locale, "de", 2) == 0) return STRINGS_DE;
  if (strncmp(locale, "fr", 2) == 0) return STRINGS_FR;
  if (strncmp(locale, "it", 2) == 0) return STRINGS_IT;
  if (strncmp(locale, "es", 2) == 0) return STRINGS_ES;
  return STRINGS_EN;
}

void strings_refresh(void) {
  s_lang = prv_pick_language(i18n_get_system_locale());
}

const char *S(StringId id) {
  // Kein Test auf id < 0: der Aufzaehlungstyp ist vorzeichenlos, der Vergleich
  // waere immer falsch und -Werror=type-limits bricht daran ab.
  if (id >= STR_COUNT) return "";
  const char *text = s_table[id][s_lang];
  if (!text) text = s_table[id][STRINGS_EN];
  return text ? text : "";
}

#endif  // CONFIG_BOULDER_KIESELSPORT
