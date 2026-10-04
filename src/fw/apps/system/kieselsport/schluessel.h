/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Die Schluessel der Nachrichten ans Telefon - DIESELBEN ZAHLEN WIE IN DER
// STORE-APP. Dort vergab sie das SDK der Reihe nach ab 10000, in der
// Reihenfolge von messageKeys in der package.json; Kiesel-Helper und die
// Konfigseite kennen sie so. Wer hier eine Zahl aendert, spricht mit dem
// Telefon in einer anderen Sprache.
#define MESSAGE_KEY_ART 10000
#define MESSAGE_KEY_BEGINN 10001
#define MESSAGE_KEY_DAUER 10002
#define MESSAGE_KEY_SCHRITTE 10003
#define MESSAGE_KEY_METER 10004
// KCAL, PULS_MITTEL und PULS_MAX gehen weiter mit, damit aeltere Telefone
// sie finden - Boulder braucht sie nicht mehr: den Puls rechnet es aus der
// entstoerten Kurve, die Kalorien aus dem Puls. KCAL (aus Schritten, auf dem
// Rad fast null) gilt dort nur, bis die Kalorien aus dem Puls da sind.
#define MESSAGE_KEY_KCAL 10005
#define MESSAGE_KEY_PULS_MITTEL 10006
#define MESSAGE_KEY_PULS_MAX 10007
#define MESSAGE_KEY_MAXPULS 10008
#define MESSAGE_KEY_ZUSTAND 10009
#define MESSAGE_KEY_BECKEN 10010
#define MESSAGE_KEY_PAUSENZIEL 10011
#define MESSAGE_KEY_EMPFIND 10012
#define MESSAGE_KEY_SAETZE 10013
#define MESSAGE_KEY_REPS 10014
#define MESSAGE_KEY_BAHNEN 10015
#define MESSAGE_KEY_ABSCHNITTE 10016
#define MESSAGE_KEY_HRV 10017
#define MESSAGE_KEY_PIN_ART 10018
#define MESSAGE_KEY_PIN_ZEIT 10019
#define MESSAGE_KEY_KURVE_AB 10020
#define MESSAGE_KEY_KURVE_ANZAHL 10021
#define MESSAGE_KEY_KURVE 10022
#define MESSAGE_KEY_NACHT_AN 10023
#define MESSAGE_KEY_NACHT_VON 10024
#define MESSAGE_KEY_NACHT_BIS 10025
// 10026-10036 vergab die Store-App fuer Nacht, HRV und SpO2. Sie bleiben
// frei: ein altes Telefon laese dort noch etwas anderes heraus.

// NEU IN DER FIRMWARE, nicht in der Store-App. Ein Telefon, das sie nicht
// kennt, ueberliest sie; fehlen sie, rechnet Boulder wie frueher.
//
// Die Uhrzeit (UTC) einer Zustandsmeldung - wann Start, Pause, Weiter oder
// Stopp geschah, nicht wann die Meldung ankam.
#define MESSAGE_KEY_ZEIT 10037
// Die Pausen in der Zusammenfassung, als Byte (Aufbau in kieselsport.h).
#define MESSAGE_KEY_PAUSEN 10038
// Wann die aktive Zeit endete (UTC): der Beginn der letzten Pause.
#define MESSAGE_KEY_ENDE 10039
// Ob der Maximalpuls eingestellt ist (1) oder die Uhr nach dem Alter
// rechnet (0). In beide Richtungen; ohne ihn gilt MAXPULS wie bisher.
#define MESSAGE_KEY_MAXPULS_GESETZT 10040

// Was nur die App im Persist haelt: der Timeline-Pin, den die Konfigseite
// und Kiesel-Helper stellen (PERSIST_PIN_ART, PERSIST_PIN_ZEIT). Die Nummern
// stehen EINMAL, beim Dienst: App und Dienst teilen sich den Persist, und
// zwei Listen liefen frueher oder spaeter auseinander.
#include "services/kieselsport/schluessel.h"
