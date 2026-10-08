/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// NUR AUF EMERY MIT DER VARIANTE (Schritt 3). fw/apps/system/CMakeLists.txt baut jede .c unter
// fw/apps/system/ fuer jedes Board; ohne den Schalter bleibt diese Datei leer, und Pebbles
// fw/apps/system/health/ liefert health_app_get_info. Mit dem Schalter laesst P7 Pebbles Ordner
// weg, sonst gaebe es die Funktion zweimal.
#ifdef CONFIG_BOULDER_HEALTH_APP

#include "zeiten.h"

#include "applib/app.h"
#include "applib/ui/app_window_stack.h"
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
// Noch das Geruest (AP0): ein leeres Fenster. Daten, Karten und Laden kommen mit AP2-AP6.

typedef struct {
  Window *fenster;
  // Zeichnet nichts. Ihr erstes Zeichnen ist das Ende des ersten Bildes, an dem L1 misst.
  Layer *flaeche;
  bool gezeichnet;
} BhApp;

const PebbleProcessMd *health_app_get_info(void);

static void prv_flaeche_zeichnen(Layer *flaeche, GContext *ctx) {
  BhApp *app = app_state_get_user_data();
  if (!app->gezeichnet) {
    app->gezeichnet = true;
    BH_ZEIT("bild");
  }
}

static void prv_start(void) {
  BhApp *app = app_zalloc_check(sizeof(BhApp));
  app_state_set_user_data(app);
  app->fenster = window_create();
  window_set_background_color(app->fenster, GColorWhite);
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
