/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// NUR MIT DER HEALTH-APP DER VARIANTE (emery): fw/apps/system/CMakeLists.txt baut jede .c unter
// fw/apps/system/ fuer jedes Board; ohne den Schalter bleibt diese Datei leer, und die Registry
// fuehrt das Zifferblatt nicht.
#ifdef CONFIG_BOULDER_TAGESBAND

#include "applib/app.h"
#include "applib/app_timer.h"
#include "applib/tick_timer_service.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/layer.h"
#include "applib/ui/window.h"
#include "apps/system/boulder_health/daten.h"
#include "bild.h"
#include "kernel/pbl_malloc.h"
#include "pbl/services/clock.h"
#include "process_management/pebble_process_md.h"
#include "process_state/app_state/app_state.h"
#include "resource/resource_ids.auto.h"

// Das Zifferblatt Tagesband: Zeit und Datum, darunter das Band der letzten 18 Stunden mit der
// Nacht in Phasen, Pebbles Sitzungen, dem laufenden Schlafmodus und dem Puls, ganz unten
// Schritte, Ruhepuls und HRV von heute.
//
// DIE DATEN DER HEALTH-APP: Es liest ueber deren Datenschicht (daten.h), einmal beim Start und
// dann im Minutentakt; so zeigen Zifferblatt und App dieselben Werte. OHNE SYSTEMRECHTE wie die
// App: kein beschreibbares static, der Zustand liegt im Heap.

typedef struct {
  Window *fenster;
  Layer *ebene;
  BhDaten daten;
  AppTimer *nachladen;
} BtApp;

static BtApp *prv_app(void) {
  return app_state_get_user_data();
}

static void prv_zeichnen(Layer *ebene, GContext *ctx) {
  const GRect flaeche = layer_get_bounds_by_value(ebene);
  const GRect frei = layer_get_unobstructed_bounds_by_value(ebene);
  bt_zeichnen(ctx, flaeche, frei.size.h < flaeche.size.h, &prv_app()->daten);
}

// Nach dem Boot laedt KernelBG Nacht, Naechte und Herz erst; wie die Health-App fragt das
// Zifferblatt dann im Sekundenabstand nach, statt eine Minute mit leerem Band zu stehen.
static void prv_nachladen(void *kontext) {
  BtApp *app = prv_app();
  app->nachladen = NULL;
  const bool weiter = bh_daten_nachladen(&app->daten);
  layer_mark_dirty(app->ebene);
  if (weiter) {
    app->nachladen = app_timer_register(BH_NACHLADEN_MS, prv_nachladen, NULL);
  }
}

static void prv_minute(struct pbl_tm *zeit, TimeUnits einheiten) {
  BtApp *app = prv_app();
  // Pebble ruft den Handler beim Anmelden und beim Wechsel 12/24 h mit 0 (applib/
  // tick_timer_service.c). Beim Anmelden ist eben alles gelesen; der Wechsel soll sofort sichtbar
  // sein, nicht erst mit der naechsten Minute.
  if (!(einheiten & MINUTE_UNIT)) {
    app->daten.h24 = clock_is_24h_style();
  } else {
    bh_daten_auffrischen(&app->daten);
  }
  layer_mark_dirty(app->ebene);
}

static void prv_main(void) {
  BtApp *app = app_zalloc_check(sizeof(BtApp));
  app_state_set_user_data(app);
  bh_daten_laden(&app->daten);
  if (app->daten.offen) {
    app->nachladen = app_timer_register(BH_NACHLADEN_MS, prv_nachladen, NULL);
  }
  app->fenster = window_create();
  window_set_background_color(app->fenster, GColorBlack);
  Layer *wurzel = window_get_root_layer(app->fenster);
  app->ebene = layer_create(layer_get_bounds_by_value(wurzel));
  layer_set_update_proc(app->ebene, prv_zeichnen);
  layer_add_child(wurzel, app->ebene);
  app_window_stack_push(app->fenster, true /* animiert */);
  tick_timer_service_subscribe(MINUTE_UNIT, prv_minute);

  app_event_loop();

  tick_timer_service_unsubscribe();
  if (app->nachladen) {
    app_timer_cancel(app->nachladen);
  }
  layer_destroy(app->ebene);
  window_destroy(app->fenster);
  task_free(app);
}

const PebbleProcessMd *boulder_tagesband_get_app_info(void) {
  static const PebbleProcessMdSystem s_info = {
    .common =
        {
          // UUID: 33f4ef6a-4e17-4ce6-96a6-5de94853fbb3
          .uuid =
              {0x33, 0xf4, 0xef, 0x6a, 0x4e, 0x17, 0x4c, 0xe6, 0x96, 0xa6, 0x5d, 0xe9, 0x48, 0x53,
               0xfb, 0xb3},
          .main_func = prv_main,
          .process_type = ProcessTypeWatchface,
          .is_unprivileged = true,
        },
    // Ohne eigenes Bild: Pebbles Liste der Zifferblaetter zeigt dann ihr allgemeines
    // (watchfaces.c).
    .icon_resource_id = RESOURCE_ID_INVALID,
    .name = "Tagesband",
  };
  return &s_info.common;
}

#endif // CONFIG_BOULDER_TAGESBAND
