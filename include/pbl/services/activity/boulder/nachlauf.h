/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Boulder V2: der Schlafkern im Nachlauf (Spezifikation 4.3).
//
// Statt die ganze Nacht bei jedem Takt aus der Minutendatei zu lesen (b26),
// rechnet der Nachlauf alle NACHLAUF_TAKT Minuten nur die letzten
// NACHLAUF_FENSTER Minuten der Nachtfelder (eingang.h) mit der unveraenderten
// b26-Rechnung (schlafanalyse.c) und schreibt jede Minute fest, die alt genug
// ist, ins Nachtbuch (fuer den Zustand und die Zyklen in der Nacht). Die
// Endfassung (bei "fertig" und jeder Aenderung bis "endgueltig") rechnet die
// ganze Nacht aus den Nachtfeldern im Speicher: derselbe Satz 0x4001 wie b26
// (Host-Test test_boulder_schlafkern).
//
// STATISCH, KEIN HEAP, KEINE DATEI. Minuten zaehlen ab Minute 0 der Nacht
// (beginn_utc des Fensters). KEIN BETRIEBSSYSTEM HIER.

#include "pbl/services/activity/boulder/schlafanalyse.h"

#include <stdbool.h>
#include <stdint.h>

// Takt: alle fuenf Minuten (der Aufrufer, Minute = 2 modulo 5).
#define NACHLAUF_TAKT 5
// So weit zurueck rechnet ein Lauf: [jetzt - 360, jetzt). 360, NICHT 300 (Rueckfall
// der Spezifikation 4.3): mit b26 lag der Nachlauf bei 150 Minuten Wachliegen
// (gestellt, 29.9.) in 134 Minuten neben der Ganznacht - der Aufwach-Schub
// haengt am Puls ringsum, und der am Median der Nacht. Mit 360 in keiner.
#define NACHLAUF_FENSTER 360
// So alt muss eine Minute sein, bevor sie fest wird.
#define NACHLAUF_ALTER 75
// Ein offener Regungs-Schub haelt das Festschreiben auf (sein Ende kann die
// Minuten davor noch wach machen): mindestens NACHLAUF_SCHUB_REGUNGEN Regungen,
// dazwischen hoechstens NACHLAUF_SCHUB_LUECKE ruhige Minuten, die letzte
// juenger als NACHLAUF_SCHUB_RUHE Minuten.
#define NACHLAUF_SCHUB_REGUNGEN 3
#define NACHLAUF_SCHUB_LUECKE 10
#define NACHLAUF_SCHUB_RUHE 70
// Spaetestens so alt wird jede Minute fest - sie muss im Fenster liegen.
#define NACHLAUF_SPAETESTENS 240

// Phase einer Minute im Nachtbuch: SchlafPhase 0..4 oder ausserhalb der Nacht.
#define NACHLAUF_AUSSERHALB 7

// Neue Rechnung ab Minute `ab` (0 oder das Ende der vorigen Nacht im Fenster),
// mit `min_schlaf` wie SchlafEingabe (0 = Vorgabe, 90 = Folgenacht). Das
// Nachtbuch ab `ab` wird geleert.
void boulder_nachlauf_neu(uint16_t ab, uint16_t min_schlaf);

// Ein Lauf zur Minute `jetzt` (die Minuten davor liegen im Eingang): rechnen,
// vorlaeufig eintragen, festschreiben.
void boulder_nachlauf_rechnen(uint16_t jetzt);

// Die ganze Nacht [ab, Ende des letzten Laufs) wie b26, mit den HRV-Fenstern bis
// zur Minute des letzten Laufs; danach steht sie auch im Nachtbuch. Einschlafen,
// Aufwachen und Abschnitte zaehlen ab `ab`, wie im Satz 0x4001. Der Zeiger gilt
// bis zum naechsten Lauf.
const SchlafErgebnis *boulder_nachlauf_endfassung(void);

// Phase der Minute `minute` im Nachtbuch (NACHLAUF_AUSSERHALB ohne Nacht).
uint8_t boulder_nachlauf_phase(uint16_t minute);

// Die erste noch nicht feste Minute.
uint16_t boulder_nachlauf_fest_bis(void);

// Sah der letzte Lauf eine Nacht, und wann endet sie (Minute ab Minute 0)? Daran
// erkennt der Lebenslauf das Aufwachen, ohne jeden Takt die ganze Nacht zu rechnen;
// "fertig" entscheidet dann erst die Endfassung.
bool boulder_nachlauf_lauf_aufwachen(uint16_t *aufwachen);

// Der Rechenplatz (SchlafArbeit, SchlafErgebnis) fuer eine andere Rechnung derselben
// Task (Nickerchen, AP3): er ist gross (rund 14 KB) und wird nur einmal angelegt.
// Danach gilt ein frueherer Zeiger der Endfassung nicht mehr.
void boulder_nachlauf_rechenplatz(SchlafArbeit **arbeit, SchlafErgebnis **ergebnis);
