/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// NUR AUF EMERY MIT DER VARIANTE (wie boulder_health.c).
#ifdef CONFIG_BOULDER_HEALTH_APP

#include "einstieg.h"

#include "pbl/services/timeline/health_layout.h"

BhEinstieg bh_einstieg(AppLaunchReason grund, ButtonId knopf, AppQuickLaunchAction aktion,
                       uint32_t argument) {
  // Nur Tippen und Halten auf Oben oder Unten haben eine Richtung. Back+Up meldet Pebble als
  // Back, Up+Down als Up, beide mit COMBO (watchface.c:97-98): keine Richtung, also Kreis.
  const bool gerichtet =
      (grund == APP_LAUNCH_QUICK_LAUNCH) &&
      ((knopf == BUTTON_ID_UP) || (knopf == BUTTON_ID_DOWN)) &&
      ((aktion == APP_QUICK_LAUNCH_ACTION_TAP) || (aktion == APP_QUICK_LAUNCH_ACTION_HOLD));
  BhEinstieg einstieg = {
    .karte = BhKarteToday,
    .kreis = !gerichtet,
    .gespiegelt = gerichtet && (knopf == BUTTON_ID_DOWN),
  };
  // Die Pins legt Pebbles activity_insights.c mit derselben Struktur an (prv_set_open_app_action).
  if (grund == APP_LAUNCH_TIMELINE_ACTION) {
    const HealthLaunchArgs pin = {.args = argument};
    if (pin.card_type == HealthCardType_Sleep) {
      einstieg.karte = BhKarteNight;
    } else if (pin.card_type == HealthCardType_Activity) {
      einstieg.karte = BhKarteActivity;
    }
  }
  return einstieg;
}

BhSchritt bh_blaettern(const BhEinstieg *einstieg, BhKarte *karte, ButtonId knopf) {
  if ((knopf != BUTTON_ID_UP) && (knopf != BUTTON_ID_DOWN)) {
    return BhSchrittNichts;
  }
  const bool weiter = einstieg->gespiegelt ? (knopf == BUTTON_ID_DOWN) : (knopf == BUTTON_ID_UP);
  const int neu = (int)*karte + (weiter ? 1 : -1);
  if (neu < 0) {
    if (!einstieg->kreis) {
      return BhSchrittZifferblatt;
    }
    *karte = (BhKarte)(BhKartenAnzahl - 1);
  } else if (neu >= BhKartenAnzahl) {
    if (!einstieg->kreis) {
      return BhSchrittNichts;
    }
    *karte = BhKarteToday;
  } else {
    *karte = (BhKarte)neu;
  }
  return BhSchrittKarte;
}

#endif  // CONFIG_BOULDER_HEALTH_APP
