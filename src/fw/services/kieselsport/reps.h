/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "plattform.h"

// Wiederholungen und Pausen beim Krafttraining - aus dem Beschleunigungsmesser.
//
// WAS DIE UHR HIER WIRKLICH SIEHT: eine Hand, die sich regelmässig auf und ab
// bewegt. Daraus Wiederholungen zu zählen, ist eine SCHÄTZUNG und keine
// Messung. Bei Curls, Bankdrücken und Schulterdrücken bewegt sich das
// Handgelenk mit jeder Wiederholung; bei Kniebeugen mit der Stange im Nacken
// bewegt es sich kaum, und dann zählt die Uhr zu wenig oder nichts. Das steht
// auch so im README — eine Zahl, die man für gemessen hält, ist schlimmer als
// keine.
//
// DIE PAUSE IST DIE EHRLICHERE ZAHL. Ob zwischen zwei Sätzen 60 oder 180
// Sekunden liegen, entscheidet über das, was der Satz bewirkt - und dafür
// muss die Uhr nur merken, dass sich nichts mehr bewegt. Das kann sie
// zuverlässig.
//
// SATZ UND PAUSE OHNE KNOPFDRUCK. Wer eine Hantel hält, drückt keine Taste.
// Der Satz ist zu Ende, wenn die Bewegung aufhört, und der nächste beginnt,
// wenn sie wieder anfängt.

// Die Einstellungen aus dem Persist - beim Start des Dienstes und jedes Satzes.
void reps_laden(void);
void reps_start(void);
void reps_stoppe(uint16_t sekunde);   //< schliesst den offenen Satz ab
void reps_pause(bool an);

// Je Sekunde aufzurufen. Uebergibt die Sekunde seit Trainingsbeginn.
void reps_tick(uint16_t sekunde);

uint16_t reps_laufend(void);   //< Wiederholungen im laufenden Satz
bool reps_ruht(void);          //< gerade Pause zwischen zwei Saetzen?
uint16_t reps_ruhe_s(void);    //< wie lange die Pause schon dauert
uint16_t reps_saetze(void);    //< abgeschlossene Saetze
// Ist die Pause eben um gewesen? Einmal true, dann wieder false - der
// Dienst brummt dann.
bool reps_brumm_holen(void);

// Wie empfindlich gezaehlt wird: 1 traege, 2 normal, 3 fein.
//
// SIE STEHT IN DEN EINSTELLUNGEN, weil niemand die richtige Schwelle kennt.
// Sie haengt an der Uebung, am Gewicht und daran, wie jemand sich bewegt;
// eine fest eingebaute Zahl waere geraten und nicht nachstellbar.
void reps_empfindlichkeit_setze(uint8_t stufe);
uint8_t reps_empfindlichkeit(void);

// Nach wie vielen Sekunden Pause die Uhr brummt (0 = nie).
void reps_pausenziel_setze(uint16_t sekunden);
uint16_t reps_pausenziel(void);

// Was der Beschleunigungsmesser liefert (plattform.c, unter der Sperre).
void reps_daten(const KsAccel *daten, uint32_t anzahl);
