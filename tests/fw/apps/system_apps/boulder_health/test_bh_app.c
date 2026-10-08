/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "applib/ui/layer.h"
#include "applib/ui/window.h"
#include "process_management/pebble_process_md.h"
#include "resource/resource_ids.auto.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Boulders Health-App beim Start (Schritt 3, AP0): wie die Einstiege sie finden, dass sie ohne
// Systemrechte laeuft, dass ihr Zustand im Heap der App liegt und alles wieder freigibt, und die
// Zeitstempel des Pruefbaus (nur mit CONFIG_BOULDER_HEALTH_ZEITEN, test_bh_app_zeiten).
// Nachgebaut sind Pebbles Fenster, Ebenen, Heap und Protokoll; die App selbst ist echt.

const PebbleProcessMd *health_app_get_info(void);

// --- Was die App ruft, mitgezaehlt ---

static char s_reihe[256];       //< die Aufrufe in ihrer Reihenfolge, je ein Buchstabe
static Window s_fenster;
static Layer s_flaeche;
static Layer *s_kind;           //< was layer_add_child an die Wurzel haengt
static LayerUpdateProc s_zeichnen;
static GColor s_hintergrund;
static bool s_animiert;
static void *s_zustand;         //< app_state_set_user_data
static void *s_heap;            //< app_zalloc_check
static int s_offen;             //< Bloecke im Heap der App
static int s_fenster_offen;
static int s_flaechen_offen;
static char s_protokoll[4][64];
static int s_zeilen;
static unsigned long s_ticks;

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
void app_window_stack_push(Window *window, bool animated) {
  prv_merke('p');
  cl_assert(window == &s_fenster);
  s_animiert = animated;
}
// Die Ereignisschleife: Pebble zeichnet das Fenster zweimal, dann schliesst der Nutzer die App.
void app_event_loop(void) {
  prv_merke('e');
  cl_assert(s_zeichnen != NULL);
  s_zeichnen(&s_flaeche, NULL);
  s_zeichnen(&s_flaeche, NULL);
}

uint64_t sys_get_ticks(void) {
  prv_merke('t');
  return s_ticks;
}
void app_log(uint8_t log_level, const char *src_filename, int src_line_number, const char *fmt,
             ...) {
  prv_merke('g');
  cl_assert(s_zeilen < 4);
  va_list args;
  va_start(args, fmt);
  vsnprintf(s_protokoll[s_zeilen++], sizeof(s_protokoll[0]), fmt, args);
  va_end(args);
}

// --- Hilfen ---

static void prv_starten(void) {
  health_app_get_info()->main_func();
}

void test_bh_app__initialize(void) {
  memset(s_reihe, 0, sizeof(s_reihe));
  memset(&s_fenster, 0, sizeof(s_fenster));
  memset(&s_flaeche, 0, sizeof(s_flaeche));
  s_kind = NULL;
  s_zeichnen = NULL;
  s_hintergrund = GColorClear;
  s_animiert = false;
  s_zustand = NULL;
  s_heap = NULL;
  s_offen = 0;
  s_fenster_offen = 0;
  s_flaechen_offen = 0;
  memset(s_protokoll, 0, sizeof(s_protokoll));
  s_zeilen = 0;
  s_ticks = 4711;
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

// Die Zeitstempel (2.6.3): mit dem Schalter "main" vor allem anderen und "bild" einmal, am Ende des
// ersten Zeichnens; ohne den Schalter kein Aufruf davon.
void test_bh_app__zeitstempel(void) {
  prv_starten();
#ifdef CONFIG_BOULDER_HEALTH_ZEITEN
  cl_assert_equal_i(s_zeilen, 2);
  cl_assert_equal_s(s_protokoll[0], "bh t=4711 main");
  cl_assert_equal_s(s_protokoll[1], "bh t=4711 bild");
  // main zuerst, bild nach dem Schieben des Fensters, beim zweiten Zeichnen nichts mehr.
  cl_assert_equal_s(s_reihe, "tgawlpetgLWf");
#else
  cl_assert_equal_i(s_zeilen, 0);
  cl_assert_equal_s(s_reihe, "awlpeLWf");
#endif
}
