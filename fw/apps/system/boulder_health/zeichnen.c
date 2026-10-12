/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#ifdef CONFIG_BOULDER_HEALTH_APP

#include "zeichnen.h"

#include "applib/fonts/fonts.h"
#include "applib/graphics/graphics.h"
#include "font_resource_keys.auto.h"

// Die Schluessel liegen im Flash (static const), die App hat kein beschreibbares static.
static const char *const s_schluessel[] = {
  [BhSchriftG14] = FONT_KEY_GOTHIC_14,
  [BhSchriftG14Fett] = FONT_KEY_GOTHIC_14_BOLD,
  [BhSchriftG18] = FONT_KEY_GOTHIC_18,
  [BhSchriftG18Fett] = FONT_KEY_GOTHIC_18_BOLD,
  [BhSchriftG24Fett] = FONT_KEY_GOTHIC_24_BOLD,
  [BhSchriftL26] = FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM,
  [BhSchriftL38] = FONT_KEY_LECO_38_BOLD_NUMBERS,
};

// Wie weit die Ziffern einer Schrift unter dem oberen Rand ihres Rahmens enden (Grundlinie), fuer
// die Einheit neben der Zahl. Am Bild ausgemessen (test_bh_karten).
static const int8_t s_grundlinie[] = {
  [BhSchriftG14] = 14,     [BhSchriftG14Fett] = 14, [BhSchriftG18] = 17, [BhSchriftG18Fett] = 17,
  [BhSchriftG24Fett] = 22, [BhSchriftL26] = 26,     [BhSchriftL38] = 38,
};

GFont bh_schrift(BhSchriftart art) {
  return fonts_get_system_font(s_schluessel[art]);
}

int16_t bh_textbreite(const char *text, BhSchriftart art) {
  const GSize groesse = app_graphics_text_layout_get_content_size(
      text, bh_schrift(art), GRect(0, 0, 400, 60), GTextOverflowModeWordWrap, GTextAlignmentLeft);
  return groesse.w;
}

void bh_schreiben(GContext *ctx, const char *text, BhSchriftart art, GRect rahmen,
                  GTextAlignment ausrichtung, GColor farbe) {
  graphics_context_set_text_color(ctx, farbe);
  graphics_draw_text(ctx, text, bh_schrift(art), rahmen, GTextOverflowModeTrailingEllipsis,
                     ausrichtung, NULL);
}

int16_t bh_zahl_mit_einheit(GContext *ctx, const char *zahl, const char *einheit, BhSchriftart art,
                            GPoint ort, GColor farbe) {
  const int16_t hoehe = s_grundlinie[art] + 12;
  bh_schreiben(ctx, zahl, art, GRect(ort.x, ort.y, 190, hoehe), GTextAlignmentLeft, farbe);
  if (!einheit) {
    return ort.x;
  }
  // Die Breite nur hier: Ein Layout je Text kostet beim Zeichnen so viel wie das Zeichnen selbst.
  const int16_t x = ort.x + bh_textbreite(zahl, art) + 3;
  const int16_t y = ort.y + s_grundlinie[art] - s_grundlinie[BhSchriftG18Fett];
  bh_schreiben(ctx, einheit, BhSchriftG18Fett, GRect(x, y, 80, 24), GTextAlignmentLeft, farbe);
  return x;
}

// GOTHIC 18 fett: Hoehe einer Zeile im Layout (am Bild ausgemessen, test_bh_karten) und die des
// Rahmens, in dem das Urteil vorher in einer Zeile stand.
#define URTEIL_ZEILE  18
#define URTEIL_RAHMEN 22
#define URTEIL_LUECKE 6 //!< zwischen Einheit und Urteil

static int16_t prv_hoehe(const char *text, BhSchriftart art, int16_t breite) {
  return app_graphics_text_layout_get_content_size(text, bh_schrift(art), GRect(0, 0, breite, 100),
                                                   GTextOverflowModeWordWrap, GTextAlignmentRight)
      .h;
}

GRect bh_urteil_rahmen(const char *text, int16_t frei_ab, int16_t rechts, int16_t oben,
                       BhSchriftart *art) {
  const int16_t links = frei_ab + URTEIL_LUECKE;
  const int16_t breite = rechts - links;
  // Der haeufige Fall kostet ein Layout: eine Zeile.
  *art = BhSchriftG18Fett;
  if (bh_textbreite(text, BhSchriftG18Fett) <= breite) {
    return GRect(links, oben, breite, URTEIL_RAHMEN);
  }
  // Zwei Zeilen: die zweite dort, wo die eine stuende.
  const int16_t hoehe = prv_hoehe(text, BhSchriftG18Fett, breite);
  if (hoehe <= 2 * URTEIL_ZEILE) {
    return GRect(links, oben + URTEIL_ZEILE - hoehe, breite, hoehe);
  }
  // Sonst GOTHIC 14 fett, unten buendig mit dem Rahmen der einen Zeile: hoeher kaeme es in den
  // Kopfbalken (Night: "10:59 h" mit "Very short night" in drei Zeilen).
  *art = BhSchriftG14Fett;
  const int16_t klein = prv_hoehe(text, BhSchriftG14Fett, breite);
  return GRect(links, oben + URTEIL_RAHMEN - klein, breite, klein);
}

void bh_urteil(GContext *ctx, const char *text, int16_t einheit_x, const char *einheit,
               int16_t rechts, int16_t oben, GColor farbe) {
  BhSchriftart art = BhSchriftG18Fett;
  GRect r;
  // SO WENIG LAYOUTS WIE MOEGLICH: Eins kostet beim Zeichnen so viel wie das Zeichnen des Texts
  // (AP8). Rechts der breitesten Einheit hat das breiteste Urteil meist Platz (keins), sonst oft
  // dieses (eins); erst sonst zaehlt die Einheit genau.
  const int16_t sicher = einheit_x + BH_EINHEIT_MAX + URTEIL_LUECKE;
  if (rechts - sicher >= BH_URTEIL_MAX ||
      bh_textbreite(text, BhSchriftG18Fett) <= rechts - sicher) {
    r = GRect(sicher, oben, rechts - sicher, URTEIL_RAHMEN);
  } else {
    r = bh_urteil_rahmen(text, einheit_x + bh_textbreite(einheit, BhSchriftG18Fett), rechts, oben,
                         &art);
  }
  graphics_context_set_text_color(ctx, farbe);
  graphics_draw_text(ctx, text, bh_schrift(art), r, GTextOverflowModeWordWrap, GTextAlignmentRight,
                     NULL);
}

void bh_rechteck(GContext *ctx, GRect r, GColor farbe) {
  graphics_context_set_fill_color(ctx, farbe);
  graphics_fill_round_rect_by_value(ctx, r, 0, GCornerNone);
}

void bh_linie(GContext *ctx, GPoint a, GPoint b, GColor farbe) {
  graphics_context_set_stroke_color(ctx, farbe);
  graphics_draw_line(ctx, a, b);
}

void bh_punkt(GContext *ctx, GPoint mitte, uint16_t radius, GColor farbe) {
  graphics_context_set_fill_color(ctx, farbe);
  graphics_fill_circle(ctx, mitte, radius);
}

void bh_ring(GContext *ctx, GPoint mitte, uint16_t radius, GColor farbe) {
  graphics_context_set_stroke_color(ctx, farbe);
  graphics_draw_circle(ctx, mitte, radius);
}

void bh_kopf_detail(GContext *ctx, int16_t breite, const char *titel, GColor farbe) {
  bh_rechteck(ctx, GRect(0, 0, breite, BH_KOPF_H), farbe);
  bh_schreiben(ctx, titel, BhSchriftG18Fett, GRect(BH_RAND, 2, breite - 2 * BH_RAND, BH_KOPF_H - 2),
               GTextAlignmentLeft, GColorWhite);
}

#endif // CONFIG_BOULDER_HEALTH_APP
