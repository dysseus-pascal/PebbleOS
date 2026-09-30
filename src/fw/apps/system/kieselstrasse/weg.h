/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "pebble_compat.h"

#define KST_STRASSE_LEN 48

/**
 * Was die Uhr ueber die laufende Navigation weiss.
 *
 * SIE WEISS NICHTS SELBST. Jedes Feld kommt von Boulder, das es bei OsmAnd
 * abholt. Diese App rechnet nichts aus, sie zeigt - und genau deshalb braucht
 * sie keine Standortberechtigung, keine Karten und keinen Router.
 *
 * DER AUFBAU BLEIBT, WIE ER IN DER STORE-APP WAR. Die System-App traegt deren
 * Kennung und liest deren Persist; ein umgestelltes Feld machte aus dem
 * letzten Stand Unsinn. Die Store-App schrieb 68 Byte (im pbw nachgesehen:
 * persist_write_data(1, &s_weg, 68)) - time_t ist dort wie hier 32 Bit.
 */
typedef struct {
  int abbiegeart;                    // OsmAnds TurnType, 0 = unbekannt
  int32_t entfernung_m;              // bis zur Abzweigung, -1 = keine Angabe
  char strasse[KST_STRASSE_LEN];
  time_t ankunft;                    // 0 = unbekannt
  int32_t rest_m;                    // bis zum Ziel, -1 = keine Angabe
  time_t empfangen;                  // wann zuletzt etwas kam
} Weg;

// Nicht im Host-Test: dort ist time_t 64 Bit breit, der Aufbau also ein
// anderer - auf der Uhr gilt die Pruefung.
#if !UNITTEST
_Static_assert(sizeof(Weg) == 68, "Weg muss zum Persist der Store-App passen");
#endif

/**
 * Beim Start: allen Zustand auf Anfang und den letzten Stand laden.
 *
 * AUF ANFANG, BEVOR GELESEN WIRD: eine System-App behaelt ihre statischen
 * Variablen ueber ihr Ende hinaus. Fehlt der Persist, stuende sonst der Stand
 * des vorigen Oeffnens da und saehe frisch aus.
 */
void weg_init(void);

/**
 * Eine Nachricht uebernehmen.
 *
 * Rueckgabe true heisst: ein NEUER SCHRITT. Das ist der Fall, wenn sich die
 * Abbiegeart geaendert hat - nicht, wenn bloss die Entfernung kleiner wurde.
 * Danach richtet sich das Summen, und nur dann wird der Persist geschrieben.
 */
bool weg_uebernimm(DictionaryIterator *iter);

/**
 * Den Stand in den Persist schreiben - beim Beenden.
 *
 * WARUM NICHT BEI JEDER NACHRICHT: Boulder fragt waehrend einer Navigation
 * alle zwei Sekunden bei OsmAnd nach und schickt, was sich aendert. Eine
 * Stunde Fahrt waeren sonst ueber tausend Schreibvorgaenge auf den Flash,
 * fuer eine Entfernung, die beim naechsten Oeffnen ohnehin veraltet ist.
 *
 * Geschrieben wird nur, was seit dem letzten Schreiben neu kam: Oeffnen und
 * Schliessen ohne Nachricht beruehrt den Flash nicht.
 */
void weg_sichern(void);

const Weg *weg_stand(void);

/** Steht eine Zahl zum Anzeigen bereit? */
bool weg_hat_zahl(void);

/**
 * Ist der Stand zu alt, um noch eine Navigation zu sein?
 *
 * Die Uhr laedt beim Oeffnen den letzten Stand. Einer von gestern saehe aus
 * wie eine gueltige Anweisung - und man faehrt danach.
 */
bool weg_veraltet(void);

/** "vor 12 min", "vor 3 Std", "gestern" - in `buf`, in der Sprache der Uhr. */
void weg_alter_text(char *buf, size_t len);

/** Wie voll der Balken steht, 0..100, oder -1 fuer keinen. */
int weg_balken_prozent(void);

// Kein Winkel bekannt - jeder echte liegt zwischen -180 und 180.
#define WEG_KEIN_WINKEL (-1000)

/**
 * Die Ausfahrt im Kreisverkehr (1, 2, 3 ...), 0 = keine bekannt.
 *
 * NICHT IM PERSIST: der Aufbau von Weg bleibt, wie die Store-App ihn schrieb.
 * Nach dem Oeffnen fehlt die Nummer darum, bis die erste Nachricht kommt -
 * und die schickt Boulder beim Oeffnen sofort.
 */
int weg_ausfahrt(void);

/** Wohin die Ausfahrt zeigt (Grad, + rechts), oder WEG_KEIN_WINKEL. */
int weg_winkel(void);
