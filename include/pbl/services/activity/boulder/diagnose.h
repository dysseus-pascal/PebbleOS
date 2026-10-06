/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Boulder V2: der taegliche Diagnosesatz (Spezifikation 7.1, 7.2, F8; AP9).
//
// WOZU: Kriterium 7 ("kein Rechenschritt > 100 ms", keine Abstuerze) wird in QEMU gemessen;
// auf der Uhr gibt es erst im Geraetetest Werte, und den Coredump holt erst der App-Schritt
// (F8, 3A). Bis dahin fuehrt die Uhr je Tag einen Satz: laengster Rechenschritt mit Namen,
// laengster ganzer Minutenteil, kleinster freier Kernel-Heap, wenigste freie Plaetze der
// KernelBG-Schlange, Neustarts. Er steht in "boulder_v2" (Schluessel "diag"), geht beim
// Tageswechsel ins Log, und `boulder zeiten` zeigt ihn mit allen gemessenen Arbeiten.
//
// BILLIG: Die Laufzeiten misst takt.c je Arbeit (zwei Vergleiche). Hier liest nur der Takt
// einmal je Minute den Heap-Niedrigstand (heap_get_minimum_headroom, O(1), nie die
// Blockliste) und rechnet den Satz aus hoechstens BOULDER_MESSUNG_MAX Eintraegen. Geschrieben
// wird in der Ablage (nur bei Aenderung), gelesen in "Laden" - nie im Minutenteil.

#include "pbl/kernel/compiler.h"
#include "pbl/services/activity/boulder/takt.h"
#include "pbl/services/settings/settings_file.h"

#include <stdint.h>
#include <time.h>

#define BOULDER_DIAGNOSE_FASSUNG 1
#define BOULDER_DIAGNOSE_NAME 16

typedef struct PBL_PACKED {
  uint8_t fassung;
  uint16_t tag;                         //< time_util_get_day: Tage seit 1970
  uint16_t neustarts;                   //< "Laden" an diesem Tag (einmal je Boot)
  uint16_t max_ms;                      //< laengster Rechenschritt
  char max_name[BOULDER_DIAGNOSE_NAME]; //< dessen Name ohne "Boulder "
  uint16_t minute_max_ms;               //< laengster ganzer Minutenteil (Kopie + Variante)
  uint32_t heap_min;                    //< kleinster freier Kernel-Heap in Byte
  uint8_t schlange_min;                 //< wenigste freie Plaetze der KernelBG-Schlange
} BoulderDiagnoseTag;

// Im Takt (KernelBG, 1/min): Heap lesen, Satz nachfuehren, beim Tageswechsel ins Log.
void boulder_diagnose_takt(time_t jetzt);

// In "Laden" (einmal je Boot): den Satz des Tages lesen und den Neustart zaehlen. `datei`
// darf NULL sein (Datei nicht offen): dann beginnt der Tag neu.
void boulder_diagnose_laden(SettingsFile *datei, time_t jetzt);

// In der Ablage: den Satz schreiben, wenn er sich seit dem letzten Schreiben geaendert hat.
void boulder_diagnose_ablegen(SettingsFile *datei);

const BoulderDiagnoseTag *boulder_diagnose_heute(void);

// Tests: wie nach einem Boot.
void boulder_diagnose_test_zuruecksetzen(void);
