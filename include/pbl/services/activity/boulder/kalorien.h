/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Boulder V2, Schritt 2: Pulskalorien und Verwerfen, Boulders Seite von P1 in
// workout_service.c (Spezifikation 2.3, 3; Entscheide F3 = A, F5 = (a) B, (b) A).
//
// DER PULS GILT, NICHT DIE STRECKE. Pebble rechnet die aktiven kcal eines Trainings nur aus den
// Schritten (workout_service.c:197-205); Velo, Kraft und Yoga bekamen 0. Hier steht Boulders
// Formel (B: Trainingsanalyse.kt:95-131, Swain): Anteil an der Pulsreserve = Anteil an der
// VO2-Reserve, 5 kcal je Liter, ohne den Ruheanteil von 3,5 ml/kg/min. Jeder Pulswert gilt bis
// zum naechsten, hoechstens 60 s, wie in Boulder. Gezaehlt wird in aktiven Sekunden des
// Trainings, eine Pause zaehlt also nicht.
//
// OHNE GUELTIGEN PULS PEBBLES SCHRITTFORMEL (F5 a = B): Eine Sekunde, die kein Pulswert deckt
// (vor dem ersten Wert, mehr als 60 s nach dem letzten), bekommt Pebbles Schritt-kcal. Ein Lauf
// ohne Puls hat so wenigstens Pebbles Wert; mit Puls zaehlt der Schritt nicht doppelt. Ohne
// brauchbares Profil (Gewicht unter 20 kg, Alter 0, Maximum nicht ueber Ruhepuls + 20) gibt es
// keine Pulskalorien, dann gilt jede Sekunde als ohne Puls, also Pebbles Rechnung wie bisher.
//
// DIE TAGESSUMME BLEIBT (F5 b = A): Nur die Sitzung, Kieselsports Anzeige und seine
// Zusammenfassung tragen die Pulskalorien; activity.c zaehlt weiter aus den Schritten.
//
// GANZE KALORIEN AN PEBBLE, OHNE DRIFT: Gerechnet wird exakt als Bruch; P1 bekommt je Aufruf
// nur, was die gerundete Summe zugelegt hat. Pebbles active_calories ist so immer die gerundete
// Summe, auch nach Stunden mit einem Wert je Sekunde.
//
// BLATT UNTER DER WORKOUT-SPERRE: Die Haken laufen unter Pebbles Sperre (KernelMain fuer Puls
// und Schritte, die Task des Aufrufers fuer Start und Stopp). Sie nehmen nur die eigene Sperre
// des Zaehlers, unter der nichts weiter geschieht, und rufen nur Getter, die statischen Speicher
// lesen (activity_prefs_*, boulder_nacht_ruhepuls_median).
//
// DIE BESTAETIGTE VELOFAHRT (F7, AP4) rechnet mit demselben Zaehler und demselben Profil
// (boulder_kalorien_profil_jetzt), gefuehrt von kontext.c.

#include "pbl/services/activity/activity.h"

#include <stdbool.h>
#include <stdint.h>

// Profil eines Trainings, beim Start festgehalten. Ganzzahlig, damit die Uhr ohne Gleitkomma
// dasselbe rechnet wie Boulder: Pulse in Zehntel bpm (Tanaka 208 - 0,7 * Alter ist damit exakt),
// VO2max in Hundertstel ml/kg/min (die Tabelle ist linear zwischen den Jahrzehnten, also exakt).
typedef struct BoulderKalorienProfil {
  uint16_t gewicht_dag;   //< 0: keine Pulskalorien
  uint16_t vo2max_c;      //< VO2max, 0,01 ml/kg/min (Mitte der Altersgruppe, FRIEND)
  uint16_t ruhe_z;        //< Ruhepuls, 0,1 bpm
  uint16_t max_z;         //< Maximalpuls, 0,1 bpm
} BoulderKalorienProfil;

// Der Zaehler eines Trainings oder einer Fahrt (F7, AP4). Zeiten in Sekunden ab einem
// beliebigen Bezug, aufsteigend; im Training sind es aktive Sekunden seit dem Start.
typedef struct BoulderKalorienZaehler {
  BoulderKalorienProfil profil;
  int64_t anteil_s;          //< Summe der Pulsanteile * Sekunden (0,1 bpm * s); 64 Bit, weil
                             //< ein vergessenes Training tagelang laufen kann (Befund 9)
  int32_t schritt_cal;       //< Pebbles Schritt-kcal der Sekunden ohne Puls, in Kalorien
  int32_t stand_s;           //< bis hierher verbucht
  int32_t puls_s;            //< Zeit des letzten gueltigen Pulswerts
  int32_t ohne_puls_s;       //< Sekunden ohne gueltigen Puls bis stand_s
  int32_t bewegung_s;        //< Zeit der letzten Schrittmeldung
  int32_t ohne_puls_bei_bewegung_s;
  int32_t gemeldet_cal;      //< was der Aufrufer schon bekommen hat (gerundete Summe)
  uint8_t bpm;               //< letzter gueltiger Pulswert, 0 = keiner
} BoulderKalorienZaehler;

// Das Profil nach Boulders Regeln. Ruhepuls ausserhalb 30..110 gilt als 60, Maximalpuls 0 heisst
// Tanaka. Gibt false (und ein leeres Profil) ohne brauchbare Eingaben.
bool boulder_kalorien_profil(BoulderKalorienProfil *profil, uint16_t gewicht_dag, uint8_t alter,
                             ActivityGender geschlecht, uint8_t ruhepuls, uint8_t maxpuls);

void boulder_kalorien_zaehler_neu(BoulderKalorienZaehler *z, const BoulderKalorienProfil *profil);
// Ein gueltiger Pulswert zur Zeit t. Gibt die Kalorien zurueck, die die gerundete Summe zulegt.
int32_t boulder_kalorien_zaehler_puls(BoulderKalorienZaehler *z, uint8_t bpm, int32_t t);
// Pebbles Schritt-kcal einer Schrittmeldung (seit der vorigen) zur Zeit t. Gutgeschrieben wird
// nur der Teil, der auf Sekunden ohne Puls faellt. Rueckgabe wie oben.
int32_t boulder_kalorien_zaehler_schritte(BoulderKalorienZaehler *z, int32_t cal, int32_t t);
// Bis t verbuchen (der letzte Pulswert gilt bis t, hoechstens 60 s). Rueckgabe wie oben.
int32_t boulder_kalorien_zaehler_bis(BoulderKalorienZaehler *z, int32_t t);
// Die Summe bis t in Kalorien, gerundet, ohne den Zaehler zu veraendern.
int32_t boulder_kalorien_zaehler_summe(const BoulderKalorienZaehler *z, int32_t t);

// ---------------------------------------------------------------------------------------
// Das Training (P1 in workout_service.c). Zeit = aktive Sekunden seit dem Start.

// Kieselsports Maximalpuls (V2, AP2), gilt ab dem naechsten Start und der naechsten Fahrt.
// 0 = Tanaka.
void boulder_kalorien_maxpuls(uint8_t bpm);

// Das Profil, mit dem ein Training oder eine Fahrt jetzt beginnt: Gewicht, Alter und Geschlecht
// der Uhr, der Ruhepuls als Median der letzten sieben Naechte (V3), Kieselsports Maximalpuls.
// false (und ein leeres Profil) ohne brauchbare Eingaben.
bool boulder_kalorien_profil_jetzt(BoulderKalorienProfil *profil);

// P1: Start. Profil festhalten, Zaehler auf 0.
void boulder_kalorien_start(void);
// P1: ungepauster Pulswert (Qualitaet mindestens Acceptable, roh wie Pebbles Mittel).
int32_t boulder_kalorien_puls(uint8_t bpm, int32_t aktiv_s);
// P1: ungepauste Schrittmeldung mit Pebbles Schritt-kcal. Gibt zurueck, was davon zaehlt.
int32_t boulder_kalorien_schritte(int32_t cal, int32_t aktiv_s);
// P1: Stopp ab 60 s, mit Pebbles Dauer. Der letzte Pulswert bis zum Ende.
int32_t boulder_kalorien_ende(int32_t aktiv_s);
// Fuer Kieselsports Anzeige und Zusammenfassung (KernelBG, ohne Pebbles Sperre; den Zaehler
// liest sie unter seiner eigenen, also nie halb geschrieben): die Summe bis aktiv_s, in
// Kalorien. Im Training kann sie um einen Pulswert hinterherhinken; in der Pause steht sie, denn
// dann ruft Pebble keinen Haken.
int32_t boulder_kalorien_bisher(int32_t aktiv_s);

// ---------------------------------------------------------------------------------------
// Verwerfen (F3 = A)

// Stoppt das laufende Training ohne Sitzung und ohne Meldung. Ohne Sperre aufrufen
// (Kieselsport von KernelBG, Regel 2.5.2). false, wenn kein Training lief.
bool boulder_training_verwerfen(void);
// P1: Kommt der Stopp aus boulder_training_verwerfen()?
bool boulder_training_verworfen(void);
