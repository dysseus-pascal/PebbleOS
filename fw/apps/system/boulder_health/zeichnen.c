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

void bh_zahl_mit_einheit(GContext *ctx, const char *zahl, const char *einheit, BhSchriftart art,
                         GPoint ort, GColor farbe) {
  const int16_t hoehe = s_grundlinie[art] + 12;
  bh_schreiben(ctx, zahl, art, GRect(ort.x, ort.y, 190, hoehe), GTextAlignmentLeft, farbe);
  if (einheit) {
    // Die Breite nur hier: Ein Layout je Text kostet beim Zeichnen so viel wie das Zeichnen selbst.
    const int16_t breite = bh_textbreite(zahl, art);
    const int16_t y = ort.y + s_grundlinie[art] - s_grundlinie[BhSchriftG18Fett];
    bh_schreiben(ctx, einheit, BhSchriftG18Fett, GRect(ort.x + breite + 3, y, 80, 24),
                 GTextAlignmentLeft, farbe);
  }
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
