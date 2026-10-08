/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "applib/app_launch_button.h"
#include "applib/app_launch_reason.h"
#include "applib/fonts/fonts.h"
#include "applib/graphics/graphics.h"
#include "applib/graphics/text.h"
#include "applib/ui/click.h"
#include "applib/ui/layer.h"
#include "applib/ui/window.h"
#include "font_resource_keys.auto.h"
#include "pbl/services/timeline/health_layout.h"
#include "process_management/pebble_process_md.h"
#include "resource/resource_ids.auto.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Boulders Health-App beim Start und an den Tasten (Schritt 3, AP0 und AP7): wie die Einstiege
// sie finden, dass sie ohne Systemrechte laeuft, dass ihr Zustand im Heap der App liegt und alles
// wieder freigibt, auf welcher Karte sie oeffnet, wohin Oben und Unten fuehren, und die
// Zeitstempel des Pruefbaus (nur mit CONFIG_BOULDER_HEALTH_ZEITEN, test_bh_app_zeiten).
// Nachgebaut sind Pebbles Fenster, Ebenen, Grafik, Start-Syscalls, Heap und Protokoll; die App,
// ihr Einstieg (einstieg.c) und ihr Kartenrahmen (karten.c) sind echt.

const PebbleProcessMd *health_app_get_info(void);

// --- Was die App ruft, mitgezaehlt ---

static char s_reihe[256];       //< die Aufrufe in ihrer Reihenfolge, je ein Buchstabe
static Window s_fenster;
static Layer s_flaeche;
static Layer *s_kind;           //< was layer_add_child an die Wurzel haengt
static LayerUpdateProc s_zeichnen;
static ClickConfigProvider s_tasten_anmelden;
static ClickHandler s_klick[NUM_BUTTONS];
static GColor s_hintergrund;
static bool s_animiert;
static void *s_zustand;         //< app_state_set_user_data
static void *s_heap;            //< app_zalloc_check
static int s_offen;             //< Bloecke im Heap der App
static int s_fenster_offen;
static int s_flaechen_offen;
static bool s_neu_zeichnen;     //< layer_mark_dirty seit dem letzten Zeichnen
static bool s_alle_weg;         //< app_window_stack_pop_all
static char s_protokoll[64][96];
static int s_zeilen;
static unsigned long s_ticks;

// Der Start, wie Pebbles Syscalls ihn der App melden.
static AppLaunchReason s_grund;
static ButtonId s_knopf;
static AppQuickLaunchAction s_aktion;
static uint32_t s_argument;

// Was ein Zeichnen hinterlaesst: der Titel im Kopf, seine Schrift und Farbe, die Mitte des
// gefuellten Punktes, und wie oft gezeichnet wurde.
static char s_titel[32];
static const char *s_titel_schrift;
static GColor s_textfarbe;
static GColor s_fuellfarbe;
static int16_t s_punkt_x;
static int s_offene_punkte;
static int s_bilder;

// Die Tasten, die die Ereignisschleife zwischen den Bildern drueckt: 'o' Oben, 'u' Unten.
static const char *s_druecke;

static void prv_merke(char c) {
  const size_t n = strlen(s_reihe);
  cl_assert(n + 1 < sizeof(s_reihe));
  s_reihe[n] = c;
}

void *app_zalloc_check(size_t size) {
  prv_merke('a');
  s_heap = calloc(1, size);
  s_offen++;
  return s_heap;
}
void task_free(void *ptr) {
  prv_merke('f');
  cl_assert(ptr == s_heap);
  free(ptr);
  s_offen--;
}
void app_state_set_user_data(void *data) { s_zustand = data; }
void *app_state_get_user_data(void) { return s_zustand; }

AppLaunchReason app_launch_reason(void) {
  prv_merke('R');
  return s_grund;
}
ButtonId app_launch_button(void) {
  prv_merke('B');
  return s_knopf;
}
AppQuickLaunchAction app_launch_get_quick_launch_action(void) {
  prv_merke('Q');
  return s_aktion;
}
uint32_t app_launch_get_args(void) {
  prv_merke('A');
  return s_argument;
}

Window *window_create(void) {
  prv_merke('w');
  s_fenster_offen++;
  return &s_fenster;
}
void window_destroy(Window *window) {
  prv_merke('W');
  cl_assert(window == &s_fenster);
  s_fenster_offen--;
}
void window_set_background_color(Window *window, GColor color) { s_hintergrund = color; }
void window_set_click_config_provider(Window *window, ClickConfigProvider provider) {
  prv_merke('c');
  cl_assert(window == &s_fenster);
  s_tasten_anmelden = provider;
}
void window_single_click_subscribe(ButtonId button_id, ClickHandler handler) {
  s_klick[button_id] = handler;
}
ButtonId click_recognizer_get_button_id(ClickRecognizerRef recognizer) {
  return *(ButtonId *)recognizer;
}
Layer *window_get_root_layer(const Window *window) { return &s_fenster.layer; }
GRect layer_get_bounds_by_value(const Layer *layer) { return GRect(0, 0, 200, 228); }
Layer *layer_create(GRect frame) {
  prv_merke('l');
  s_flaechen_offen++;
  return &s_flaeche;
}
void layer_destroy(Layer *layer) {
  prv_merke('L');
  cl_assert(layer == &s_flaeche);
  s_flaechen_offen--;
}
void layer_set_update_proc(Layer *layer, LayerUpdateProc update_proc) { s_zeichnen = update_proc; }
void layer_add_child(Layer *parent, Layer *child) {
  cl_assert(parent == &s_fenster.layer);
  s_kind = child;
}
void layer_mark_dirty(Layer *layer) {
  prv_merke('d');
  cl_assert(layer == &s_flaeche);
  s_neu_zeichnen = true;
}
void app_window_stack_push(Window *window, bool animated) {
  prv_merke('p');
  cl_assert(window == &s_fenster);
  s_animiert = animated;
}
void app_window_stack_pop_all(const bool animated) {
  prv_merke('z');
  cl_assert(animated);
  s_alle_weg = true;
}

// Grafik: Es zaehlt, was der Kopfbalken zeigt.
GFont fonts_get_system_font(const char *font_key) { return (GFont)font_key; }
void graphics_context_set_fill_color(GContext *ctx, GColor color) { s_fuellfarbe = color; }
void graphics_context_set_text_color(GContext *ctx, GColor color) { s_textfarbe = color; }
void graphics_context_set_stroke_color(GContext *ctx, GColor color) {}
void graphics_fill_round_rect_by_value(GContext *ctx, GRect rect, uint16_t corner_radius,
                                       GCornerMask corner_mask) {
  // Der Kopfbalken: 0-27 px ueber die ganze Breite.
  cl_assert_equal_i(rect.origin.y, 0);
  cl_assert_equal_i(rect.size.h, 28);
  cl_assert_equal_i(rect.size.w, 200);
}
void graphics_draw_text(GContext *ctx, const char *text, GFont const font, const GRect box,
                        const GTextOverflowMode overflow_mode, const GTextAlignment alignment,
                        GTextAttributes *text_attributes) {
  snprintf(s_titel, sizeof(s_titel), "%s", text);
  s_titel_schrift = (const char *)font;
}
void graphics_fill_circle(GContext *ctx, GPoint p, uint16_t radius) {
  cl_assert(gcolor_equal(s_fuellfarbe, GColorWhite));
  s_punkt_x = p.x;
}
void graphics_draw_circle(GContext *ctx, GPoint p, uint16_t radius) { s_offene_punkte++; }

uint64_t sys_get_ticks(void) {
  prv_merke('t');
  return s_ticks;
}
void app_log(uint8_t log_level, const char *src_filename, int src_line_number, const char *fmt,
             ...) {
  prv_merke('g');
  cl_assert(s_zeilen < 64);
  va_list args;
  va_start(args, fmt);
  vsnprintf(s_protokoll[s_zeilen++], sizeof(s_protokoll[0]), fmt, args);
  va_end(args);
}

static void prv_bild(void) {
  s_neu_zeichnen = false;
  s_titel[0] = '\0';
  s_offene_punkte = 0;
  s_zeichnen(&s_flaeche, NULL);
  s_bilder++;
}

// Die Ereignisschleife: Pebble zeichnet das Fenster, dann drueckt der Nutzer die Tasten aus
// s_druecke - nach jeder zeichnet Pebble neu, wenn die App es verlangt -, und schliesslich
// zeichnet Pebble noch einmal und der Nutzer schliesst die App. Fuehrt eine Taste zum
// Zifferblatt, endet die Schleife dort.
void app_event_loop(void) {
  prv_merke('e');
  cl_assert(s_zeichnen != NULL);
  cl_assert(s_tasten_anmelden != NULL);
  s_tasten_anmelden(&s_fenster);
  prv_bild();
  for (const char *t = s_druecke; t && *t; t++) {
    ButtonId knopf = (*t == 'o') ? BUTTON_ID_UP : BUTTON_ID_DOWN;
    cl_assert(s_klick[knopf] != NULL);
    prv_merke(*t);
    s_klick[knopf](&knopf, &s_fenster);
    if (s_alle_weg) {
      return;
    }
    if (s_neu_zeichnen) {
      prv_bild();
    }
  }
  prv_bild();
}

// --- Hilfen ---

static void prv_starten(void) {
  health_app_get_info()->main_func();
}

static void prv_start_setzen(AppLaunchReason grund, ButtonId knopf, AppQuickLaunchAction aktion,
                             uint32_t argument) {
  s_grund = grund;
  s_knopf = knopf;
  s_aktion = aktion;
  s_argument = argument;
}

// Die Mitte des Punktes der Karte i (0 = Today): rechts der letzte, je 10 px nach links.
static int16_t prv_punkt_x(int karte) {
  return 200 - 8 - 3 - (4 - karte) * 10;
}

void test_bh_app__initialize(void) {
  memset(s_reihe, 0, sizeof(s_reihe));
  memset(&s_fenster, 0, sizeof(s_fenster));
  memset(&s_flaeche, 0, sizeof(s_flaeche));
  s_kind = NULL;
  s_zeichnen = NULL;
  s_tasten_anmelden = NULL;
  memset(s_klick, 0, sizeof(s_klick));
  s_hintergrund = GColorClear;
  s_animiert = false;
  s_zustand = NULL;
  s_heap = NULL;
  s_offen = 0;
  s_fenster_offen = 0;
  s_flaechen_offen = 0;
  s_neu_zeichnen = false;
  s_alle_weg = false;
  memset(s_protokoll, 0, sizeof(s_protokoll));
  s_zeilen = 0;
  s_ticks = 4711;
  s_titel[0] = '\0';
  s_titel_schrift = NULL;
  s_textfarbe = GColorClear;
  s_fuellfarbe = GColorClear;
  s_punkt_x = -1;
  s_offene_punkte = 0;
  s_bilder = 0;
  s_druecke = NULL;
  // Wie aus dem Startmenue (launcher/default/menu_layer.c:87-92).
  prv_start_setzen(APP_LAUNCH_USER, BUTTON_ID_SELECT, APP_QUICK_LAUNCH_ACTION_NONE, 0);
}

void test_bh_app__cleanup(void) {}

// DIE APP, DIE PEBBLES EINSTIEGE FINDEN. Quick Launch und die Pins der Insights suchen die UUID
// von Pebbles Health-App (Spezifikation 4.2). Sie steht hier ausgeschrieben und nicht aus dem
// Kopf, damit ein vertipptes Byte dort auffaellt.
void test_bh_app__kennung_wie_pebble(void) {
  const uint8_t *b = (const uint8_t *)&health_app_get_info()->uuid;
  char hex[2 * sizeof(Uuid) + 1];
  for (size_t i = 0; i < sizeof(Uuid); i++) {
    snprintf(&hex[2 * i], 3, "%02x", b[i]);
  }
  cl_assert_equal_s(hex, "36d8c6ed4c834fa1a9e28f12dc941f8c");
}

// OHNE SYSTEMRECHTE (Kriterium 2): Pebble nimmt der App-Task die Rechte vor main, wenn die App es
// so traegt. Ein Fehler beendet dann nur die App.
void test_bh_app__ohne_systemrechte(void) {
  cl_assert(health_app_get_info()->is_unprivileged);
}

// Name, Symbol und Sichtbarkeit wie Pebbles App: einmal "Health" im Startmenue und in der
// Quick-Launch-Auswahl (T3.1).
void test_bh_app__sichtbar_wie_pebbles_app(void) {
  const PebbleProcessMd *md = health_app_get_info();
  cl_assert_equal_i(md->process_storage, ProcessStorageBuiltin);
  cl_assert_equal_i(md->process_type, ProcessTypeApp);
  cl_assert_equal_i(md->visibility, ProcessVisibilityShown);
  cl_assert(md->main_func != NULL);
  const PebbleProcessMdSystem *system = (const PebbleProcessMdSystem *)md;
  cl_assert_equal_s(system->name, "Health");
  cl_assert_equal_i(system->icon_resource_id, RESOURCE_ID_MENU_ICON_HEALTH);
}

// Ein weisses Fenster mit Pebbles Uebergang; der Zustand liegt im Heap der App (ein beschreibbares
// static laege im Kernel-RAM). Beim Schliessen ist alles wieder frei.
void test_bh_app__start_und_ende(void) {
  prv_starten();
  cl_assert(s_zustand != NULL);
  cl_assert(s_zustand == s_heap);
  cl_assert(s_kind == &s_flaeche);
  cl_assert_equal_i(s_hintergrund.argb, GColorWhiteARGB8);
  cl_assert(s_animiert);
  cl_assert_equal_i(s_offen, 0);
  cl_assert_equal_i(s_fenster_offen, 0);
  cl_assert_equal_i(s_flaechen_offen, 0);
}

// Der Kopfbalken (2.1): Titel in GOTHIC 18 fett und weiss, von fuenf Punkten der der Karte
// gefuellt, die anderen vier offen.
void test_bh_app__kopfbalken(void) {
  prv_starten();
  cl_assert_equal_s(s_titel, "Today");
  cl_assert_equal_s(s_titel_schrift, FONT_KEY_GOTHIC_18_BOLD);
  cl_assert(gcolor_equal(s_textfarbe, GColorWhite));
  cl_assert_equal_i(s_punkt_x, prv_punkt_x(0));
  cl_assert_equal_i(s_offene_punkte, 4);
}

// T3.4: die erste Karte je Einstieg, aus Pebbles Start-Syscalls gelesen, vor dem ersten Bild.
void test_bh_app__erste_karte(void) {
  static const struct {
    AppLaunchReason grund;
    ButtonId knopf;
    AppQuickLaunchAction aktion;
    uint32_t argument;
    const char *titel;
    int platz;
  } faelle[] = {
    {APP_LAUNCH_USER, BUTTON_ID_SELECT, APP_QUICK_LAUNCH_ACTION_NONE, 0, "Today", 0},
    {APP_LAUNCH_QUICK_LAUNCH, BUTTON_ID_UP, APP_QUICK_LAUNCH_ACTION_TAP, 0, "Today", 0},
    {APP_LAUNCH_QUICK_LAUNCH, BUTTON_ID_DOWN, APP_QUICK_LAUNCH_ACTION_HOLD, 0, "Today", 0},
    {APP_LAUNCH_PHONE, BUTTON_ID_BACK, APP_QUICK_LAUNCH_ACTION_NONE, 0, "Today", 0},
    {APP_LAUNCH_TIMELINE_ACTION, BUTTON_ID_BACK, APP_QUICK_LAUNCH_ACTION_NONE,
     HealthCardType_Sleep, "Night", 1},
    {APP_LAUNCH_TIMELINE_ACTION, BUTTON_ID_BACK, APP_QUICK_LAUNCH_ACTION_NONE,
     HealthCardType_Activity, "Activity", 3},
  };
  for (size_t i = 0; i < sizeof(faelle) / sizeof(faelle[0]); i++) {
    test_bh_app__initialize();
    prv_start_setzen(faelle[i].grund, faelle[i].knopf, faelle[i].aktion, faelle[i].argument);
    prv_starten();
    cl_assert_equal_s(s_titel, faelle[i].titel);
    cl_assert_equal_i(s_punkt_x, prv_punkt_x(faelle[i].platz));
    // Der Start ist gelesen, bevor das Fenster entsteht.
    cl_assert(strchr(s_reihe, 'R') < strchr(s_reihe, 'w'));
  }
}

// Oben und Unten zeichnen die neue Karte; nach Quick Launch Oben fuehrt Unten auf Today zum
// Zifferblatt, ohne noch etwas zu zeichnen.
void test_bh_app__tasten(void) {
  prv_start_setzen(APP_LAUNCH_QUICK_LAUNCH, BUTTON_ID_UP, APP_QUICK_LAUNCH_ACTION_TAP, 0);
  s_druecke = "oo";
  prv_starten();
  cl_assert_equal_s(s_titel, "Heart");
  cl_assert_equal_i(s_punkt_x, prv_punkt_x(2));
  cl_assert_equal_i(s_bilder, 4);
  cl_assert(!s_alle_weg);

  test_bh_app__initialize();
  prv_start_setzen(APP_LAUNCH_QUICK_LAUNCH, BUTTON_ID_UP, APP_QUICK_LAUNCH_ACTION_TAP, 0);
  s_druecke = "ouu";
  prv_starten();
  cl_assert(s_alle_weg);
  cl_assert_equal_i(s_bilder, 3);
  cl_assert_equal_s(s_titel, "Today");
  cl_assert_equal_i(s_offen, 0);

  // Gespiegelt nach Quick Launch Unten.
  test_bh_app__initialize();
  prv_start_setzen(APP_LAUNCH_QUICK_LAUNCH, BUTTON_ID_DOWN, APP_QUICK_LAUNCH_ACTION_TAP, 0);
  s_druecke = "uoo";
  prv_starten();
  cl_assert(s_alle_weg);
  cl_assert_equal_i(s_bilder, 3);

  // Im Kreis aus dem Startmenue: Unten auf Today fuehrt zu Settings.
  test_bh_app__initialize();
  s_druecke = "u";
  prv_starten();
  cl_assert_equal_s(s_titel, "Settings");
  cl_assert_equal_i(s_punkt_x, prv_punkt_x(4));
}

// L3 (2.6.2): Eine Taste liest nichts neu. Zwischen dem ersten Bild und dem Ende ruft die App
// keinen Start-Syscall mehr und - ohne den Schalter des Pruefbaus - ueberhaupt keinen Syscall.
void test_bh_app__tasten_ohne_syscall(void) {
  s_druecke = "ouuo";
  prv_starten();
  const char *nach_dem_start = strchr(s_reihe, 'e');
  cl_assert(nach_dem_start != NULL);
  cl_assert(strpbrk(nach_dem_start, "RBQA") == NULL);
#ifndef CONFIG_BOULDER_HEALTH_ZEITEN
  cl_assert(strpbrk(nach_dem_start, "tg") == NULL);
  cl_assert_equal_s(s_reihe, "aRBQAwclpeodududodLWf");
#endif
}

// Die Zeitstempel (2.6.3): mit dem Schalter "main" vor allem anderen, der Start mit seinen
// Werten und "bild" am Ende des ersten Zeichnens; nach jeder Taste "taste", wohin sie fuehrt, und
// "bild" am Ende des Zeichnens danach. Ohne den Schalter kein Aufruf davon.
void test_bh_app__zeitstempel(void) {
  prv_start_setzen(APP_LAUNCH_QUICK_LAUNCH, BUTTON_ID_UP, APP_QUICK_LAUNCH_ACTION_TAP, 0);
  s_druecke = "oooooo" "uuuuuu";
  prv_starten();
#ifdef CONFIG_BOULDER_HEALTH_ZEITEN
  static const char *const soll[] = {
    "bh t=4711 main",
    "bh t=4711 start grund=5 knopf=1 aktion=2 argument=0 Today kreis=0 spiegel=0",
    "bh t=4711 bild",
    "bh t=4711 taste", "bh t=4711 karte Night", "bh t=4711 bild",
  };
  cl_assert_equal_i(APP_LAUNCH_QUICK_LAUNCH, 5);
  cl_assert_equal_i(BUTTON_ID_UP, 1);
  cl_assert_equal_i(APP_QUICK_LAUNCH_ACTION_TAP, 2);
  cl_assert(s_zeilen >= (int)(sizeof(soll) / sizeof(soll[0])));
  for (size_t i = 0; i < sizeof(soll) / sizeof(soll[0]); i++) {
    cl_assert_equal_s(s_protokoll[i], soll[i]);
  }
  // Settings ist die letzte Karte: Oben tut dort nichts. Zuletzt Unten auf Today.
  cl_assert_equal_s(s_protokoll[s_zeilen - 1], "bh t=4711 zifferblatt");
  bool nichts = false;
  for (int i = 0; i < s_zeilen; i++) {
    nichts |= (strcmp(s_protokoll[i], "bh t=4711 nichts") == 0);
  }
  cl_assert(nichts);
  // main zuerst, bild nach dem Schieben des Fensters.
  cl_assert_equal_i(strncmp(s_reihe, "tgaRBQAtgwclpetg", 16), 0);
#else
  cl_assert_equal_i(s_zeilen, 0);
#endif
}
