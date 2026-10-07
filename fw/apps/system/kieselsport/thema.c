/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Nur mit der Variante: fw/apps/system/ baut jedes Board (siehe kieselsport.c).
#ifdef CONFIG_BOULDER_KIESELSPORT

#include <string.h>
#include "thema.h"

GColor thema_zonenfarbe(int zone) {
#if PBL_COLOR
  switch (zone) {
    case 1: return GColorPictonBlue;
    case 2: return GColorJaegerGreen;
    case 3: return GColorLimerick;
    case 4: return GColorChromeYellow;
    case 5: return GColorFolly;
    default: return GColorLightGray;
  }
#else
  // SCHWARZWEISS: eine Farbe, die es nicht gibt, ist keine Auskunft. Dort
  // traegt die Ziffer neben dem Puls die Zone, und der Balken darunter
  // zeigt sie noch einmal als Laenge.
  return zone > 0 ? GColorBlack : GColorLightGray;
#endif
}

GColor thema_sportfarbe(Sportart art) {
#if PBL_COLOR
  // Aus Kiesel-Helper (colors.xml, sport_*), auf die naechste Farbe der
  // Uhr gerundet. Strasse/Gravel und MTB teilen sich dort eine; hier traegt
  // MTB das erdigere Braun, damit der Startschirm sagt, welches Rad es ist.
  switch (art) {
    case ArtLaufen:    return GColorOrange;
    case ArtBike:      return GColorChromeYellow;
    case ArtWandern:   return GColorArmyGreen;
    case ArtKraft:     return GColorRoseVale;
    case ArtMTB:       return GColorWindsorTan;
    case ArtYoga:      return GColorPurpureus;
    case ArtSchwimmen: return GColorTiffanyBlue;
    default:           return KS_FARBE_LEISTE;
  }
#else
  (void)art;
  return GColorWhite;
#endif
}

// Das Dreieck fuer Start, 14 Punkte hoch, um seinen Mittelpunkt.
static GPoint s_dreieck_punkte[] = { {-5, -7}, {7, 0}, {-5, 7} };
static const GPathInfo s_dreieck_info = { 3, s_dreieck_punkte };
static GPath *s_dreieck;

// Die Zeichen sind alle in Weiss (AUF_LEISTE) auf der Leiste; wo ein Zeichen
// eine Aussparung braucht, ist die in der Leistenfarbe gemalt.
static void prv_symbol(GContext *ctx, Symbol was, GPoint m) {
  const GColor weiss = KS_FARBE_AUF_LEISTE;
  const GColor grund = KS_FARBE_LEISTE;
  graphics_context_set_fill_color(ctx, weiss);
  graphics_context_set_stroke_color(ctx, weiss);
  graphics_context_set_stroke_width(ctx, 1);
  switch (was) {
    case SymbolStart:
      if (!s_dreieck) s_dreieck = gpath_create(&s_dreieck_info);
      gpath_move_to(s_dreieck, m);
      gpath_draw_filled(ctx, s_dreieck);
      break;
    case SymbolPause:
      graphics_fill_rect(ctx, GRect(m.x - 6, m.y - 7, 4, 14), 0, GCornerNone);
      graphics_fill_rect(ctx, GRect(m.x + 2, m.y - 7, 4, 14), 0, GCornerNone);
      break;
    case SymbolSpeichern:
      // Der Haken, gruen wie "fertig" - eine Diskette kennt am Handgelenk
      // kaum noch jemand. Schwarzweiss bleibt er weiss.
      graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorGreen, weiss));
      graphics_context_set_stroke_width(ctx, 4);
      graphics_draw_line(ctx, GPoint(m.x - 7, m.y), GPoint(m.x - 2, m.y + 5));
      graphics_draw_line(ctx, GPoint(m.x - 2, m.y + 5), GPoint(m.x + 7, m.y - 6));
      graphics_context_set_stroke_width(ctx, 1);
      break;
    case SymbolLoeschen:
    case SymbolLoeschenFrage: {
      // Der Eimer: Griff, Deckel, Koerper mit zwei Rillen. Beim zweiten
      // Druck steht ein Fragezeichen daneben: jetzt ist es ernst.
      const int16_t dx = was == SymbolLoeschenFrage ? -4 : 0;
      // Rot wie "weg" - das Gegenstueck zum gruenen Haken oben.
      const GColor eimer = PBL_IF_COLOR_ELSE(GColorRed, weiss);
      graphics_context_set_fill_color(ctx, eimer);
      graphics_fill_rect(ctx, GRect(m.x + dx - 2, m.y - 9, 4, 2), 0, GCornerNone);
      graphics_fill_rect(ctx, GRect(m.x + dx - 7, m.y - 7, 14, 2), 0, GCornerNone);
      graphics_fill_rect(ctx, GRect(m.x + dx - 5, m.y - 4, 10, 13), 1, GCornerBottomLeft | GCornerBottomRight);
      graphics_context_set_fill_color(ctx, grund);
      graphics_fill_rect(ctx, GRect(m.x + dx - 2, m.y - 2, 1, 8), 0, GCornerNone);
      graphics_fill_rect(ctx, GRect(m.x + dx + 1, m.y - 2, 1, 8), 0, GCornerNone);
      if (was == SymbolLoeschenFrage) {
        graphics_context_set_text_color(ctx, eimer);
        graphics_draw_text(ctx, "?", fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                           GRect(m.x + 5, m.y - 12, 12, 22),
                           GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
      }
      break;
    }
    case SymbolWahl:
      graphics_context_set_stroke_width(ctx, 3);
      graphics_draw_line(ctx, GPoint(m.x - 3, m.y - 6), GPoint(m.x + 3, m.y));
      graphics_draw_line(ctx, GPoint(m.x + 3, m.y), GPoint(m.x - 3, m.y + 6));
      graphics_context_set_stroke_width(ctx, 1);
      break;
    case SymbolWeiter:
      // Zwei Winkel hintereinander: weiterblaettern.
      graphics_context_set_stroke_width(ctx, 3);
      for (int16_t dx = -4; dx <= 4; dx += 8) {
        graphics_draw_line(ctx, GPoint(m.x + dx - 3, m.y - 5), GPoint(m.x + dx + 2, m.y));
        graphics_draw_line(ctx, GPoint(m.x + dx + 2, m.y), GPoint(m.x + dx - 3, m.y + 5));
      }
      graphics_context_set_stroke_width(ctx, 1);
      break;
    default:
      break;
  }
}

void thema_leiste(GContext *ctx, GRect b, Symbol oben, Symbol mitte, Symbol unten) {
  const int16_t sx = b.size.w - KS_LEISTE_B;
  graphics_context_set_fill_color(ctx, KS_FARBE_LEISTE);
  graphics_fill_rect(ctx, GRect(sx, 0, KS_LEISTE_B, b.size.h), 0, GCornerNone);

  const int16_t cx = sx + KS_LEISTE_B / 2 - KS_LEISTE_DX;

  // Die Zeichen auf Tastenhoehe: die Tasten sitzen bei einem Viertel, der
  // Haelfte und drei Vierteln der Hoehe. Wer hinschaut, sieht neben der
  // Taste, was sie tut - statt unten eine Zeile fuer alle drei.
  const Symbol zeichen[3] = { oben, mitte, unten };
  for (int i = 0; i < 3; i++) {
    if (zeichen[i] == SymbolKeins) continue;
    prv_symbol(ctx, zeichen[i], GPoint(cx, b.size.h * (i + 1) / 4));
  }
}

void thema_zuruecksetzen(void) {
  memset(&s_dreieck, 0, sizeof(s_dreieck));
}

#endif  // CONFIG_BOULDER_KIESELSPORT
