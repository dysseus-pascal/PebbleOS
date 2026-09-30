/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/kernel/compiler.h"

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

// Die Nacht in der Firmware: rechnet die laufende Nacht mit der Schlafanalyse
// (schlafanalyse.h) und traegt sie als Sitzungen ein.
//
// WARUM NEBEN KRAEPELIN: dessen Schlaferkennung liefert keine Sitzungen mehr
// (kraepelin_algorithm.c); ihre inneren Zustaende - "schlaeft gerade", Schritte
// im Schlaf auf null - bleiben, weil andere Teile der Firmware daran haengen.
//
// ZWEI AUSGABEN, WEIL DIE UHR HOECHSTENS 32 SITZUNGEN JE TAG HAELT. Als
// Sitzungen gehen nur die zusammenhaengenden Schlafbloecke (an Wachphasen
// geteilt) und die Tiefschlaf-Abschnitte - daraus rechnen die Uhr und andere
// Apps Schlafdauer und Tiefschlaf. Die ganze Nacht mit allen Phasen geht als
// ein Datalogging-Satz (Tag 88) an Boulder.
//
// NUR IM NACHTFENSTER (20 bis 11 Uhr): eine abgelegte Uhr auf dem Tisch saehe
// tagsueber aus wie Schlaf. Alle fuenfzehn Minuten wird die laufende Nacht neu
// gerechnet; fertig ist sie nach einer Viertelstunde wach oder am Ende des
// Fensters.

// Bitfeld wie bei den Sitzungen: Bit 0 muss gesetzt sein, Bit 1 heisst "mit
// Akku und Messstufe am Ende" (ab Fassung 3).
#define NACHT_DLS_VERSION 3
#define NACHT_DLS_ABSCHNITTE 60

typedef struct PBL_PACKED {
  uint16_t von;    //< Minuten ab beginn_utc
  uint16_t bis;
  uint8_t phase;   //< SchlafPhase
} NachtDlsAbschnitt;

// Ein Satz auf Tag 88. Eine Nacht mit mehr als NACHT_DLS_ABSCHNITTE Abschnitten
// geht in mehreren Teilen; die Kopfwerte stehen in jedem.
typedef struct PBL_PACKED {
  uint16_t version;        //< NACHT_DLS_VERSION; Bit 0 gesetzt, wie bei den Sitzungen
  uint16_t size;           //< sizeof(NachtDlsRecord)
  uint32_t beginn_utc;     //< Minute 0: Beginn der Rechnung (Fenster oder letztes Aufwachen)
  int32_t utc_to_local;
  uint16_t einschlafen;    //< Minuten ab beginn_utc
  uint16_t aufwachen;
  uint16_t schlaf_minuten;
  uint8_t ruhepuls;        //< 0 = keiner
  uint8_t teil;            //< 0 ..
  uint8_t teile;
  uint16_t hrv;            //< ms, 0 = keine
  uint8_t anzahl;          //< Abschnitte in diesem Teil
  NachtDlsAbschnitt abschnitte[NACHT_DLS_ABSCHNITTE];
  // Ab Fassung 3: was die Nacht den Akku kostete, und bei welcher Messstufe.
  // So laesst sich der Regler an echten Naechten eichen statt an Schaetzungen.
  uint8_t akku_beginn;     //< % beim Einschlafen (erste Rechnung mit Schlaf), 0xFF = unbekannt
  uint8_t akku_ende;       //< % als die Nacht fertig war
  uint8_t flags;           //< Bit 0: nachts am Ladekabel - dann sagt der Verbrauch nichts
  uint8_t messstufe;       //< 1..5
} NachtDlsRecord;

// Boulder liest den Satz Byte fuer Byte (NachtParser.kt); aendert sich die
// Groesse, muss dort dieselbe Aenderung hin - und NACHT_DLS_VERSION hoch.
_Static_assert(sizeof(NachtDlsRecord) == 328, "NachtDlsRecord: Boulder erwartet 328 Byte");

#define NACHT_DLS_FLAG_GELADEN 0x01

// Jede Minute aus dem Aktivitaetsdienst (KernelBG), ausserhalb seiner Sperre.
// DIE LETZTE NACHT FUER DIE UHR SELBST - die Health-App zeigt sie. Dieselben
// Zahlen wie im Satz fuer Boulder, dazu die Minuten je Phase und die
// Abschnitte fuer das Hypnogramm. Mehr als NACHT_BILD_ABSCHNITTE werden fuer
// die Anzeige verdichtet: der kuerzeste geht im Nachbarn auf.
#define NACHT_BILD_ABSCHNITTE 96
#define NACHT_PHASEN 5   //< SchlafPhase: Wach, Leicht, Tief, REM, Schlaf ohne Phase

typedef struct {
  uint32_t beginn_utc;     //< Minute 0 der Rechnung
  int32_t utc_to_local;
  uint16_t einschlafen;    //< Minuten ab beginn_utc
  uint16_t aufwachen;
  uint16_t schlaf_minuten;
  uint16_t minuten[NACHT_PHASEN];
  uint16_t hrv;            //< ms, 0 = keine
  uint8_t ruhepuls;        //< 0 = keiner
  bool fertig;             //< false: die Nacht laeuft noch
  uint8_t anzahl;
  NachtDlsAbschnitt abschnitte[NACHT_BILD_ABSCHNITTE];
} NachtBild;

// false, wenn es noch keine gibt.
bool boulder_nacht_letzte(NachtBild *aus);

// Testhilfe: verdichtet `a` auf hoechstens `max` Abschnitte, Rueckgabe: wie viele.
#include "pbl/services/activity/boulder/schlafanalyse.h"
int boulder_nacht_verdichten(SchlafAbschnitt *a, int n, int max);

void boulder_nacht_minute(time_t utc_sec);

#ifdef UNITTEST
// Wie ein Neustart der Uhr: alle Zustaende weg, die Datei bleibt.
void boulder_nacht_test_reset(void);
#endif

// Das Nachtfenster, in dem `utc` liegt - oder das zuletzt vergangene.
void boulder_nacht_fenster(time_t utc, time_t *beginn, time_t *ende);
// Liegt `utc` im Nachtfenster?
bool boulder_nacht_im_fenster(time_t utc);
