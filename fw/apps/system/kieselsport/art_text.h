/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "pebble_compat.h"
#include "kachel.h"

// Die Namen der Sportarten. Was eine Art kann, weiss der Dienst
// (kieselsport_art_info); wie sie heisst, nur die App - der Dienst zeigt
// nichts an.

// art_name() steht in kachel.h - die Kachel im Startmenue braucht ihn auch.

// Der Obertitel im Untermenue: der Name der Gruppe ("Bike", "Swimming"), sonst NULL.
const char *art_gruppe(Sportart art);

// --- Die Menues ---

// Die Eintraege im Hauptmenue, die ein Untermenue oeffnen. Sie liegen hinter den Arten, damit ein
// Eintrag beides sein kann.
#define MENUE_BIKE ArtAnzahl
#define MENUE_SCHWIMMEN (ArtAnzahl + 1)

uint16_t menue_zeilen(void);
// Eine Sportart oder eine Gruppe.
int menue_eintrag(uint16_t zeile);
bool menue_ist_gruppe(int eintrag);

// Eine Gruppe: ihr Name und ihre Arten in der Reihenfolge des Untermenues. Ihre Zeile im
// Hauptmenue traegt Bild und Farbe der ersten Art.
const char *menue_gruppe_name(int gruppe);
uint16_t menue_gruppe_zeilen(int gruppe);
Sportart menue_gruppe_art(int gruppe, uint16_t zeile);
