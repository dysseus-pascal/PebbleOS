/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "pebble_compat.h"

// Texte der Oberflaeche. Die Uhr gibt die Sprache vor
// (Settings -> Display -> Language); die App folgt ihr, es gibt keinen eigenen
// Sprachschalter. Alle Texte stehen in texte_tabelle.h, eine Zeile je Text.
// Dasselbe Muster wie in Kieselsport (apps/system/kieselsport/texte.c).
//
// EIGENE NAMEN (ST, strasse_texte_lesen, StrasseText...): Kieselsport liegt im
// selben Firmware-Abbild und hat schon S() und strings_refresh(). Zwei
// gleichnamige Funktionen lassen den Linker abbrechen - oder, schlimmer, eine
// App liest die Tabelle der anderen.
//
// Rueckfall ist ENGLISCH: fuer eine Sprache ohne eigene Spalte (Nederlands,
// Portugues, Polski ...) ist eine englische Oberflaeche besser als eine
// deutsche.
//
// NICHT HIER: Strassennamen und alles andere, was vom Telefon kommt. Das
// liefert OsmAnd schon in der Sprache des Telefons, und die Uhr gibt es
// unveraendert weiter.

typedef enum {
  StrasseSpracheEN = 0,   //< Spalte 0, zugleich der Rueckfall
  StrasseSpracheDE,
  StrasseSpracheFR,
  StrasseSpracheIT,
  StrasseSpracheES,
  StrasseSprachen,
} StrasseSprache;

typedef enum {
#define STR(id, maxbytes, en, de, fr, it, es) id,
#include "texte_tabelle.h"
#undef STR
  StrasseTexte,
} StrasseText;

// Liefert den Text in der aktuellen Sprache. Nie NULL: ein unbekannter
// Schluessel und eine fehlende Spalte fallen auf Englisch zurueck, eine
// fehlende englische Spalte auf den leeren String.
const char *ST(StrasseText id);

// Sprache neu von der Uhr lesen. i18n_get_system_locale() ist ein Syscall und
// es gibt kein Ereignis fuer einen Sprachwechsel - also einmal beim Start.
// Setzt zugleich den Zustand dieser Datei zurueck (siehe kieselstrasse.c).
void strasse_texte_lesen(void);
