/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "plattform.h"

// Die Pausen eines Trainings - fuer das Telefon, mit der Zusammenfassung.
//
// DIE UHR ZAEHLT IN AKTIVER ZEIT: Pulskurve, Saetze und Bahnen stehen bei
// "Sekunde 1800 des Trainings", ohne die Pausen. Das Telefon braucht aber
// die Uhrzeit. Frueher nahm es dafuer die Ankunft der Meldungen Pause und
// Weiter - die kamen Sekunden spaet, und ging eine verloren, verwarf es
// alle Pausen. Hier steht, bei welcher aktiven Sekunde eine Pause begann und
// wie lange sie dauerte; daraus rechnet das Telefon jede Sekunde genau um.
//
// NUR ABGESCHLOSSENE PAUSEN. Die letzte, aus der gespeichert wird, hat kein
// Weiter; wo sie begann, sagt pausen_offen_seit() - das Ende der aktiven
// Zeit (MESSAGE_KEY_ENDE in der App).
//
// WAS NICHT IN ZWEI BYTE PASST, ZAEHLT NUR MIT. Eine Pause nach ueber 18
// Stunden Training oder eine laenger als 18 Stunden steht nicht in der
// Liste, wohl aber in der Gesamtzahl - das Telefon sieht dann, dass die
// Liste unvollstaendig ist, und rechnet wie frueher.

void pausen_leeren(void);
// Die Pause beginnt: bei dieser aktiven Sekunde, zu dieser Uhrzeit.
void pausen_beginn(uint32_t aktiv_s, time_t jetzt);
// Weiter: die offene Pause ist zu Ende.
void pausen_ende(time_t jetzt);
// Seit wann die offene Pause laeuft, 0 ohne offene Pause.
time_t pausen_offen_seit(void);

// So viele Pausen stehen einzeln da (hoechstens KS_PAUSEN_MAX).
int pausen_anzahl(void);
// Alle abgeschlossenen Pausen, auch die, die nicht in der Liste stehen.
uint16_t pausen_gesamt(void);

// Die Liste, wie sie ans Telefon geht (Aufbau in kieselsport.h). Rueckgabe:
// die Laenge in Byte, 0 wenn [platz] nicht reicht.
size_t pausen_als_bytes(uint8_t *aus, size_t platz);
