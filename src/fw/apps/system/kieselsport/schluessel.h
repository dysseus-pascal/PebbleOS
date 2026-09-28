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
#define MESSAGE_KEY_HRV_ZEIT 10031
#define MESSAGE_KEY_SPO2_FRAGE 10032
#define MESSAGE_KEY_SPO2_WERT 10033
#define MESSAGE_KEY_SPO2_ZEIT 10034
#define MESSAGE_KEY_SPO2_NACHT_MITTEL 10035
#define MESSAGE_KEY_SPO2_NACHT_TIEF 10036

// Was nur die App im Persist haelt: der Timeline-Pin, den die Konfigseite
// und Kiesel-Helper stellen. Dieselben Nummern wie in services/kieselsport.
#define PERSIST_PIN_ART 5
#define PERSIST_PIN_ZEIT 6
