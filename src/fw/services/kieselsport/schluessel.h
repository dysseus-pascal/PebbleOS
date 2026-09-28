/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Die Persist-Schluessel - DIESELBEN NUMMERN WIE IN KIESELSPORT. Der Dienst
// schreibt in den Speicher der App; wer hier eine Nummer aendert, liest die
// Einstellungen und das Archiv der Store-App nicht mehr.
//
// Frei geworden, aber nicht neu vergeben: 7-9 und 60-66 (die Nacht, jetzt in
// services/activity/boulder), 20-21 (der Startbefehl an den Worker) und 67
// (ob der Worker trainiert).
#define PERSIST_MAXPULS 1
#define PERSIST_EMPFIND 2
#define PERSIST_PAUSENZIEL 3
#define PERSIST_BECKEN 4
#define PERSIST_PIN_ART 5
#define PERSIST_PIN_ZEIT 6

#define PERSIST_ARCHIV_ANZAHL 10
#define PERSIST_ARCHIV_NAECHST 11

// Die Zusammenfassung, die auf das Telefon wartet (siehe wartend.h).
#define PERSIST_WARTET 30
#define PERSIST_WARTET_LISTE_A 31
#define PERSIST_WARTET_LISTE_B 32

// Die Pulskurve, die auf das Telefon wartet (siehe kurve.h).
#define PERSIST_KURVE_ANZAHL 40
#define PERSIST_KURVE_BEGINN 41
#define PERSIST_KURVE_AB 42
#define PERSIST_KURVE_BASIS 50

#define PERSIST_ARCHIV_BASIS 100
