/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Die Texte der Health-App aus texte_tabelle.h (Spezifikation Schritt 3, 2.5).

typedef enum {
#define TEXT(schluessel, englisch) schluessel,
#include "texte_tabelle.h"
#undef TEXT
  BhTAnzahl,
} BhText;

// Nie NULL; ein unbekannter Schluessel liefert "".
const char *bh_text(BhText text);
