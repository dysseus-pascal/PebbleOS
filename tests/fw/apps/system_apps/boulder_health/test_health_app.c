/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "stubs_logging.h"

#include "fake_health_quellen.h"
#include "apps/system/boulder_health/karten.h"
#include "process_management/pebble_process_md.h"

// Die Health-App (health.c) mit ihren echten Daten (daten.c). Nachgebaut sind
// die Quellen (fake_health_quellen.h), Fenster, Tasten, Wecker, Sekundentakt
// und der Pulssensor; gezeichnet wird nicht, gezaehlt wird, wie oft die App
// darum bittet.

const PebbleProcessMd *health_app_get_info(void);

// --- Karten: Zeichnen ist nicht Teil dieses Tests ---

void karten_sprache(void) {}
void karten_zeichnen(GContext *ctx, GRect b, Karte karte, const HealthDaten *d, int herz) {}

// --- App-Zustand ---

static void *s_nutzerdaten;
void app_state_set_user_data(void *data) { s_nutzerdaten = data; }
void *app_state_get_user_data(void) { return s_nutzerdaten; }
void *app_zalloc_check(size_t bytes) { return calloc(1, bytes); }

// --- Wecker ---

typedef struct {
  uint32_t id;
  uint32_t faellig_ms;
  AppTimerCallback cb;
  void *daten;
} Wecker;
#define WECKER_MAX 16
static Wecker s_wecker[WECKER_MAX];
static uint32_t s_wecker_id;
static uint32_t s_ms;

AppTimer *app_timer_register(uint32_t timeout_ms, AppTimerCallback cb, void *daten) {
  for (int i = 0; i < WECKER_MAX; i++) {
    if (!s_wecker[i].id) {
      s_wecker[i] = (Wecker){ .id = ++s_wecker_id, .faellig_ms = s_ms + timeout_ms,
                              .cb = cb, .daten = daten };
      return (AppTimer *)(uintptr_t)s_wecker[i].id;
    }
  }
  cl_fail("zu viele Wecker");
  return NULL;
}

void app_timer_cancel(AppTimer *timer) {
  for (int i = 0; i < WECKER_MAX; i++) {
    if (s_wecker[i].id == (uint32_t)(uintptr_t)timer) s_wecker[i].id = 0;
  }
}

// --- Sekundentakt ---

static TickHandler s_tick;
void tick_timer_service_subscribe(TimeUnits tick_units, TickHandler handler) { s_tick = handler; }
void tick_timer_service_unsubscribe(void) { s_tick = NULL; }

// Eine Sekunde vergeht: erst die Wecker, dann der Takt.
static void prv_sekunde(void) {
  const uint32_t bis = s_ms + 1000;
  for (;;) {
    int n = -1;
    for (int i = 0; i < WECKER_MAX; i++) {
      if (s_wecker[i].id && s_wecker[i].faellig_ms <= bis &&
          (n < 0 || s_wecker[i].faellig_ms < s_wecker[n].faellig_ms)) {
        n = i;
      }
    }
    if (n < 0) break;
    const Wecker w = s_wecker[n];
    s_wecker[n].id = 0;
    if (w.faellig_ms > s_ms) s_ms = w.faellig_ms;
    w.cb(w.daten);
  }
  s_ms = bis;
  s_jetzt++;
  if (s_tick) {
    struct tm zeit;
    prv_zerlegen(s_jetzt, &zeit);
    s_tick(&zeit, SECOND_UNIT);
  }
}

// --- Pulssensor ---

static uint16_t s_messtakt;
bool health_service_set_heart_rate_sample_period(uint16_t interval_sec) {
  s_messtakt = interval_sec;
  return true;
}

// --- Fenster, Schicht, Tasten ---

static Window s_fenster;
static Layer s_wurzel;
static Layer s_flaeche;
static bool s_flaeche_lebt;
static int s_neu_zeichnen;
static WindowHandlers s_handler;
static ClickConfigProvider s_tasten;
static ClickHandler s_klick[NUM_BUTTONS];

Window *window_create(void) { return &s_fenster; }
void window_destroy(Window *window) {}
void window_set_background_color(Window *window, GColor color) {}
void window_set_click_config_provider(Window *window, ClickConfigProvider provider) {
  s_tasten = provider;
}
void window_set_window_handlers_by_value(Window *window, WindowHandlers handlers) {
  s_handler = handlers;
}
Layer *window_get_root_layer(const Window *window) { return &s_wurzel; }
void app_window_stack_push(Window *window, bool animated) {
  s_handler.load(window);
  s_tasten(NULL);
}
void window_single_repeating_click_subscribe(ButtonId button_id, uint16_t repeat_interval_ms,
                                             ClickHandler handler) {
  s_klick[button_id] = handler;
}
Layer *layer_create(GRect frame) {
  s_flaeche_lebt = true;
  return &s_flaeche;
}
void layer_destroy(Layer *layer) { s_flaeche_lebt = false; }
void layer_set_update_proc(Layer *layer, LayerUpdateProc update_proc) {}
void layer_add_child(Layer *parent, Layer *child) {}
GRect layer_get_bounds_by_value(const Layer *layer) { return GRect(0, 0, 200, 228); }
void layer_mark_dirty(Layer *layer) {
  // Eine zerstoerte Flaeche zu markieren ist ein Zugriff auf freigegebenen
  // Speicher.
  cl_assert(layer == &s_flaeche && s_flaeche_lebt);
  s_neu_zeichnen++;
}

// --- Der Lauf der App: was zwischen Start und Ende geschieht ---

static void (*s_szenario)(void);
void app_event_loop(void) {
  if (s_szenario) s_szenario();
}

static void prv_app(void (*szenario)(void)) {
  s_szenario = szenario;
  const PebbleProcessMdSystem *md = (const PebbleProcessMdSystem *)health_app_get_info();
  md->common.main_func();
}

void test_health_app__initialize(void) {
  memset(s_wecker, 0, sizeof(s_wecker));
  memset(s_metriken, 0, sizeof(s_metriken));
  memset(s_klick, 0, sizeof(s_klick));
  s_wecker_id = 0;
  s_ms = 0;
  s_umstellung = 0;
  s_versatz_vor = s_versatz_nach = 7200;
  s_jetzt = 1790493000 - 1790493000 % 60 + 1;
  s_tick = NULL;
  s_messtakt = 0;
  s_neu_zeichnen = 0;
  s_metriken[ActivityMetricStepCount] = 4200;
}

void test_health_app__cleanup(void) {}

// --- Nach dem Entladen ---

static void prv_szenario_entladen(void) {
  prv_sekunde();
  // Das letzte Fenster geht: es wird sofort entladen, die App endet erst
  // spaeter. Was dazwischen noch kommt, darf die Flaeche nicht anfassen.
  s_handler.unload(&s_fenster);
  cl_assert(!s_tick);
  for (int i = 0; i < 5; i++) prv_sekunde();
}

void test_health_app__nach_dem_entladen_kein_zugriff_auf_die_flaeche(void) {
  prv_app(prv_szenario_entladen);
}

// --- Live-Puls nur zwei Minuten ---

static void prv_szenario_live(void) {
  s_klick[BUTTON_ID_DOWN](NULL, NULL);
  s_klick[BUTTON_ID_DOWN](NULL, NULL);
  cl_assert_equal_i(s_messtakt, 1);
  for (int i = 0; i < 119; i++) prv_sekunde();
  cl_assert_equal_i(s_messtakt, 1);
  prv_sekunde();
  // Zwei Minuten um: zurueck zum gewohnten Takt.
  cl_assert_equal_i(s_messtakt, 0);
  // Weg und wieder hin: zwei neue Minuten.
  s_klick[BUTTON_ID_UP](NULL, NULL);
  cl_assert_equal_i(s_messtakt, 0);
  s_klick[BUTTON_ID_DOWN](NULL, NULL);
  cl_assert_equal_i(s_messtakt, 1);
  for (int i = 0; i < 121; i++) prv_sekunde();
  cl_assert_equal_i(s_messtakt, 0);
}

void test_health_app__live_puls_nur_zwei_minuten(void) {
  prv_app(prv_szenario_live);
  cl_assert_equal_i(s_messtakt, 0);
}

// --- Neu gezeichnet nur bei Aenderung ---

static void prv_szenario_ruhig(void) {
  // Die Minutenhistorie kommt herein.
  for (int i = 0; i < 3; i++) prv_sekunde();
  const int vorher = s_neu_zeichnen;
  // Zehn Sekunden, nichts aendert sich: kein neues Bild.
  for (int i = 0; i < 10; i++) prv_sekunde();
  cl_assert_equal_i(s_neu_zeichnen, vorher);
  // Ein Puls kommt: ein neues Bild.
  s_metriken[ActivityMetricHeartRateRawBPM] = 72;
  s_metriken[ActivityMetricHeartRateRawUpdatedTimeUTC] = (int32_t)s_jetzt;
  prv_sekunde();
  cl_assert_equal_i(s_neu_zeichnen, vorher + 1);
  // Er kommt jede Sekunde wieder, mit derselben Zahl: das Bild bleibt.
  for (int i = 0; i < 10; i++) {
    s_metriken[ActivityMetricHeartRateRawUpdatedTimeUTC] = (int32_t)s_jetzt;
    prv_sekunde();
  }
  cl_assert_equal_i(s_neu_zeichnen, vorher + 1);
  // Eine andere Zahl: ein neues Bild.
  s_metriken[ActivityMetricHeartRateRawBPM] = 75;
  s_metriken[ActivityMetricHeartRateRawUpdatedTimeUTC] = (int32_t)s_jetzt;
  prv_sekunde();
  cl_assert_equal_i(s_neu_zeichnen, vorher + 2);
}

void test_health_app__ohne_aenderung_kein_neues_bild(void) {
  prv_app(prv_szenario_ruhig);
}
