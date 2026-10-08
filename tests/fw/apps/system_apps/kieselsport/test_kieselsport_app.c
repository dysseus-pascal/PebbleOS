/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "stubs_logging.h"
#include "stubs_pbl_malloc.h"

// Zuletzt: die Koepfe der App ziehen pebble_compat.h nach, und dessen Makros (malloc, time, ...)
// sollen die Koepfe davor nicht umschreiben.
#include "apps/system/kieselsport/art_text.h"
#include "apps/system/kieselsport/lauf_window.h"
#include "apps/system/kieselsport/sende_window.h"
#include "apps/system/kieselsport/symbole.h"
#include "apps/system/kieselsport/telefon.h"
#include "apps/system/kieselsport/texte.h"
#include "apps/system/kieselsport/thema.h"
#include "process_management/pebble_process_md.h"
#undef malloc
#undef calloc
#undef free
#undef time

#include <stdio.h>

// Die App beim Start (kieselsport.c): wie Boulder und der Persist sie kennen, dass sie den
// Dienst anstoesst, und wohin sie beim Oeffnen fuehrt. Nachgebaut sind Fenster, Dienst und die
// anderen Teile der App; gezeichnet wird nichts - das Menue laedt erst mit dem Fenster.

const PebbleProcessMd *kieselsport_app_get_info(void);

// --- Was die App beim Start ruft ---

static int s_angestossen;   //< kieselsport_init_anstossen
static int s_telefon_init;
static int s_angestossen_vor_telefon;
static bool s_laeuft;
static AppLaunchReason s_grund;
static uint32_t s_argumente;
static bool s_telefon_wartet;
static bool s_stopp_wartet;
static int s_fenster;        //< geschobene Fenster
static int s_laufschirm_laufend;
static int s_laufschirm_bereit;
static Sportart s_laufschirm_art;
static int s_sendeschirm;
static bool s_sendeschirm_nur_stopp;

void kieselsport_init_anstossen(void) { s_angestossen++; }
bool kieselsport_laeuft(void) { return s_laeuft; }
void telefon_init(void) {
  s_telefon_init++;
  if (s_angestossen) s_angestossen_vor_telefon++;
}
bool telefon_wartet(void) { return s_telefon_wartet; }
bool telefon_stopp_wartet(void) { return s_stopp_wartet; }
void telefon_zuruecksetzen(void) {}
void thema_zuruecksetzen(void) {}
void lauf_window_zuruecksetzen(void) {}
void sende_window_zuruecksetzen(void) {}
void strings_refresh(void) {}
void lauf_window_zeige_laufend(void) { s_laufschirm_laufend++; }
void lauf_window_zeige(Sportart art) {
  s_laufschirm_bereit++;
  s_laufschirm_art = art;
}
void sende_window_zeige(bool nur_stopp) {
  s_sendeschirm++;
  s_sendeschirm_nur_stopp = nur_stopp;
}
AppLaunchReason app_launch_reason(void) { return s_grund; }
uint32_t app_launch_get_args(void) { return s_argumente; }

// --- Fenster ---

static Window s_menue;
Window *window_create(void) { return &s_menue; }
void window_destroy(Window *window) {}
void window_set_background_color(Window *window, GColor color) {}
void window_set_window_handlers_by_value(Window *window, WindowHandlers handlers) {}
void app_window_stack_push(Window *window, bool animated) { s_fenster++; }
void app_event_loop(void) {}

// --- Das Menue: erst mit dem Fenster geladen, hier nie ---

Layer *window_get_root_layer(const Window *window) { cl_fail("Menue geladen"); return NULL; }
GRect layer_get_bounds_by_value(const Layer *layer) { cl_fail("Menue geladen"); return GRectZero; }
void layer_add_child(Layer *parent, Layer *child) { cl_fail("Menue geladen"); }
MenuLayer *menu_layer_create(GRect frame) { cl_fail("Menue geladen"); return NULL; }
void menu_layer_destroy(MenuLayer *menu_layer) { cl_fail("Menue geladen"); }
Layer *menu_layer_get_layer(const MenuLayer *menu_layer) { cl_fail("Menue geladen"); return NULL; }
void menu_layer_set_callbacks_by_value(MenuLayer *menu_layer, void *callback_context,
                                       MenuLayerCallbacks callbacks) {
  cl_fail("Menue geladen");
}
void menu_layer_set_normal_colors(MenuLayer *menu_layer, GColor background, GColor foreground) {
  cl_fail("Menue geladen");
}
void menu_layer_set_highlight_colors(MenuLayer *menu_layer, GColor background,
                                     GColor foreground) {
  cl_fail("Menue geladen");
}
void menu_layer_set_click_config_onto_window(MenuLayer *menu_layer, struct Window *window) {
  cl_fail("Menue geladen");
}
bool menu_cell_layer_is_highlighted(const Layer *cell_layer) {
  cl_fail("gezeichnet");
  return false;
}
GColor thema_sportfarbe(Sportart art) { return GColorBlack; }
GColor gcolor_legible_over(GColor background_color) { return GColorBlack; }
void graphics_context_set_fill_color(GContext *ctx, GColor color) { cl_fail("gezeichnet"); }
void graphics_context_set_text_color(GContext *ctx, GColor color) { cl_fail("gezeichnet"); }
void graphics_fill_round_rect_by_value(GContext *ctx, GRect rect, uint16_t radius,
                                       GCornerMask corner_mask) {
  cl_fail("gezeichnet");
}
void graphics_draw_text(GContext *ctx, const char *text, GFont const font, const GRect box,
                        const GTextOverflowMode overflow_mode, const GTextAlignment alignment,
                        GTextAttributes *text_attributes) {
  cl_fail("gezeichnet");
}
GFont fonts_get_system_font(const char *font_key) { return NULL; }
void symbol_sport(GContext *ctx, Sportart art, GPoint mitte, int16_t groesse, GColor farbe) {
  cl_fail("gezeichnet");
}
const char *art_name(Sportart art) { return "?"; }
const char *art_gruppe(Sportart art) { return NULL; }

// --- Hilfen ---

static void prv_starten(void) {
  kieselsport_app_get_info()->main_func();
}

void test_kieselsport_app__initialize(void) {
  s_angestossen = 0;
  s_telefon_init = 0;
  s_angestossen_vor_telefon = 0;
  s_laeuft = false;
  s_grund = APP_LAUNCH_USER;
  s_argumente = 0;
  s_telefon_wartet = false;
  s_stopp_wartet = false;
  s_fenster = 0;
  s_laufschirm_laufend = 0;
  s_laufschirm_bereit = 0;
  s_laufschirm_art = ArtAnzahl;
  s_sendeschirm = 0;
  s_sendeschirm_nur_stopp = false;
}

void test_kieselsport_app__cleanup(void) {}

// DIE APP, WIE BOULDER UND DER PERSIST SIE KENNEN. Boulder 0.52 erkennt Kieselsport an der UUID
// der b25- und Store-App, und der Persist liegt in "ps" + UUID (Spezifikation K6). Die UUID steht
// hier ausgeschrieben und nicht aus dem Header, damit ein vertipptes Byte dort auffaellt.
void test_kieselsport_app__kennung_wie_b25(void) {
  const uint8_t *b = (const uint8_t *)&kieselsport_app_get_info()->uuid;
  char hex[2 * sizeof(Uuid) + 1];
  for (size_t i = 0; i < sizeof(Uuid); i++) {
    snprintf(&hex[2 * i], 3, "%02x", b[i]);
  }
  cl_assert_equal_s(hex, "6c386250a6d74a19a48552541b5d7c0d");
}

// Eine sichtbare System-App im Startmenue und in der Quick-Launch-Auswahl (K6).
void test_kieselsport_app__sichtbare_system_app(void) {
  const PebbleProcessMd *md = kieselsport_app_get_info();
  cl_assert_equal_i(md->process_storage, ProcessStorageBuiltin);
  cl_assert_equal_i(md->process_type, ProcessTypeApp);
  cl_assert_equal_i(md->visibility, ProcessVisibilityShown);
  cl_assert(md->main_func != NULL);
  const PebbleProcessMdSystem *system = (const PebbleProcessMdSystem *)md;
  cl_assert_equal_s(system->name, "Kieselsport");
  cl_assert_equal_i(system->icon_resource_id, RESOURCE_ID_ACTIVITY_TINY);
}

// Regel 2.5.5: Ohne Pebble-Haken startet den Dienst der erste Takt der Variante oder die App -
// nach einem Boot am Ladekabel ist es die App. Vor dem Telefon: dessen erste Nachricht soll den
// Dienst schon mit seinen gespeicherten Einstellungen finden.
void test_kieselsport_app__start_stoesst_den_dienst_an(void) {
  prv_starten();
  cl_assert_equal_i(s_angestossen, 1);
  cl_assert_equal_i(s_telefon_init, 1);
  cl_assert_equal_i(s_angestossen_vor_telefon, 1);
  // Ohne Training und ohne Auftrag: nur das Menue.
  cl_assert_equal_i(s_fenster, 1);
  cl_assert_equal_i(s_laufschirm_laufend + s_laufschirm_bereit + s_sendeschirm, 0);
}

// Laeuft ein Training, fuehrt die App gleich in den Laufschirm (der Weg zurueck ins Training, aus
// dem Startmenue mit der Kachel P6 und ueber Quick Launch) - vor jedem anderen Anlass.
void test_kieselsport_app__laufendes_training_gleich_in_den_laufschirm(void) {
  s_laeuft = true;
  s_grund = APP_LAUNCH_WORKER;
  s_telefon_wartet = true;
  prv_starten();
  cl_assert_equal_i(s_laufschirm_laufend, 1);
  cl_assert_equal_i(s_sendeschirm, 0);
}

// Hat der Dienst die App geholt (Nachsenden), zeigt sie den Sendeschirm - fuer eine
// Zusammenfassung oder nur fuer den Stopp eines verworfenen Trainings.
void test_kieselsport_app__vom_dienst_geholt_der_sendeschirm(void) {
  s_grund = APP_LAUNCH_WORKER;
  s_telefon_wartet = true;
  prv_starten();
  cl_assert_equal_i(s_sendeschirm, 1);
  cl_assert(!s_sendeschirm_nur_stopp);

  test_kieselsport_app__initialize();
  s_grund = APP_LAUNCH_WORKER;
  s_stopp_wartet = true;
  prv_starten();
  cl_assert_equal_i(s_sendeschirm, 1);
  cl_assert(s_sendeschirm_nur_stopp);

  // Nichts wartet mehr (schon angekommen): nur das Menue.
  test_kieselsport_app__initialize();
  s_grund = APP_LAUNCH_WORKER;
  prv_starten();
  cl_assert_equal_i(s_sendeschirm, 0);
  // Vom Nutzer geoeffnet, auch wenn etwas wartet: das Menue, gesendet wird im Hintergrund.
  test_kieselsport_app__initialize();
  s_telefon_wartet = true;
  prv_starten();
  cl_assert_equal_i(s_sendeschirm, 0);
}

// Aus dem Timeline-Pin: Art + 1 als Launch-Code, der Laufschirm steht bereit. Unsinn tut nichts.
void test_kieselsport_app__aus_dem_pin_bereit(void) {
  s_grund = APP_LAUNCH_TIMELINE_ACTION;
  s_argumente = ArtWandern + 1;
  prv_starten();
  cl_assert_equal_i(s_laufschirm_bereit, 1);
  cl_assert_equal_i(s_laufschirm_art, ArtWandern);

  const uint32_t unsinn[] = { 0, ArtAnzahl + 1 };
  for (size_t i = 0; i < sizeof(unsinn) / sizeof(unsinn[0]); i++) {
    test_kieselsport_app__initialize();
    s_grund = APP_LAUNCH_TIMELINE_ACTION;
    s_argumente = unsinn[i];
    prv_starten();
    cl_assert_equal_i(s_laufschirm_bereit, 0);
  }
}
