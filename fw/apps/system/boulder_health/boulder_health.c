/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// NUR AUF EMERY MIT DER VARIANTE (Schritt 3). fw/apps/system/CMakeLists.txt baut jede .c unter
// fw/apps/system/ fuer jedes Board; ohne den Schalter bleibt diese Datei leer, und Pebbles
// fw/apps/system/health/ liefert health_app_get_info. Mit dem Schalter laesst P7 Pebbles Ordner
// weg, sonst gaebe es die Funktion zweimal.
#ifdef CONFIG_BOULDER_HEALTH_APP

#include "daten.h"
#include "details.h"
#include "einstieg.h"
#include "inhalt.h"
#include "karten.h"
#include "probe.h"
#include "zeichnen.h"
#include "zeiten.h"

#include "applib/app.h"
#include "applib/app_launch_button.h"
#include "applib/app_launch_reason.h"
#include "applib/app_timer.h"
#include "applib/health_service.h"
#include "applib/tick_timer_service.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/click.h"
#include "applib/ui/layer.h"
#include "applib/ui/scroll_layer.h"
#include "applib/ui/window.h"
#include "kernel/pbl_malloc.h"
#include "pbl/services/activity/boulder/messstufe.h"
#include "pbl/services/activity/boulder/schnittstelle.h"
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
// LADEN (Spezifikation Schritt 3, 2.6.2): Phase 0 liest den Start, Phase 1 alle Daten vor dem
// ersten Bild, Phase 2 fehlendes nach. Danach liest nur der Minutentakt; eine Taste liest nichts
// (L3). Die einzige Schreibung an einer Taste ist der Puls-Takt beim Wechsel auf oder weg von
// Heart (V3) und das Setzen der Messstufe in Precision.

// V3: Puls im Sekundentakt nur auf Heart und hoechstens so lange.
#define PULS_LIVE_MS (2 * 60 * 1000)

typedef struct {
  Window *fenster;
  // Zeichnet die gezeigte Karte. Ihr Zeichnen nach dem Start und nach jeder Taste ist das Ende
  // des Bildes, an dem L1 und L2 messen.
  Layer *flaeche;
  BhEinstieg einstieg;
  BhKarte karte;
  bool bild_offen; //< das naechste Zeichnen beendet einen Start, eine Taste oder ein Auffrischen
  BhDaten daten;
  AppTimer *nachladen;
  AppTimer *puls_ende;
  bool puls_live;

  // Das offene Detail; NULL ohne.
  Window *detail_fenster;
  Layer *detail_kopf;
  ScrollLayer *rolle;
  Layer *detail_inhalt;
  BhDetail detail;
  uint8_t auswahl; //< Precision: markierte Stufe 0..4
} BhApp;

const PebbleProcessMd *health_app_get_info(void);

static BhApp *prv_app(void) {
  return app_state_get_user_data();
}

// --- Zeichnen ---

static void prv_flaeche_zeichnen(Layer *flaeche, GContext *ctx) {
  BhApp *app = prv_app();
  const GRect r = layer_get_bounds_by_value(flaeche);
  bh_kopf_zeichnen(ctx, r, app->karte);
  bh_inhalt_zeichnen(ctx, r, app->karte, &app->daten);
  if (app->bild_offen) {
    app->bild_offen = false;
    BH_ZEIT("bild");
  }
}

static void prv_detail_kopf_zeichnen(Layer *kopf, GContext *ctx) {
  BhApp *app = prv_app();
  bh_kopf_detail(ctx, layer_get_bounds_by_value(kopf).size.w, bh_detail_titel(app->detail),
                 bh_karte_farbe(app->karte));
}

static void prv_detail_inhalt_zeichnen(Layer *inhalt, GContext *ctx) {
  BhApp *app = prv_app();
  bh_detail_zeichnen(ctx, layer_get_bounds_by_value(inhalt).size.w, app->detail, &app->daten,
                     app->auswahl);
  if (app->bild_offen) {
    app->bild_offen = false;
    BH_ZEIT("bild");
  }
}

static void prv_neu_zeichnen(BhApp *app) {
  app->bild_offen = true;
  layer_mark_dirty(app->flaeche);
  if (app->detail_inhalt) {
    layer_mark_dirty(app->detail_inhalt);
  }
}

// --- Puls live (V3) ---

static void prv_puls(BhApp *app, bool an);

static void prv_puls_ende(void *kontext) {
  BhApp *app = prv_app();
  app->puls_ende = NULL;
  prv_puls(app, false);
}

static void prv_puls(BhApp *app, bool an) {
  if (an == app->puls_live) {
    return;
  }
  app->puls_live = an;
  health_service_set_heart_rate_sample_period(an ? 1 : 0 /* zurueck zur Vorgabe */);
  if (app->puls_ende) {
    app_timer_cancel(app->puls_ende);
    app->puls_ende = NULL;
  }
  if (an) {
    app->puls_ende = app_timer_register(PULS_LIVE_MS, prv_puls_ende, NULL);
  }
  BH_PROTOKOLL("puls %s", an ? "an" : "aus");
}

static void prv_ereignis(HealthEventType ereignis, void *kontext) {
  BhApp *app = prv_app();
  if (ereignis == HealthEventHeartRateUpdate && app->karte == BhKarteHeart) {
    bh_daten_puls(&app->daten);
    layer_mark_dirty(app->flaeche);
  }
}

// --- Laden ---

static void prv_nachladen(void *kontext) {
  BhApp *app = prv_app();
  app->nachladen = NULL;
  const bool weiter = bh_daten_nachladen(&app->daten);
  BH_PROTOKOLL("nachgeladen offen=%u", (unsigned)app->daten.offen);
  prv_neu_zeichnen(app);
  if (weiter) {
    app->nachladen = app_timer_register(BH_NACHLADEN_MS, prv_nachladen, NULL);
  }
}

static void prv_minute(struct tm *zeit, TimeUnits einheiten) {
  BhApp *app = prv_app();
  BH_ZEIT("auffrischen");
  bh_daten_auffrischen(&app->daten);
  prv_neu_zeichnen(app);
}

// --- Details ---

static void prv_detail_entladen(Window *fenster) {
  BhApp *app = prv_app();
  if (app->rolle) {
    scroll_layer_destroy(app->rolle);
  }
  layer_destroy(app->detail_inhalt);
  layer_destroy(app->detail_kopf);
  window_destroy(app->detail_fenster);
  app->detail_fenster = NULL;
  app->detail_kopf = NULL;
  app->detail_inhalt = NULL;
  app->rolle = NULL;
  app->detail = BhDetailKeins;
}

static void prv_stufe_waehlen(ClickRecognizerRef anlass, void *kontext) {
  BhApp *app = prv_app();
  const ButtonId knopf = click_recognizer_get_button_id(anlass);
  if (knopf == BUTTON_ID_UP && app->auswahl > 0) {
    app->auswahl--;
  } else if (knopf == BUTTON_ID_DOWN && app->auswahl + 1 < MESSSTUFE_MAX) {
    app->auswahl++;
  } else {
    return;
  }
  app->bild_offen = true;
  layer_mark_dirty(app->detail_inhalt);
}

static void prv_stufe_setzen(ClickRecognizerRef anlass, void *kontext) {
  BhApp *app = prv_app();
  const uint8_t stufe = app->auswahl + 1;
  // Der Kern setzt die Stufe auf KernelBG (S4). Die Karte zeigt sie sofort; die Lage bestaetigt
  // sie im naechsten Minutentakt.
  if (sys_boulder_messstufe_setzen(stufe)) {
    app->daten.lage.messstufe = stufe;
  }
  BH_PROTOKOLL("stufe %u", stufe);
  app_window_stack_pop(true /* animiert */);
  prv_neu_zeichnen(app);
}

static void prv_precision_tasten(void *kontext) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 150, prv_stufe_waehlen);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 150, prv_stufe_waehlen);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_stufe_setzen);
}

static void prv_detail_oeffnen(BhApp *app, BhDetail detail) {
  app->detail = detail;
  const uint8_t stufe = app->daten.lage.messstufe;
  app->auswahl = (stufe >= 1 && stufe <= MESSSTUFE_MAX) ? stufe - 1 : 0;
  app->detail_fenster = window_create();
  window_set_background_color(app->detail_fenster, GColorWhite);
  window_set_window_handlers_by_value(app->detail_fenster,
                                      (WindowHandlers){.unload = prv_detail_entladen});
  Layer *wurzel = window_get_root_layer(app->detail_fenster);
  const GRect r = layer_get_bounds_by_value(wurzel);
  app->detail_kopf = layer_create(GRect(0, 0, r.size.w, BH_KOPF_H));
  layer_set_update_proc(app->detail_kopf, prv_detail_kopf_zeichnen);
  const GRect unten = GRect(0, BH_KOPF_H, r.size.w, r.size.h - BH_KOPF_H);
  const int16_t hoehe = bh_detail_hoehe(detail, &app->daten);
  app->detail_inhalt =
      layer_create((detail == BhDetailPrecision) ? unten : GRect(0, 0, r.size.w, hoehe));
  layer_set_update_proc(app->detail_inhalt, prv_detail_inhalt_zeichnen);
  if (detail == BhDetailPrecision) {
    layer_add_child(wurzel, app->detail_inhalt);
    window_set_click_config_provider(app->detail_fenster, prv_precision_tasten);
  } else {
    app->rolle = scroll_layer_create(unten);
    scroll_layer_set_shadow_hidden(app->rolle, true);
    scroll_layer_set_content_size(app->rolle, GSize(r.size.w, hoehe));
    scroll_layer_add_child(app->rolle, app->detail_inhalt);
    scroll_layer_set_click_config_onto_window(app->rolle, app->detail_fenster);
    layer_add_child(wurzel, scroll_layer_get_layer(app->rolle));
  }
  layer_add_child(wurzel, app->detail_kopf);
  app->bild_offen = true;
  app_window_stack_push(app->detail_fenster, true /* animiert */);
}

// --- Karten ---

// Oben und Unten (2.4). OHNE LESEN bis zum Bild (L3): Die Karte kommt aus dem Zustand im Heap,
// nur der Pruefbau schreibt die Zeitstempel.
static void prv_blaettern(ClickRecognizerRef anlass, void *kontext) {
  BhApp *app = prv_app();
  const ButtonId knopf = click_recognizer_get_button_id(anlass);
  BH_ZEIT("taste");
  switch (bh_blaettern(&app->einstieg, &app->karte, knopf)) {
    case BhSchrittKarte:
      BH_PROTOKOLL("karte %s", bh_karte_titel(app->karte));
      app->bild_offen = true;
      layer_mark_dirty(app->flaeche);
      prv_puls(app, app->karte == BhKarteHeart);
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

static void prv_auswaehlen(ClickRecognizerRef anlass, void *kontext) {
  BhApp *app = prv_app();
  BH_ZEIT("taste");
  const BhDetail detail = bh_detail_von(app->karte);
  if (detail == BhDetailKeins) {
    BH_ZEIT("nichts"); // Today hat kein Detail (F12)
    return;
  }
  BH_PROTOKOLL("detail %s", bh_detail_titel(detail));
  prv_detail_oeffnen(app, detail);
}

static void prv_tasten(void *kontext) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_blaettern);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_blaettern);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_auswaehlen);
}

static void prv_start(void) {
  BhApp *app = app_zalloc_check(sizeof(BhApp));
  app_state_set_user_data(app);
  // Phase 0: wie die App gestartet ist, einmal hier, vor dem ersten Bild.
  const AppLaunchReason grund = app_launch_reason();
  const ButtonId knopf = app_launch_button();
  const AppQuickLaunchAction aktion = app_launch_get_quick_launch_action();
  const uint32_t argument = app_launch_get_args();
  app->einstieg = bh_einstieg(grund, knopf, aktion, argument);
  app->karte = app->einstieg.karte;
  app->bild_offen = true;
  BH_PROTOKOLL("start grund=%d knopf=%d aktion=%d argument=%lu %s kreis=%d spiegel=%d", (int)grund,
               (int)knopf, (int)aktion, (unsigned long)argument, bh_karte_titel(app->karte),
               (int)app->einstieg.kreis, (int)app->einstieg.gespiegelt);
  // Phase 1: alle Daten vor dem ersten Bild; fehlendes holt Phase 2.
  bh_daten_laden(&app->daten);
  BH_ZEIT("daten");
  if (app->daten.offen) {
    app->nachladen = app_timer_register(BH_NACHLADEN_MS, prv_nachladen, NULL);
  }
  app->fenster = window_create();
  window_set_background_color(app->fenster, GColorWhite);
  window_set_click_config_provider(app->fenster, prv_tasten);
  Layer *wurzel = window_get_root_layer(app->fenster);
  app->flaeche = layer_create(layer_get_bounds_by_value(wurzel));
  layer_set_update_proc(app->flaeche, prv_flaeche_zeichnen);
  layer_add_child(wurzel, app->flaeche);
  app_window_stack_push(app->fenster, true /* animiert */);
  tick_timer_service_subscribe(MINUTE_UNIT, prv_minute);
  health_service_events_subscribe(prv_ereignis, NULL);
  prv_puls(app, app->karte == BhKarteHeart);
}

static void prv_ende(void) {
  BhApp *app = prv_app();
  // Abos zuerst: Nach dem Ende bleibt kein Puls-Takt und kein Abo (Kriterium 7).
  prv_puls(app, false);
  health_service_events_unsubscribe();
  tick_timer_service_unsubscribe();
  if (app->nachladen) {
    app_timer_cancel(app->nachladen);
  }
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
  .common =
      {
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

#endif // CONFIG_BOULDER_HEALTH_APP
