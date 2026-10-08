/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// NUR AUF EMERY MIT DER VARIANTE (wie boulder_health.c).
#ifdef CONFIG_BOULDER_HEALTH_APP

#include "karten.h"

#include "applib/fonts/fonts.h"
#include "applib/graphics/graphics.h"
#include "applib/graphics/text.h"

#define KOPF_H        28 //< 0-27 px
#define RAND          8
#define PUNKT_R       3
#define PUNKT_ABSTAND 10

// Statisch und const in beiden Stufen: ein beschreibbares static laege in .kernel_bss.
static const char *const s_titel[BhKartenAnzahl] = {
  [BhKarteToday] = "Today",
  [BhKarteNight] = "Night",
  [BhKarteHeart] = "Heart",
  [BhKarteActivity] = "Activity",
  [BhKarteSettings] = "Settings",
};

const char *bh_karte_titel(BhKarte karte) {
  return (karte < BhKartenAnzahl) ? s_titel[karte] : "";
}

void bh_kopf_zeichnen(GContext *ctx, GRect flaeche, BhKarte karte) {
  // VORERST IN DER FARBE DER APP (Startmenue, Registry -82). Die Farbe je Karte waehlt AP6 mit
  // dem Kontrasttest (V10); Weiss auf diesem Rot hat 3,1:1.
  graphics_context_set_fill_color(ctx, GColorSunsetOrange);
  graphics_fill_round_rect_by_value(ctx, GRect(0, 0, flaeche.size.w, KOPF_H), 0, GCornerNone);

  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, bh_karte_titel(karte), fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(RAND, 2, flaeche.size.w / 2, KOPF_H - 2),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  // Die Punkte von links nach rechts in der Reihenfolge der Karten; der rechte ist Settings.
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_context_set_stroke_color(ctx, GColorWhite);
  const int16_t mitte_y = KOPF_H / 2;
  for (int i = 0; i < BhKartenAnzahl; i++) {
    const GPoint mitte =
        GPoint(flaeche.size.w - RAND - PUNKT_R - (BhKartenAnzahl - 1 - i) * PUNKT_ABSTAND, mitte_y);
    if (i == (int)karte) {
      graphics_fill_circle(ctx, mitte, PUNKT_R);
    } else {
      graphics_draw_circle(ctx, mitte, PUNKT_R - 1);
    }
  }
}

#endif  // CONFIG_BOULDER_HEALTH_APP
