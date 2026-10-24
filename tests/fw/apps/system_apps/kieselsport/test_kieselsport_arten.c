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
  cl_assert(gcolor_equal(thema_sportfarbe(ArtIndoor), GColorRajah));
  cl_assert(gcolor_equal(thema_sportfarbe(ArtFreiwasser), GColorVividCerulean));
  cl_assert(gcolor_equal(thema_sportfarbe(ArtFreiwasserGps), GColorBlueMoon));
  cl_assert(gcolor_equal(thema_sportfarbe(ArtOffen), GColorFolly));
  cl_assert(gcolor_equal(thema_sportfarbe(ArtOffenGps), GColorFashionMagenta));
}

// Die Zahl jeder Art geht ans Telefon und steht in der Gesundheitsakte. Boulder liest 8 als Velo
// auf der Rolle, 9 und 10 als Freiwasser, 11 und 12 als offenes Training, und an 10 und 12
// erkennt es, dass es die Strecke aufzeichnen soll. Eine neue Art kommt darum nur hinten dazu, und
// keine Zahl verschiebt sich.
void test_kieselsport_arten__die_zahlen_bleiben(void) {
  cl_assert_equal_i(ArtLaufen, 0);
  cl_assert_equal_i(ArtBike, 1);
  cl_assert_equal_i(ArtWandern, 2);
  cl_assert_equal_i(ArtKraft, 3);
  cl_assert_equal_i(ArtMTB, 4);
  cl_assert_equal_i(ArtYoga, 5);
  cl_assert_equal_i(ArtSchwimmen, 6);
  cl_assert_equal_i(ArtAtem, 7);
  cl_assert_equal_i(ArtIndoor, 8);
  cl_assert_equal_i(ArtFreiwasser, 9);
  cl_assert_equal_i(ArtFreiwasserGps, 10);
  cl_assert_equal_i(ArtOffen, 11);
  cl_assert_equal_i(ArtOffenGps, 12);
  cl_assert_equal_i(ArtAnzahl, 13);
}

// --- Die beiden Menues ---

// Jede Art steht genau einmal: im Hauptmenue oder im Untermenue einer Gruppe, und jede Gruppe hat
// genau eine Zeile. Fehlte eine Art, liesse sie sich nicht starten; stuende sie doppelt, haette das
// Menue eine tote Zeile.
void test_kieselsport_arten__jede_art_genau_einmal_im_menue(void) {
  int gesehen[ArtAnzahl] = {0};
  int zeilen_bike = 0, zeilen_schwimmen = 0;
  for (uint16_t z = 0; z < menue_zeilen(); z++) {
    const int eintrag = menue_eintrag(z);
    if (menue_ist_gruppe(eintrag)) {
      zeilen_bike += eintrag == MENUE_BIKE;
      zeilen_schwimmen += eintrag == MENUE_SCHWIMMEN;
      for (uint16_t u = 0; u < menue_gruppe_zeilen(eintrag); u++) {
        gesehen[menue_gruppe_art(eintrag, u)]++;
      }
    } else {
      cl_assert(eintrag >= 0 && eintrag < ArtAnzahl);
      gesehen[eintrag]++;
    }
  }
  cl_assert_equal_i(zeilen_bike, 1);
  cl_assert_equal_i(zeilen_schwimmen, 1);
  for (int a = 0; a < ArtAnzahl; a++) {
    cl_assert_equal_i(gesehen[a], 1);
  }
}

// Die Raeder in der Reihenfolge der Strasse nach innen, das Schwimmen vom Becken ins Freie, dort
// zuerst ohne GPS. Jede Gruppe steht im Hauptmenue dort, wo vorher ihre erste Art allein stand,
// unter ihrem Namen; das offene Training steht am Ende, ohne und mit GPS.
void test_kieselsport_arten__die_gruppen(void) {
  cl_assert_equal_i(menue_gruppe_zeilen(MENUE_BIKE), 3);
  cl_assert_equal_i(menue_gruppe_art(MENUE_BIKE, 0), ArtBike);
  cl_assert_equal_i(menue_gruppe_art(MENUE_BIKE, 1), ArtMTB);
  cl_assert_equal_i(menue_gruppe_art(MENUE_BIKE, 2), ArtIndoor);
  cl_assert_equal_i(menue_gruppe_zeilen(MENUE_SCHWIMMEN), 3);
  cl_assert_equal_i(menue_gruppe_art(MENUE_SCHWIMMEN, 0), ArtSchwimmen);
  cl_assert_equal_i(menue_gruppe_art(MENUE_SCHWIMMEN, 1), ArtFreiwasser);
  cl_assert_equal_i(menue_gruppe_art(MENUE_SCHWIMMEN, 2), ArtFreiwasserGps);
  cl_assert_equal_i(menue_zeilen(), 9);
  cl_assert_equal_i(menue_eintrag(0), ArtLaufen);
  cl_assert_equal_i(menue_eintrag(1), MENUE_BIKE);
  cl_assert_equal_i(menue_eintrag(5), MENUE_SCHWIMMEN);
  cl_assert_equal_i(menue_eintrag(6), ArtAtem);
  cl_assert_equal_i(menue_eintrag(7), ArtOffen);
  cl_assert_equal_i(menue_eintrag(8), ArtOffenGps);
  cl_assert_equal_s(menue_gruppe_name(MENUE_BIKE), "Bike");
  cl_assert_equal_s(menue_gruppe_name(MENUE_SCHWIMMEN), "Swimming");
}

// Ausserhalb der Listen nichts Startbares mit fremder Art: das Hauptmenue oeffnet die Raeder, ein
// Untermenue nimmt seine erste Art, und keine Art gilt als Gruppe.
void test_kieselsport_arten__ausserhalb_der_listen(void) {
  cl_assert_equal_i(menue_eintrag(menue_zeilen()), MENUE_BIKE);
  cl_assert_equal_i(menue_gruppe_art(MENUE_BIKE, menue_gruppe_zeilen(MENUE_BIKE)), ArtBike);
  cl_assert_equal_i(menue_gruppe_art(MENUE_SCHWIMMEN, menue_gruppe_zeilen(MENUE_SCHWIMMEN)),
                    ArtSchwimmen);
  for (int a = 0; a < ArtAnzahl; a++) {
    cl_assert(!menue_ist_gruppe(a));
  }
  cl_assert(!menue_ist_gruppe(MENUE_SCHWIMMEN + 1));
  cl_assert(!menue_ist_gruppe(-1));
  cl_assert_equal_i(menue_gruppe_art(MENUE_SCHWIMMEN + 1, 0), ArtBike);
}

// Unter jeder Art einer Gruppe steht ihr Name, und nur dort.
void test_kieselsport_arten__obertitel_nur_in_den_gruppen(void) {
  for (int a = 0; a < ArtAnzahl; a++) {
    const char *gruppe = art_gruppe((Sportart)a);
    if (a == ArtBike || a == ArtMTB || a == ArtIndoor) {
      cl_assert_equal_s(gruppe, "Bike");
    } else if (a == ArtSchwimmen || a == ArtFreiwasser || a == ArtFreiwasserGps) {
      cl_assert_equal_s(gruppe, "Swimming");
    } else {
      cl_assert(gruppe == NULL);
    }
  }
  cl_assert_equal_s(art_name(ArtIndoor), "Indoor");
  cl_assert_equal_s(art_name(ArtSchwimmen), "Pool");
  cl_assert_equal_s(art_name(ArtFreiwasser), "Open water");
  cl_assert_equal_s(art_name(ArtFreiwasserGps), "Open water GPS");
  cl_assert_equal_s(art_name(ArtOffen), "Custom");
  cl_assert_equal_s(art_name(ArtOffenGps), "Custom GPS");
}
