/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Boulder V2: die Nacht als Zyklen in Pebbles Sitzungsliste (Spezifikation 4.5, 4.6).
//
// WARUM ZYKLEN: Pebble zaehlt den Schlaf als Summe seiner Sleep-Sitzungen und den Zustand
// "schlaeft" am Ende der letzten (activity_sessions.c:302-445). Eine Nacht als ein Block
// zaehlte die Wachminuten mit; als Zyklen zwischen den Wachabschnitten ist die Summe der
// Schlaf ohne Wachminuten (Kriterium 3), ohne einen Summen-Haken in Pebble.
//
// SITZUNGEN (Spezifikation 4.5):
// - P: vorlaeufiger Schlaf ab dem Live-Automaten (Sleep), solange kein Zyklus offen ist
//   (vor dem Einschlafen, nach "fertig"), bis das Nachtbuch den Schlaf festschreibt.
// - C: Zyklus zwischen zwei festgeschriebenen Wachabschnitten (Sleep).
// - R: festgeschriebener Tiefschlaf-Abschnitt (RestfulSleep).
// - neutral (P0): eine Sitzung, die nicht mehr gilt, wird RestfulSleep mit Laenge 0 und
//   bleibt laufend - Pebble loggt sie nie und raeumt sie beim Boot oder um Mitternacht weg.
//   So wird nie eine Sitzung geloescht oder durch einen anderen Start ersetzt.
//
// WARUM ALLES LAUFEND BIS "ENDGUELTIG": Pebble loggt eine Schlafsitzung, sobald sie nicht
// mehr laeuft und spaeter endet als die zuletzt geloggte ihrer Klasse (:541). Eine schon
// geloggte Sitzung liesse sich nicht mehr kuerzen. Danach schliesst der gestaffelte
// Abschluss je Minute hoechstens eine Sitzung je Klasse, die mit dem fruehesten Ende
// zuerst: so geht jede genau einmal ans Telefon, aufsteigend (DLS 84).
//
// SCHLUESSEL IST DER START: Pebble erkennt eine Schlafsitzung an ihrem Start, gleich
// welcher Schlaftyp (:132-152). Deshalb hat hier jeder Start hoechstens einen Eintrag, und
// ein R beginnt nie in der Startminute eines C (sonst ersetzte es ihn, Regel 3).
//
// LAUFZEIT: Alle Aufrufe auf KernelBG, ohne gehaltene Sperre des Aufrufers; Pebble wird
// direkt gerufen (activity_sessions_prv_add_activity_session nimmt die Activity-Sperre).
// Kein Heap, keine Datei. Minuten zaehlen ab Minute 0 des Nachtfensters.

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#define ZYKLEN_P_MAX 3
#define ZYKLEN_C_MAX 10
#define ZYKLEN_R_MAX 8
// So viele Plaetze der Liste (32) bleiben frei: fuer Spaziergaenge und Trainings des Tages.
// Zuerst entfaellt ein neues R oder Nickerchen, dann ein weiteres P; ein C immer zuletzt.
#define ZYKLEN_FREI 4
// Ein P, das so lange nicht verlaengert wurde, bleibt stehen; der naechste Schlaflauf
// bekommt ein neues.
#define ZYKLEN_P_STEHT 30
#define ZYKLEN_EINTRAEGE 32

// Alles vergessen (Boot, Tests). Pebbles Liste bleibt, wie sie ist.
void boulder_zyklen_zuruecksetzen(void);

// Eine neue Nacht im Fenster ab `beginn_utc`, ab Minute `ab` (0 oder das Ende der
// vorigen, endgueltigen Nacht). Die Sitzungen frueherer Naechte schliesst der Abschluss
// weiter ab.
void boulder_zyklen_nacht(time_t beginn_utc, uint16_t ab);

// Jede Minute (Minutenteil): Zustand des Automaten und der Beginn seines Schlaflaufs;
// `ende` ist die Minute nach der zuletzt verarbeiteten (Minutenwende, nie Zukunft).
// Schlaeft er, wird die offene Sitzung bis `ende` verlaengert; vor dem Einschlafen
// entsteht dafuer ein P.
void boulder_zyklen_live(bool schlaeft, int lauf_beginn, uint16_t ende);

// Nach dem Nachlauf oder der Endfassung: das Nachtbuch (nachlauf.h) ist bis `fest_bis`
// fest. Gleicht die Sitzungen der Nacht damit ab.
void boulder_zyklen_fest(uint16_t fest_bis);

// Die Nacht ist fertig: Der letzte Zyklus endet bei `aufwachen` und wird nicht mehr
// verlaengert. Schlaeft der Automat danach wieder, traegt ein P den Live-Zustand, bis das
// Nachtbuch einen neuen Zyklus festschreibt (die Nacht waechst).
void boulder_zyklen_fertig(uint16_t aufwachen);

// Die Nacht ist endgueltig: ohne festes Einschlafen werden alle P neutral; alle C und R
// gehen in den gestaffelten Abschluss.
void boulder_zyklen_endgueltig(void);

// Jede Minute: hoechstens eine Sitzung je Klasse abschliessen, wenn der Automat und Pebble
// "wach" sagen (Pebble loggt Schlaf nur dann, :549).
void boulder_zyklen_abschluss_schritt(bool automat_wach);

// Ende der offenen Sitzung (P oder letzter C), 0 = keine.
time_t boulder_zyklen_offen_ende(void);

// Ein Nickerchen als abgeschlossene Sleep-Sitzung melden (Pebbles post_process macht
// daraus Nap). Rueckgabe: gemeldet.
bool boulder_zyklen_nickerchen(time_t start_utc, uint16_t laenge_min);

// Diagnose: zaehlt, was das Budget oder die Tabelle verhindert hat.
typedef struct {
  uint16_t c_ueberspannt;   //< kein Platz fuer ein C: der vorige ueberspannt die Wachzeit
  uint16_t r_entfallen;     //< Tiefschlaf-Abschnitt nicht gemeldet (Budget, > 8)
  uint16_t p_entfallen;     //< kein neues P (Budget), das letzte wird weiter verlaengert
  uint16_t tabelle_voll;
  uint16_t zu_geaendert;    //< eine schon abgeschlossene Sitzung haette sich geaendert
} BoulderZyklenDiagnose;
const BoulderZyklenDiagnose *boulder_zyklen_diagnose(void);
