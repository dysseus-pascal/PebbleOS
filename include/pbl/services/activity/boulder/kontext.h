/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Boulder V2: Velo und Auto (Spezifikation 3.3-3.7, Kriterium 5).
//
// AUS MINUTENWERTEN: Jede fertige Minute (D2) wird velo-artig, auto-artig oder keins. Aus
// den letzten zehn Minuten entsteht eine Kandidatin; die Uhr fragt nach (nachfrage.c), und
// erst ein "Ja" streicht die Schritte. Ohne Antwort gilt "Nein" (F4): nichts wird
// gestrichen, ohne dass der Nutzer es bestaetigt.
//
// DIE SCHWELLEN SIND STARTWERTE. Echte Fahrten des Nutzers lagen beim Bau nicht vor; sie
// sind an gestellten Faellen geprueft und muessen an echten Fahrten eingestellt werden
// (getrennte Einstell- und Pruefmenge, Spezifikation 9).
//
// ZURUECKHALTEN (D3): Schon die erste fahrtartige Minute haelt die Minutensaetze zurueck,
// sonst laege der Fahrtbeginn bereits ungenullt in DLS 81, wenn die Kandidatin nach zehn
// Minuten entsteht. Bis zum Entscheid bleibt alles ab dem Fahrtbeginn im Puffer; die zweite
// Frage kommt spaetestens nach 45 min, so liegt beim "Ja" jede Fahrtminute noch dort (60).
//
// NACH DEM "JA" (D5): Neue Schritte gehen in den Minutensatz, nicht in die Tagessumme. Ob
// eine Minute noch zur Fahrt gehoert, steht erst fest, wenn die naechste fahrtartige kommt
// oder drei nicht fahrtartige das Ende zeigen. Diese "Schwanz"-Minuten bleiben bis dahin im
// Puffer; endet die Fahrt, bekommt der Zaehler ihre Schritte zurueck, und ihre Saetze gehen
// ungenullt hinaus. So verliert niemand die Schritte nach dem Absteigen.
//
// TASKS: Zustand aendern nur der Minutenteil und die Callbacks auf KernelBG. Die Antwort
// des Dialogs (KernelMain) ist nur ein Merker. Die Abfragen der Kopie (auch vom Health-Sync)
// lesen einzelne Worte, O(1), ohne Sperre, ohne Heap.
//
// TRAINING (Schritt 2, AP4): Start und Stopp setzen nur Merker und Zeit (D15). Die Minute, in
// der ein Training beginnt, beendet eine laufende Fahrt am Trainingsstart; eine Minute, die ein
// Training beruehrt, ist nie fahrtartig, und nach dem Training beginnt der Ring neu. So beginnt
// keine Kandidatin vor dem Ende eines Trainings und keine Fahrt ueberdeckt es (Kriterium 4).
//
// PULSKALORIEN DER VELOFAHRT (F7): derselbe Zaehler wie im Training (kalorien.c), ab der
// Kandidatin, mit dem Minutenpuls der Minuten davor und danach jedem Sekundenwert des Abos
// "Aktiv" (boulder_kontext_puls). Die Sitzung traegt sie in step_data.active_kcalories.

#include "pbl/kernel/compiler.h"
#include "pbl/services/activity/boulder/variante.h"

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

// Merkmale je Minute (3.3), Startwerte
#define KONTEXT_VELO_SCHRITTE_UNTER 40   //< b19-Schwelle: darunter ist eine Minute kein Gehen
#define KONTEXT_VELO_VMC_AB 250          //< eine "Regung" (schlafanalyse.c REGUNG)
#define KONTEXT_AUTO_SCHRITTE_BIS 10
#define KONTEXT_AUTO_VMC_AB 30
#define KONTEXT_PULS_UEBER 15            //< Minutenmedian ueber dem Ruhepuls
// Kandidatin aus den letzten KONTEXT_FENSTER Minuten
#define KONTEXT_FENSTER 10
#define KONTEXT_VELO_ARTIG 8
#define KONTEXT_VELO_LAGE 6
#define KONTEXT_VELO_FENSTER_PULS 20     //< Fenstermedian ueber dem Ruhepuls
#define KONTEXT_FENSTER_PULSWERTE 3      //< erst ab so vielen Pulswerten zaehlt der Puls
#define KONTEXT_AUTO_ARTIG 8
#define KONTEXT_AUTO_LAGE 7
#define KONTEXT_VORSTUFE 3               //< velo-artig in Folge: Puls dichter (3.7)
#define KONTEXT_ENDE 3                   //< weder noch in Folge: Fahrtende
#define KONTEXT_HOECHSTENS_MIN (6 * 60)
#define KONTEXT_SPRUNG_MIN 5             //< Zeitsprung: die Fahrt endet
// Nachfrage (F4)
#define KONTEXT_ZWEITE_FRAGE_MIN 45      //< Zurueckhalten so lang: zweite Frage
#define KONTEXT_DIALOG_MIN 2             //< so lang steht der Dialog
#define KONTEXT_DIALOG_SICHER_MIN 4      //< ohne Rueckmeldung gilt der Dialog als geschlossen
#define KONTEXT_RUHEPULS_SONST 60

typedef enum {
  FahrtArtKeine = 0,
  FahrtArtVelo = 1,
  FahrtArtAuto = 2,
} BoulderFahrtArt;

typedef enum {
  FahrtKeine = 0,
  FahrtKandidatin,   //< gefragt oder zu fragen; alles ab Beginn zurueckgehalten
  FahrtBestaetigt,   //< "Ja": Schritte gestrichen
  FahrtAbgelehnt,    //< "Nein" oder keine Antwort: bis zum Ende dieser Fahrt keine Frage
} BoulderFahrtZustand;

typedef enum {
  NachfrageOhneAntwort = 0,
  NachfrageJa = 1,
  NachfrageNein = 2,
} BoulderNachfrageAntwort;

// Was ueber einen Neustart gerettet wird (7.4): nur eine bestaetigte, laufende Fahrt.
typedef struct PBL_PACKED {
  uint8_t fassung;
  uint8_t art;
  uint8_t nr;
  uint32_t beginn;
  uint32_t letzte_artige;
  int32_t kcal_cal;   //< Fassung 2 (F7): Pulskalorien bis letzte_artige + 1 min, in Kalorien
} BoulderFahrtAblage;
#define KONTEXT_ABLAGE_FASSUNG 2

// Minutenteil (Dauerteil), nach dem Satz der Minute. `ruhepuls` = der der letzten Nacht.
void boulder_kontext_minute(const BoulderMinute *minute, uint8_t ruhepuls);

// Takt und Callback "Boulder Antwort" (KernelBG): die Antwort abholen.
void boulder_kontext_takt(time_t jetzt);

// KernelMain (nachfrage.c): der Dialog fuer Fahrt `nr` ist zu.
void boulder_kontext_antwort(uint8_t nr, BoulderNachfrageAntwort antwort);

// Training an/aus (D15, enable_activity_tracking unter Pebbles Workout-Sperre; App-Task oder
// KernelBG): nur Merker und Zeit. Die naechste Minute beendet eine laufende Fahrt am
// Trainingsstart; im Training keine Kandidatin.
void boulder_kontext_training(bool laeuft);
bool boulder_kontext_training_laeuft(void);
// V1 (Schritt 2): Das laufende Training zaehlt keine Schritte. Kieselsport setzt es nach dem
// Start (KernelBG, ohne Sperre); jeder Start und Stopp loescht es, so trifft es nie ein anderes
// Training.
void boulder_kontext_training_ohne_schritte(bool ohne);
// D17: ein laufendes Training ohne Schritte.
bool boulder_kontext_training_ohne_schritte_laeuft(void);

// F7: ein Pulswert des Abos "Aktiv" (puls.c: KernelBG, im HRM-Rueckruf unter der Sperre des
// Managers, gueltig und mindestens "Acceptable"). O(1), ohne Sperre.
void boulder_kontext_puls(uint8_t bpm, time_t utc);
// F7: die Pulskalorien der laufenden oder zuletzt beendeten Velofahrt bis zum Ende ihrer
// Sitzung, in Kalorien (Diagnose, Tests).
int32_t boulder_kontext_kcal(void);
// early_deinit: eine laufende Velo-Sitzung abschliessen (vor dem Speichern der Liste).
void boulder_kontext_sitzung_beenden(void);
// deinit (D13): ein offener Entscheid gilt als "Nein", die Fahrt endet.
void boulder_kontext_tracking_aus(void);

// Naht der Kopie (variante.h)
time_t boulder_kontext_offen_seit(void);
bool boulder_kontext_fahrt_minute(time_t minute_utc);
bool boulder_kontext_fahrt_laeuft(void);
bool boulder_kontext_ueberdeckt(time_t start_utc, uint32_t laenge_s);
bool boulder_kontext_aktiv(void);
// Eine Frage steht und ist noch nicht beantwortet (der QEMU-Pruefbau haelt dann an).
bool boulder_kontext_frage_offen(void);

// Speicher (speicher.c): ablegen und nach dem Boot laden.
void boulder_kontext_ablage(BoulderFahrtAblage *ablage);
void boulder_kontext_laden(const BoulderFahrtAblage *ablage);

// Diagnose und Tests
BoulderFahrtZustand boulder_kontext_zustand(void);
BoulderFahrtArt boulder_kontext_art(void);
time_t boulder_kontext_beginn(void);
uint8_t boulder_kontext_fragen(void);
void boulder_kontext_test_zuruecksetzen(void);

// Die Nachfrage auf KernelMain (nachfrage.c). `nr` kommt mit der Antwort zurueck.
void boulder_nachfrage_stellen(BoulderFahrtArt art, uint8_t nr);
