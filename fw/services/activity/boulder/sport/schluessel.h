/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Die Persist-Schluessel - DIESELBEN NUMMERN WIE IN b25 UND DER STORE-APP. Der Dienst
// schreibt in den Speicher der App; wer hier eine Nummer aendert, liest nach dem Wechsel von
// b25 die Einstellungen und das Liegengebliebene nicht mehr (Kriterium 6).
//
// Frei geworden, aber nicht neu vergeben: 7-9 und 60-66 (die Nacht, jetzt in
// services/activity/boulder), 20-21 (der Startbefehl an den Worker), 67 (ob der Worker
// trainiert) und das Archiv (V7: nie gelesen) mit 10, 11 und 100-119. Ein b25-Archiv bleibt
// liegen; geschrieben wird dort nichts mehr.
#define PERSIST_MAXPULS 1
#define PERSIST_EMPFIND 2
#define PERSIST_PAUSENZIEL 3
#define PERSIST_BECKEN 4
#define PERSIST_PIN_ART 5
#define PERSIST_PIN_ZEIT 6

// Reserviert (V7), nur damit niemand die Nummern neu vergibt.
#define PERSIST_ARCHIV_ANZAHL 10
#define PERSIST_ARCHIV_NAECHST 11

// Ob PERSIST_MAXPULS ausdruecklich eingestellt ist (1) oder nicht (0). EIN
// EIGENER SCHLUESSEL: die Store-App und aeltere Firmware lesen nur
// PERSIST_MAXPULS und schrieben dort 190 auch dann, wenn niemand etwas
// eingestellt hatte (siehe puls.c).
#define PERSIST_MAXPULS_GESETZT 12

// Die Zusammenfassung, die auf das Telefon wartet (siehe wartend.h).
#define PERSIST_WARTET 30
#define PERSIST_WARTET_LISTE_A 31
#define PERSIST_WARTET_LISTE_B 32
// Dazu die Pausen und das Ende der aktiven Zeit - neu, darum hinten. Unter
// 34 steht mit dem Ende der Beginn des Trainings, zu dem beide gehoeren.
#define PERSIST_WARTET_PAUSEN 33
#define PERSIST_WARTET_ENDE 34

// Der Stopp eines verworfenen Trainings, bis das Telefon ihn hat.
#define PERSIST_STOPP 35

// Die Pulskurve, die auf das Telefon wartet (siehe kurve.h).
#define PERSIST_KURVE_ANZAHL 40
#define PERSIST_KURVE_BEGINN 41
#define PERSIST_KURVE_AB 42
#define PERSIST_KURVE_BASIS 50

// Reserviert (V7): 100 bis 100 + 19.
#define PERSIST_ARCHIV_BASIS 100
#define PERSIST_ARCHIV_PLAETZE 20
