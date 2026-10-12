/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#ifdef CONFIG_BOULDER_HEALTH_APP

#include "texte.h"

// static const char *const: Tabelle und Texte liegen im Flash, nicht in .kernel_bss, das die App
// ohne Systemrechte nicht schreiben darf.
static const char *const s_texte[BhTAnzahl] = {
#define TEXT(schluessel, englisch) [schluessel] = englisch,
#include "texte_tabelle.h"
#undef TEXT
};

const char *bh_text(BhText text) {
  return (text < BhTAnzahl) ? s_texte[text] : "";
}

#endif // CONFIG_BOULDER_HEALTH_APP
