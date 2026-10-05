/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

// Die Messstufe: ein Regler zwischen Genauigkeit und Akkulaufzeit.
//
// EIN REGLER STATT VIER SCHALTER. Pulstakt am Tag, Pulstakt in der Nacht,
// HRV-Fenster und SpO2 haengen zusammen - wer genauer schlafen messen will,
// zahlt mit Akku, egal an welcher Stelle. Die Stufe waehlt eine der fuenf
// Zusammenstellungen; Boulder stellt sie als Schieberegler ein
// (Einstellungsschluessel "boulderMessstufe"), die Uhr in ihren Einstellungen.
//
//  Stufe      Puls Nacht  Puls Tag  HRV Nacht  SpO2
//  1 sparsam  10 min      30 min    aus        aus
//  2          10 min      10 min    60 min     60 min
//  3          5 min       10 min    30 min     30 min   (Vorgabe)
//  4          2 min       10 min    15 min     10 min
//  5 genau    1 min       5 min     10 min     10 min
//
// "Nacht" heisst: im Nachtfenster UND die letzte Minute ruhig. Wer um sieben
// schon herumlaeuft, misst mit dem Tagestakt - der dichte Nachtpuls ist fuer
// den Schlaf da, nicht fuer den Morgen.

#define MESSSTUFE_MIN 1
#define MESSSTUFE_MAX 5
#define MESSSTUFE_VORGABE 3

// Gespeichert in den Einstellungen (shell/normal/prefs.c).
uint8_t boulder_messstufe(void);
void boulder_messstufe_setzen(uint8_t stufe);

// Wie oft der Puls gemessen wird, in Sekunden.
uint32_t boulder_puls_takt_s(time_t utc, uint16_t letzte_bewegung);
// Wie oft SpO2 gemessen wird, in Sekunden; 0 = gar nicht.
uint32_t boulder_spo2_takt_s(void);
// Alle wie viele Minuten nachts ein HRV-Fenster; 0 = keine.
uint16_t boulder_hrv_takt_min(void);
