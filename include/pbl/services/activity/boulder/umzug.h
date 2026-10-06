/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Boulder V2: der Wechsel von b25/b26 (Spezifikation 8, Kriterium 8, F1).
//
// WAS b25 HINTERLAESST und was daraus wird:
// - boulder_nacht/"zustand" (18 B): was zuletzt geschickt wurde und bis wohin das Fenster
//   erledigt ist. Einmal gelesen, solange boulder_v2 noch keinen Kopf hat: dieselbe Nacht
//   geht nicht noch einmal ans Telefon, und im selben Fenster entsteht keine zweite Nacht.
// - boulder_nacht/"letzte" (NachtBild 0/1): liest ausgabe.c und schreibt es fort.
// - boulder_hrv (Ring der HRV-Fenster): nur in die laufende, offene Nacht uebernommen.
// - Pebbles Sitzungsliste: mit einer Movement-Sitzung (Typ 8) nullt Pebble sie beim Boot
//   und startet einmal neu (activity_sessions.c:612-626, F1 hingenommen). Ohne Typ 8 steht
//   dort b25s Nachtblock (Einschlafen bis Aufwachen, das Wachliegen darin). In beiden Faellen
//   legt der Umzug die letzte b25-Nacht als Zyklen aus dem NachtBild an, solange sie in
//   Pebbles laufenden Schlaftag faellt; den Block schreibt er ueber denselben Start in den
//   ersten Zyklus um, b25s Tiefschlaf ebenso je Abschnitt (ohne b25s zusammengelegte Luecken).
//   Abgeschlossen und nie spaeter als Pebbles Merker der zuletzt geloggten Enden
//   (activity.c:1422-1433): DLS 84 sendet nichts davon noch einmal.
// - Die Messstufe liegt unter demselben Schluessel in shellpref (messstufe.c).
// - Die b25-Dateien bleiben, wie sie sind; nur "letzte" wird fortgeschrieben.
//
// WARUM DIE ZYKLEN BEI JEDEM BOOT: Pebble sichert die Liste nur alle 15 Minuten
// (activity.c:799-846). Ein Neustart davor verloere sie; das Anlegen mit denselben Starts ist
// idempotent. Welche Nacht dem Umzug gehoert, merkt sich boulder_v2 ("umzug").
//
// Alles auf KernelBG, im Callback "Laden" und im eigenen Callback "Umzug" danach; nie im init
// und nie im Minutenteil. Kein Heap.

#include "pbl/services/activity/boulder/speicher.h"

#include <stdbool.h>
#include <time.h>

// Im Callback "Laden", wenn boulder_v2 keinen Kopf hat (erster Start nach b25): den Kopf aus
// b25s Zustand fuer das Fenster ab `fenster`. Rueckgabe: es gab einen Zustand oder ein fertiges
// NachtBild (dann ist `kopf` gefuellt).
bool boulder_umzug_kopf(time_t fenster, BoulderKopf *kopf);

// Am Ende von "Laden": den Callback "Umzug" einreihen. `erster_start`: boulder_v2 hatte keinen
// Kopf - dann entscheidet der Umzug einmal, welche b25-Nacht ihm gehoert, und uebernimmt die
// HRV-Fenster der offenen Nacht.
void boulder_umzug_anstossen(bool erster_start);

// Tests: wie nach einem Boot.
// Im Callback "Umzug", bei jedem Boot: b25s eigene Prefs (tsBedrock, boulderTimeStyle) aus
// shellpref loeschen, damit V2s 2048 B Luft fuer Pebbles Prefs lassen (umzug_prefs.c).
// Rueckgabe: wie viele geloescht wurden.
int boulder_umzug_prefs_aufraeumen(void);

void boulder_umzug_test_neustart(void);
