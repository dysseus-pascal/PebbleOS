/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/kernel/compiler.h"

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
// gerechnet; fertig ist sie nach einer Stunde wach oder am Ende des Fensters.

#define NACHT_DLS_VERSION 1
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
  uint32_t beginn_utc;     //< Minute 0: Beginn des Nachtfensters
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
} NachtDlsRecord;

// Boulder liest den Satz Byte fuer Byte (NachtParser.kt); aendert sich die
// Groesse, muss dort dieselbe Aenderung hin - und NACHT_DLS_VERSION hoch.
_Static_assert(sizeof(NachtDlsRecord) == 324, "NachtDlsRecord: Boulder erwartet 324 Byte");

// Jede Minute aus dem Aktivitaetsdienst (KernelBG), ausserhalb seiner Sperre.
void boulder_nacht_minute(time_t utc_sec);
