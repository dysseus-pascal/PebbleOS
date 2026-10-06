/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Boulder V2: die Messstufe (Spezifikation 5, Entscheidung F3).
//
// EIN REGLER STATT VIER SCHALTER. Puls am Tag, Puls in der Nacht, HRV-Fenster und SpO2
// haengen zusammen: wer genauer schlafen messen will, zahlt mit Akku, egal an welcher
// Stelle. Boulder schreibt die Stufe als 1 Byte unter "boulderMessstufe" (Whitelist E3,
// Datei shellpref), die Uhr stellt sie im Einstellungsmenue ein (settings/health.c).
//
//  Stufe            Puls Nacht  Puls Tag  HRV-Fenster nachts  SpO2
//  1 Pebble          10 min      10 min    aus                 aus
//  2                 10 min      10 min    60 min              60 min
//  3 (Vorgabe)       5 min       10 min    30 min              30 min
//  4                 2 min       10 min    15 min              10 min
//  5                 1 min       5 min     10 min              10 min
//
// WER WAS MISST: "Puls Tag" und SpO2 misst Pebble selbst (activity.c) - die Uhr setzt
// dafuer Pebbles eigene Einstellungen passend zur Stufe (F3). Den dichteren Nachtpuls und
// die HRV-Fenster misst das eine Abo der Variante (puls.c). "Nacht" heisst: im
// Nachtfenster und die letzte Minute ruhig (VMC <= SCHLAF_SCHWELLE_VMC).

#include <stdbool.h>
#include <stdint.h>

#define MESSSTUFE_MIN 1
#define MESSSTUFE_MAX 5
#define MESSSTUFE_VORGABE 3
#define BOULDER_MESSSTUFE_SCHLUESSEL "boulderMessstufe"
// Titel im Einstellungsmenue (nur Englisch, wie Gravel).
#define BOULDER_MESSSTUFE_TITEL "Precision"

typedef struct {
  uint16_t puls_nacht_s;  //< Takt des Abos der Variante in Nachtruhe
  uint8_t puls_tag_min;   //< Pebbles Puls-Takt (Einstellung "HR Monitoring")
  uint8_t hrv_takt_min;   //< alle so viele Minuten nachts ein Fenster, 0 = keine
  uint8_t spo2_min;       //< Pebbles SpO2-Takt, 0 = aus
} BoulderStufe;

// Die Werte einer Stufe; ungueltige Stufen gelten als Vorgabe.
const BoulderStufe *boulder_messstufe_werte(uint8_t stufe);

// Fuer das Menue (App-Task): die geltende Stufe. Ist sie noch nicht gelesen, liest das
// den Speicher (Datei, Heap) - deshalb nie aus dem Minutenteil.
uint8_t boulder_messstufe(void);
// Fuer das Menue: die Namen der Stufen 1-5 (MESSSTUFE_MAX Eintraege).
const char **boulder_messstufe_namen(void);
// Das Menue waehlt eine Stufe: speichern (geht ans Telefon), Pebbles Werte und das Abo
// folgen im eigenen Callback.
void boulder_messstufe_setzen(uint8_t stufe);

// Dauerteil (KernelBG). Die Stufe, wie sie der Minutenteil verwendet: 0 = noch nicht
// gelesen. Ohne Datei, O(1).
uint8_t boulder_messstufe_geladen(void);
// Minutenteil: steht Arbeit an (erster Start, Lesen nach einem Ereignis, Pebbles Werte
// noch nicht gesetzt), reiht sie verwerfbar ein. O(1), ohne Heap und Datei.
void boulder_messstufe_minute(void);
// Ablage (AP4): zur Sicherheit neu lesen und Pebbles Werte nachziehen.
void boulder_messstufe_nachlesen(void);

#ifdef UNITTEST
void boulder_messstufe_test_zuruecksetzen(void);
#endif
