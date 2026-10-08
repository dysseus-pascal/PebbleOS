/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "apps/system/boulder_health/einstieg.h"
#include "pbl/services/timeline/health_layout.h"

#include <stdio.h>

// Start und Blaettern der Health-App (Spezifikation Schritt 3, 2.4; T3.2, T3.4, T5.3 fuer Oben
// und Unten). Geprueft wird die echte einstieg.c gegen die Tabelle in 2.4. Die Starts stehen hier
// so, wie Pebble sie ausloest (Belege an jedem Fall); dass Pebbles Code sie wirklich so ausloest,
// prueft test_bh_einstiege gegen watchface.c, app_run_state.c und timeline.c.

static const char *const s_name[] = {"Today", "Night", "Heart", "Activity", "Settings"};

static void prv_start(AppLaunchReason grund, ButtonId knopf, AppQuickLaunchAction aktion,
                      uint32_t argument, BhKarte karte, bool kreis, bool gespiegelt) {
  const BhEinstieg e = bh_einstieg(grund, knopf, aktion, argument);
  cl_assert_equal_s(s_name[e.karte], s_name[karte]);
  cl_assert_equal_b(e.kreis, kreis);
  cl_assert_equal_b(e.gespiegelt, gespiegelt);
}

// Blaettert mit einer Folge von Tasten ('o' Oben, 'u' Unten) und schreibt, wo es landet:
// den Titel jeder Karte, "Z" fuer das Zifferblatt (dann ist Schluss), "-" fuer nichts.
static void prv_blaettern(BhEinstieg e, const char *tasten, const char *soll) {
  char ist[256] = "";
  BhKarte karte = e.karte;
  for (const char *t = tasten; *t; t++) {
    const BhKarte vorher = karte;
    const BhSchritt s = bh_blaettern(&e, &karte, (*t == 'o') ? BUTTON_ID_UP : BUTTON_ID_DOWN);
    const size_t n = strlen(ist);
    if (s == BhSchrittKarte) {
      snprintf(ist + n, sizeof(ist) - n, "%s%s", n ? " " : "", s_name[karte]);
    } else {
      // Ohne neue Karte bleibt die Karte, wie sie war.
      cl_assert_equal_i(karte, vorher);
      snprintf(ist + n, sizeof(ist) - n, "%s%s", n ? " " : "", (s == BhSchrittZifferblatt) ? "Z" : "-");
      if (s == BhSchrittZifferblatt) {
        break;
      }
    }
  }
  cl_assert_equal_s(ist, soll);
}

void test_bh_einstieg__initialize(void) {}
void test_bh_einstieg__cleanup(void) {}

// --- Start (2.4, Zeile "Start") ---

// Startmenue: Pebbles Startmenue startet mit APP_LAUNCH_USER und Select
// (launcher/default/menu_layer.c:87-92). Today, Karten im Kreis.
void test_bh_einstieg__startmenue(void) {
  prv_start(APP_LAUNCH_USER, BUTTON_ID_SELECT, APP_QUICK_LAUNCH_ACTION_NONE, 0, BhKarteToday,
            true, false);
}

// Quick Launch mit Tippen oder Halten auf Oben: Today, kein Kreis. Tippen und Halten melden
// beide APP_LAUNCH_QUICK_LAUNCH mit der Taste (watchface.c:180-199, :201-217, :219-231).
void test_bh_einstieg__quick_launch_oben(void) {
  prv_start(APP_LAUNCH_QUICK_LAUNCH, BUTTON_ID_UP, APP_QUICK_LAUNCH_ACTION_TAP, 0, BhKarteToday,
            false, false);
  prv_start(APP_LAUNCH_QUICK_LAUNCH, BUTTON_ID_UP, APP_QUICK_LAUNCH_ACTION_HOLD, 0, BhKarteToday,
            false, false);
}

// Quick Launch Unten: Today, kein Kreis, die Richtung gespiegelt.
void test_bh_einstieg__quick_launch_unten(void) {
  prv_start(APP_LAUNCH_QUICK_LAUNCH, BUTTON_ID_DOWN, APP_QUICK_LAUNCH_ACTION_TAP, 0, BhKarteToday,
            false, true);
  prv_start(APP_LAUNCH_QUICK_LAUNCH, BUTTON_ID_DOWN, APP_QUICK_LAUNCH_ACTION_HOLD, 0,
            BhKarteToday, false, true);
}

// Select- und Back-Halten blaettern im Kreis.
void test_bh_einstieg__quick_launch_select_und_back(void) {
  prv_start(APP_LAUNCH_QUICK_LAUNCH, BUTTON_ID_SELECT, APP_QUICK_LAUNCH_ACTION_HOLD, 0,
            BhKarteToday, true, false);
  prv_start(APP_LAUNCH_QUICK_LAUNCH, BUTTON_ID_BACK, APP_QUICK_LAUNCH_ACTION_HOLD, 0,
            BhKarteToday, true, false);
}

// Kombinationen blaettern im Kreis. Up+Down meldet Pebble als Oben (watchface.c:97-98) - ohne
// den Blick auf COMBO saehe das wie Quick Launch Oben aus und haette keinen Kreis.
void test_bh_einstieg__quick_launch_kombinationen(void) {
  prv_start(APP_LAUNCH_QUICK_LAUNCH, BUTTON_ID_BACK, APP_QUICK_LAUNCH_ACTION_COMBO, 0,
            BhKarteToday, true, false);
  prv_start(APP_LAUNCH_QUICK_LAUNCH, BUTTON_ID_UP, APP_QUICK_LAUNCH_ACTION_COMBO, 0,
            BhKarteToday, true, false);
}

// Start vom Telefon (app_run_state.c:61-66): Grund APP_LAUNCH_PHONE, keine Taste gesetzt (0 ist
// BUTTON_ID_BACK). Today, Kreis.
void test_bh_einstieg__vom_telefon(void) {
  prv_start(APP_LAUNCH_PHONE, BUTTON_ID_BACK, APP_QUICK_LAUNCH_ACTION_NONE, 0, BhKarteToday, true,
            false);
}

// "Open App" aus einem Pin der Insights (timeline.c:817-831): card_type 1 (Schlaf) oeffnet Night,
// card_type 0 (Aktivitaet) Activity, beide im Kreis wie bei Pebble.
void test_bh_einstieg__pin(void) {
  prv_start(APP_LAUNCH_TIMELINE_ACTION, BUTTON_ID_BACK, APP_QUICK_LAUNCH_ACTION_NONE,
            HealthCardType_Sleep, BhKarteNight, true, false);
  prv_start(APP_LAUNCH_TIMELINE_ACTION, BUTTON_ID_BACK, APP_QUICK_LAUNCH_ACTION_NONE,
            HealthCardType_Activity, BhKarteActivity, true, false);
}

// Ein unbekannter Kartentyp oeffnet Today. Pebble nimmt wie hier nur das untere Byte (die Union
// in health_layout.h:33-44); was darueber steht, aendert die Karte nicht.
void test_bh_einstieg__pin_unbekannt(void) {
  prv_start(APP_LAUNCH_TIMELINE_ACTION, BUTTON_ID_BACK, APP_QUICK_LAUNCH_ACTION_NONE,
            HealthCardTypeCount, BhKarteToday, true, false);
  prv_start(APP_LAUNCH_TIMELINE_ACTION, BUTTON_ID_BACK, APP_QUICK_LAUNCH_ACTION_NONE, 0xff,
            BhKarteToday, true, false);
  prv_start(APP_LAUNCH_TIMELINE_ACTION, BUTTON_ID_BACK, APP_QUICK_LAUNCH_ACTION_NONE, 0x0101,
            BhKarteNight, true, false);
}

// Das Argument zaehlt nur fuer "Open App". Aus dem Startmenue ist es bei Pebble 0
// (process_manager.c:712-718) - dasselbe wie "Activity" aus einem Pin -, und auch ein anderer Wert
// waehlt dort keine Karte.
void test_bh_einstieg__argument_nur_aus_dem_pin(void) {
  prv_start(APP_LAUNCH_USER, BUTTON_ID_SELECT, APP_QUICK_LAUNCH_ACTION_NONE, HealthCardType_Sleep,
            BhKarteToday, true, false);
  prv_start(APP_LAUNCH_PHONE, BUTTON_ID_BACK, APP_QUICK_LAUNCH_ACTION_NONE, HealthCardType_Sleep,
            BhKarteToday, true, false);
  prv_start(APP_LAUNCH_QUICK_LAUNCH, BUTTON_ID_UP, APP_QUICK_LAUNCH_ACTION_TAP,
            HealthCardType_Sleep, BhKarteToday, false, false);
}

// Jeder andere Grund oeffnet Today im Kreis; eine Taste ohne Quick Launch hat keine Richtung.
void test_bh_einstieg__andere_gruende(void) {
  const AppLaunchReason gruende[] = {APP_LAUNCH_SYSTEM, APP_LAUNCH_WAKEUP, APP_LAUNCH_WORKER,
                                     APP_LAUNCH_SMARTSTRAP};
  for (size_t i = 0; i < sizeof(gruende) / sizeof(gruende[0]); i++) {
    for (int knopf = 0; knopf < NUM_BUTTONS; knopf++) {
      prv_start(gruende[i], (ButtonId)knopf, APP_QUICK_LAUNCH_ACTION_TAP, HealthCardType_Sleep,
                BhKarteToday, true, false);
    }
  }
  prv_start(APP_LAUNCH_QUICK_LAUNCH, BUTTON_ID_UP, APP_QUICK_LAUNCH_ACTION_NONE, 0, BhKarteToday,
            true, false);
}

// --- Blaettern (2.4, Zeile "Oben / Unten") ---
//
// Pebbles card_view.c:280-301: Oben blaettert weiter, Unten zurueck; nach Quick Launch Unten
// umgekehrt. Ohne Kreis fuehrt "zurueck" auf der ersten Karte zum Zifferblatt, "weiter" auf der
// letzten tut nichts.

// Startmenue, Pin, Telefon, Select-/Back-Halten, Kombinationen: im Kreis, in beide Richtungen.
void test_bh_einstieg__kreis(void) {
  const BhEinstieg e = bh_einstieg(APP_LAUNCH_USER, BUTTON_ID_SELECT, APP_QUICK_LAUNCH_ACTION_NONE,
                                   0);
  prv_blaettern(e, "oooooo", "Night Heart Activity Settings Today Night");
  prv_blaettern(e, "uuuuuu", "Settings Activity Heart Night Today Settings");
  prv_blaettern(e, "ouuo", "Night Today Settings Today");
}

// Quick Launch Oben: Oben weiter bis Settings, dann nichts; Unten zurueck, auf Today zum
// Zifferblatt - dorthin, wo der Shutter beim Schliessen hinfaellt (shell.c:48-54).
void test_bh_einstieg__quick_launch_oben_ohne_kreis(void) {
  const BhEinstieg e = bh_einstieg(APP_LAUNCH_QUICK_LAUNCH, BUTTON_ID_UP,
                                   APP_QUICK_LAUNCH_ACTION_TAP, 0);
  prv_blaettern(e, "oooooo", "Night Heart Activity Settings - -");
  prv_blaettern(e, "u", "Z");
  prv_blaettern(e, "oouuu", "Night Heart Night Today Z");
  prv_blaettern(e, "oooooouuuu", "Night Heart Activity Settings - - Activity Heart Night Today");
}

// Quick Launch Unten spiegelt: Unten weiter, Oben zurueck und auf Today zum Zifferblatt.
void test_bh_einstieg__quick_launch_unten_gespiegelt(void) {
  const BhEinstieg e = bh_einstieg(APP_LAUNCH_QUICK_LAUNCH, BUTTON_ID_DOWN,
                                   APP_QUICK_LAUNCH_ACTION_HOLD, 0);
  prv_blaettern(e, "uuuuuu", "Night Heart Activity Settings - -");
  prv_blaettern(e, "o", "Z");
  prv_blaettern(e, "uuooo", "Night Heart Night Today Z");
}

// Aus dem Pin auf Night: im Kreis von dort aus.
void test_bh_einstieg__pin_blaettert_im_kreis(void) {
  const BhEinstieg schlaf = bh_einstieg(APP_LAUNCH_TIMELINE_ACTION, BUTTON_ID_BACK,
                                        APP_QUICK_LAUNCH_ACTION_NONE, HealthCardType_Sleep);
  prv_blaettern(schlaf, "uuo", "Today Settings Today");
  const BhEinstieg aktiv = bh_einstieg(APP_LAUNCH_TIMELINE_ACTION, BUTTON_ID_BACK,
                                       APP_QUICK_LAUNCH_ACTION_NONE, HealthCardType_Activity);
  prv_blaettern(aktiv, "oo", "Settings Today");
}

// Select und Zurueck blaettern nicht: Select oeffnet Details (AP6), Zurueck schliesst die App ueber
// Pebbles Fenster-Stapel.
void test_bh_einstieg__andere_tasten_blaettern_nicht(void) {
  const BhEinstieg e = bh_einstieg(APP_LAUNCH_QUICK_LAUNCH, BUTTON_ID_UP,
                                   APP_QUICK_LAUNCH_ACTION_TAP, 0);
  BhKarte karte = BhKarteToday;
  cl_assert_equal_i(bh_blaettern(&e, &karte, BUTTON_ID_SELECT), BhSchrittNichts);
  cl_assert_equal_i(bh_blaettern(&e, &karte, BUTTON_ID_BACK), BhSchrittNichts);
  cl_assert_equal_i(karte, BhKarteToday);
}
