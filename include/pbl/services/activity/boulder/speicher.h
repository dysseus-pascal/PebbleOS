/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Boulder V2: was einen Neustart uebersteht (Spezifikation 7.3, AP4), in der Datei
// "boulder_v2" (8 KiB).
//
// KLEIN UND NUR ANGEHAENGT: settings_file haengt jeden Schreibvorgang an und verdichtet erst,
// wenn die Datei voll ist (settings_file.c:222-232, Heap und bis 60 s Waechter-Pause). Deshalb
// schreibt die Ablage nur, was sich geaendert hat, und das meiste nur einmal je Nacht:
// - "kopf": Lebenslauf, zuletzt Geschicktes, Abschluss - nur bei Aenderung (rund 40 B);
// - "a00".."a07": das Nachtbuch in Bloecken zu 120 Minuten, wenn ein Block fest ist, und bei
//   "fertig"/Aenderung (die Endfassung kann fruehere Minuten umschreiben);
// - "h00".."h23": die HRV-Fenster zu je 4, sobald eines dazukam;
// - "p00".."p16": das Protokoll zu je 4 Takten, sobald einer dazukam;
// - "r00".."r03": der Ringrest - die Minuten der laufenden Nacht als Ring von vier
//   Viertelstunden, solange die Nacht Schlaf hat. Pebbles Minutendatei bekommt ihre Gruppen
//   erst, wenn Kraepelin sie freigibt (D3, D12); was dort nach einem Absturz fehlt, steht hier.
// Jeder Block traegt den Fensterbeginn: Bloecke eines anderen Fensters gelten nicht.
//
// NACH DEM BOOT: init fuellt den Eingang der Nacht aus Pebbles Minutendatei (D14). Der
// Callback "Laden" liest dann Kopf und Bloecke, setzt den Ringrest darauf, rechnet den
// Automaten nach und legt die Sitzungen der Nacht mit ihren Starts neu an (nacht.c).
//
// Alles auf KernelBG, in eigenen Callbacks; nie im Minutenteil.

#include "pbl/kernel/compiler.h"

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#define SPEICHER_DATEI "boulder_v2"
#define SPEICHER_DATEI_GROESSE (8 * 1024)
#define SPEICHER_FASSUNG 1

typedef struct PBL_PACKED {
  uint8_t fassung;
  uint32_t fenster;          //< Beginn des Nachtfensters, fuer das der Lebenslauf gilt
  // Lebenslauf (nacht.c)
  uint8_t stufe;             //< NachtStufe
  uint16_t ab;               //< Beginn der laufenden Nacht (Folgenacht > 0)
  uint16_t fertig_auf;
  uint8_t ruhepuls;
  uint16_t tag_ab;
  uint16_t buch_bis;         //< bis hier ist das Buch fest (Bloecke koennen weniger haben)
  // Die zuletzt endgueltige Nacht des Fensters, solange ihr Abschluss laeuft
  uint8_t alt_offen;
  uint16_t alt_ab;
  uint16_t alt_auf;
  uint16_t alt_fest;
  // Ausgabe
  uint32_t gesendet_beginn;
  uint16_t gesendet_aufwachen;
  uint16_t gesendet_schlaf;
  uint8_t geladen;
} BoulderKopf;

// Die Ablage anstossen: `alles` auch die angefangenen Bloecke (bei "fertig", Senden,
// endgueltig), sonst nur volle und den Kopf. Der Callback faellt nie in den Minutenteil.
void boulder_speicher_anstossen(bool alles);

// Im Callback "Laden" (nach dem ersten init): Kopf und Bloecke lesen und an nacht.c geben.
void boulder_speicher_laden(void);

// Tests: wie nach einem Boot (die Datei bleibt).
void boulder_speicher_test_neustart(void);
