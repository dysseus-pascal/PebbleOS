/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "pebble_compat.h"
#include "art_text.h"

// Der laufende Schirm - die Anzeige zu dem, was der Dienst misst.
//
// DIE BEDIENUNG:
//   Select   Start - dann Pause und Weiter im Wechsel
//   Oben     in der Pause: speichern
//   Unten    in der Pause: verwerfen (zweimal, damit kein Fehlgriff ein
//            Training kostet)
//   Zurueck  die App verlassen; das Training laeuft im Hintergrund weiter,
//            und das Startmenue zeigt es unter Kieselsport an

// Mit einer gewaehlten Art: der Schirm steht bereit, Select startet.
void lauf_window_zeige(Sportart art);
// Ein Training laeuft schon (die App wurde neu geoeffnet): gleich hinein.
void lauf_window_zeige_laufend(void);

// Der Zustand dieser Datei auf Anfang - beim Start der App (siehe kieselsport.c).
void lauf_window_zuruecksetzen(void);
