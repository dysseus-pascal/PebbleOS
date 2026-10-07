/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "plattform.h"

// Bahnen zählen beim Schwimmen - mit dem Magnetometer.
//
// DIE IDEE: ein Becken hat eine Achse. Man schwimmt sie hinunter und wieder
// hinauf, und bei jeder Wende dreht sich die Blickrichtung um 180 Grad. Wer
// die Richtungswechsel zählt, zählt Bahnen — ganz ohne GPS, das unter Wasser
// ohnehin nichts empfängt.
//
// WARUM NICHT DER BESCHLEUNIGUNGSMESSER: der sieht Züge, keine Wenden, und
// eine Wende ist beim Kraulen kaum von einem kräftigen Zug zu unterscheiden.
// Die Richtung dagegen ist eindeutig - sie ist entweder die eine oder die
// andere.
//
// WAS DARAN WACKELIG IST, und es steht auch so im README:
// * Der Kompass muss kalibriert sein. Beim Losschwimmen ist er das oft nicht;
//   die App sagt es dann, statt zu zählen, als wüsste sie etwas.
// * Ein Handgelenk dreht sich beim Kraulen mit jedem Zug. Deshalb wird über
//   mehrere Sekunden gemittelt - eine Bahn dauert zwanzig, ein Zug eine.
// * In Hallen mit viel Stahl zeigt ein Magnetometer irgendwohin. Es muss
//   nicht nach Norden zeigen: gezählt wird der WECHSEL zwischen zwei
//   Richtungen, nicht die Richtung selbst.
//
// DIE BECKENLÄNGE MACHT ERST EINE STRECKE DARAUS. Sie steht in den
// Einstellungen, weil die Uhr sie nicht wissen kann - 25 Meter sind üblich,
// 50 kommen vor, und in einem Hotelbecken ist alles möglich.

// Die Beckenlaenge aus dem Persist.
void bahnen_laden(void);
void bahnen_start(void);
void bahnen_stoppe(uint32_t sekunde);
void bahnen_pause(bool an);

// Je Sekunde aufzurufen, mit der Sekunde seit Trainingsbeginn.
void bahnen_tick(uint32_t sekunde);

uint16_t bahnen_anzahl(void);
uint32_t bahnen_meter(void);

// Weiss der Kompass, wo er ist? Solange nicht, wird nicht gezählt.
bool bahnen_bereit(void);

void bahnen_becken_setze(uint16_t meter);
uint16_t bahnen_becken(void);

// Was der Kompass meldet (plattform.c, unter der Sperre): die Richtung in
// TRIG_MAX_ANGLE und ob er weiss, wo er ist.
void bahnen_kompass(int32_t richtung, bool bereit);
