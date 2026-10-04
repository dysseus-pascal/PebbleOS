/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "stubs_logging.h"
#include "stubs_pbl_malloc.h"

#include "apps/system/kieselsport/lauf_window.h"
#include "apps/system/kieselsport/symbole.h"
#include "apps/system/kieselsport/telefon.h"
#include "apps/system/kieselsport/thema.h"
#undef malloc
#undef calloc
#undef free
#undef time

// Der laufende Schirm (lauf_window.c): wann er neu gezeichnet wird. Der
// Dienst liefert einen Stand, den der Test stellt; Uhr, Wecker, Fenster und
// Tasten sind nachgebaut. Gezeichnet wird hier nichts - gezaehlt wird, wie oft
// der Schirm darum bittet (layer_mark_dirty).

// --- Uhr und Wecker ---

static time_t s_jetzt;
time_t rtc_get_time(void) { return s_jetzt; }

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

// Laesst [ms] vergehen; die Uhr in Sekunden laeuft mit.
static void prv_warte(uint32_t ms) {
  const uint32_t bis = s_ms + ms;
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
    if (w.faellig_ms > s_ms) {
      s_jetzt += (w.faellig_ms / 1000) - (s_ms / 1000);
      s_ms = w.faellig_ms;
    }
    w.cb(w.daten);
  }
  s_jetzt += (bis / 1000) - (s_ms / 1000);
  s_ms = bis;
}

// --- Der Dienst ---

static KsStand s_stand;
static int s_pause_befehle;
void kieselsport_stand(KsStand *aus) { *aus = s_stand; }
const ArtInfo *kieselsport_art_info(Sportart art) {
  static const ArtInfo s_info = { .schritte = true, .distanz = true };
  return &s_info;
}
int kieselsport_zone(uint16_t bpm) { return bpm >= 130 ? 3 : 2; }
void kieselsport_starte(Sportart art, uint32_t beginn) {}
void kieselsport_pause_umschalten(void) { s_pause_befehle++; }
void kieselsport_speichern(void) {}
void kieselsport_verwerfen(void) {}
bool kieselsport_kurve_zeiten(uint32_t sekunden[KS_ZONEN + 1]) { return false; }
uint32_t kieselsport_kurve_beginn(void) { return 0; }

// --- Telefon ---

void telefon_melde_zustand(Trainingsmeldung was, uint8_t art, uint32_t beginn) {}
void telefon_nachsenden(void) {}
bool telefon_wartet(void) { return false; }
void telefon_stopp_nachsenden(void) {}
bool telefon_stopp_wartet(void) { return false; }

// --- Fenster, Schicht, Tasten ---

static Window s_fenster;
static Layer s_wurzel;
static Layer s_flaeche;
static WindowHandlers s_handler;
static ClickConfigProvider s_tasten;
static ClickHandler s_klick[NUM_BUTTONS];
static int s_neu_zeichnen;

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
  if (s_tasten) s_tasten(NULL);
}
bool app_window_stack_remove(Window *window, bool animated) { return true; }
Window *app_window_stack_pop(bool animated) { return NULL; }
void app_window_stack_pop_all(const bool animated) {}
void window_single_click_subscribe(ButtonId button_id, ClickHandler handler) {
  s_klick[button_id] = handler;
}

Layer *layer_create(GRect frame) { return &s_flaeche; }
void layer_destroy(Layer *layer) {}
void layer_set_update_proc(Layer *layer, LayerUpdateProc update_proc) {}
void layer_add_child(Layer *parent, Layer *child) {}
GRect layer_get_bounds_by_value(const Layer *layer) { return GRect(0, 0, 200, 228); }
GRect layer_get_unobstructed_bounds_by_value(const Layer *layer) { return GRect(0, 0, 200, 228); }
void layer_mark_dirty(Layer *layer) {
  cl_assert(layer == &s_flaeche);
  s_neu_zeichnen++;
}
void app_unobstructed_area_service_subscribe(UnobstructedAreaHandlers handlers, void *context) {}
void app_unobstructed_area_service_unsubscribe(void) {}

// --- Was nur beim Zeichnen gebraucht wuerde: hier nie gerufen ---

void graphics_context_set_text_color(GContext *ctx, GColor color) { cl_fail("gezeichnet"); }
void graphics_context_set_fill_color(GContext *ctx, GColor color) { cl_fail("gezeichnet"); }
void graphics_context_set_stroke_color(GContext *ctx, GColor color) { cl_fail("gezeichnet"); }
void graphics_context_set_stroke_width(GContext *ctx, uint8_t width) { cl_fail("gezeichnet"); }
void graphics_fill_round_rect_by_value(GContext *ctx, GRect rect, uint16_t radius,
                                       GCornerMask corner_mask) { cl_fail("gezeichnet"); }
void graphics_draw_rect_by_value(GContext *ctx, GRect rect) { cl_fail("gezeichnet"); }
void graphics_draw_line(GContext *ctx, GPoint p0, GPoint p1) { cl_fail("gezeichnet"); }
void graphics_fill_circle(GContext *ctx, GPoint p, uint16_t radius) { cl_fail("gezeichnet"); }
void graphics_draw_circle(GContext *ctx, GPoint p, uint16_t radius) { cl_fail("gezeichnet"); }
void graphics_draw_text(GContext *ctx, const char *text, GFont const font, const GRect box,
                        const GTextOverflowMode overflow_mode, const GTextAlignment alignment,
                        GTextAttributes *text_attributes) { cl_fail("gezeichnet"); }
GSize app_graphics_text_layout_get_content_size(const char *text, GFont const font,
                                                const GRect box,
                                                const GTextOverflowMode overflow_mode,
                                                const GTextAlignment alignment) {
  cl_fail("gezeichnet");
  return GSizeZero;
}
GFont fonts_get_system_font(const char *font_key) { return NULL; }
void gpath_init(GPath *path, const GPathInfo *init) { cl_fail("gezeichnet"); }
void gpath_move_to(GPath *path, GPoint point) { cl_fail("gezeichnet"); }
void gpath_draw_filled(GContext *ctx, GPath *path) { cl_fail("gezeichnet"); }
void gpath_draw_outline(GContext *ctx, GPath *path) { cl_fail("gezeichnet"); }
GColor gcolor_legible_over(GColor background_color) { return GColorBlack; }
void thema_leiste(GContext *ctx, GRect b, Symbol oben, Symbol mitte, Symbol unten) {
  cl_fail("gezeichnet");
}
GColor thema_zonenfarbe(int zone) { return GColorBlack; }
GColor thema_sportfarbe(Sportart art) { return GColorBlack; }
void symbol_sport(GContext *ctx, Sportart art, GPoint mitte, int16_t groesse, GColor farbe) {
  cl_fail("gezeichnet");
}
void clock_copy_time_string(char *buffer, uint8_t size) { cl_fail("gezeichnet"); }
bool clock_is_24h_style(void) { return true; }
struct tm *pbl_override_localtime(const time_t *timep) {
  static struct tm t;
  return gmtime_r(timep, &t);
}
bool connection_service_peek_pebble_app_connection(void) { return true; }
const char *app_get_system_locale(void) { return "en_US"; }
void vibes_short_pulse(void) {}
void vibes_long_pulse(void) {}

// --- Hilfen ---

static void prv_laufend(uint32_t dauer, uint16_t puls) {
  s_stand.zustand = LaufLaeuft;
  s_stand.t.dauer_s = dauer;
  s_stand.t.beginn = 1790000000;
  s_stand.puls = puls;
  s_stand.frisch = true;
  s_stand.minute[dauer % KS_MINUTE_S] = (uint8_t)puls;
}

void test_kieselsport_lauf__initialize(void) {
  memset(s_wecker, 0, sizeof(s_wecker));
  s_wecker_id = 0;
  s_ms = 0;
  s_jetzt = 1790000000 - 1790000000 % 60 + 5;
  memset(&s_stand, 0, sizeof(s_stand));
  memset(s_klick, 0, sizeof(s_klick));
  s_tasten = NULL;
  s_neu_zeichnen = 0;
  s_pause_befehle = 0;
  lauf_window_zuruecksetzen();
}

void test_kieselsport_lauf__cleanup(void) {}

void test_kieselsport_lauf__gleicher_stand_zeichnet_nicht_neu(void) {
  prv_laufend(100, 120);
  lauf_window_zeige_laufend();
  prv_warte(100);
  const int erstes = s_neu_zeichnen;
  cl_assert(erstes >= 1);
  // Zwei Sekunden, viermal gefragt - derselbe Stand, kein neues Bild.
  prv_warte(2000);
  cl_assert_equal_i(s_neu_zeichnen, erstes);
  // Eine Sekunde weiter: genau ein neues Bild.
  prv_laufend(101, 120);
  prv_warte(500);
  cl_assert_equal_i(s_neu_zeichnen, erstes + 1);
  // Der Puls aendert sich: auch.
  s_stand.puls = 125;
  prv_warte(500);
  cl_assert_equal_i(s_neu_zeichnen, erstes + 2);
}

void test_kieselsport_lauf__pause_steht_still(void) {
  prv_laufend(100, 120);
  s_stand.zustand = LaufPause;
  lauf_window_zeige_laufend();
  prv_warte(100);
  const int erstes = s_neu_zeichnen;
  // Zwanzig Sekunden Pause: die Zeit steht, der Schirm auch.
  prv_warte(20000);
  cl_assert(s_neu_zeichnen - erstes <= 1);
}

void test_kieselsport_lauf__uhrzeit_und_hinweis_kommen_trotzdem(void) {
  prv_laufend(100, 120);
  s_stand.zustand = LaufPause;
  lauf_window_zeige_laufend();
  prv_warte(100);
  // Eine neue Minute: die Uhrzeit auf dem Schirm geht weiter.
  int vorher = s_neu_zeichnen;
  prv_warte(60000);
  cl_assert(s_neu_zeichnen > vorher);
  // Der Hinweis "nochmal druecken" verschwindet nach drei Sekunden wieder.
  prv_warte(5000);
  cl_assert(s_klick[BUTTON_ID_DOWN]);
  s_klick[BUTTON_ID_DOWN](NULL, NULL);
  vorher = s_neu_zeichnen;
  prv_warte(4000);
  cl_assert(s_neu_zeichnen > vorher);
}
