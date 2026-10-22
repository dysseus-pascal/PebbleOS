/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/ui/vibes.h"
#include "pbl/services/activity/boulder/sport/kieselsport.h"

#include <stdbool.h>
#include <stdint.h>

// Die Atemuebung als Rechnung: wo sie steht, aus dem Rhythmus (kieselsport_atem) und der Zeit
// seit dem ersten Einatmen. Ohne Uhr und ohne Zeichnen; die App (lauf_window.c) fragt, zeichnet
// und brummt.
//
// EINE RUNDE sind die vier Phasen in der Folge von AtemWert. Eine Phase von 0 s faellt weg: Sie
// kommt nie als Stand vor und brummt darum auch nie.

typedef struct {
  AtemWert phase;     //< AtemEin bis AtemLeer
  uint8_t runde;      //< 1 bis zur Zahl der Runden
  uint32_t dauer_ms;  //< Laenge der Phase
  uint32_t rest_ms;   //< bis zur naechsten Phase, 1 bis dauer_ms; 0 nach dem Ende
  bool fertig;        //< alle Runden um; dann steht da die letzte Phase der letzten Runde
} Atemstand;

uint32_t atem_runde_ms(const Atemrhythmus *r);
uint32_t atem_gesamt_ms(const Atemrhythmus *r);

// Wo die Uebung nach [ms] steht. Die naechste Phase gilt ab ihrer ersten Millisekunde.
Atemstand atem_stand(const Atemrhythmus *r, uint32_t ms);

// Die Sekunden, die von der Phase bleiben, aufgerundet wie ein Countdown: 4, 3, 2, 1.
uint32_t atem_rest_s(const Atemstand *s);

// Der Kreis waechst beim Einatmen von [r_min] auf [r_max], mit Pebbles ease-in-out, steht beim
// Halten und schrumpft beim Ausatmen; nach dem Ende [r_min].
int16_t atem_radius(const Atemstand *s, int16_t r_min, int16_t r_max);

// Wann sich das Bild wieder aendert, in ms: im Atmen nach einem Bild (gut 15 je Sekunde), beim
// Halten mit der naechsten Sekunde; nie nach dem Ende der Phase. 0 nach dem Ende der Uebung.
uint32_t atem_naechstes_bild_ms(const Atemstand *s);

// Das Brummen zum Beginn einer Phase, mit geschlossenen Augen zu unterscheiden: einatmen ein
// kurzes, halten zwei sehr kurze, ausatmen ein langes. Dazu das Ende der letzten Runde.
VibePattern atem_muster(AtemWert phase);
VibePattern atem_muster_ende(void);
