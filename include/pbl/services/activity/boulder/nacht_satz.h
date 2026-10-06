/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/kernel/compiler.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// DIE NACHT FUER BOULDER - Formate unveraendert aus b26 (main a7d8b16e1,
// include/pbl/services/activity/boulder/nacht.h). Boulder liest die Saetze Byte
// fuer Byte (NachtParser.kt, NachtHrvParser.kt); die Brueckenfassung 0.50 muss
// sie von V2 genau so bekommen. Aendert sich hier etwas, bricht der Bau
// (Static-Asserts unten) und test_boulder_vertraege.

// Eigene Datalogging-Tags ab 0x4000. 88 und 89 (bis b24) bleiben gesperrt:
// Sitzungen von b24 liegen womoeglich noch im Flash.
#define BOULDER_DLS_TAG_NACHT 0x4001u      //< NachtDlsRecord, bis b24 Tag 88
#define BOULDER_DLS_TAG_NACHT_HRV 0x4002u  //< NachtHrvDlsRecord, bis b24 Tag 89
#define BOULDER_DLS_TAG_NACHT_ALT 88u
#define BOULDER_DLS_TAG_NACHT_HRV_ALT 89u

// Bit 0 gesetzt wie bei den Sitzungen; Fassung 3 mit Akku und Messstufe am Ende.
#define NACHT_DLS_VERSION 3
#define NACHT_DLS_ABSCHNITTE 60

typedef struct PBL_PACKED {
  uint16_t von;    //< Minuten ab beginn_utc
  uint16_t bis;
  uint8_t phase;   //< 0 wach, 1 leicht, 2 tief, 3 REM, 4 Schlaf ohne Phase
} NachtDlsAbschnitt;

// Ein Satz auf BOULDER_DLS_TAG_NACHT. Mehr als NACHT_DLS_ABSCHNITTE Abschnitte
// gehen in mehreren Teilen; die Kopfwerte stehen in jedem.
typedef struct PBL_PACKED {
  uint16_t version;        //< NACHT_DLS_VERSION
  uint16_t size;           //< sizeof(NachtDlsRecord)
  uint32_t beginn_utc;     //< Minute 0 der Rechnung
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
  uint8_t akku_beginn;     //< % beim Einschlafen, 0xFF = unbekannt
  uint8_t akku_ende;       //< % als die Nacht fertig war
  uint8_t flags;           //< NACHT_DLS_FLAG_GELADEN
  uint8_t messstufe;       //< 1..5
} NachtDlsRecord;

#define NACHT_DLS_FLAG_GELADEN 0x01

#define NACHT_HRV_DLS_VERSION 1
#define NACHT_HRV_DLS_FENSTER 48

typedef struct PBL_PACKED {
  uint16_t minute;   //< Beginn des Fensters (fuenf Minuten), Minuten ab beginn_utc
  uint16_t rmssd;    //< ms
  uint8_t puls;      //< bpm, 0 = unbekannt
} NachtHrvDlsFenster;

typedef struct PBL_PACKED {
  uint16_t version;        //< NACHT_HRV_DLS_VERSION
  uint16_t size;           //< sizeof(NachtHrvDlsRecord)
  uint32_t beginn_utc;     //< derselbe wie im Satz der Nacht
  uint8_t teil;
  uint8_t teile;
  uint8_t anzahl;          //< Fenster in diesem Teil
  NachtHrvDlsFenster fenster[NACHT_HRV_DLS_FENSTER];
} NachtHrvDlsRecord;

// Die letzte Nacht fuer die Uhr selbst (Datei boulder_nacht, Schluessel "letzte").
#define NACHT_BILD_ABSCHNITTE 96
#define NACHT_PHASEN 5
#define NACHT_BILD_VERSION 1
#define NACHT_BILD_VERSION_B24 0

typedef struct {
  uint32_t beginn_utc;
  int32_t utc_to_local;
  uint16_t einschlafen;
  uint16_t aufwachen;
  uint16_t schlaf_minuten;
  uint16_t minuten[NACHT_PHASEN];
  uint16_t hrv;
  uint8_t ruhepuls;
  bool fertig;
  uint8_t anzahl;
  NachtDlsAbschnitt abschnitte[NACHT_BILD_ABSCHNITTE];
  uint8_t version;         //< in b24 ein Fuellbyte (0)
} NachtBild;

// Der Aufbau Byte fuer Byte wie in b26.
_Static_assert(sizeof(NachtDlsAbschnitt) == 5, "NachtDlsAbschnitt: 5 Byte");
_Static_assert(sizeof(NachtDlsRecord) == 328, "NachtDlsRecord: Boulder erwartet 328 Byte");
_Static_assert(offsetof(NachtDlsRecord, beginn_utc) == 4, "NachtDlsRecord.beginn_utc");
_Static_assert(offsetof(NachtDlsRecord, utc_to_local) == 8, "NachtDlsRecord.utc_to_local");
_Static_assert(offsetof(NachtDlsRecord, einschlafen) == 12, "NachtDlsRecord.einschlafen");
_Static_assert(offsetof(NachtDlsRecord, schlaf_minuten) == 16, "NachtDlsRecord.schlaf_minuten");
_Static_assert(offsetof(NachtDlsRecord, ruhepuls) == 18, "NachtDlsRecord.ruhepuls");
_Static_assert(offsetof(NachtDlsRecord, teil) == 19, "NachtDlsRecord.teil");
_Static_assert(offsetof(NachtDlsRecord, hrv) == 21, "NachtDlsRecord.hrv");
_Static_assert(offsetof(NachtDlsRecord, anzahl) == 23, "NachtDlsRecord.anzahl");
_Static_assert(offsetof(NachtDlsRecord, abschnitte) == 24, "NachtDlsRecord.abschnitte");
_Static_assert(offsetof(NachtDlsRecord, akku_beginn) == 324, "NachtDlsRecord.akku_beginn");
_Static_assert(offsetof(NachtDlsRecord, messstufe) == 327, "NachtDlsRecord.messstufe");
_Static_assert(sizeof(NachtHrvDlsFenster) == 5, "NachtHrvDlsFenster: 5 Byte");
_Static_assert(sizeof(NachtHrvDlsRecord) == 251, "NachtHrvDlsRecord: Boulder erwartet 251 Byte");
_Static_assert(offsetof(NachtHrvDlsRecord, teil) == 8, "NachtHrvDlsRecord.teil");
_Static_assert(offsetof(NachtHrvDlsRecord, fenster) == 11, "NachtHrvDlsRecord.fenster");
_Static_assert(sizeof(NachtBild) == 512, "NachtBild: Aufbau geaendert - NACHT_BILD_VERSION hoch");
_Static_assert(offsetof(NachtBild, abschnitte) == 29, "NachtBild: Abschnitte wie in b24");
_Static_assert(offsetof(NachtBild, version) == 509, "NachtBild: Fassung in den Fuellbytes von b24");
