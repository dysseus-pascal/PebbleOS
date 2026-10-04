/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/kernel/compiler.h"
#include "pbl/services/activity/boulder/schlafanalyse.h"

#include <stdbool.h>
#include <stddef.h>
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
// Sitzungen gehen EIN Schlafblock je Nacht, vom Einschlafen bis zum Aufstehen,
// und die Tiefschlafbloecke darin - daraus rechnen die Uhr und andere Apps
// Schlafdauer und Tiefschlaf. Das Wachliegen im Block traegt die Sitzung mit
// (nicht_schlaf_min); die Schlafsumme der Uhr zaehlt es nicht. Die ganze Nacht
// mit allen Phasen, auch den Wachphasen, geht als ein Datalogging-Satz
// (BOULDER_DLS_TAG_NACHT) an Boulder.
//
// NUR IM NACHTFENSTER (20 bis 11 Uhr): eine abgelegte Uhr auf dem Tisch saehe
// tagsueber aus wie Schlaf. Alle fuenfzehn Minuten wird die laufende Nacht neu
// gerechnet; dazwischen waechst ihr Schlafblock mit, solange man am Ende der
// letzten Rechnung schlief. Fertig ist sie nach einer Viertelstunde wach oder
// am Ende des Fensters, endgueltig nach zwei Stunden wach - erst dann sind
// ihre Sitzungen abgeschlossen. Ist das Aufwachen schon zu sehen, wird
// gerechnet, sobald die Viertelstunde voll sein kann - nicht erst im naechsten
// Takt.
//
// EIN KIESELSPORT-TRAINING IST KEIN SCHLAF: seine Minuten zaehlen wie
// abgelegt (Yoga liegt still, aber niemand schlaeft dabei).

// DIE EIGENEN DATALOGGING-TAGS, ab 0x4000. Pebble vergibt seine System-Tags von
// 78 an aufwaerts (data_logging_service.h); 88 und 89, die Boulder bis b24
// nahm, waeren die naechsten - ein Upstream-Merge haette sie still doppelt
// belegt. Die Tags sind auf der Uhr, im Flash und im Protokoll 32 Bit breit.
// 88 und 89 bleiben fuer immer gesperrt: Sitzungen von b24 liegen womoeglich
// noch im Flash und gehen nach dem Update mit ihrem alten Tag hinaus. Die
// Saetze sind dieselben. Wache: boulder/vertraege.c, tools/boulder/vertraege.py.
#define BOULDER_DLS_TAG_NACHT 0x4001u      //< NachtDlsRecord, bis b24 Tag 88
#define BOULDER_DLS_TAG_NACHT_HRV 0x4002u  //< NachtHrvDlsRecord, bis b24 Tag 89
#define BOULDER_DLS_TAG_NACHT_ALT 88u
#define BOULDER_DLS_TAG_NACHT_HRV_ALT 89u

// Bitfeld wie bei den Sitzungen: Bit 0 muss gesetzt sein, Bit 1 heisst "mit
// Akku und Messstufe am Ende" (ab Fassung 3).
#define NACHT_DLS_VERSION 3
#define NACHT_DLS_ABSCHNITTE 60

typedef struct PBL_PACKED {
  uint16_t von;    //< Minuten ab beginn_utc
  uint16_t bis;
  uint8_t phase;   //< SchlafPhase
} NachtDlsAbschnitt;

// Ein Satz auf BOULDER_DLS_TAG_NACHT. Eine Nacht mit mehr als NACHT_DLS_ABSCHNITTE
// Abschnitten geht in mehreren Teilen; die Kopfwerte stehen in jedem.
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

// DIE HRV-FENSTER DER NACHT, auf BOULDER_DLS_TAG_NACHT_HRV. Ein eigener Satz
// statt eines laengeren fuer die Nacht: deren Satz bleibt Byte fuer Byte, wie
// ihn jeder Boulder kennt, und ein Boulder ohne HRV quittiert diesen und
// wirft ihn weg. Er geht mit jeder Nacht, im selben Augenblick und mit
// demselben beginn_utc; mehr als NACHT_HRV_DLS_FENSTER Fenster gehen in
// mehreren Teilen. Bit 0 der Version gesetzt, wie bei den Sitzungen.
#define NACHT_HRV_DLS_VERSION 1
#define NACHT_HRV_DLS_FENSTER 48

typedef struct PBL_PACKED {
  uint16_t minute;   //< Beginn des Fensters (fuenf Minuten), Minuten ab beginn_utc
  uint16_t rmssd;    //< ms
  uint8_t puls;      //< bpm aus den Schlagabstaenden, 0 = unbekannt
} NachtHrvDlsFenster;

typedef struct PBL_PACKED {
  uint16_t version;        //< NACHT_HRV_DLS_VERSION
  uint16_t size;           //< sizeof(NachtHrvDlsRecord)
  uint32_t beginn_utc;     //< derselbe wie im Satz der Nacht
  uint8_t teil;            //< 0 ..
  uint8_t teile;
  uint8_t anzahl;          //< Fenster in diesem Teil
  NachtHrvDlsFenster fenster[NACHT_HRV_DLS_FENSTER];
} NachtHrvDlsRecord;

// Boulder liest auch diesen Satz Byte fuer Byte (NachtHrvParser.kt).
_Static_assert(sizeof(NachtHrvDlsRecord) == 251, "NachtHrvDlsRecord: Boulder erwartet 251 Byte");

// Jede Minute aus dem Aktivitaetsdienst (KernelBG), ausserhalb seiner Sperre.
// DIE LETZTE NACHT FUER DIE UHR SELBST - die Health-App zeigt sie. Dieselben
// Zahlen wie im Satz fuer Boulder, dazu die Minuten je Phase und die
// Abschnitte fuer das Hypnogramm. Mehr als NACHT_BILD_ABSCHNITTE werden fuer
// die Anzeige verdichtet: der kuerzeste geht im Nachbarn auf.
#define NACHT_BILD_ABSCHNITTE 96
#define NACHT_PHASEN 5   //< SchlafPhase: Wach, Leicht, Tief, REM, Schlaf ohne Phase
// Das Bild liegt roh in einer Datei. AENDERT SICH SEIN AUFBAU, MUSS DIE FASSUNG
// HOCH: sonst laese die neue Firmware ein altes Bild gleicher Groesse falsch.
#define NACHT_BILD_VERSION 1
// FASSUNG 0 IST DAS BILD BIS b24: derselbe Aufbau, nur ohne Fassung. Wo sie
// jetzt steht, lagen dort Fuellbytes, und die waren immer 0 - b24 nullte das
// ganze Bild vor dem Fuellen (memset im obelix-Bau von nacht.c, geprueft am
// Maschinencode). So liest die neue Firmware das Bild der letzten Nacht weiter.
#define NACHT_BILD_VERSION_B24 0

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
  uint8_t version;         //< NACHT_BILD_VERSION; in b24 ein Fuellbyte (0)
} NachtBild;

// Der Aufbau von b24 bleibt Byte fuer Byte; die Fassung sitzt in seinen
// Fuellbytes. Nur mit einer neuen Fassung darf sich hier etwas verschieben.
_Static_assert(sizeof(NachtBild) == 512, "NachtBild: Aufbau geaendert - NACHT_BILD_VERSION hoch");
_Static_assert(offsetof(NachtBild, abschnitte) == 29,
               "NachtBild: Abschnitte nicht mehr wie in b24");
_Static_assert(offsetof(NachtBild, version) == 509,
               "NachtBild: Fassung nicht mehr in den Fuellbytes von b24");

// false, wenn es noch keine gibt.
bool boulder_nacht_letzte(NachtBild *aus);

void boulder_nacht_minute(time_t utc_sec);

#ifdef UNITTEST
// Wie ein Neustart der Uhr: alle Zustaende weg, die Datei bleibt.
void boulder_nacht_test_reset(void);
// Wie viel Heap eine Rechnung braucht.
size_t boulder_nacht_test_rechnung_groesse(void);
#endif

// Das Nachtfenster, in dem `utc` liegt - oder das zuletzt vergangene.
void boulder_nacht_fenster(time_t utc, time_t *beginn, time_t *ende);
// Liegt `utc` im Nachtfenster?
bool boulder_nacht_im_fenster(time_t utc);
