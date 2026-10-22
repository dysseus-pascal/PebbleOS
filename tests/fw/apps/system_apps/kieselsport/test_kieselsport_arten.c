/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "stubs_logging.h"
#include "stubs_pbl_malloc.h"

// Zuletzt: die Koepfe der App ziehen pebble_compat.h nach, und dessen Makros (malloc, time, ...)
// sollen die Koepfe davor nicht umschreiben.
#include "apps/system/kieselsport/art_text.h"
#include "apps/system/kieselsport/symbole.h"
#include "apps/system/kieselsport/texte.h"
#include "apps/system/kieselsport/thema.h"
#undef malloc
#undef calloc
#undef free
#undef time

// JEDE ART HAT IHREN NAMEN, IHR SYMBOL UND IHRE FARBE (art_text.c, symbole.c, thema.c). Die drei
// Tabellen stehen in drei Dateien; fehlt einer Art die Zeile, zeigte die App "Running" (der Name
// faellt auf 0), stuerzte beim Symbol ab (NULL) oder zeichnete den Balken schwarz wie die Leiste.
// Gezeichnet wird in Attrappen, die nur zaehlen, wohin.

// --- Sprache ---

static const char *s_locale = "en_US";
const char *app_get_system_locale(void) { return s_locale; }

// --- Zeichnen: gezaehlt, mit dem Rahmen aller Punkte ---

static int s_striche;
static int16_t s_links, s_rechts, s_oben, s_unten;

static void prv_punkt(GPoint p, int16_t rand) {
  if (p.x - rand < s_links) s_links = p.x - rand;
  if (p.x + rand > s_rechts) s_rechts = p.x + rand;
  if (p.y - rand < s_oben) s_oben = p.y - rand;
  if (p.y + rand > s_unten) s_unten = p.y + rand;
}

void graphics_context_set_stroke_color(GContext *ctx, GColor color) {}
void graphics_context_set_fill_color(GContext *ctx, GColor color) {}
void graphics_context_set_text_color(GContext *ctx, GColor color) {}
void graphics_context_set_stroke_width(GContext *ctx, uint8_t width) {}
void graphics_draw_line(GContext *ctx, GPoint p0, GPoint p1) {
  s_striche++;
  prv_punkt(p0, 0);
  prv_punkt(p1, 0);
}
void graphics_draw_circle(GContext *ctx, GPoint p, uint16_t radius) {
  s_striche++;
  prv_punkt(p, (int16_t)radius);
}
void graphics_fill_circle(GContext *ctx, GPoint p, uint16_t radius) {
  s_striche++;
  prv_punkt(p, (int16_t)radius);
}
void graphics_fill_round_rect_by_value(GContext *ctx, GRect rect, uint16_t radius,
                                       GCornerMask corner_mask) {
  s_striche++;
  prv_punkt(rect.origin, 0);
  prv_punkt(GPoint(rect.origin.x + rect.size.w - 1, rect.origin.y + rect.size.h - 1), 0);
}
GPath *gpath_create(const GPathInfo *init) { return NULL; }
void gpath_move_to(GPath *path, GPoint point) {}
void gpath_draw_filled(GContext *ctx, GPath *path) {}
GFont fonts_get_system_font(const char *font_key) { return NULL; }
void graphics_draw_text(GContext *ctx, const char *text, GFont const font, const GRect box,
                        const GTextOverflowMode overflow_mode, const GTextAlignment alignment,
                        GTextAttributes *text_attributes) {}

void test_kieselsport_arten__initialize(void) {
  s_locale = "en_US";
  strings_refresh();
}

void test_kieselsport_arten__cleanup(void) {}

// In jeder Sprache ein eigener Name, nie der Rueckfall "?" und nie leer.
void test_kieselsport_arten__jede_art_hat_einen_namen(void) {
  static const char *const s_sprachen[] = {"en_US", "de_CH", "fr_FR", "it_IT", "es_ES"};
  for (size_t l = 0; l < sizeof(s_sprachen) / sizeof(s_sprachen[0]); l++) {
    s_locale = s_sprachen[l];
    strings_refresh();
    for (int a = 0; a < ArtAnzahl; a++) {
      const char *name = art_name((Sportart)a);
      cl_assert(name[0] != '\0');
      cl_assert(strcmp(name, "?") != 0);
      for (int b = 0; b < a; b++) {
        // MTB heisst ueberall so; Strasse/Gravel traegt "Bike" nur als Obertitel.
        cl_assert_(strcmp(name, art_name((Sportart)b)) != 0, name);
      }
    }
  }
  cl_assert_equal_s(art_name(ArtAnzahl), "?");
  s_locale = "en_US";
  strings_refresh();
  cl_assert_equal_s(art_name(ArtAtem), "Breathing");
  cl_assert(art_gruppe(ArtAtem) == NULL);
}

// Jedes Symbol zeichnet etwas und bleibt im Raster von 28 Punkten um die Mitte.
void test_kieselsport_arten__jede_art_hat_ein_symbol(void) {
  const GPoint mitte = GPoint(100, 100);
  for (int a = 0; a < ArtAnzahl; a++) {
    s_striche = 0;
    s_links = s_oben = INT16_MAX;
    s_rechts = s_unten = INT16_MIN;
    symbol_sport(NULL, (Sportart)a, mitte, 28, GColorBlack);
    cl_assert(s_striche >= 3);
    cl_assert(s_links >= mitte.x - 15 && s_rechts <= mitte.x + 15);
    cl_assert(s_oben >= mitte.y - 15 && s_unten <= mitte.y + 15);
  }
  // Eine Art ausserhalb zeichnet nichts.
  s_striche = 0;
  symbol_sport(NULL, ArtAnzahl, mitte, 28, GColorBlack);
  cl_assert_equal_i(s_striche, 0);
}

// Jede Art ihre eigene Farbe, keine die der Leiste (die Vorgabe des switch) oder des Grundes.
void test_kieselsport_arten__jede_art_hat_eine_farbe(void) {
  for (int a = 0; a < ArtAnzahl; a++) {
    const GColor farbe = thema_sportfarbe((Sportart)a);
    cl_assert(!gcolor_equal(farbe, KS_FARBE_LEISTE));
    cl_assert(!gcolor_equal(farbe, KS_FARBE_GRUND));
    for (int b = 0; b < a; b++) {
      cl_assert(!gcolor_equal(farbe, thema_sportfarbe((Sportart)b)));
    }
  }
  cl_assert(gcolor_equal(thema_sportfarbe(ArtAtem), GColorCobaltBlue));
}
