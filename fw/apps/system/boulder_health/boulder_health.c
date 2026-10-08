/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// NUR AUF EMERY MIT DER VARIANTE (Schritt 3). fw/apps/system/CMakeLists.txt baut jede .c unter
// fw/apps/system/ fuer jedes Board; ohne den Schalter bleibt diese Datei leer, und Pebbles
// fw/apps/system/health/ liefert health_app_get_info. Mit dem Schalter laesst P7 Pebbles Ordner
// weg, sonst gaebe es die Funktion zweimal.
#ifdef CONFIG_BOULDER_HEALTH_APP

#include "einstieg.h"
#include "karten.h"
#include "probe.h"
#include "zeiten.h"

#include "applib/app.h"
#include "applib/app_launch_button.h"
#include "applib/app_launch_reason.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/click.h"
#include "applib/ui/layer.h"
#include "applib/ui/window.h"
#include "kernel/pbl_malloc.h"
#include "process_management/pebble_process_md.h"
#include "process_state/app_state/app_state.h"
#include "pbl/services/i18n/i18n.h"
#include "pbl/services/timeline/timeline.h"
#include "resource/resource_ids.auto.h"

// Die Health-App der Boulder-Firmware, an der Stelle von Pebbles.
//
// GLEICHE ID, UUID UND health_app_get_info: So fuehren Startmenue, Quick Launch, die Pins der
// Insights und der Uebergang der Shell ohne weiteren Pebble-Code hierher.
//
// OHNE SYSTEMRECHTE: Ein Fehler beendet nur die App, nicht die Uhr, und vom Kern sieht sie nur,
// was ein Syscall ihr kopiert. Darum gibt es hier kein beschreibbares static - es laege in
// .kernel_bss, das die App nicht schreiben darf. Der Zustand liegt im Heap der App.
//
// Noch der Rahmen: Start und Blaettern (AP7), jede Karte mit ihrem Kopfbalken. Daten, Inhalt der
// Karten, Details und Laden kommen mit AP2-AP6.

typedef struct {
  Window *fenster;
  // Zeichnet die gezeigte Karte. Ihr Zeichnen nach dem Start und nach jeder Taste ist das Ende
  // des Bildes, an dem L1 und L2 messen.
  Layer *flaeche;
  BhEinstieg einstieg;
  BhKarte karte;
  bool bild_offen; //< das naechste Zeichnen beendet einen Start oder einen Tastendruck
} BhApp;

const PebbleProcessMd *health_app_get_info(void);

static void prv_flaeche_zeichnen(Layer *flaeche, GContext *ctx) {
  BhApp *app = app_state_get_user_data();
  bh_kopf_zeichnen(ctx, layer_get_bounds_by_value(flaeche), app->karte);
  if (app->bild_offen) {
    app->bild_offen = false;
    BH_ZEIT("bild");
  }
}

// Oben und Unten (2.4). OHNE SYSCALL bis zum Bild (L3): Die Karte kommt aus dem Zustand im Heap,
// nur der Pruefbau schreibt die Zeitstempel.
static void prv_blaettern(ClickRecognizerRef anlass, void *kontext) {
  BhApp *app = app_state_get_user_data();
  const ButtonId knopf = click_recognizer_get_button_id(anlass);
  BH_ZEIT("taste");
  switch (bh_blaettern(&app->einstieg, &app->karte, knopf)) {
    case BhSchrittKarte:
      BH_PROTOKOLL("karte %s", bh_karte_titel(app->karte));
      app->bild_offen = true;
      layer_mark_dirty(app->flaeche);
      break;
    case BhSchrittZifferblatt:
      BH_ZEIT("zifferblatt");
      // Wie Pebble (card_view.c:291): Die App endet, und Pebble kehrt zum Zifferblatt zurueck,
      // von dem Quick Launch sie geoeffnet hat - mit dem Uebergang nach dem Start (shell.c:48-54).
      app_window_stack_pop_all(true /* animiert */);
      break;
    case BhSchrittNichts:
      BH_ZEIT("nichts");
      break;
  }
}

static void prv_tasten(void *kontext) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_blaettern);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_blaettern);
}

static void prv_start(void) {
  BhApp *app = app_zalloc_check(sizeof(BhApp));
  app_state_set_user_data(app);
  // Wie die App gestartet ist, liest sie einmal hier, vor dem ersten Bild (Phase 0).
  const AppLaunchReason grund = app_launch_reason();
  const ButtonId knopf = app_launch_button();
  const AppQuickLaunchAction aktion = app_launch_get_quick_launch_action();
  const uint32_t argument = app_launch_get_args();
  app->einstieg = bh_einstieg(grund, knopf, aktion, argument);
  app->karte = app->einstieg.karte;
  app->bild_offen = true;
  BH_PROTOKOLL("start grund=%d knopf=%d aktion=%d argument=%lu %s kreis=%d spiegel=%d",
               (int)grund, (int)knopf, (int)aktion, (unsigned long)argument,
               bh_karte_titel(app->karte), (int)app->einstieg.kreis,
               (int)app->einstieg.gespiegelt);
  app->fenster = window_create();
  window_set_background_color(app->fenster, GColorWhite);
  window_set_click_config_provider(app->fenster, prv_tasten);
  Layer *wurzel = window_get_root_layer(app->fenster);
  app->flaeche = layer_create(layer_get_bounds_by_value(wurzel));
  layer_set_update_proc(app->flaeche, prv_flaeche_zeichnen);
  layer_add_child(wurzel, app->flaeche);
  app_window_stack_push(app->fenster, true /* animiert */);
}

static void prv_ende(void) {
  BhApp *app = app_state_get_user_data();
  layer_destroy(app->flaeche);
  window_destroy(app->fenster);
  task_free(app);
}

static void prv_boulder_health_main(void) {
  BH_ZEIT("main");
  // Nur im Pruefbau: "boulder probe" und "boulder fehlerprobe" starten statt der App eine Probe
  // (probe.h). Sonst ist bh_probe() false und der Zweig faellt weg.
  if (bh_probe()) {
    return;
  }
  prv_start();
  app_event_loop();
  prv_ende();
}

// Ausserhalb der Funktion und mit eigenem Namen, damit nm auf jedem Board zeigt, wessen App
// gebaut ist (T3.5).
static const PebbleProcessMdSystem s_boulder_health_info = {
  .common = {
    .main_func = prv_boulder_health_main,
    .uuid = UUID_HEALTH_DATA_SOURCE,
    .is_unprivileged = true,
  },
  .icon_resource_id = RESOURCE_ID_MENU_ICON_HEALTH,
  .name = i18n_noop("Health"),
};

const PebbleProcessMd *health_app_get_info(void) {
  return &s_boulder_health_info.common;
}

#endif  // CONFIG_BOULDER_HEALTH_APP
