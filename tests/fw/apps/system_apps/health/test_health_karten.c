/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "stubs_logging.h"

#include "apps/system/health/karten.h"
#undef malloc
#undef calloc
#undef free
#undef time
#undef localtime

// Die Karten der Health-App (karten.c), ohne Bildschirm: gezaehlt wird, was
// sie zeichnen wollen. Hier: das Hypnogramm der Schlaf-Karte liest nie hinter
// das Ende der Abschnitte.

time_t rtc_get_time(void) { return 1790493000; }
const char *app_get_system_locale(void) { return "en_US"; }

static int s_rechtecke;
void graphics_fill_round_rect_by_value(GContext *ctx, GRect rect, uint16_t radius,
                                       GCornerMask corner_mask) {
  s_rechtecke++;
}
void graphics_context_set_fill_color(GContext *ctx, GColor color) {}
void graphics_context_set_text_color(GContext *ctx, GColor color) {}
void graphics_context_set_stroke_color(GContext *ctx, GColor color) {}
void graphics_fill_circle(GContext *ctx, GPoint p, uint16_t radius) {}
void graphics_draw_line(GContext *ctx, GPoint p0, GPoint p1) {}
void gpath_draw_filled(GContext *ctx, GPath *path) {}
void graphics_draw_text(GContext *ctx, const char *text, GFont const font, const GRect box,
                        const GTextOverflowMode overflow_mode, const GTextAlignment alignment,
                        GTextAttributes *text_attributes) {}
GFont fonts_get_system_font(const char *font_key) { return NULL; }
GColor gcolor_legible_over(GColor background_color) { return GColorBlack; }

// Die Nacht steht in einem groesseren Block - dahinter liegt, was ein Lesen
// ueber das Ende hinaus faelschlich als Abschnitte naehme.
static struct {
  HealthDaten d;
  NachtDlsAbschnitt dahinter[200];
} s_block;

static int prv_zeichne_schlaf(uint8_t anzahl) {
  memset(&s_block, 0, sizeof(s_block));
  s_block.d.nacht_da = true;
  NachtBild *n = &s_block.d.nacht;
  n->einschlafen = 0;
  n->aufwachen = 480;
  n->fertig = true;
  for (int k = 0; k < NACHT_BILD_ABSCHNITTE; k++) {
    n->abschnitte[k] = (NachtDlsAbschnitt){ .von = k * 5, .bis = k * 5 + 5, .phase = 1 };
  }
  for (int k = 0; k < 200; k++) {
    s_block.dahinter[k] = (NachtDlsAbschnitt){ .von = 0, .bis = 5, .phase = 1 };
  }
  n->anzahl = anzahl;
  s_rechtecke = 0;
  karten_zeichnen(NULL, GRect(0, 0, 200, 228), KarteSchlaf, &s_block.d, 0);
  return s_rechtecke;
}

void test_health_karten__initialize(void) { karten_sprache(); }
void test_health_karten__cleanup(void) {}

void test_health_karten__nie_mehr_abschnitte_als_platz(void) {
  const int voll = prv_zeichne_schlaf(NACHT_BILD_ABSCHNITTE);
  // Eine kaputte Datei mit 200 Abschnitten zeichnet nicht mehr als eine volle.
  cl_assert_equal_i(prv_zeichne_schlaf(200), voll);
  cl_assert_equal_i(prv_zeichne_schlaf(10), voll - (NACHT_BILD_ABSCHNITTE - 10));
}
