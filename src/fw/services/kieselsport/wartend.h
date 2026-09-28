/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "plattform.h"

// Die Zusammenfassung, die auf das Telefon wartet.
//
// SIE LIEGT IM PERSIST, NICHT IM SPEICHER. Der Dienst schreibt sie beim
// Speichern, die App liest sie und schickt sie, und erst die Bestaetigung
// des Telefons loescht sie. Dazwischen darf die App geschlossen, die Uhr neu
// gestartet werden - das Training bleibt, bis es angekommen ist.
//
// Die Abschnittsliste (Saetze, Bahnen) gehoert dazu: ein Text von bis zu
// dreihundert Zeichen, in zwei Persist-Haelften, weil eine nur 256 traegt.

void wartend_merken(const Trainingsstand *t, const char *liste);
// true, wenn eine da ist; fuellt dann Stand und Liste.
bool wartend_laden(Trainingsstand *t, char *liste, size_t platz);
void wartend_vergessen(void);
