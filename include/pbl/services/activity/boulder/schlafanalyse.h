/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Die Nacht aus den Minutenwerten: wann eingeschlafen, wann aufgewacht, und
// welche Phase in jeder Minute dazwischen.
//
// WACH ODER SCHLAF kommt aus Kiesel-Helper (Schlafanalyse.kt): Cole-Kripke
// auf der Bewegung, die Regeln von Webster, ein hoher Puls als Wachsein ohne
// Bewegung. Dazu, eingestellt an vier Naechten neben einem Fitbit (29.9.,
// 30.9., 2.10. und - nur das Einschlafen - 5.10.2026): Aufwach-Schuebe -
// Regungen dicht hintereinander mit einem Pulsstoss - sind ganz wach, auch
// zwischen den Regungen; eingeschlafen ist man erst, wenn das Herumwaelzen
// aufhoert (keine zwei kraeftigen Regungen mehr in vierzig Minuten). DIE
// PHASEN: Tiefschlaf ist eine Viertelstunde ohne jede Regung bei ruhigem Herz,
// REM ein unregelmaessiges Herz bei ruhendem Koerper (ohne dichten Puls: der
// zuckende Koerper). Der Host-Test rechnet alle Naechte nach.
//
// KEIN BETRIEBSSYSTEM HIER. Eingabe sind Felder, Ausgabe ein Ergebnis; der
// Aufrufer holt die Minuten aus der Datei und legt die Sitzungen an. So laesst
// sich die Rechnung auf dem Rechner pruefen.
//
// GANZZAHLIG. Mediane von Pulsen sind halbe Schlaege; hier stehen sie mal
// zwei, ohne Gleitkomma.

// Laenger wird keine Nacht gerechnet: sechzehn Stunden.
#define SCHLAF_MINUTEN_MAX 960
// 96: bei Messstufe 5 (alle 10 Minuten) reicht das fuer das ganze
// 15-Stunden-Fenster; 48 deckten nur acht Stunden.
#define SCHLAF_HRV_MAX 96
#define SCHLAF_ABSCHNITTE_MAX 160

typedef enum {
  SchlafPhaseWach = 0,
  SchlafPhaseLeicht,
  SchlafPhaseTief,
  SchlafPhaseRem,
  // Schlaf ohne Phase: in einer Nacht ganz ohne Puls laesst sich Tief- von
  // Leichtschlaf nicht unterscheiden - und eine Phase, die niemand gemessen
  // hat, wird nicht erfunden.
  SchlafPhaseSchlaf,
} SchlafPhase;

// Eine HRV-Messung: `minute` ist der Beginn des Fensters (fuenf Minuten).
typedef struct {
  uint16_t minute;
  uint16_t rmssd;   //< ms, 0 = nicht brauchbar
  uint8_t puls;     //< bpm aus den Schlagabstaenden, 0 = unbekannt
} SchlafHrv;

typedef struct {
  uint16_t von;     //< Minute ab Beginn der Eingabe
  uint16_t bis;     //< erste Minute danach
  uint8_t phase;    //< SchlafPhase
} SchlafAbschnitt;

typedef struct {
  // Je Minute: das Bewegungsmass (VMC) und der Puls. `gueltig` falsch heisst:
  // nicht getragen oder keine Daten - eine solche Minute ist kein Schlaf.
  const uint16_t *bewegung;
  const uint8_t *puls;       //< bpm, 0 = keiner
  const bool *gueltig;
  uint16_t anzahl;
  // Ab welchem gewichteten Mittel eine Minute wach ist, in den Einheiten von
  // `bewegung`. Mit dem VMC der Uhr: SCHLAF_SCHWELLE_VMC.
  uint32_t schwelle;
  const SchlafHrv *hrv;
  uint8_t hrv_anzahl;
  // So viel Schlaf braucht eine Nacht mindestens, in Minuten; 0 = die
  // Vorgabe (eine Stunde). Der Morgenschlaf nach einer fertigen Nacht braucht
  // mehr (nacht.c).
  uint16_t min_schlaf;
} SchlafEingabe;

// Die Schwelle fuer das VMC der Uhr - eingestellt an der Nacht 25./26.9.2026
// neben einem Fitbit, bestaetigt an der Nacht 26./27.9.
#define SCHLAF_SCHWELLE_VMC 800

typedef struct {
  bool gefunden;            //< false: in den Minuten steckt keine Nacht
  uint16_t einschlafen;     //< erste Minute des Schlafs
  uint16_t aufwachen;       //< erste Minute danach
  uint16_t schlaf_minuten;  //< ohne die Wachphasen dazwischen
  uint8_t ruhepuls;         //< 0 = keiner
  uint16_t hrv;             //< Median der HRV im Schlaf, 0 = keine
  uint16_t abschnitt_anzahl;
  SchlafAbschnitt abschnitte[SCHLAF_ABSCHNITTE_MAX];
} SchlafErgebnis;

// Der Arbeitsspeicher der Rechnung. Gross (rund 13 KB) - der Aufrufer legt ihn
// an, auf dem Heap; die Rechnung selbst holt sich nichts.
typedef struct {
  bool wach[SCHLAF_MINUTEN_MAX];
  int16_t glatt2[SCHLAF_MINUTEN_MAX];     //< Puls mal zwei, INT16_MIN = keiner
  int16_t relativ2[SCHLAF_MINUTEN_MAX];   //< mal zwei, INT16_MIN = keiner
  int16_t sortiert2[SCHLAF_MINUTEN_MAX];
  uint8_t phase[SCHLAF_MINUTEN_MAX];
  SchlafAbschnitt liste[SCHLAF_MINUTEN_MAX];
} SchlafArbeit;

// Die Nacht auswerten. Rueckgabe: ergebnis->gefunden.
bool schlafanalyse_werte(const SchlafEingabe *eingabe, SchlafArbeit *arbeit,
                         SchlafErgebnis *ergebnis);

// Verdichtet `a` auf hoechstens `max` Abschnitte: der kuerzeste geht im
// Nachbarn auf. Die Liste bleibt lueckenlos. Rueckgabe: wie viele.
int schlafanalyse_verdichten(SchlafAbschnitt *a, int n, int max);
