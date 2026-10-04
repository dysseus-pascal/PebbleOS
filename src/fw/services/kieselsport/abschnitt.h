/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "plattform.h"

// Die Abschnitte eines Trainings: ein Satz beim Krafttraining, eine Bahn
// beim Schwimmen.
//
// EINE LISTE FÜR ZWEIERLEI, weil es dasselbe ist: ein Stück Training, das
// anfängt, eine Weile dauert und eine Anzahl hat. Beim Krafttraining sind das
// die Wiederholungen, beim Schwimmen ist es die eine Bahn. Auf dem Telefon
// wird daraus ein Satz beziehungsweise eine Bahn in der Gesundheitsakte —
// die kennt für beides ein Feld.
//
// ZWANZIG REICHEN. Mehr Sätze macht niemand, und mehr Bahnen passen nicht in
// eine Nachricht: der Postausgang der Uhr fasst ein paar hundert Zeichen.
// Was darüber hinausgeht, zählt weiter mit, steht aber nicht einzeln da.

#define KS_ABSCHNITT_MAX 20

typedef struct {
  // Die Sekunden in vier Byte: ein Training ueber 18 Stunden liefe in
  // zweien ueber. Der Text ans Telefon ist eine Zahl, der ist es gleich.
  uint32_t beginn_s;   //< Sekunden seit Trainingsbeginn
  uint16_t anzahl;     //< Wiederholungen (Kraft) oder 1 (Bahn)
  uint32_t dauer_s;
} Abschnitt;

void abschnitt_leeren(void);
void abschnitt_hinzu(uint32_t beginn_s, uint16_t anzahl, uint32_t dauer_s);

// Wie viele EINZELN dastehen (höchstens KS_ABSCHNITT_MAX).
int abschnitt_anzahl(void);
// Wie viele es insgesamt gab - auch die, die nicht mehr hineinpassten.
uint16_t abschnitt_gesamt(void);
// Alle Anzahlen zusammen, ebenfalls ungekürzt.
uint32_t abschnitt_summe(void);

// "beginn:anzahl:dauer;..." - so geht die Liste ans Telefon.
void abschnitt_als_text(char *aus, size_t platz);
