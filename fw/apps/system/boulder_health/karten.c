/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// NUR AUF EMERY MIT DER VARIANTE (wie boulder_health.c).
#ifdef CONFIG_BOULDER_HEALTH_APP

#include "karten.h"

#include "texte.h"
#include "zeichnen.h"

#include "applib/graphics/graphics.h"

#define KOPF_H        BH_KOPF_H
#define RAND          BH_RAND
#define PUNKT_R       3
#define PUNKT_ABSTAND 10

const char *bh_karte_titel(BhKarte karte) {
  // Die Titel stehen in der Texttabelle in der Reihenfolge der Karten (texte_tabelle.h).
  return (karte < BhKartenAnzahl) ? bh_text((BhText)(BhTToday + karte)) : "";
}

GColor bh_karte_farbe(BhKarte karte) {
  // Entscheid F14 (12.10.): Today, Night und Heart wie b25. Weiss auf jeder Farbe mindestens 3:1
  // (Titel GOTHIC 18 fett), geprueft in test_bh_karten.
  switch (karte) {
    case BhKarteToday:
      return GColorOrange;
    case BhKarteNight:
      return GColorIndigo;
    case BhKarteHeart:
      return GColorFolly;
    case BhKarteActivity:
      return GColorDarkGreen;
    default:
      return GColorDarkGray;
  }
}

BhDetail bh_detail_von(BhKarte karte) {
  switch (karte) {
    case BhKarteNight:
      return BhDetailNight;
    case BhKarteHeart:
      return BhDetailHeart;
    case BhKarteActivity:
      return BhDetailSessions;
    case BhKarteSettings:
      return BhDetailPrecision;
    default:
      return BhDetailKeins; // Today (F12)
  }
}

const char *bh_detail_titel(BhDetail detail) {
  switch (detail) {
    case BhDetailNight:
      return bh_text(BhTNight);
    case BhDetailHeart:
      return bh_text(BhTHeart);
    case BhDetailSessions:
      return bh_text(BhTSessions);
    case BhDetailPrecision:
      return bh_text(BhTPrecision);
    default:
      return "";
  }
}

void bh_kopf_zeichnen(GContext *ctx, GRect flaeche, BhKarte karte) {
  graphics_context_set_fill_color(ctx, bh_karte_farbe(karte));
  graphics_fill_round_rect_by_value(ctx, GRect(0, 0, flaeche.size.w, KOPF_H), 0, GCornerNone);

  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, bh_karte_titel(karte), bh_schrift(BhSchriftG18Fett),
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

#endif // CONFIG_BOULDER_HEALTH_APP
