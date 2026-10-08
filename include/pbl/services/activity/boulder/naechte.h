/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Boulder V2, Schritt 3: HRV und Ruhepuls der letzten Wachtage fuer die Health-App
// (Spezifikation Schritt 3, 3.8.1, Entscheid F2; S5 in schnittstelle.h).
//
// NUR DATUM, HRV UND RUHEPULS (F2): Das Ruhepuls-Urteil und das HRV-Band brauchen die frueheren
// Naechte, sonst nichts. Die Uhr behaelt von ihnen keine Schlafdauer, keine Phasen, keine Zeiten.
//
// WIE BOULDER (Gesundheitsrechnung.kt, nachtTag, jeTag, jeTagMitHrv): Eine Nacht gehoert zum
// Ortsdatum ihres Aufwachens (Wachtag). Je Wachtag zaehlt fuer den Ruhepuls die laengste Nacht,
// fuer die HRV die laengste Nacht mit HRV; bei gleicher Laenge die fruehere (Boulder sortiert nach
// Beginn). Die beiden Schlafminuten im Kopf der Liste sind nur dafuer da und gelten fuer den
// juengsten Wachtag.
//
// EINGETRAGEN WIRD, WENN EINE NACHT ENDGUELTIG WIRD (nacht.c): Bis dahin kann sie noch wachsen. So
// steht jede Nacht genau einmal mit ihren letzten Werten da - denselben wie im NachtBild.
// Nickerchen werden nie endgueltig und zaehlen nicht (V5).
//
// UHR ZURUECK (nach einem Neustart mit falscher Zeit): Ein aelterer Wachtag, den die Liste noch
// nicht hat, wird einsortiert. Hat sie ihn schon, bleibt der Eintrag - wie lang seine Nacht war,
// weiss die Uhr nicht (F2), Boulder naehme die laengere. Geloescht wird dabei nie etwas.
//
// ABLAGE: im RAM unter einer eigenen Blattsperre (S5 liest), in der Datei boulder_v2 unter
// "naechte" (230 B), geschrieben von der Ablage nach jeder Aenderung, gelesen in "Laden". Nur
// KernelBG schreibt die Liste.
//
// WECHSEL b26 -> V2: Die Liste beginnt mit der Nacht, die der Umzug von b26 uebernimmt (umzug.c).
// Daten vom Telefon kommen nie dazu.

#include "pbl/services/activity/boulder/nacht_satz.h"
#include "pbl/services/activity/boulder/schnittstelle.h"
#include "pbl/services/settings/settings_file.h"

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

// KernelBG, wenn eine Nacht endgueltig wird: aufgewacht um `aufwachen_utc` nach `schlaf_min`
// Minuten Schlaf, mit ihrer HRV und ihrem Ruhepuls (0 = keine). Ohne Datei und Heap.
void boulder_naechte_eintragen(time_t aufwachen_utc, uint16_t schlaf_min, uint16_t hrv,
                               uint8_t ruhepuls);

// Umzug von b26, im Callback "Laden" nach boulder_naechte_laden: die Nacht des NachtBilds.
void boulder_naechte_saat(const NachtBild *bild);

// S5: die Liste in den Puffer des Aufrufers, unter der Sperre nur die Kopie. false, solange
// "Laden" sie seit dem Boot nicht gelesen hat (dann eine leere Liste).
bool boulder_naechte_kopieren(BoulderNaechte *aus);

// "Laden": die Liste aus der Datei (NULL: keine Datei, die Liste beginnt leer).
void boulder_naechte_laden(SettingsFile *datei);

// Ablage: "naechte" schreiben, wenn sich die Liste seit dem letzten Schreiben geaendert hat.
void boulder_naechte_ablegen(SettingsFile *datei);

// Pruefbefehl: die Liste ohne Kopie. Nur auf KernelBG - nur er schreibt sie.
const BoulderNaechte *boulder_naechte_kern(void);

// Tests: wie nach einem Boot (nicht gelesen). Nicht in boulder_dauerteil_test_neustart: Der
// Pruefbau beginnt damit jede Pruefnacht neu, liest die Liste danach aber nicht wieder.
void boulder_naechte_test_neustart(void);
