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
//
// Die Pausen und das Ende der aktiven Zeit liegen daneben, unter eigenen
// Schluesseln: der Stand selbst hat die Anordnung der Store-App und waechst
// nicht. Sie gelten nur mit dem Beginn des wartenden Stands; fehlen sie oder
// passt er nicht, stammt die Zusammenfassung von aelterer Firmware.

void wartend_merken(const Trainingsstand *t, const char *liste, const uint8_t *pausen,
                    size_t pausen_laenge, uint32_t ende);
// true, wenn eine da ist; fuellt dann Stand und Liste.
bool wartend_laden(Trainingsstand *t, char *liste, size_t platz);
// Die Pausen als Byte (Aufbau in kieselsport.h); 0, wenn keine da sind.
size_t wartend_pausen(uint8_t *aus, size_t platz);
// Das Ende der aktiven Zeit, 0 wenn unbekannt.
uint32_t wartend_ende(void);
// Nur, wenn die wartende noch zu [beginn] gehoert; 0 vergisst jede.
void wartend_vergessen(uint32_t beginn);
// Liegt eine da? Ohne sie zu lesen.
bool wartend_da(void);

// --- Der Stopp eines verworfenen Trainings (siehe kieselsport.h) ---

void stopp_merken(uint32_t beginn, uint32_t zeit, uint8_t art);
bool stopp_laden(uint32_t *beginn, uint32_t *zeit, uint8_t *art);
// Nur, wenn der gemerkte noch zu [beginn] gehoert; 0 vergisst jeden.
void stopp_vergessen(uint32_t beginn);
bool stopp_da(void);
