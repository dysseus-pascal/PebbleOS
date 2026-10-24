/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Nur mit der Variante: fw/apps/system/ baut jedes Board (siehe kieselsport.c).
#ifdef CONFIG_BOULDER_KIESELSPORT

#include "symbole.h"

// DIE SYMBOLE ALS KLEINE PROGRAMME: je Befehl ein Kennbyte, dann seine
// Zahlen, im Raster -14..14 um die Mitte. Radien in halben Punkten.
//
//   LZ n x0 y0 x1 y1 ...   Linienzug durch n Punkte
//   KR x y r2              Kreis, nur der Rand
//   KV x y r2              Kreis, gefuellt (Koepfe)
//   RE x y b h             Rechteck, gefuellt (Hantelscheiben, Rucksack)
//
// Entworfen in einer Vorschau auf 24 und 64 Punkten; wer eines aendert,
// sieht es sich am besten erst so an - die Striche werden mit der Groesse
// dicker, und was auf 64 Punkten fein ist, laeuft auf 20 zu.
enum { ENDE = 0, LZ, KR, KV, RE };

static const int8_t s_laufen[] = {
  KV, 4, -9, 5,
  LZ, 2, 2, -5, -1, 2,
  LZ, 3, 1, -4, 5, -1, 8, -3,
  LZ, 3, 1, -4, -3, -3, -6, 0,
  LZ, 3, -1, 2, 4, 5, 3, 10,
  LZ, 3, -1, 2, -4, 6, -9, 7,
  ENDE,
};

static const int8_t s_bike[] = {
  KR, -6, 4, 11,
  KR, 7, 4, 11,
  LZ, 4, -6, 4, -2, -4, 1, 4, -6, 4,
  LZ, 3, -2, -4, 5, -4, 7, 4,
  LZ, 2, 5, -4, 1, 4,
  LZ, 2, -5, -6, 0, -6,
  LZ, 3, 5, -4, 4, -7, 8, -7,
  ENDE,
};

static const int8_t s_wandern[] = {
  KV, 1, -9, 5,
  RE, -6, -6, 4, 7,
  LZ, 2, 0, -5, -1, 3,
  LZ, 2, -1, 3, -4, 11,
  LZ, 3, -1, 3, 3, 7, 3, 11,
  LZ, 2, 0, -3, 4, 1,
  LZ, 2, 5, -3, 7, 11,
  ENDE,
};

static const int8_t s_kraft[] = {
  LZ, 2, -12, 0, 12, 0,
  RE, -10, -8, 3, 16,
  RE, -7, -5, 2, 10,
  RE, 7, -8, 3, 16,
  RE, 5, -5, 2, 10,
  ENDE,
};

// MTB: das Bike tiefer, dahinter zwei Gipfel - das Gelaende ist der
// Unterschied zu Strasse/Gravel, nicht das Rad.
static const int8_t s_mtb[] = {
  LZ, 5, -12, -4, -7, -11, -3, -6, 0, -9, 5, -3,
  KR, -6, 6, 10,
  KR, 7, 6, 10,
  LZ, 4, -6, 6, -2, -1, 1, 6, -6, 6,
  LZ, 3, -2, -1, 5, -1, 7, 6,
  LZ, 2, 5, -1, 1, 6,
  ENDE,
};

static const int8_t s_yoga[] = {
  KV, 0, -9, 5,
  LZ, 2, 0, -6, 0, 2,
  LZ, 3, 0, -4, -5, 0, -8, 4,
  LZ, 3, 0, -4, 5, 0, 8, 4,
  LZ, 3, 0, 2, -10, 6, 5, 7,
  LZ, 3, 0, 2, 10, 6, -5, 7,
  ENDE,
};

static const int8_t s_schwimmen[] = {
  KV, -4, -4, 5,
  LZ, 3, -1, -2, 4, -7, 9, -4,
  LZ, 2, -2, -1, -10, 1,
  LZ, 7, -12, 5, -8, 3, -4, 5, 0, 3, 4, 5, 8, 3, 12, 5,
  LZ, 7, -12, 10, -8, 8, -4, 10, 0, 8, 4, 10, 8, 8, 12, 10,
  ENDE,
};

// ATMEN: die Lunge - Luftroehre, Bronchien, zwei Fluegel. Die Fluegel bleiben drei Punkte von der
// Luftroehre weg, sonst laufen sie auf 32 Punkten zusammen.
static const int8_t s_atem[] = {
  LZ, 2, 0, -13, 0, -5,
  LZ, 3, -6, 1, 0, -5, 6, 1,
  LZ, 10, -3, -5, -6, -9, -9, -9, -12, -3, -13, 4, -12, 9, -9, 11, -5, 11, -3, 8, -3, -5,
  LZ, 10, 3, -5, 6, -9, 9, -9, 12, -3, 13, 4, 12, 9, 9, 11, 5, 11, 3, 8, 3, -5,
  ENDE,
};

// INDOOR: das Bike etwas kleiner in einem Haus. Beim MTB sagt das Gelaende, wo gefahren wird,
// hier Dach und Waende; ein Dach allein las sich auf 26 Punkten nicht als drinnen. Die Waende
// stehen am Rand des Rasters: naeher an den Raedern liefen sie in QEMU auf 26 und 64 Punkten mit
// ihnen zusammen.
static const int8_t s_indoor[] = {
  LZ, 5, -14, 12, -14, -2, 0, -13, 14, -2, 14, 12,
  KR, -6, 7, 8,
  KR, 6, 7, 8,
  LZ, 4, -6, 7, -2, 0, 1, 7, -6, 7,
  LZ, 3, -2, 0, 4, 0, 6, 7,
  LZ, 2, 4, 0, 1, 7,
  LZ, 2, -4, -2, -1, -2,
  ENDE,
};

// FREIWASSER: der Schwimmer tiefer, dahinter zwei Gipfel wie beim MTB - die Landschaft ist der
// Unterschied zum Becken. Nur eine Welle: mit beiden lief es auf 26 Punkten zu.
static const int8_t s_freiwasser[] = {
  LZ, 5, -13, -5, -8, -12, -4, -8, 0, -11, 4, -6,
  KV, -4, 0, 5,
  LZ, 3, -1, 2, 4, -3, 9, 0,
  LZ, 2, -2, 3, -10, 5,
  LZ, 7, -12, 9, -8, 7, -4, 9, 0, 7, 4, 9, 8, 7, 12, 9,
  ENDE,
};

// OFFEN: das Herz, wie Pebble es fuer sein "Workout" zeigt.
static const int8_t s_offen[] = {
  LZ, 11, 0, -6, -3, -10, -8, -11, -12, -7, -12, -2, 0, 11, 12, -2, 12, -7, 8, -11, 3, -10, 0, -6,
  ENDE,
};

// Dieselbe Reihenfolge wie Sportart in kern/art.h. Eine Art mit GPS zeigt das Bild ihrer Art ohne;
// was sie unterscheidet, sagen Name und Farbe.
static const int8_t *const s_symbole[ArtAnzahl] = {
  s_laufen, s_bike, s_wandern, s_kraft, s_mtb, s_yoga, s_schwimmen, s_atem,
  s_indoor, s_freiwasser, s_freiwasser, s_offen, s_offen,
};

static GPoint prv_punkt(GPoint m, int16_t g, int x, int y) {
  return GPoint(m.x + x * g / 28, m.y + y * g / 28);
}

void symbol_sport(GContext *ctx, Sportart art, GPoint m, int16_t g, GColor farbe) {
  if (art >= ArtAnzahl) return;
  const int8_t *c = s_symbole[art];
  // Die Strichstaerke waechst mit: auf 20 Punkten zwei, auf 64 fuenf.
  int16_t w = g / 11;
  if (w < 2) w = 2;
  graphics_context_set_stroke_color(ctx, farbe);
  graphics_context_set_fill_color(ctx, farbe);
  graphics_context_set_stroke_width(ctx, (uint8_t)w);
  while (*c != ENDE) {
    switch (*c) {
      case LZ: {
        const int n = c[1];
        for (int i = 0; i + 1 < n; i++) {
          graphics_draw_line(ctx, prv_punkt(m, g, c[2 + 2 * i], c[3 + 2 * i]),
                             prv_punkt(m, g, c[4 + 2 * i], c[5 + 2 * i]));
        }
        c += 2 + 2 * n;
        break;
      }
      case KR:
        graphics_draw_circle(ctx, prv_punkt(m, g, c[1], c[2]), (uint16_t)(c[3] * g / 56));
        c += 4;
        break;
      case KV:
        graphics_fill_circle(ctx, prv_punkt(m, g, c[1], c[2]), (uint16_t)(c[3] * g / 56 + w / 4));
        c += 4;
        break;
      case RE: {
        const GPoint p = prv_punkt(m, g, c[1], c[2]);
        graphics_fill_rect(ctx, GRect(p.x, p.y, c[3] * g / 28 + 1, c[4] * g / 28 + 1), 0, GCornerNone);
        c += 5;
        break;
      }
      default:
        return;   // ein Fehler in der Tabelle: lieber nichts als Unsinn
    }
  }
  graphics_context_set_stroke_width(ctx, 1);
}

#endif  // CONFIG_BOULDER_KIESELSPORT
