/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Wie die Health-App startet und blaettert (Spezifikation Schritt 3, 2.4; AP7).
//
// UNTEN BLAETTERT VORWAERTS, OBEN ZURUECK, nach jedem Start (Entscheid F23 vom 14.10.2026; bis b29
// blaetterte Oben vorwaerts wie in Pebbles Health-App, card_view.c:280-301, nach Quick Launch
// Unten gespiegelt). Start, Kreis und erste Karte bleiben wie bei Pebble (card_view.c:325-340):
// Nach Quick Launch fuehrt Oben auf der ersten Karte zurueck zum Zifferblatt.
//
// REIN: nur Argumente hinein, Ergebnis heraus, kein Syscall. Die Taste blaettert also ohne
// Syscall (L3); den Start liest boulder_health.c einmal vor dem ersten Bild.

#include "karten.h"

#include "applib/app_launch_reason.h"

#include <pbl/drivers/button_id.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
  BhKarte karte;   //< die erste Karte
  // Die Karten laufen im Kreis. Nicht nach Quick Launch mit Tippen oder Halten auf Oben oder
  // Unten: Dort liegt das Zifferblatt hinter der ersten Karte.
  bool kreis;
} BhEinstieg;

// Aus dem Start der App (app_launch_reason, app_launch_button, app_launch_get_quick_launch_action,
// app_launch_get_args). "Open App" aus einem Pin der Insights oeffnet Night (card_type 1) oder
// Activity (card_type 0), jeder andere Start Today. Pebble reicht das Argument nur fuer
// APP_LAUNCH_TIMELINE_ACTION durch; aus dem Startmenue ist es 0 wie "Activity" aus einem Pin.
BhEinstieg bh_einstieg(AppLaunchReason grund, ButtonId knopf, AppQuickLaunchAction aktion,
                       uint32_t argument);

typedef enum {
  BhSchrittKarte,       //< *karte ist die neue Karte
  BhSchrittZifferblatt, //< die App schliesst, zurueck zum Zifferblatt
  BhSchrittNichts,      //< letzte Karte ohne Kreis, oder keine Blaettertaste
} BhSchritt;

// Oben oder Unten auf einer Karte. Andere Tasten tun hier nichts.
BhSchritt bh_blaettern(const BhEinstieg *einstieg, BhKarte *karte, ButtonId knopf);
