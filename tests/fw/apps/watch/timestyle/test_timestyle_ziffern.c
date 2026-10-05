/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include <pebble.h>
#include "apps/watch/timestyle/clock_area.h"
#include "apps/watch/timestyle/settings.h"
#include "apps/watch/timestyle/sidebar.h"
#include <pebble-fctx/fctx.h>

#include "applib/graphics/framebuffer.h"

#include "stubs_app_state.h"
#include "stubs_compiled_with_legacy2_sdk.h"
#include "stubs_logging.h"
#include "stubs_passert.h"

#include <stdio.h>

#undef malloc
#undef free
#undef time

// Die Uhrzeit von TimeStyle (clock_area.c) mit dem echten fctx, den echten
// Schriften und echter Grafik in einen Bildspeicher wie auf emery (200x228,
// 8 Bit). Nachgebaut sind nur die Ressourcen (aus resources/ gelesen) und die
// Schicht, an der die Uhr haengt. Gemessen wird zweierlei: wann fctx rechnet,
// und wie viel Speicher die Uhr braucht.

// --- Was settings.c und sidebar.c sonst stellen ---

Settings settings;
DynamicSettings dynamicSettings;
// Die Leiste auf emery: 34 Punkte, mit grosser Schrift 43 (sidebar.c).
#define LEISTE_EMERY 34
#define LEISTE_EMERY_GROSS 43
int sidebarWidth = LEISTE_EMERY;

extern char time_hours[3];
extern char time_minutes[3];

// --- Ressourcen: die Schriften aus resources/normal/base/timestyle ---

static size_t s_geladen;   //< Byte, die aus Ressourcen in den Heap kamen

static const char *prv_datei(uint32_t id) {
  switch (id) {
    case RESOURCE_ID_TIMESTYLE_LECO_REGULAR_FFONT: return "LECO1976-Regular.ffont";
    case RESOURCE_ID_TIMESTYLE_SANS_REGULAR_FFONT: return "KumbhSans-Regular.ffont";
    case RESOURCE_ID_TIMESTYLE_SANS_BOLD_FFONT: return "KumbhSans-Bold.ffont";
    default: return NULL;
  }
}

static FILE *prv_oeffnen(uint32_t id) {
  char pfad[512];
  snprintf(pfad, sizeof(pfad), "%s/../../resources/normal/base/timestyle/%s", CLAR_FIXTURE_PATH,
           prv_datei(id));
  FILE *f = fopen(pfad, "rb");
  cl_assert(f);
  return f;
}

ResHandle applib_resource_get_handle(uint32_t resource_id) {
  return (ResHandle)(uintptr_t)resource_id;
}

size_t applib_resource_size(ResHandle h) {
  FILE *f = prv_oeffnen((uint32_t)(uintptr_t)h);
  fseek(f, 0, SEEK_END);
  const long n = ftell(f);
  fclose(f);
  return (size_t)n;
}

size_t applib_resource_load(ResHandle h, uint8_t *buffer, size_t max_length) {
  FILE *f = prv_oeffnen((uint32_t)(uintptr_t)h);
  const size_t n = fread(buffer, 1, max_length, f);
  fclose(f);
  s_geladen += n;
  return n;
}

// Was gbitmap.c fuer Bilder aus Ressourcen braucht - hier nie gerufen.
ResAppNum sys_get_current_resource_num(void) { return 0; }
size_t sys_resource_size(ResAppNum app_num, uint32_t handle) { return 0; }
void *applib_resource_mmap_or_load(ResAppNum app_num, uint32_t resource_id, size_t offset,
                                   size_t num_bytes, bool used_aligned) {
  return NULL;
}
void applib_resource_munmap_or_free(void *bytes) {}
bool applib_resource_is_mmapped(void *bytes) { return false; }
bool gbitmap_png_data_is_png(const uint8_t *data, size_t data_size) { return false; }
bool gbitmap_init_with_png_data(GBitmap *bitmap, const uint8_t *data, size_t data_size) {
  return false;
}

// --- Heap der App: hier der des Rechners ---

void *app_malloc_check(size_t bytes) { return malloc(bytes); }
void *app_malloc(size_t bytes) { return malloc(bytes); }
void app_free(void *ptr) { free(ptr); }
size_t heap_bytes_free(void) { return 0; }
bool clock_is_24h_style(void) { return true; }

// --- Die Schicht ---

static Layer s_schicht;
static Window s_fenster;
Layer *window_get_root_layer(const Window *window) { return &s_schicht; }
GRect layer_get_bounds_by_value(const Layer *layer) { return GRect(0, 0, 200, 228); }
GRect layer_get_unobstructed_bounds_by_value(const Layer *layer) {
  return GRect(0, 0, 200, 228);
}
Layer *layer_create(GRect frame) { return &s_schicht; }
void layer_destroy(Layer *layer) {}
void layer_add_child(Layer *parent, Layer *child) {}
void layer_set_update_proc(Layer *layer, LayerUpdateProc update_proc) {}
void layer_mark_dirty(Layer *layer) {}

// --- Die Bilder der Uhr, mitgeschrieben (--wrap) ---
//
// Flag-Puffer und Kopie der Ziffern sind GBitmaps im App-Heap. Ein Absturz
// laesst die Statics der System-App stehen, der Heap kommt beim naechsten
// Start neu und genullt (app_manager.c, prv_app_start). Nachgebaut: was bis
// dahin angelegt war, wird genullt und gilt als tot - wer es danach freigibt,
// greift in den alten Heap (auf der Uhr: Double Free, die App stuerzt ab).

#define BILDER_MAX 16
static GBitmap *s_lebende[BILDER_MAX];
static int s_lebende_anzahl;
static GBitmap *s_tote[BILDER_MAX];
static void *s_tote_daten[BILDER_MAX];
static int s_tote_anzahl;

GBitmap *__real_gbitmap_create_blank(GSize size, GBitmapFormat format);
GBitmap *__wrap_gbitmap_create_blank(GSize size, GBitmapFormat format) {
  GBitmap *b = __real_gbitmap_create_blank(size, format);
  if (b) {
    cl_assert(s_lebende_anzahl < BILDER_MAX);
    s_lebende[s_lebende_anzahl++] = b;
  }
  return b;
}

void __real_gbitmap_destroy(GBitmap *bitmap);
void __wrap_gbitmap_destroy(GBitmap *bitmap) {
  for (int i = 0; bitmap && i < s_tote_anzahl; i++) {
    if (bitmap == s_tote[i]) {
      cl_fail("gbitmap_destroy auf ein Bild aus dem Heap vor dem Absturz");
    }
  }
  for (int i = 0; i < s_lebende_anzahl; i++) {
    if (s_lebende[i] == bitmap) {
      s_lebende[i] = s_lebende[--s_lebende_anzahl];
      break;
    }
  }
  __real_gbitmap_destroy(bitmap);
}

// Der Absturz: kein ClockArea_deinit, der Heap ist weg (genullt).
static void prv_absturz(void) {
  for (int i = 0; i < s_lebende_anzahl; i++) {
    cl_assert(s_tote_anzahl < BILDER_MAX);
    s_tote_daten[s_tote_anzahl] = gbitmap_get_data(s_lebende[i]);
    memset(s_lebende[i], 0, sizeof(GBitmap));
    s_tote[s_tote_anzahl++] = s_lebende[i];
  }
  s_lebende_anzahl = 0;
}

// --- Bildspeicher ---

static FrameBuffer s_fb;
static GContext s_ctx;

// Wie window_render: erst der Grund des Fensters, dann die Uhr.
static void prv_bild(void) {
  graphics_context_set_fill_color(&s_ctx, settings.timeBgColor);
  graphics_fill_rect(&s_ctx, GRect(0, 0, 200, 228), 0, GCornerNone);
  update_clock_area_layer(&s_schicht, &s_ctx);
}

static uint8_t s_bild_vorher[200 * 228];

static void prv_bild_merken(void) {
  memcpy(s_bild_vorher, s_fb.buffer, sizeof(s_bild_vorher));
}

static bool prv_bild_gleich(void) {
  return memcmp(s_bild_vorher, s_fb.buffer, sizeof(s_bild_vorher)) == 0;
}

void test_timestyle_ziffern__initialize(void) {
  framebuffer_init(&s_fb, &(GSize){ 200, 228 });
  graphics_context_init(&s_ctx, &s_fb, GContextInitializationMode_App);
  framebuffer_clear(&s_fb);
  memset(&settings, 0, sizeof(settings));
  memset(&dynamicSettings, 0, sizeof(dynamicSettings));
  settings.timeColor = GColorBlack;
  settings.timeBgColor = GColorWhite;
  settings.clockFontId = FONT_SETTING_DEFAULT;
  settings.sidebarOnLeft = false;
  sidebarWidth = LEISTE_EMERY;
  s_geladen = 0;
  s_lebende_anzahl = 0;
  s_tote_anzahl = 0;
  strcpy(time_hours, "10");
  strcpy(time_minutes, "42");
  ClockArea_init(&s_fenster);
}

void test_timestyle_ziffern__cleanup(void) {
  ClockArea_deinit();
  // Was im "alten Heap" lag, gibt der Rechner hier zurueck.
  for (int i = 0; i < s_tote_anzahl; i++) {
    free(s_tote_daten[i]);
    free(s_tote[i]);
  }
  s_tote_anzahl = 0;
}

void test_timestyle_ziffern__mit_sekunden_nur_bei_neuer_minute_gerechnet(void) {
  dynamicSettings.updateScreenEverySecond = true;
  prv_bild();
  cl_assert_equal_i(ClockArea_gerechnet(), 1);
  prv_bild_merken();
  // Die naechsten Sekunden: dieselbe Minute, kein neues Rechnen - und das
  // Bild ist Pixel fuer Pixel dasselbe.
  for (int i = 0; i < 5; i++) {
    prv_bild();
  }
  cl_assert_equal_i(ClockArea_gerechnet(), 1);
  cl_assert(prv_bild_gleich());
  // Die Minute wechselt: neu gerechnet, und das Bild ist ein anderes.
  strcpy(time_minutes, "43");
  prv_bild();
  cl_assert_equal_i(ClockArea_gerechnet(), 2);
  cl_assert(!prv_bild_gleich());
  // Eine andere Farbe ist auch eine Aenderung.
  settings.timeColor = GColorRed;
  prv_bild();
  cl_assert_equal_i(ClockArea_gerechnet(), 3);
}

void test_timestyle_ziffern__die_leiste_bleibt_unberuehrt(void) {
  dynamicSettings.updateScreenEverySecond = true;
  prv_bild();
  // Die Leiste (rechts, 34 Punkte) malt hier jemand anders; die Kopie darf
  // dort nichts ueberschreiben.
  graphics_context_set_fill_color(&s_ctx, GColorBlue);
  graphics_fill_rect(&s_ctx, GRect(200 - LEISTE_EMERY, 0, LEISTE_EMERY, 228), 0, GCornerNone);
  update_clock_area_layer(&s_schicht, &s_ctx);
  cl_assert_equal_i(ClockArea_gerechnet(), 1);
  const GBitmapDataRowInfo zeile = gbitmap_get_data_row_info(graphics_context_get_bitmap(&s_ctx), 100);
  cl_assert_equal_i(zeile.data[185], GColorBlue.argb);
}

// Die Kopie deckt nur den Bereich neben der Leiste ab. Ragte eine Ziffer in
// die Leiste, fehlte dieses Stueck in jeder kopierten Sekunde. Darum fuer
// jede Schrift, beide Seiten und beide Breiten: das kopierte Bild ist Punkt
// fuer Punkt das gerechnete.
void test_timestyle_ziffern__kopie_gleich_dem_gerechneten_bild(void) {
  dynamicSettings.updateScreenEverySecond = true;
  strcpy(time_hours, "08");
  strcpy(time_minutes, "58");
  const int breiten[] = { LEISTE_EMERY, LEISTE_EMERY_GROSS };
  uint32_t gerechnet = ClockArea_gerechnet();
  for (uint8_t schrift = FONT_SETTING_DEFAULT; schrift <= FONT_SETTING_BOLD_M; schrift++) {
    for (int links = 0; links <= 1; links++) {
      for (size_t b = 0; b < ARRAY_LENGTH(breiten); b++) {
        settings.clockFontId = schrift;
        settings.sidebarOnLeft = links;
        sidebarWidth = breiten[b];
        ClockArea_redraw();
        prv_bild();
        cl_assert_equal_i(ClockArea_gerechnet(), ++gerechnet);
        prv_bild_merken();
        prv_bild();
        cl_assert_equal_i(ClockArea_gerechnet(), gerechnet);
        if (!prv_bild_gleich()) {
          printf("Schrift %d, Leiste %s, %d breit: Kopie weicht ab\n", schrift,
                 links ? "links" : "rechts", breiten[b]);
        }
        cl_assert(prv_bild_gleich());
      }
    }
  }
}

void test_timestyle_ziffern__ohne_sekunden_keine_kopie(void) {
  dynamicSettings.updateScreenEverySecond = false;
  prv_bild();
  prv_bild();
  cl_assert_equal_i(ClockArea_gerechnet(), 2);
  cl_assert_equal_i(ClockArea_kopie_bytes(), 0);
}

void test_timestyle_ziffern__der_puffer_bleibt_und_passt_sich_an(void) {
  // Kantengeglaettet: ein Byte je Punkt.
  prv_bild();
  cl_assert_equal_i(fctx_puffer_bytes(), 200 * 228);
  prv_bild();
  cl_assert_equal_i(fctx_puffer_bytes(), 200 * 228);
  // LECO ohne Glaettung: ein Bit je Punkt.
  settings.clockFontId = FONT_SETTING_LECO;
  ClockArea_redraw();
  prv_bild();
  cl_assert(fctx_puffer_bytes() > 0);
  cl_assert(fctx_puffer_bytes() < 200 * 228 / 6);
  // Am Ende ist er weg.
  ClockArea_deinit();
  cl_assert_equal_i(fctx_puffer_bytes(), 0);
  ClockArea_init(&s_fenster);
}

// Was die Uhr im ungünstigsten Fall im App-Heap haelt: alle drei Schriften,
// der kantengeglaettete Flag-Puffer und die Kopie fuer das Sekunden-Widget.
// Der Heap eines System-Zifferblatts auf emery: das App-Segment
// (CONFIG_APP_RAM_4X_SEGMENT_SIZE = 135 168) ohne den Stapel (4 096).
#define APP_HEAP_EMERY (135168 - 4096)

void test_timestyle_ziffern__speicher_im_app_heap(void) {
  dynamicSettings.updateScreenEverySecond = true;
  prv_bild();
  const size_t schriften = s_geladen;
  const size_t puffer = fctx_puffer_bytes();
  const size_t kopie = ClockArea_kopie_bytes();
  printf("TimeStyle-Uhr im App-Heap: Schriften %zu, Flag-Puffer %zu, Kopie %zu, zusammen %zu "
         "von %d Byte\n", schriften, puffer, kopie, schriften + puffer + kopie, APP_HEAP_EMERY);
  cl_assert(schriften > 0);
  cl_assert_equal_i(puffer, 200 * 228);
  cl_assert_equal_i(kopie, (200 - LEISTE_EMERY) * 228);
  // Auch mit beidem bleibt mehr als ein Viertel frei.
  cl_assert(schriften + puffer + kopie < APP_HEAP_EMERY * 3 / 4);
}

// NACH EINEM ABSTURZ GREIFT DIE UHR NICHT IN DEN ALTEN HEAP. Flag-Puffer und
// Kopie der Ziffern ueberleben als Statics; ClockArea_init muss sie vergessen.
// Sonst zeichnete die Kopie im selben Minutenstand ein Bild, das es nicht mehr
// gibt, und fctx gaebe den alten Puffer frei - auf der Uhr ein Absturz bei
// jedem Start, bis sie neu startet (zwei in einer Minute: Zifferblatt weg).
void test_timestyle_ziffern__nach_einem_absturz_kein_griff_in_den_alten_heap(void) {
  dynamicSettings.updateScreenEverySecond = true;
  prv_bild();
  cl_assert_equal_i(fctx_puffer_bytes(), 200 * 228);
  cl_assert(ClockArea_kopie_bytes() > 0);
  prv_bild_merken();

  prv_absturz();
  ClockArea_init(&s_fenster);

  // Dieselbe Minute: neu gerechnet, dasselbe Bild wie vor dem Absturz.
  framebuffer_clear(&s_fb);
  prv_bild();
  cl_assert(prv_bild_gleich());
  cl_assert_equal_i(ClockArea_gerechnet(), 1);
  // Eine neue Minute: fctx rechnet in einem eigenen, neuen Puffer.
  strcpy(time_minutes, "43");
  prv_bild();
  cl_assert_equal_i(ClockArea_gerechnet(), 2);
  cl_assert_equal_i(fctx_puffer_bytes(), 200 * 228);
}
