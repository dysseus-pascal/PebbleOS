/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Boulder V2: der Lebenslauf einer Nacht und die Nickerchen des Tages (Spezifikation 4.4,
// 4.7). Stand AP3: was die Zyklen brauchen - Fenster, Rechnen, fertig, waechst,
// endgueltig, Folgenacht. Saetze 0x4001/0x4002, NachtBild, Protokoll und Persistenz kommen
// mit AP4 (ausgabe.c, speicher.c) und haengen sich an "fertig".
//
// DAS FENSTER ist ein Tag ab 20 Uhr Ortszeit (beginn_utc, Versatz am Fensterbeginn), in
// Minuten: 0..899 die Nacht (20-11 Uhr), 900..1439 der Tag (11-20 Uhr) fuer Nickerchen.
// WARUM DER TAG IN DEN NACHTFELDERN: Nach dem Fensterende ist die Nacht endgueltig und ihre
// Felder sind frei - die Nickerchen brauchen so keinen eigenen Speicher (Spezifikation 7.2
// sah 1,7 KB vor).
//
// ZWEI TEILE: boulder_nacht_minute laeuft im Minutenteil (O(1): Eingang, Automat,
// Verlaengern) und sagt, wann der Takt faellig ist. boulder_nacht_takt laeuft im eigenen
// Callback "Nachlauf" (Fensterrechnung, Endfassung, Abgleich, Nickerchen). Beides auf
// KernelBG, kein Heap, keine Datei.

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#define NACHT_MINUTEN (15 * 60)
#define NACHT_FENSTER_MINUTEN (24 * 60)
// Gerechnet wird ab einer Stunde nach Fensterbeginn (wie b25/b26).
#define NACHT_RECHNEN_AB 60
// So lange wach (an vorhandenen Minuten), und die Nacht ist fertig bzw. endgueltig.
#define NACHT_WACH_FERTIG 15
#define NACHT_WACH_ENDGUELTIG 120
// Schlaf nach einer endgueltigen Nacht ist erst ab so viel Schlaf eine eigene Nacht.
#define NACHT_FOLGENACHT 90
// Nickerchen (V3): dieselbe Rechnung mit 30 Minuten Mindestschlaf - mit der Halte-Regel
// heisst das etwa 45 Minuten. Bestaetigt nur mit Puls: mindestens 3 Werte, Median hoechstens
// Ruhepuls + 10 (Sofa und Fernsehen sind keins).
#define NICKERCHEN_MIN_SCHLAF 30
#define NICKERCHEN_PULSE_MIN 3
#define NICKERCHEN_PULS_UEBER 10

typedef enum {
  NachtOffen = 0,  //< Nacht laeuft (oder Folgenacht)
  NachtFertig,     //< gemeldet, kann noch wachsen
  NachtErledigt,   //< Fensterende: Tag fuer Nickerchen
} NachtStufe;

// Die Minute `minute` des Fensters ab `beginn_utc`. Rueckgabe: der Takt ist faellig.
bool boulder_nacht_minute(time_t beginn_utc, int minute, uint16_t vmc, uint8_t puls,
                          bool gueltig);

// Ein fertiges HRV-Fenster (puls.c, AP5), das bei Minute `minute` des Fensters ab
// `beginn_utc` begann. Nur in die laufende Nacht, in zeitlicher Folge. Rueckgabe: genommen.
bool boulder_nacht_hrv(time_t beginn_utc, int minute, uint16_t rmssd, uint8_t puls);

// Der Takt (Callback "Nachlauf"): idempotent, ein ausgefallener holt der naechste nach.
void boulder_nacht_takt(void);

void boulder_nacht_zuruecksetzen(void);
NachtStufe boulder_nacht_stufe(void);
uint8_t boulder_nacht_ruhepuls(void);

// Diagnose: abgelehnte und gemeldete Nickerchen.
uint16_t boulder_nacht_nickerchen_gemeldet(void);
uint16_t boulder_nacht_nickerchen_abgelehnt(void);
