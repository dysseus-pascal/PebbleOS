/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "pebble_compat.h"

// Der Draht zum Telefon.
//
// ZWEI RICHTUNGEN, ZWEI ZWECKE. Vom Telefon kommt die Einstellung
// (Maximalpuls, Becken, Pause, Empfindlichkeit) aus der Konfigseite. Zur Uhr
// hinaus geht die Zusammenfassung eines Trainings - und die hört NICHT nur
// die eigene pkjs-Seite: die Pebble-App reicht jede eingehende Nachricht auch
// an klassische Companion-Apps weiter. Kiesel-Helper trägt sie von dort in
// die Gesundheitsakte ein, ohne dass diese App etwas davon wissen muss.
//
// KEIN companionApp-Eintrag in der package.json. Ein Paketname dort schaltet
// die Pebble-App auf PebbleKit2 um, und damit fällt genau dieser
// Nebenempfänger weg.
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

void telefon_melde_zustand(Trainingsmeldung was, uint8_t art, uint32_t beginn);

// Wird gerufen, wenn nichts mehr aufs Telefon wartet - die App, die der
// Dienst fuer ein liegengebliebenes Training geholt hat, geht dann zu.
void telefon_bei_fertig(void (*fertig)(void));

// --- Eine einzelne HRV-Messung (Menuepunkt "HRV") ---
void telefon_hrv(uint16_t rmssd, time_t wann);
bool telefon_hrv_offen(void);

// --- SpO2 vom Telefon (Menuepunkt "Messen" -> "SpO2") ---
//
// Die Uhr fragt, Kiesel-Helper antwortet mit dem juengsten Wert aus Health
// Connect. wert = 0: keiner da. Die Nachtwerte sind 0, wenn es in der
// letzten Nacht keine gab.
typedef struct {
  uint8_t wert;          //< Prozent
  uint32_t zeit;         //< Unix-Sekunden der Messung
  uint8_t nacht_mittel;  //< Prozent, 0 = keine
  uint8_t nacht_tief;
} Spo2Antwort;

// Fragen - `antwort` kommt, sobald das Telefon antwortet. NULL: die Frage
// vergessen (das Fenster ging zu).
void telefon_spo2_frage(void (*antwort)(const Spo2Antwort *a));

// Der Zustand dieser Datei auf Anfang - beim Start der App (siehe kieselsport.c).
void telefon_zuruecksetzen(void);
