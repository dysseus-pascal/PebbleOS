/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Boulder V2: die Nacht nach aussen (Spezifikation 4.8, AP4) - die Saetze 0x4001 und 0x4002
// fuer Boulder (Format b26, nacht_satz.h), das Protokoll (Akku, Messstufe) und das NachtBild
// fuer die Uhr selbst.
//
// WARUM EIGENE CALLBACKS: dls_create wartet bis 4 s auf Platz (dls_main.c:412-415) und beendet
// mit resume = false eine laufende Sitzung gleichen Tags (:376-379). Es laeuft deshalb nur im
// Callback "Anlage", hoechstens einmal je Boot und Tag, nie im Minutenteil und nie in der
// Ruhezeit um eine Weckzeit (2.4, Regel 6). Jeder Teil eines Satzes geht in einem eigenen
// Callback "Ausgabe", damit keiner lange laeuft.
//
// ALS GESCHICKT GILT EINE NACHT ERST, WENN ALLE TEILE IM DATALOGGING LIEGEN (wie b26). Faellt
// ein Teil aus (Schlange voll, Pool voll, die Endfassung wurde neu gerechnet), bleibt sie
// "offen", und der naechste Takt des Lebenslaufs (nacht.c) schickt sie ganz neu. Boulder
// ersetzt eine Nacht mit gleichem beginn_utc (0.50).
//
// Alles auf KernelBG; nur boulder_nacht_letzte liest aus anderen Tasks (Health-App), unter
// einer eigenen Sperre. Kein Heap.

#include "pbl/services/activity/boulder/nacht_satz.h"
#include "pbl/services/activity/boulder/schlafanalyse.h"

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

// Das Protokoll: Akku und Messstufe alle 15 Minuten ab Fensterbeginn, bis eine Stunde nach
// dem Nachtfenster (b26: PROTOKOLL_TAKTE). Im Satz zaehlt der Stand beim Einschlafen.
#define AUSGABE_PROTOKOLL_TAKT 15
#define AUSGABE_PROTOKOLL_TAKTE ((15 * 60 + 60) / AUSGABE_PROTOKOLL_TAKT + 1)
#define AUSGABE_UNBEKANNT 0xFF

typedef struct {
  time_t fenster;
  uint8_t akku[AUSGABE_PROTOKOLL_TAKTE];
  uint8_t stufe[AUSGABE_PROTOKOLL_TAKTE];
  bool geladen;  //< am Kabel, seit die Nacht Schlaf hat: der Verbrauch sagt dann nichts
} BoulderProtokoll;

// Was zuletzt ganz ans Telefon ging (Kopf, ueberlebt einen Neustart).
typedef struct {
  uint32_t beginn;
  uint16_t aufwachen;
  uint16_t schlaf;
} BoulderGesendet;

// Der Takt: Protokoll-Takt `takt` (0..) des Fensters ab `fenster`. Ein neues Fenster
// beginnt ein leeres Protokoll. `schlaf_da`: die Nacht hat Schlaf (fuer "geladen").
void boulder_ausgabe_protokoll(time_t fenster, int takt, uint8_t akku, uint8_t stufe,
                               bool am_kabel, bool schlaf_da);
BoulderProtokoll *boulder_ausgabe_protokoll_daten(void);

// Unterscheidet sich die Nacht von der zuletzt geschickten?
bool boulder_ausgabe_noetig(time_t beginn, uint16_t aufwachen, uint16_t schlaf);

// Die Endfassung `e` der Nacht ab `beginn` (Minute 0 des Satzes) schicken. Der Zeiger gilt,
// solange der Nachlauf nicht neu rechnet (boulder_nachlauf_stand); die HRV-Fenster kommen aus
// derselben Rechnung.
void boulder_ausgabe_nacht(time_t beginn, const SchlafErgebnis *e);

// Wurde eine Nacht angefangen und noch nicht ganz geschickt?
bool boulder_ausgabe_offen(void);
// Den angefangenen Auftrag fallen lassen (die Nacht ist nicht mehr fertig).
void boulder_ausgabe_verwerfen(void);

// Die Nacht ist endgueltig: das Flag "geladen" gilt fuer die naechste nicht mehr (b26).
void boulder_ausgabe_endgueltig(void);

// Das NachtBild aus der Endfassung (b26: bei jeder Rechnung im Speicher, in die Datei nur
// "fertig" und nur bei Aenderung - das schreibt die Ablage). Rueckgabe: in die Datei faellig.
bool boulder_ausgabe_bild(time_t beginn, const SchlafErgebnis *e, bool fertig);

// Im Callback "Ablage": das NachtBild in boulder_nacht/"letzte", wenn es faellig ist.
void boulder_ausgabe_bild_ablegen(void);

// Die letzte Nacht fuer die Health-App (wie b26: aus der Datei, solange die Uhr seit dem
// Boot noch keine gerechnet hat). Jede Task.
bool boulder_nacht_letzte(NachtBild *aus);

// Jeder Takt: den Schub ans Telefon nach einer geschickten Nacht (dls_send_all_sessions),
// nur bei Platz in der Schlange.
void boulder_ausgabe_takt(time_t jetzt);

// Kopf: was zuletzt geschickt wurde (Laden setzt es zurueck).
BoulderGesendet *boulder_ausgabe_gesendet(void);

// Tests: wie nach einem Boot.
void boulder_ausgabe_test_neustart(void);
