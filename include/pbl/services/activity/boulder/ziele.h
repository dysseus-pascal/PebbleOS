/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Boulder V2, Schritt 3: die Ziele fuer Schritte und Schlaf (Spezifikation Schritt 3, 3.8.4,
// Entscheid "Ziele", F10 = A).
//
// EINGESTELLT WIRD IN BOULDER, nicht auf der Uhr (V15): Das Telefon schreibt den Satz
// BoulderZieleSatz (schnittstelle.h) unter "boulder_ziele" (13 B, ohne Nullbyte) in die BlobDB
// Health. Pebbles
// healthdb speichert jeden Schluessel mit "_", und dieser aendert keine Metrik (health_db.c,
// health_db_insert und prv_notify_health_listeners). So braucht der Weg keine Pebble-Zeile, und
// libpebble3 gleicht die Zeile von selbst ab. Die Uhr schreibt die Ziele nie.
//
// GELESEN WIRD AUF KERNELBG im eigenen Auftrag "Boulder Ziele" (Datei und Heap): nach dem ersten
// Minutenlauf, nach jedem Einfuegen oder Loeschen des Schluessels, nach jedem Leeren der
// Datenbank Health (das Telefon leert bei neuer Kopplung und sendet dann alles neu) und zur
// Sicherheit in jeder Ablage. Danach traegt die Lage die Ziele zur App.
//
// GUELTIG ist nur ein Satz mit genau 8 B, Fassung 1, 1 000-50 000 Schritten und 240-720 min.
// Sonst, ohne Satz und nach dem Leeren gilt die Vorgabe 10 000 und 8:00 h - dieselbe wie Boulder.

#include "pbl/services/activity/boulder/schnittstelle.h"

#include <stdbool.h>
#include <stdint.h>

#define BOULDER_ZIELE_SCHLUESSEL "boulder_ziele"
#define BOULDER_ZIELE_FASSUNG 1
#define ZIEL_SCHRITTE_VORGABE 10000
#define ZIEL_SCHRITTE_MIN 1000
#define ZIEL_SCHRITTE_MAX 50000
#define ZIEL_SCHLAF_VORGABE 480
#define ZIEL_SCHLAF_MIN 240
#define ZIEL_SCHLAF_MAX 720

// Die geltenden Ziele (KernelBG, O(1), ohne Datei).
uint16_t boulder_ziel_schritte(void);
uint16_t boulder_ziel_schlaf_min(void);

// Gilt dieser Satz von `laenge` Byte? Rein, fuer den Auftrag und die Tests.
bool boulder_ziele_gueltig(const BoulderZieleSatz *satz, int laenge);

// Minutenteil: beim ersten Mal und solange ein Lesen aussteht, den Auftrag einreihen. O(1).
void boulder_ziele_minute(void);
// Ablage: zur Sicherheit neu lesen.
void boulder_ziele_nachlesen(void);

// Tests: wie nach einem Boot (Vorgabe, nichts gelesen, kein Abo).
void boulder_ziele_test_neustart(void);
