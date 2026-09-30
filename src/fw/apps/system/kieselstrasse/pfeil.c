/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pebble_compat.h"
#include "pfeil.h"
#include "thema.h"

// EIN Pfeil, gedreht. Ein Abbiegepfeil ist nichts anderes als ein gerader
// Pfeil in einem anderen Winkel, und eine Form weniger ist eine Form weniger,
// die auseinanderlaufen kann. Kreisverkehr und "abseits der Route" fallen
// bewusst heraus: die sind keine Richtung, sondern eine Lage.
static GPoint s_punkte[7];
static GPathInfo s_umriss = { .num_points = 7, .points = s_punkte };
static GPath *s_pfad;

// Halbe Hoehe, halbe Kopfbreite, halbe Schaftbreite, Kopfhoehe.
// Groesser als zuerst gebaut: am Steuer ist der Blick kurz, und ein Pfeil, den
// man suchen muss, ist keiner.
#define P_H  (KST_BREIT ? 21 : 15)
#define P_K  (KST_BREIT ? 15 : 11)
#define P_S  (KST_BREIT ?  6 :  5)
#define P_KH (KST_BREIT ? 20 : 14)

// OsmAnds TurnType. Die Namen stehen hier, damit man beim Lesen nicht in einer
// anderen Datei nachschlagen muss.
#define TT_C     1
#define TT_TL    2
#define TT_TSLL  3
#define TT_TSHL  4
#define TT_TR    5
#define TT_TSLR  6
#define TT_TSHR  7
#define TT_KL    8
#define TT_KR    9
#define TT_TU   10
#define TT_TRU  11
#define TT_OFFR 12
#define TT_RNDB 13
#define TT_RNLB 14

void pfeil_init(void) {
  if (s_pfad) return;
  const int16_t h = P_H, k = P_K, s = P_S, kh = P_KH;
  const int16_t basis = (int16_t)(-h + kh);   // Unterkante des Kopfes
  s_punkte[0] = GPoint(0, (int16_t)-h);       // Spitze
  s_punkte[1] = GPoint(k, basis);
  s_punkte[2] = GPoint(s, basis);
  s_punkte[3] = GPoint(s, h);
  s_punkte[4] = GPoint((int16_t)-s, h);
  s_punkte[5] = GPoint((int16_t)-s, basis);
  s_punkte[6] = GPoint((int16_t)-k, basis);
  s_pfad = gpath_create(&s_umriss);
}

void pfeil_deinit(void) {
  if (s_pfad) {
    gpath_destroy(s_pfad);
    s_pfad = NULL;
  }
}

void pfeil_zuruecksetzen(void) {
  // Nicht gpath_destroy: der Speicher dahinter gehoert schon niemandem mehr.
  s_pfad = NULL;
  memset(s_punkte, 0, sizeof(s_punkte));
}

int16_t pfeil_breite(void) { return (int16_t)(2 * P_K); }
int16_t pfeil_hoehe(void) { return (int16_t)(2 * P_H); }

bool pfeil_kennt(int art) { return art >= TT_C && art <= TT_RNLB; }

/**
 * Der Winkel zur Kennzahl.
 *
 * "Halten" (KL/KR) bekommt denselben Winkel wie "leicht" - auf einem Schirm
 * von zweihundert Punkten ist der Unterschied zwischen 30 und 45 Grad kein
 * Unterschied mehr, und zwei fast gleiche Pfeile nebeneinander verwirren mehr,
 * als sie sagen.
 */
static int32_t prv_winkel(int art) {
  switch (art) {
    case TT_C:    return 0;
    case TT_TSLR: return TRIG_MAX_ANGLE / 8;
    case TT_KR:   return TRIG_MAX_ANGLE / 8;
    case TT_TR:   return TRIG_MAX_ANGLE / 4;
    case TT_TSHR: return TRIG_MAX_ANGLE * 3 / 8;
    case TT_TU:   return TRIG_MAX_ANGLE / 2;
    case TT_TRU:  return TRIG_MAX_ANGLE / 2;
    case TT_TSHL: return TRIG_MAX_ANGLE * 5 / 8;
    case TT_TL:   return TRIG_MAX_ANGLE * 3 / 4;
    case TT_TSLL: return TRIG_MAX_ANGLE * 7 / 8;
    case TT_KL:   return TRIG_MAX_ANGLE * 7 / 8;
    default:      return 0;
  }
}

/**
 * Der Kreisverkehr: ein Ring mit einer Ausfahrt.
 *
 * Kein gedrehter Pfeil. Ein Kreisverkehr als schraege Spitze zu zeichnen
 * hiesse, ueber die Kreuzung zu luegen, die gleich kommt - man faehrt hinein
 * und sucht die Abzweigung, die der Pfeil versprochen hat.
 */
static void prv_kreisel(GContext *ctx, GPoint m, bool links) {
  const int16_t r = P_K;
  const int16_t dick = KST_BREIT ? 3 : 2;
  graphics_context_set_stroke_color(ctx, KST_FARBE_TEXT);
  graphics_context_set_stroke_width(ctx, dick);
  graphics_draw_circle(ctx, m, (int16_t)(r - dick));

  // Die Ausfahrt: ein kurzer Strich nach oben, auf der Seite, auf der man den
  // Kreisel verlaesst.
  const int16_t weg = (int16_t)(links ? -r : r);
  graphics_draw_line(ctx, GPoint((int16_t)(m.x + weg), m.y),
                     GPoint((int16_t)(m.x + weg + (links ? -dick * 2 : dick * 2)),
                            (int16_t)(m.y - r)));
  // Und die Einfahrt von unten, damit der Ring nicht als blosser Kreis liest.
  graphics_draw_line(ctx, GPoint(m.x, (int16_t)(m.y + r - dick)),
                     GPoint(m.x, (int16_t)(m.y + r + dick * 2)));
  graphics_context_set_stroke_width(ctx, 1);
}

/** Abseits der Route: ein durchgestrichener Kreis. */
static void prv_abseits(GContext *ctx, GPoint m) {
  const int16_t r = P_K;
  const int16_t dick = KST_BREIT ? 3 : 2;
  graphics_context_set_stroke_color(ctx, KST_FARBE_TEXT);
  graphics_context_set_stroke_width(ctx, dick);
  graphics_draw_circle(ctx, m, (int16_t)(r - dick));
  const int16_t d = (int16_t)((r - dick) * 7 / 10);
  graphics_draw_line(ctx, GPoint((int16_t)(m.x - d), (int16_t)(m.y - d)),
                     GPoint((int16_t)(m.x + d), (int16_t)(m.y + d)));
  graphics_context_set_stroke_width(ctx, 1);
}

void pfeil_zeichne(GContext *ctx, GPoint mitte, int art) {
  if (!pfeil_kennt(art)) return;

  graphics_context_set_fill_color(ctx, KST_FARBE_TEXT);

  if (art == TT_RNDB || art == TT_RNLB) {
    prv_kreisel(ctx, mitte, art == TT_RNLB);
    return;
  }
  if (art == TT_OFFR) {
    prv_abseits(ctx, mitte);
    return;
  }

  if (!s_pfad) pfeil_init();
  gpath_rotate_to(s_pfad, prv_winkel(art));
  gpath_move_to(s_pfad, mitte);
  gpath_draw_filled(ctx, s_pfad);
}
