/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

// Die Zustandsmeldungen ans Telefon (Start, Pause, Weiter, Stopp), der Reihe
// nach.
//
// EINE SCHLANGE STATT EINES PLATZES. Frueher ersetzte eine neue Meldung die
// offene: ein schnelles Pause-Weiter verdraengte die Pause, und das Telefon
// sah ein Weiter ohne Pause. Jetzt geht jede, in ihrer Reihenfolge, mit der
// Uhrzeit, zu der sie geschah - nicht der, zu der sie ankommt.
//
// NICHT ENDLOS. Jede Meldung bekommt ein paar Anlaeufe; die Zusammenfassung
// am Ende traegt die Pausen ohnehin noch einmal, und sie fasst nach, bis sie
// da ist. Ist die Schlange voll, faellt die aelteste weg, die nicht gerade
// unterwegs ist.
//
// OHNE SDK, damit sich die Reihenfolge auf dem Rechner pruefen laesst.

#define MELDUNGEN_MAX 8

typedef struct {
  uint8_t was;         //< Trainingsmeldung (telefon.h)
  uint8_t art;
  uint32_t beginn;
  uint32_t zeit;       //< wann es geschah, Unix-Sekunden
  bool aus_persist;    //< der gemerkte Stopp: nach der Bestaetigung dort vergessen
} Meldung;

typedef struct {
  Meldung platz[MELDUNGEN_MAX];
  uint8_t kopf;
  uint8_t anzahl;
  uint8_t anlaeufe;    //< der vorderen Meldung
  bool unterwegs;      //< die vordere ist gerade beim Telefon unterwegs
} Meldungen;

void meldungen_leeren(Meldungen *m);
bool meldungen_leer(const Meldungen *m);
uint8_t meldungen_anzahl(const Meldungen *m);

// Hinten anstellen.
void meldungen_anstellen(Meldungen *m, Meldung neu);
// Steht ein gemerkter Stopp fuer [beginn] schon in der Schlange?
bool meldungen_hat_stopp(const Meldungen *m, uint32_t beginn);
// Steht eine Meldung fuer dieses Training an? Die Zusammenfassung wartet
// auf sie - sonst kaeme ein Weiter nach dem Ende an.
bool meldungen_fuer(const Meldungen *m, uint32_t beginn);

// Die vordere Meldung, NULL wenn keine da ist.
const Meldung *meldungen_vorn(const Meldungen *m);
// Ein Anlauf fuer die vordere. false: sie hatte [hoechstens] und faellt weg -
// dann ist die naechste vorn.
bool meldungen_anlauf(Meldungen *m, uint8_t hoechstens);
// Die vordere ist angekommen (oder aufgegeben): weg damit.
void meldungen_vorn_weg(Meldungen *m);
