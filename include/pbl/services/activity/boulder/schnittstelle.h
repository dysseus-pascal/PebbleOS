/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Boulder V2, Schritt 3: die Schnittstelle der Variante zur eigenen Health-App (Spezifikation
// Schritt 3, 3.1-3.4; AP2: S1-S4, die Lage, der Weg der Ziele; AP3: S5, die Naechte; AP4: S6 und
// S7, Herzband und Woche).
//
// WARUM SYSCALLS: Die Health-App laeuft ohne Systemrechte. Was die Variante weiss, liegt in
// Kernel-RAM, das die App nicht lesen darf. Jeder Syscall prueft den Puffer der App, nimmt
// hoechstens eine Blattsperre der Variante, kopiert direkt in den Puffer und kehrt zurueck. Er
// oeffnet keine Datei, holt keinen Heap und wartet nie auf eine Arbeit von KernelBG (L5, L6).
//
// WAS OHNE SPERRE LEBT (Lebenslauf der Nacht, Live-Zustand, Messstufe, Ziele), fasst die LAGE
// zusammen: KernelBG veroeffentlicht sie in O(1) unter einer eigenen Blattsperre - jede Minute
// und nach jedem Ereignis, das sie aendert (Boot, Messstufe, Ziele).
//
// ARBEIT MIT DATEIEN (NachtBild nach dem Boot laden, Messstufe setzen) geht als Auftrag an
// KernelBG, nie ueber boulder_aufgabe_einreihen: Aus einem Syscall laeuft der Code auf der
// App-Task, und system_task_get_available_space() mass dort die Schlange der App, nicht die, die
// system_task_add_callback_droppable fuellt (3.1, Regel 6). Darum system_task_add_callback aus
// der App-Task in deren eigene Schlange, ein Merker je Auftrag, hoechstens zwei Auftraege.
//
// Die Strukturen sind die Fassung 2 aus 3.3; ihre Groessen und Versaetze stehen unten fest.

#include "pbl/services/activity/boulder/nacht_satz.h"
#include "pbl/services/activity/boulder/spo2.h"
#include "pbl/kernel/compiler.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

// Antwort von S1.
typedef enum {
  BoulderLesenDa,      //!< das Bild steht im Puffer
  BoulderLesenKeine,   //!< die Uhr hat keine Nacht
  BoulderLesenSpaeter, //!< seit dem Boot noch nicht geladen; KernelBG laedt es, in 1 s erneut
} BoulderLesen;

#define BOULDER_LAGE_FASSUNG 2

typedef struct {
  uint8_t fassung;          //!< BOULDER_LAGE_FASSUNG
  bool tracking;            //!< Pebbles Schalter "Health Tracking", vom Syscall frisch gelesen
  uint8_t messstufe;        //!< 1..5, 0 = noch nicht gelesen
  uint8_t nacht_stufe;      //!< NachtStufe (nacht.h)
  uint32_t stand_utc;       //!< letzte Veroeffentlichung, 0 = noch keine seit dem Boot
  uint32_t fenster_utc;     //!< Minute 0 des laufenden Nachtfensters, 0 = keins
  uint16_t ziel_schritte;   //!< 1 000..50 000, Vorgabe 10 000 (ziele.h)
  uint16_t ziel_schlaf_min; //!< 240..720, Vorgabe 480
  uint16_t maxpuls;         //!< wie KsStand.maxpuls (Einstellung oder Tanaka), 0 = keiner
  uint8_t ruhepuls_median;  //!< Median der letzten sieben Naechte (nacht.h), 0 = keiner
  bool schlaeft;            //!< Live-Automat (live.h)
  uint8_t spo2_takt_min;    //!< SpO2 alle so viele Minuten nach Stufe und Pebbles Schalter, 0 = aus
  uint8_t nickerchen;       //!< heute gemeldete Nickerchen (im laufenden Fenster)
  uint8_t reserve[2];
} BoulderLage;

_Static_assert(sizeof(BoulderLage) == 24, "BoulderLage: 24 B (Spezifikation 3.3)");
_Static_assert(offsetof(BoulderLage, stand_utc) == 4, "BoulderLage: stand_utc");
_Static_assert(offsetof(BoulderLage, fenster_utc) == 8, "BoulderLage: fenster_utc");
_Static_assert(offsetof(BoulderLage, ziel_schritte) == 12, "BoulderLage: ziel_schritte");
_Static_assert(offsetof(BoulderLage, maxpuls) == 16, "BoulderLage: maxpuls");
_Static_assert(offsetof(BoulderLage, ruhepuls_median) == 18, "BoulderLage: ruhepuls_median");
_Static_assert(offsetof(BoulderLage, spo2_takt_min) == 20, "BoulderLage: spo2_takt_min");

// Der Wert des Schluessels "boulder_ziele" in der BlobDB Health (F10 = A), little endian. Das
// Telefon schreibt ihn (Boulder 0.53), die Uhr liest ihn nur (ziele.h).
typedef struct PBL_PACKED {
  uint8_t fassung;     //!< BOULDER_ZIELE_FASSUNG
  uint8_t reserve;
  uint16_t schritte;   //!< 1 000..50 000
  uint16_t schlaf_min; //!< 240..720
  uint16_t reserve2;
} BoulderZieleSatz;

// 8 B: healthdb nimmt nur Vielfache von 4 (health_db.c, prv_value_is_valid).
_Static_assert(sizeof(BoulderZieleSatz) == 8, "BoulderZieleSatz: 8 B");
_Static_assert(offsetof(BoulderZieleSatz, schritte) == 2, "BoulderZieleSatz: schritte");
_Static_assert(offsetof(BoulderZieleSatz, schlaf_min) == 4, "BoulderZieleSatz: schlaf_min");

// HRV und Ruhepuls je Wachtag (AP3, F2; naechte.h). Nur Datum, HRV und Ruhepuls - sonst behaelt
// die Uhr von frueheren Naechten nichts.
//
// WARUM 37: Das HRV-Band eines Tages kommt aus den 30 Tagen davor, Heart-Detail zeigt 7 Naechte
// mit je ihrem Band. Die aelteste dieser 7 braucht Werte bis 36 Tage zurueck (3.3).
#define NAECHTE_MAX             37
#define BOULDER_NAECHTE_FASSUNG 1

typedef struct PBL_PACKED {
  uint16_t tag;     //!< Wachtag: Tage seit 1.1.2000, Ortsdatum des Aufwachens
  uint16_t hrv;     //!< ms, laengste Nacht des Wachtags mit HRV (Boulders jeTagMitHrv), 0 = keine
  uint8_t ruhepuls; //!< laengste Nacht des Wachtags (Boulders jeTag), 0 = keiner
  uint8_t reserve;
} BoulderNachtEintrag;

// Ein Ring: tage[neu] ist der juengste Wachtag, die aelteren folgen rueckwaerts. Die Tage sind
// streng aufsteigend, jeder hoechstens einmal.
typedef struct PBL_PACKED {
  uint8_t fassung; //!< BOULDER_NAECHTE_FASSUNG
  uint8_t anzahl;  //!< 0..NAECHTE_MAX
  uint8_t neu;     //!< Platz des juengsten Wachtags
  uint8_t reserve;
  uint16_t min_ruhe; //!< Schlafminuten der Nacht, die ruhepuls des juengsten Tags lieferte
  uint16_t min_hrv;  //!< dasselbe fuer hrv; beides nur Auswahlhilfe, nie angezeigt
  BoulderNachtEintrag tage[NAECHTE_MAX];
} BoulderNaechte;

_Static_assert(sizeof(BoulderNachtEintrag) == 6, "BoulderNachtEintrag: 6 B (Spezifikation 3.3)");
_Static_assert(offsetof(BoulderNachtEintrag, hrv) == 2, "BoulderNachtEintrag: hrv");
_Static_assert(offsetof(BoulderNachtEintrag, ruhepuls) == 4, "BoulderNachtEintrag: ruhepuls");
_Static_assert(sizeof(BoulderNaechte) == 230, "BoulderNaechte: 8 + 37 * 6 = 230 B (3.3)");
_Static_assert(offsetof(BoulderNaechte, min_ruhe) == 4, "BoulderNaechte: min_ruhe");
_Static_assert(offsetof(BoulderNaechte, min_hrv) == 6, "BoulderNaechte: min_hrv");
_Static_assert(offsetof(BoulderNaechte, tage) == 8, "BoulderNaechte: tage");

// Das Herzband der letzten 24 h und die Zonenminuten von heute (AP4, F8 = A; herz.h). Die
// Variante fuehrt beides jede Minute mit, die App liest keinen Minutenverlauf (2.6.2).
//
// EIN RING VON 48 HALBSTUNDEN: Platz = Nummer % 48, die Nummer ist utc / 1800. Gueltig sind die
// Nummern neueste - 47 .. neueste; ein Platz ohne Messung hat n = 0. Die Halbstunden liegen in UTC:
// So bleibt ein Platz ueber die Zeitumstellung derselbe, und die App rechnet die Ortszeit selbst.
// "neueste" ist die Halbstunde der juengsten Minute ueberhaupt, mit oder ohne Puls.
#define HERZ_HALBSTUNDEN     48
#define HERZ_HALBSTUNDE_S    1800
#define HERZ_ZONEN           4

typedef struct {
  uint32_t neueste;                 //!< Nummer der juengsten Halbstunde (utc / 1800), 0 = leer
  uint8_t min[HERZ_HALBSTUNDEN];    //!< bpm; 0 = keine Messung
  uint8_t max[HERZ_HALBSTUNDEN];
  uint8_t mittel[HERZ_HALBSTUNDEN]; //!< gerundetes Mittel der Minuten mit Puls
  uint8_t n[HERZ_HALBSTUNDEN];      //!< Minuten mit Puls
  uint16_t zonen_min[HERZ_ZONEN];   //!< heute: leicht, moderat, kraeftig, Spitze (Boulders pulsZone)
  uint16_t zonen_tag;               //!< Tage seit 1.1.2000 (Ortszeit) von zonen_min
  uint8_t grenzen[HERZ_ZONEN - 1];  //!< Karvonen 40/60/80 %, mit denen gezaehlt wurde
  uint8_t reserve[3];               //!< ohne PBL_PACKED rundet der Compiler auf 4 B auf (3.3)
} BoulderHerz;

_Static_assert(sizeof(BoulderHerz) == 212, "BoulderHerz: 4 + 192 + 8 + 2 + 3 + 3 = 212 B (3.3)");
_Static_assert(offsetof(BoulderHerz, min) == 4, "BoulderHerz: min");
_Static_assert(offsetof(BoulderHerz, n) == 4 + 3 * HERZ_HALBSTUNDEN, "BoulderHerz: n");
_Static_assert(offsetof(BoulderHerz, zonen_min) == 196, "BoulderHerz: zonen_min");
_Static_assert(offsetof(BoulderHerz, zonen_tag) == 204, "BoulderHerz: zonen_tag");
_Static_assert(offsetof(BoulderHerz, grenzen) == 206, "BoulderHerz: grenzen");

// Die Schritte der sechs Vortage (AP4, F9 = A; woche.h): Pebbles Tageswert, wie die Uhr ihn am
// Tagesende hatte - nicht Pebbles Tageshistorie, die das Telefon ueberschreibt.
#define WOCHE_VORTAGE     6
#define WOCHE_KEIN_WERT   UINT32_MAX

typedef struct {
  uint16_t tag;                      //!< Tage seit 1.1.2000 (Ortszeit) von schritte[0], 0 = leer
  uint16_t reserve;
  uint32_t schritte[WOCHE_VORTAGE];  //!< [0] der Tag `tag` .. [5] fuenf Tage davor
} BoulderWoche;

_Static_assert(sizeof(BoulderWoche) == 28, "BoulderWoche: 28 B (3.3)");
_Static_assert(offsetof(BoulderWoche, schritte) == 4, "BoulderWoche: schritte");

// --- Die Syscalls der App (S1-S7) -------------------------------------------------------------

// S1: die letzte Nacht als Block mit Phasen. Ohne Datei: Ist das Bild seit dem Boot noch nicht
// geladen, laedt es ein Auftrag auf KernelBG, und die Antwort ist BoulderLesenSpaeter.
BoulderLesen sys_boulder_nacht(NachtBild *aus);

// S2: die Lage. Rueckgabe false, solange seit dem Boot keine veroeffentlicht ist; *aus traegt
// dann die Vorgaben (Ziele 10 000 und 480 min, sonst 0) und den frischen Schalter "tracking".
bool sys_boulder_lage(BoulderLage *aus);

// S3: die SpO2-Werte in [von, bis), aelteste zuerst, hoechstens `max`. Rueckgabe: wie viele.
uint32_t sys_boulder_spo2(time_t von, time_t bis, Spo2Wert *aus, uint32_t max);

// S4: die Messstufe setzen wie in Settings -> Health. Der Wunsch geht nicht verloren, ein
// zweiter vor dem Setzen ersetzt den ersten. false nur fuer eine ungueltige Stufe (nicht 1..5).
bool sys_boulder_messstufe_setzen(uint8_t stufe);

// S5: HRV und Ruhepuls je Wachtag (naechte.h). Rueckgabe false, solange "Laden" die Liste seit
// dem Boot nicht gelesen hat; *aus ist dann eine leere Liste.
bool sys_boulder_naechte(BoulderNaechte *aus);

// S6: das Herzband und die Zonen von heute (herz.h). Rueckgabe false, solange "Herz nachrechnen"
// seit dem Boot nicht gelaufen ist; *aus ist dann leer.
bool sys_boulder_herz(BoulderHerz *aus);

// S7: die Schritte der Vortage (woche.h). Rueckgabe false, solange "Laden" die Woche seit dem
// Boot nicht gelesen hat; *aus ist dann leer.
bool sys_boulder_woche(BoulderWoche *aus);

// --- Kern ------------------------------------------------------------------------------------

// KernelBG: die Lage neu veroeffentlichen. O(1), ohne Heap und ohne Datei; ruft nichts von
// Pebble unter der Sperre. Der Minutenteil ruft es jede Minute, der Takt auch am Kabel, dazu
// "Laden" nach dem Boot, das Setzen der Messstufe und das Lesen der Ziele.
void boulder_lage_veroeffentlichen(void);

// Pruefbau und Tests: die zuletzt veroeffentlichte Lage, ohne den frischen Schalter.
void boulder_lage_lesen(BoulderLage *aus);

// Tests: wie nach einem Boot.
void boulder_schnittstelle_test_neustart(void);
