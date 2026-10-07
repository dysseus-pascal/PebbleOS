/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Boulder V2: der Lebenslauf einer Nacht und die Nickerchen des Tages (Spezifikation 4.4,
// 4.7). Stand AP3: was die Zyklen brauchen - Fenster, Rechnen, fertig, waechst,
// endgueltig, Folgenacht. Stand AP4: bei "fertig" und bei jeder Aenderung bis "endgueltig"
// gehen 0x4001/0x4002 und das NachtBild hinaus (ausgabe.c); der Lebenslauf uebersteht einen
// Neustart (speicher.c, Laden).
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

#include "pbl/services/activity/boulder/speicher.h"

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
// AP4: Eine fertige Nacht rechnet der Lebenslauf alle 15 Minuten ganz (wie b26) - aendern
// sich Aufwachen oder Schlaf, geht sie noch einmal hinaus.
#define NACHT_NACHSEHEN 15
// So viele Takte nach dem Fensterende wartet "endgueltig" auf eine nicht ganz geschickte
// Nacht (b26: eine Stunde), dann gibt der Lebenslauf sie auf.
#define NACHT_AUFGEBEN_TAKTE 60

// V3 (Schritt 2): Ruhepuls fuer Kalorien und Maximalpuls aus so vielen Naechten.
#define NACHT_RUHE_NAECHTE 7

// Die Ruhepulse der letzten Naechte, in der Datei "boulder_v2" (Schluessel "ruhe"). Sieben
// Byte und dazu, welche Nacht die juengste ist: Eine Nacht wird bis "endgueltig" mehrmals
// fertig, und jede Endfassung ersetzt ihren Wert, statt einen zweiten anzuhaengen.
typedef struct PBL_PACKED {
  uint8_t puls[NACHT_RUHE_NAECHTE];  //< 0 = leer
  uint8_t neu;                       //< Platz der juengsten Nacht
  uint32_t nacht;                    //< Beginn der juengsten (Fenster + Folgenacht), 0 = keine
} BoulderRuheRing;

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

// AP4, nach dem Boot (Spezifikation 7.3): Der erste init beginnt das Fenster ab `beginn` und
// fuellt seinen Eingang aus Pebbles Minutendatei (D14, boulder_nacht_ring_minute). Bis
// "Laden" lief, nehmen auch die Minuten des Minutenteils nur diesen Weg - kein Automat, keine
// Sitzung, kein Takt.
void boulder_nacht_ring_neu(time_t beginn);
void boulder_nacht_ring_minute(time_t beginn_utc, int minute, uint16_t vmc, uint8_t puls,
                               bool gueltig);
bool boulder_nacht_wartet(void);
// "Laden": den Lebenslauf aus dem Kopf (NULL = keiner fuer dieses Fenster), das Buch ist bis
// `fest` geladen. Rechnet den Automaten nach und legt die Sitzungen neu an.
void boulder_nacht_laden(const BoulderKopf *kopf, uint16_t fest);
// Der Lebenslauf fuer den Kopf (Ablage).
void boulder_nacht_kopf(BoulderKopf *kopf);
// Minute 0 des laufenden Fensters (vor dem Laden: des Rings), 0 = keins.
time_t boulder_nacht_fenster(void);
// Lohnt der Ringrest? Die Nacht laeuft und hat Schlaf.
bool boulder_nacht_rest_noetig(void);
NachtStufe boulder_nacht_stufe(void);
// Der Ruhepuls der letzten fertigen Nacht (Nickerchen, Kandidatin fuer Velo und Auto).
uint8_t boulder_nacht_ruhepuls(void);
// V3: der Median der letzten sieben Naechte wie in Boulder (Trainingsanalyse.kt
// ruhepulsAusNaechten), abgeschnitten wie dort (52,5 -> 52); 0 = keine Nacht. Die Grenzen
// 30..110 und den Ersatz 60 setzt, wer rechnet (kalorien.c, Kieselsports Maximalpuls).
// ANDERS ALS BOULDER zaehlen die letzten sieben Naechte der Uhr, auch wenn eine davon mehr als
// 14 Tage zurueckliegt (wer die Uhr eine Woche nicht traegt): Die Uhr fuehrt keine Daten dazu.
uint8_t boulder_nacht_ruhepuls_median(void);
// Ablage (speicher.c) und Laden nach dem Boot.
void boulder_nacht_ruhe_ablage(BoulderRuheRing *ring);
void boulder_nacht_ruhe_laden(const BoulderRuheRing *ring);

// Diagnose: abgelehnte und gemeldete Nickerchen.
uint16_t boulder_nacht_nickerchen_gemeldet(void);
uint16_t boulder_nacht_nickerchen_abgelehnt(void);
