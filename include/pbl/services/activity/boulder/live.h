/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Boulder V2: der Live-Automat (Spezifikation 4.2).
//
// WARUM EIN EIGENER AUTOMAT: Der Nachlauf schreibt eine Minute erst nach 75 bis 240
// Minuten fest. Pebbles Zustand "schlaeft" (TimeStyle, Wecker, Benachrichtigungen)
// braucht die Antwort sofort. Der Automat entscheidet jede Minute in O(1), mit zwei
// Minuten Nachlauf fuer Cole-Kripke, und liest dafuer die Nachtfelder (eingang.h).
//
// Regeln (PC-Probe strom/probe_live2.c, gemessen auf 29.9., 30.9., 3.10.):
// - Cole-Kripke ueber Minute-4..Minute+2 mit Gewichten und Schwelle wie schlafanalyse.c,
//   fehlende Nachbarn neu gewichtet; eine ungueltige Minute ist wach.
// - Puls ueber dem laufenden Median der Schlafpulse + 20 ist wach (ab 20 Werten).
// - "schlaeft" nach 10 Schlafminuten in Folge, "wach" nach 5 Wachminuten in Folge.
//
// STATISCH, KEIN HEAP, KEIN BETRIEBSSYSTEM.

#include <stdbool.h>
#include <stdint.h>

#define LIVE_NACHLAUF 2
#define LIVE_SCHLAF_FOLGE 10
#define LIVE_WACH_FOLGE 5
#define LIVE_PULS_UEBER 20
#define LIVE_PULS_AB 20

// Fensterbeginn: wach, Pulse vergessen.
void boulder_live_neu(void);

// Die Minute `minute` steht im Eingang (angenommen oder fehlend). Entscheidet die Minute
// `minute - LIVE_NACHLAUF`. Der Aufrufer ruft jede Minute in Folge auf, auch fehlende.
void boulder_live_minute(int minute);

bool boulder_live_schlaeft(void);

// Erste Minute des Laufs von LIVE_SCHLAF_FOLGE Schlafminuten, mit dem der Automat zuletzt
// eingeschlafen ist; -1 = noch nie.
int boulder_live_lauf_beginn(void);
