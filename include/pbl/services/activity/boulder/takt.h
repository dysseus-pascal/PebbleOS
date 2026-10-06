/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Boulder V2: benannte Arbeit auf KernelBG (Spezifikation 2.4, Regel 1).
//
// WARUM SO: Pebbles nicht verwerfbare Callbacks warten bei voller Schlange 3 s und
// setzen dann die Uhr zurueck (system_task.h:96-106). Die Variante reiht deshalb nur
// verwerfbar ein, nur bei genug freiem Platz und hoechstens einmal je Name. Jede Arbeit
// ist idempotent: Faellt ein Einreihen aus, holt es der naechste Anlass nach. Eigene
// Funktionen je Arbeit lassen einen Watchdog-Grund unter ihrem Namen erscheinen.
//
// Der eigene Takt (AP4) ist ein Cron zur Sekunde 10 (dauerteil.c); er reiht nur ein.

#include "pbl/services/system_task.h"

#include <stdbool.h>
#include <time.h>

// So viel Platz muss in der KernelBG-Schlange frei bleiben (system_task.h:124).
#define BOULDER_SCHLANGE_FREI 8

typedef struct {
  const char *name;
  SystemTaskEventCallback lauf;
  bool steht_an;  //< eingereiht und noch nicht gelaufen
} BoulderAufgabe;

#define BOULDER_AUFGABE(n, f) {.name = (n), .lauf = (f), .steht_an = false}

// Reiht die Arbeit ein, wenn sie nicht schon ansteht und Platz ist. Rueckgabe: steht an.
bool boulder_aufgabe_einreihen(BoulderAufgabe *aufgabe);

// Erste Zeile jedes Callbacks: ab hier darf die Arbeit wieder eingereiht werden.
void boulder_aufgabe_laeuft(BoulderAufgabe *aufgabe);

// Der Takt des Dauerteils (Callback "Boulder Takt", Tests direkt): Luecken, Protokoll,
// Ablage, Schub ans Telefon, "Laden" nachholen.
void boulder_dauerteil_takt(time_t jetzt);
