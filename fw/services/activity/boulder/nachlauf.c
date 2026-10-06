/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/activity/boulder/nachlauf.h"

#include "pbl/services/activity/boulder/eingang.h"

#include <stdbool.h>
#include <string.h>

// DAS NACHTBUCH: je Minute vier Bit - die Phase (0..4, NACHLAUF_AUSSERHALB)
// und, ob die Minute fuer die Rechnung wach war (die Phase allein sagt das
// nicht: geglaettet wird).
#define BUCH_WACH 0x8
#define BUCH_PHASE 0x7
#define BUCH_LEER (BUCH_WACH | NACHLAUF_AUSSERHALB)

static uint8_t s_buch[(SCHLAF_MINUTEN_MAX + 1) / 2];
static uint16_t s_ab;
static uint16_t s_min_schlaf;
static uint16_t s_fest_bis;
static uint16_t s_ende;   //< so weit reicht der letzte Lauf (letzte gueltige Minute + 1)
static uint16_t s_jetzt;  //< die Minute des letzten Laufs
static bool s_lauf_gefunden;     //< der letzte Lauf sah eine Nacht
static uint16_t s_lauf_aufwachen;  //< ihr Aufwachen, ab Minute 0

// Die Rechnung selbst: gross, aber fest (kein Heap im Minutenpfad, F8).
static SchlafArbeit s_arbeit;
static SchlafErgebnis s_ergebnis;
static SchlafHrv s_hrv[SCHLAF_HRV_MAX];

static uint8_t prv_buch(int i) {
  return (s_buch[i / 2] >> ((i % 2) * 4)) & 0xF;
}

static void prv_buch_setzen(int i, uint8_t wert) {
  const int schub = (i % 2) * 4;
  s_buch[i / 2] = (uint8_t)((s_buch[i / 2] & ~(0xF << schub)) | ((wert & 0xF) << schub));
}

void boulder_nachlauf_neu(uint16_t ab, uint16_t min_schlaf) {
  s_ab = (ab < SCHLAF_MINUTEN_MAX) ? ab : SCHLAF_MINUTEN_MAX;
  s_min_schlaf = min_schlaf;
  s_fest_bis = s_ab;
  s_ende = s_ab;
  s_jetzt = s_ab;
  s_lauf_gefunden = false;
  for (int i = s_ab; i < SCHLAF_MINUTEN_MAX; i++) {
    prv_buch_setzen(i, BUCH_LEER);
  }
}

// Eine Regung wie in schlafanalyse.c (REGUNG_SECHZEHNTEL, 250 beim VMC der Uhr).
static bool prv_regung(const BoulderEingang *e, int i) {
  return e->gueltig[i] && 16 * (uint32_t)e->bewegung[i] >= 5 * (uint32_t)SCHLAF_SCHWELLE_VMC;
}

// LIEGT DIE MINUTE IN EINEM OFFENEN SCHUB? Ein Aufwach-Schub wird erst als
// Ganzes wach (schlafanalyse.c, prv_aufwach_schuebe), und ob er es wird, haengt
// an seinem Ende und am Puls danach. Solange seine letzte Regung juenger ist
// als NACHLAUF_SCHUB_RUHE, bleibt er offen.
static bool prv_schub_offen(const BoulderEingang *e, int m, int ende) {
  int letzte = -1;
  int zahl = 0;
  int ruhig = 0;
  for (int i = m; i >= s_ab && ruhig <= NACHLAUF_SCHUB_LUECKE; i--) {
    if (prv_regung(e, i)) {
      zahl++;
      letzte = (letzte < 0) ? i : letzte;
      ruhig = 0;
    } else {
      ruhig++;
    }
  }
  ruhig = 0;
  for (int i = m + 1; i < ende && ruhig <= NACHLAUF_SCHUB_LUECKE; i++) {
    if (prv_regung(e, i)) {
      zahl++;
      letzte = i;
      ruhig = 0;
    } else {
      ruhig++;
    }
  }
  return zahl >= NACHLAUF_SCHUB_REGUNGEN && s_jetzt - 1 - letzte < NACHLAUF_SCHUB_RUHE;
}

// Die b26-Rechnung ueber [von, ende) der Nachtfelder, mit den HRV-Fenstern des
// Ausschnitts ab seinem Beginn gezaehlt - wie b26 sie der Rechnung gibt (Beginn
// im Ausschnitt, Mitte vor jetzt). Das Ergebnis traegt sie ab `ab` ins Nachtbuch
// ein (Phase und Wach-Bit; was ausserhalb der Nacht liegt, ausserhalb).
static void prv_rechnen(int von, int ende, int ab) {
  const BoulderEingang *e = boulder_eingang();
  int hrv_anzahl = 0;
  for (int k = 0; k < e->hrv_anzahl; k++) {
    const SchlafHrv *f = &e->hrv[k];
    if (f->minute < von || f->minute + 2 >= s_jetzt) {
      continue;
    }
    s_hrv[hrv_anzahl] = *f;
    s_hrv[hrv_anzahl].minute = (uint16_t)(f->minute - von);
    hrv_anzahl++;
  }
  const SchlafEingabe eingabe = {
    .bewegung = &e->bewegung[von],
    .puls = &e->puls[von],
    .gueltig = &e->gueltig[von],
    .anzahl = (uint16_t)(ende - von),
    .schwelle = SCHLAF_SCHWELLE_VMC,
    .hrv = s_hrv,
    .hrv_anzahl = (uint8_t)hrv_anzahl,
    .min_schlaf = s_min_schlaf,
  };
  schlafanalyse_werte(&eingabe, &s_arbeit, &s_ergebnis);
  for (int i = ab; i < ende; i++) {
    prv_buch_setzen(i, (s_arbeit.wach[i - von] ? BUCH_WACH : 0) | NACHLAUF_AUSSERHALB);
  }
  for (int k = 0; k < s_ergebnis.abschnitt_anzahl; k++) {
    const SchlafAbschnitt *a = &s_ergebnis.abschnitte[k];
    for (int i = von + a->von; i < von + a->bis; i++) {
      if (i >= ab) {
        prv_buch_setzen(i, (prv_buch(i) & BUCH_WACH) | a->phase);
      }
    }
  }
}

void boulder_nachlauf_rechnen(uint16_t jetzt) {
  const BoulderEingang *e = boulder_eingang();
  const int ende = (e->ende < jetzt) ? e->ende : jetzt;
  s_jetzt = jetzt;
  // Fehlt der juengste Teil (Uhr abgelegt), zaehlt er nicht: wie b26 rechnet
  // der Lauf bis zur letzten vorhandenen Minute.
  for (int i = (ende > s_fest_bis) ? ende : s_fest_bis; i < s_ende; i++) {
    prv_buch_setzen(i, BUCH_LEER);
  }
  s_ende = (ende > s_ab) ? (uint16_t)ende : s_ab;
  s_lauf_gefunden = false;
  if (ende <= s_ab) {
    return;
  }
  // Das Fenster beginnt spaetestens an der ersten offenen Minute: fiel ein
  // Lauf aus, rechnet dieser weiter zurueck, damit keine Minute fehlt.
  int von = (int)jetzt - NACHLAUF_FENSTER;
  von = (von > s_fest_bis) ? s_fest_bis : von;
  von = (von < s_ab) ? s_ab : von;
  // Vorlaeufig: alle noch offenen Minuten aus diesem Lauf.
  prv_rechnen(von, ende, (von > s_fest_bis) ? von : s_fest_bis);
  s_lauf_gefunden = s_ergebnis.gefunden;
  s_lauf_aufwachen = (uint16_t)(von + s_ergebnis.aufwachen);

  // Festschreiben, der Reihe nach. SOLANGE DAS FENSTER KEINE NACHT SIEHT, WARTET
  // ES (bis NACHLAUF_SPAETESTENS): eine Nacht gilt erst ab einer Stunde Schlaf
  // (MIN_NACHT), und ihre ersten Minuten sind dann schon aelter als
  // NACHLAUF_ALTER - als "ausserhalb" festgeschrieben, fehlten sie ihr (26.9.:
  // eingeschlafen 22:17, gefunden erst um 23:57).
  const bool nacht = s_ergebnis.gefunden;
  while (s_fest_bis < ende && jetzt >= s_fest_bis + 1 + NACHLAUF_ALTER &&
         (jetzt - s_fest_bis >= NACHLAUF_SPAETESTENS ||
          (nacht && !prv_schub_offen(e, s_fest_bis, ende)))) {
    s_fest_bis++;
  }
}

// DIE ENDFASSUNG RECHNET DIE GANZE NACHT, aus den Nachtfeldern im Speicher (kein
// Heap, keine Datei). Rueckfall der Spezifikation 4.3 fuer HRV-Naechte: die
// HRV-Regel des Tiefschlafs vergleicht mit dem Median der ruhigen Fenster der
// GANZEN Nacht - eine Minute, die der Nachlauf festschreibt, kennt die spaeteren
// Fenster nicht. Am 5.10. waren so 22:14-22:19 im Nachlauf Tief, in der Nacht
// Leicht (Fenster 22:15 mit 24 ms, Median spaeter 51 ms); ein groesseres Fenster
// (360, 480) aendert daran nichts.
// DIE ENDFASSUNG SCHREIBT DIE NACHT FEST, ins Nachtbuch und bis zu ihrem Ende:
// danach gilt dort dasselbe wie im Satz. Sonst blieben Minuten offen, solange
// ein Schub laeuft - und das Aufstehen ist ein Schub, der nicht endet (29.9.:
// 05:33-06:00 waeren erst nach 240 Minuten fest geworden, aus einem Fenster,
// das die Nacht nicht mehr sieht). Waechst die Nacht danach, rechnet die
// naechste Endfassung sie wieder ganz.
const SchlafErgebnis *boulder_nachlauf_endfassung(void) {
  if (s_ende <= s_ab) {
    memset(&s_ergebnis, 0, sizeof(s_ergebnis));
    return &s_ergebnis;
  }
  prv_rechnen(s_ab, s_ende, s_ab);
  s_fest_bis = s_ende;
  return &s_ergebnis;
}

uint8_t boulder_nachlauf_phase(uint16_t minute) {
  return (minute < SCHLAF_MINUTEN_MAX) ? (prv_buch(minute) & BUCH_PHASE) : NACHLAUF_AUSSERHALB;
}

uint16_t boulder_nachlauf_fest_bis(void) {
  return s_fest_bis;
}

bool boulder_nachlauf_lauf_aufwachen(uint16_t *aufwachen) {
  *aufwachen = s_lauf_aufwachen;
  return s_lauf_gefunden;
}

void boulder_nachlauf_rechenplatz(SchlafArbeit **arbeit, SchlafErgebnis **ergebnis) {
  *arbeit = &s_arbeit;
  *ergebnis = &s_ergebnis;
}
