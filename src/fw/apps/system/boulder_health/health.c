/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "karten.h"

#include "applib/health_service.h"
#include "process_management/pebble_process_md.h"
#include "pbl/services/i18n/i18n.h"
#include "pbl/services/timeline/timeline.h"
#include "pbl/services/activity/activity.h"

// Die Health-App der Boulder-Firmware - an der Stelle von Pebbles.
//
// DIESELBE KENNUNG WIE PEBBLES APP: so fuehrt die untere Taste am Zifferblatt
// weiter hierher, mit derselben Animation, und die Einstellungen der Uhr
// finden sie. Inhalt und Aussehen sind neu: drei Karten, zwischen denen Oben
// und Unten blaettern - Heute, Schlaf, Herz & SpO2.
//
// DER PULS IST LIVE auf der Herz-Karte: dort bittet die App den Sensor um
// einen Wert je Sekunde, und ein Herz schlaegt im Takt, solange er misst.
// ZWEI MINUTEN LANG, dann misst er wieder im gewohnten Takt: die App hat
// kein Zeitlimit, und wer sie auf der Herz-Karte offen liess, bezahlte den
// Sekundentakt mit dem Akku, bis jemand sie schloss. Wer wieder hinblaettert,
// bekommt zwei neue Minuten.
#define HERZ_LIVE_MS (2 * 60 * 1000)

typedef struct {
  Window *fenster;
  Layer *flaeche;
  Karte karte;
  HealthDaten daten;
  MinutenLesen lesen;
  AppTimer *minuten_timer;
  AppTimer *herz_timer;
  AppTimer *live_timer;     //< Ende der zwei Minuten Live-Puls
  bool herz_gross;
  // Was zuletzt gezeichnet wurde (prv_sichtbar): neu gezeichnet wird nur,
  // wenn sich etwas aendert - jede Sekunde den ganzen Schirm kostete Strom
  // fuer nichts.
  HealthDaten gezeichnet;
} HealthApp;

// Nach dem Entladen des Fensters ist die Flaeche weg; Tick und Timer koennen
// bis zum Ende der App noch kommen.
static void prv_neu_zeichnen(HealthApp *app) {
  if (app->flaeche) {
    layer_mark_dirty(app->flaeche);
  }
}

// Die Daten so, wie die Karten sie zeigen: von den Zeitpunkten nur, was zu
// sehen ist - "vor 3 min" statt der Sekunde der letzten Messung, die sich
// beim Live-Puls jede Sekunde aendert, ohne dass sich das Bild aendert.
static void prv_sichtbar(const HealthDaten *d, HealthDaten *aus) {
  memcpy(aus, d, sizeof(*aus));
  const time_t vor = d->puls ? (time(NULL) - d->puls_zeit) / SECONDS_PER_MINUTE : 0;
  aus->puls_zeit = vor < 2 ? 0 : vor;
  aus->spo2_zeit = 0;
}

static void prv_neu_zeichnen_wenn_anders(HealthApp *app) {
  HealthDaten jetzt;
  prv_sichtbar(&app->daten, &jetzt);
  if (memcmp(&app->gezeichnet, &jetzt, sizeof(jetzt)) == 0) {
    return;
  }
  memcpy(&app->gezeichnet, &jetzt, sizeof(jetzt));
  prv_neu_zeichnen(app);
}

static void prv_zeichne(Layer *layer, GContext *ctx) {
  HealthApp *app = app_state_get_user_data();
  const int herz = app->herz_timer ? (app->herz_gross ? 2 : 1) : 0;
  karten_zeichnen(ctx, layer_get_bounds(layer), app->karte, &app->daten, herz);
}

static void prv_minuten_schritt(void *data) {
  HealthApp *app = app_state_get_user_data();
  app->minuten_timer = NULL;
  if (daten_minuten_schritt(&app->lesen, &app->daten)) {
    prv_neu_zeichnen_wenn_anders(app);
    return;
  }
  // Zwischen zwei Stuecken kommen Tasten und Zeichnen zum Zug.
  app->minuten_timer = app_timer_register(10, prv_minuten_schritt, NULL);
}

static void prv_minuten_lesen(HealthApp *app) {
  if (app->minuten_timer) {
    return;
  }
  daten_minuten_beginnen(&app->lesen);
  app->minuten_timer = app_timer_register(30, prv_minuten_schritt, NULL);
}

// DAS HERZ SCHLAEGT IM TAKT DES PULSES, solange der Sensor live misst: kurz
// gross, dann klein bis zum naechsten Schlag.
#define SCHLAG_MS 150

static bool prv_herz_schlaegt(HealthApp *app) {
  return app->karte == KarteHerz && app->daten.puls_live;
}

static void prv_schlag(void *data) {
  HealthApp *app = app_state_get_user_data();
  app->herz_timer = NULL;
  if (!prv_herz_schlaegt(app)) {
    app->herz_gross = false;
    prv_neu_zeichnen(app);
    return;
  }
  app->herz_gross = !app->herz_gross;
  const uint32_t pause = 60000 / app->daten.puls;
  app->herz_timer = app_timer_register(app->herz_gross ? SCHLAG_MS : pause - SCHLAG_MS,
                                       prv_schlag, NULL);
  prv_neu_zeichnen(app);
}

static void prv_herz_pruefen(HealthApp *app) {
  if (prv_herz_schlaegt(app) && !app->herz_timer) {
    app->herz_gross = false;
    prv_schlag(NULL);
  }
}

static void prv_tick(struct tm *zeit, TimeUnits einheiten) {
  HealthApp *app = app_state_get_user_data();
  // Jede Minute die Tageswerte, alle fuenf Minuten die Kurven, dazwischen nur der Puls.
  if (zeit->tm_sec == 0) {
    daten_sammeln(&app->daten);
    if (zeit->tm_min % 5 == 0) {
      prv_minuten_lesen(app);
    }
  }
  daten_puls(&app->daten);
  prv_herz_pruefen(app);
  prv_neu_zeichnen_wenn_anders(app);
}

static void prv_live_ende(void *data) {
  HealthApp *app = app_state_get_user_data();
  app->live_timer = NULL;
  health_service_set_heart_rate_sample_period(0 /* zurueck zur Vorgabe */);
}

// Der Sensor im Sekundentakt - nur auf der Herz-Karte und nur zwei Minuten.
static void prv_live(HealthApp *app, bool an) {
  if (app->live_timer) {
    app_timer_cancel(app->live_timer);
    app->live_timer = NULL;
  }
  health_service_set_heart_rate_sample_period(an ? 1 : 0);
  if (an) {
    app->live_timer = app_timer_register(HERZ_LIVE_MS, prv_live_ende, NULL);
  }
}

static void prv_blaettern(int richtung) {
  HealthApp *app = app_state_get_user_data();
  const int neu = (int)app->karte + richtung;
  if (neu < 0 || neu >= KartenAnzahl) {
    return;
  }
  // DER SENSOR MISST SEKUENDLICH NUR AUF DER HERZ-KARTE - sonst zeigt nichts
  // den Puls, und der Akku zahlte umsonst.
  if (app->karte == KarteHerz || neu == KarteHerz) {
    prv_live(app, neu == KarteHerz);
  }
  app->karte = (Karte)neu;
  prv_herz_pruefen(app);
  prv_neu_zeichnen(app);
}

static void prv_oben(ClickRecognizerRef anlass, void *context) { prv_blaettern(-1); }
static void prv_unten(ClickRecognizerRef anlass, void *context) { prv_blaettern(1); }

static void prv_tasten(void *context) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 150, prv_oben);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 150, prv_unten);
}

static void prv_laden(Window *fenster) {
  HealthApp *app = app_state_get_user_data();
  Layer *wurzel = window_get_root_layer(fenster);
  app->flaeche = layer_create(layer_get_bounds(wurzel));
  layer_set_update_proc(app->flaeche, prv_zeichne);
  layer_add_child(wurzel, app->flaeche);
}

static void prv_entladen(Window *fenster) {
  HealthApp *app = app_state_get_user_data();
  // ERST ALLES ABMELDEN, WAS NOCH ZEICHNEN WILL. Das letzte Fenster wird
  // sofort entladen; bis die App wirklich endet, kamen Tick und Timer noch
  // und markierten die zerstoerte Flaeche.
  tick_timer_service_unsubscribe();
  if (app->herz_timer) {
    app_timer_cancel(app->herz_timer);
    app->herz_timer = NULL;
  }
  if (app->minuten_timer) {
    app_timer_cancel(app->minuten_timer);
    app->minuten_timer = NULL;
  }
  layer_destroy(app->flaeche);
  app->flaeche = NULL;
}

static void prv_main(void) {
  HealthApp *app = app_zalloc_check(sizeof(HealthApp));
  app_state_set_user_data(app);
  karten_sprache();
  daten_sammeln(&app->daten);

  app->fenster = window_create();
  window_set_background_color(app->fenster, GColorWhite);
  window_set_click_config_provider(app->fenster, prv_tasten);
  window_set_window_handlers(app->fenster, (WindowHandlers){
    .load = prv_laden,
    .unload = prv_entladen,
  });
  window_stack_push(app->fenster, true);

  prv_minuten_lesen(app);

  tick_timer_service_subscribe(SECOND_UNIT, prv_tick);

  app_event_loop();

  tick_timer_service_unsubscribe();
  if (app->minuten_timer) {
    app_timer_cancel(app->minuten_timer);
  }
  if (app->herz_timer) {
    app_timer_cancel(app->herz_timer);
  }
  if (app->live_timer) {
    app_timer_cancel(app->live_timer);
  }
  health_service_set_heart_rate_sample_period(0 /* zurueck zur Vorgabe */);
  window_destroy(app->fenster);
  app_free(app);
}

const PebbleProcessMd *health_app_get_info(void) {
  static const PebbleProcessMdSystem s_health_app_info = {
    .common = {
      .main_func = &prv_main,
      .uuid = UUID_HEALTH_DATA_SOURCE,
#if CAPABILITY_HAS_CORE_NAVIGATION4
      .visibility = ProcessVisibilityHidden,
#endif
    },
    .icon_resource_id = RESOURCE_ID_MENU_ICON_HEALTH,
    .name = i18n_noop("Health"),
  };
  return (const PebbleProcessMd *)&s_health_app_info;
}
