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
//
// MESSUNG (AP9, Kriterium 7): Jede Arbeit misst ihre Laufzeit selbst (Hoechst- und
// Gesamtwert, Zahl der Laeufe). BOULDER_AUFGABE_LAEUFT als erste Zeile eines Callbacks legt
// eine Variable mit __attribute__((cleanup)) an: Die Messung endet bei JEDEM Verlassen der
// Funktion, auch bei den fruehen Rueckspruengen, ohne dass jeder Ausgang sie kennen muss. Der
// Callback bleibt eine eigene Funktion je Arbeit - ein Waechter-Grund nennt so weiter ihren
// Namen (system_task/service.c:114 merkt sich den Callback, nicht einen Verteiler).

#include "pbl/services/system_task.h"
#include <pbl/drivers/rtc.h>

#include <stdbool.h>
#include <time.h>

// So viel Platz muss in der KernelBG-Schlange frei bleiben (system_task.h:124).
#define BOULDER_SCHLANGE_FREI 8

typedef struct {
  const char *name;
  SystemTaskEventCallback lauf;  //< NULL: nur gemessen (Minutenteil, Dialog), nie eingereiht
  bool steht_an;  //< eingereiht und noch nicht gelaufen
  // Messung (diagnose.c): in ms nach rtc_get_ticks, Hoechstwert seit Boot und heute.
  bool gemeldet;      //< steht in der Liste der Diagnose
  uint16_t max_ms;
  uint16_t tag_max_ms;
  uint32_t summe_ticks;  //< fuer den Mittelwert unter 1 ms
  uint32_t laeufe;
} BoulderAufgabe;

#define BOULDER_AUFGABE(n, f) {.name = (n), .lauf = (f), .steht_an = false}

typedef struct {
  BoulderAufgabe *aufgabe;
  RtcTicks beginn;
} BoulderLauf;

// Reiht die Arbeit ein, wenn sie nicht schon ansteht und Platz ist. Rueckgabe: steht an.
bool boulder_aufgabe_einreihen(BoulderAufgabe *aufgabe);

// Erste Zeile jedes Callbacks: ab hier darf die Arbeit wieder eingereiht werden, und die
// Messung laeuft bis zum Verlassen der Funktion.
#define BOULDER_AUFGABE_LAEUFT(aufgabe)                                              \
  BoulderLauf boulder_lauf_ __attribute__((cleanup(boulder_aufgabe_endet), unused)) = \
      boulder_aufgabe_beginnt(aufgabe)

BoulderLauf boulder_aufgabe_beginnt(BoulderAufgabe *aufgabe);
void boulder_aufgabe_endet(BoulderLauf *lauf);

// Die gemessenen Arbeiten in der Reihenfolge ihres ersten Laufs (fuer Diagnose und Shell).
// Mit Kieselsport je Befehl, Pulsrueckruf und Beschleunigung (AP7) sind es 24 Namen, so viele
// wie bisher Platz hatten - also mit Luft, denn was nicht mehr in die Liste passt, misst niemand.
#define BOULDER_MESSUNG_MAX 32
int boulder_messung_liste(BoulderAufgabe *const **liste);

// Der ganze Minutenteil (Kopie und Variante, D16 in der Kopie): nur gemessen.
BoulderAufgabe *boulder_messung_minutenteil(void);

// Wenigste freie Plaetze der KernelBG-Schlange, gesehen beim Einreihen.
uint32_t boulder_messung_schlange_min(void);

// Neuer Tag: die Tageshoechstwerte und den Schlangen-Niedrigstand neu beginnen.
void boulder_messung_tag_neu(void);

// Der Takt des Dauerteils (Callback "Boulder Takt", Tests direkt): Luecken, Protokoll,
// Ablage, Schub ans Telefon, "Laden" nachholen.
void boulder_dauerteil_takt(time_t jetzt);
