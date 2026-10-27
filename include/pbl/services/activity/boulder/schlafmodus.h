/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Boulder V2: der Schlafmodus von Hand (F53-F56, nach dem Vorbild von Morpheuz).
//
// EIN EIGENES NACHTFENSTER: Das Fenster beginnt sonst um 20 Uhr (dauerteil.c). Mit dem Start von
// Hand beginnt es stattdessen in dieser Minute. Alles, was am Fenster haengt, folgt so von selbst:
// die Rechnung der Nacht, der Live-Automat und Pebbles Sitzungen, die Nachtruhe der Messung (Puls,
// HRV, SpO2 im Schlaf), die Weckregel, die Ablage und das Nachfuellen nach einem Neustart. Auch am
// Tag, nach einer Nachtschicht.
//
// DAS ENDE (F54): von Hand ("Aufgewacht"), sonst von selbst, wenn die Nacht endgueltig wird (zwei
// Stunden wach nach dem Aufwachen oder das Ende des Nachtteils nach 15 Stunden), oder wenn der
// Live-Automat zwei Stunden nach dem Start noch nie geschlafen hat. Danach gilt das Fenster von
// Hand weiter wie ein gewoehnliches (eine Folgenacht ab 90 Minuten Schlaf), bis das naechste
// Fenster ab 20 Uhr beginnt, hoechstens 24 Stunden lang.
//
// QUIET TIME BLEIBT, WIE SIE IST (F55): Der Schlafmodus misst nur.
//
// Alles auf KernelBG; nur boulder_schlafmodus_schalten darf von einer App-Task kommen.

#include "pbl/kernel/compiler.h"

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

// So lange nach dem Start ohne Schlaf, und der Schlafmodus endet von selbst (F54).
#define SCHLAFMODUS_OHNE_SCHLAF_MIN 120
// Das Fenster von Hand gilt hoechstens so lange (Felder und Ablage fassen einen Tag).
#define SCHLAFMODUS_FENSTER_MAX_S (24 * 60 * 60)

#define SCHLAFMODUS_FASSUNG 1

// In der Datei "boulder_v2" unter "hand" (speicher.c).
typedef struct PBL_PACKED {
  uint8_t fassung;     //< SCHLAFMODUS_FASSUNG
  uint8_t aktiv;       //< gestartet und noch nicht beendet
  uint8_t geschlafen;  //< der Live-Automat schlief seit dem Start
  uint8_t reserve;
  uint32_t start;      //< Minute 0 des Fensters von Hand, 0 = keins
  uint32_t ende;       //< Ende des Schlafmodus, solange er laeuft 0
} BoulderSchlafmodus;

// Das Nachtfenster fuer `utc`: das von Hand, solange es gilt, sonst `regulaer` (ab 20 Uhr).
time_t boulder_schlafmodus_fenster(time_t utc, time_t regulaer);

bool boulder_schlafmodus_aktiv(void);
// Beginn des laufenden Schlafmodus, 0 = keiner.
time_t boulder_schlafmodus_seit(void);

// Ist die Nacht ab `satz_beginn`, eingeschlafen um `eingeschlafen`, die von Hand (0x4001, Flag
// 0x02)? Nur die erste Nacht des Fensters, und nur wenn der Schlaf vor dem Ende begann.
bool boulder_schlafmodus_nacht_von_hand(time_t satz_beginn, time_t eingeschlafen);

// Start ab `minute`, Ende um `jetzt`. Rueckgabe: der Zustand hat sich geaendert.
bool boulder_schlafmodus_starten(time_t minute);
bool boulder_schlafmodus_beenden(time_t jetzt);

// Jede Minute (Minutenteil): `schlaeft` ist der Live-Automat. Rueckgabe: der Schlafmodus endet
// eben von selbst, weil seit dem Start zwei Stunden kein Schlaf war.
bool boulder_schlafmodus_minute(time_t minute_utc, bool schlaeft);

// Die Nacht des Fensters ab `fenster` ist endgueltig (nacht.c), ihr Ende `jetzt`. Gehoert sie
// zum laufenden Schlafmodus, endet er damit. Rueckgabe: er endete eben.
bool boulder_schlafmodus_nacht_endgueltig(time_t fenster, time_t jetzt);

// Von jeder Task: an oder aus. Die Arbeit laeuft auf KernelBG (boulder_dauerteil_schlafmodus).
void boulder_schlafmodus_schalten(bool an);

// Ablage und Laden (speicher.c).
void boulder_schlafmodus_ablage(BoulderSchlafmodus *aus);
void boulder_schlafmodus_laden(const BoulderSchlafmodus *ein);

void boulder_schlafmodus_test_neustart(void);
