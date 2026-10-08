/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "pebble_compat.h"
#include "kachel.h"

// Texte der Oberflaeche. Die Uhr gibt die Sprache vor
// (Settings -> Display -> Language); die App folgt ihr, es gibt keinen eigenen
// Sprachschalter. Alle Texte stehen in texte_tabelle.h, eine Zeile je Text.
// Dasselbe Muster wie in den Schwester-Apps (Drinktervall).
//
// Rueckfall ist ENGLISCH: die Uhr kennt weitere eingebaute Sprachen, fuer die
// wir keine Spalte haben (Nederlands, Portugues, Polski ...). Eine deutsche
// Oberflaeche auf einer polnischen Uhr waere schlechter als eine englische.
//
// NUR DIE APP. Der Dienst (services/activity/boulder/sport) zeigt nichts an.

typedef enum {
  STRINGS_EN = 0,   //< Spalte 0, zugleich der Rueckfall
  STRINGS_DE,
  STRINGS_FR,
  STRINGS_IT,
  STRINGS_ES,
  STRINGS_LANG_COUNT,
} StringLang;

typedef enum {
#define STR(id, maxbytes, en, de, fr, it, es) id,
#include "texte_tabelle.h"
#undef STR
  STR_COUNT,
} StringId;

// Liefert den Text in der aktuellen Sprache. Nie NULL: ein unbekannter
// Schluessel und eine fehlende Spalte fallen auf Englisch zurueck, eine
// fehlende englische Spalte auf den leeren String.
const char *S(StringId id);

// strings_refresh() steht in kachel.h - die Kachel im Startmenue ruft ihn auch.
