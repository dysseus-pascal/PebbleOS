/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "pebble_compat.h"

// Der Draht zum Telefon.
//
// ZWEI RICHTUNGEN, ZWEI ZWECKE. Vom Telefon kommt die Einstellung
// (Maximalpuls, Becken, Pause, Empfindlichkeit). Zum Telefon hinaus gehen
// die Zustandsmeldungen (Boulder zeichnet danach die Strecke auf) und die
// Zusammenfassung eines Trainings. Empfaenger ist Boulders eingebauter
// Begleiter fuer diese UUID (KieselsportBegleiter): er erkennt die Nachricht
// an ihren Schluesseln, darum bleiben sie bytegleich zu b25.
//
// NUR DIE APP REDET MIT DEM TELEFON. Der Dienst legt die Zusammenfassung
// in den Persist, und telefon_nachsenden() holt sie von dort.

void telefon_init(void);

// Die wartende Zusammenfassung aus dem Persist schicken, falls eine da ist.
// Fasst nach, bis das Telefon sie bestaetigt.
void telefon_nachsenden(void);
// Wartet noch eine Zusammenfassung auf die Bestaetigung des Telefons?
bool telefon_wartet(void);

// Was gerade geschieht - damit das Telefon weiss, wann es mitschreiben soll.
//
// DIE UHR HAT KEIN GPS. Die Strecke kann nur das Telefon aufzeichnen, und das
// muss dafuer wissen, dass ein Training laeuft. Ohne diese Meldung erfuehre es
// erst am Ende davon - und haette nichts aufgezeichnet.
typedef enum {
  ZustandStop = 0,
  ZustandStart = 1,
  ZustandPause = 2,
  ZustandWeiter = 3,
} Trainingsmeldung;

// Stellt die Meldung hinten an - mit der Uhrzeit von jetzt (meldungen.h).
void telefon_melde_zustand(Trainingsmeldung was, uint8_t art, uint32_t beginn);

// Den Stopp eines verworfenen Trainings aus dem Persist schicken, falls er
// noch wartet; erst die Bestaetigung des Telefons loescht ihn dort.
void telefon_stopp_nachsenden(void);
bool telefon_stopp_wartet(void);

// Wird gerufen, wenn nichts mehr aufs Telefon wartet - die App, die der
// Dienst fuer ein liegengebliebenes Training geholt hat, geht dann zu.
void telefon_bei_fertig(void (*fertig)(void));

// Der Zustand dieser Datei auf Anfang - beim Start der App (siehe kieselsport.c).
void telefon_zuruecksetzen(void);
