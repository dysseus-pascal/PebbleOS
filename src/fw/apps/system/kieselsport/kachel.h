/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Was die Kachel im Startmenue (launcher/default/app_glance_kieselsport.c)
// aus der App braucht.
//
// OHNE pebble_compat.h: dessen Makros (malloc, time, layer_get_bounds ...)
// schrieben den Startmenue-Code um. Frueher deklarierte die Kachel die
// beiden deshalb selbst - und eine geaenderte Unterschrift hier fiel dem
// Compiler dort nicht auf. Jetzt binden App und Kachel denselben Kopf ein.

#include "pbl/services/kieselsport/kieselsport.h"

// Der Name einer Sportart in der Sprache der Uhr, auch fuer eine Art
// ausserhalb des Bereichs (dann "?"). art_text.c
const char *art_name(Sportart art);

// Sprache neu von der Uhr lesen. i18n_get_system_locale() ist ein Syscall und
// es gibt kein Ereignis fuer einen Sprachwechsel - also einmal beim Start.
// texte.c
void strings_refresh(void);
