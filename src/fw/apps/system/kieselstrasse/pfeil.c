/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pebble_compat.h"
#include "pfeil.h"
#include "thema.h"
#include "weg.h"

#include "pbl/util/math.h" // integer_sqrt

// WIE DER PFEIL ENTSTEHT: eine Mittellinie, wie ein Stift sie faehrt - ein
// Stueck geradeaus, ein Bogen, wieder geradeaus -, und darum ein Band von
// fester Breite mit einer Spitze am Ende. So braucht jede Abbiegeart nur drei,
// vier Zeilen, und alle haben denselben Schaft und dieselbe Spitze. Das Bild
// ist OsmAnds Wegweiser (TurnPathHelper); gezeichnet ist es hier neu.
//
// Gerechnet wird in einem Entwurfsquadrat von 100 Einheiten zu je 16 Teilen
// (E), nur in ganzen Zahlen. Erst beim Zeichnen wird auf den Kasten skaliert.

#define E(v) ((int32_t)(v) * 16)

// Die Masse, in Einheiten des 100er-Quadrats. Wie bei OsmAnd ist die Spitze
// rund dreimal so breit wie der Schaft - sie ist das, was man im
// Augenwinkel erkennt.
#define SCHAFT_HALB    120      // 7,5
#define SPITZE_HALB    E(23)
#define SPITZE_LANG    E(28)

// Der Kreisverkehr: Ring auf Radius 24, gleich breit wie der Schaft - innen
// bleibt Platz fuer die Nummer der Ausfahrt.
#define RING_R         E(24)
#define RING_AUSSEN    (RING_R + SCHAFT_HALB)
#define RING_INNEN     (RING_R - SCHAFT_HALB)
#define RING_ZULAUF    E(10)
#define RING_ABLAUF    E(4)
// Naeher als so an der Einfahrt stiesse die Spitze der Ausfahrt in den Zulauf.
#define RING_MIN_GRAD  50
// Auf der langen Seite (fast ganz herum, auch Wenden im Kreisel) reichen die
// Kanten von Zulauf und Ausfahrt bis an den Innenkreis. Unter 2*asin(7,5/16,5)
// = 54 Grad kreuzten sie sich dort, und der Fueller liesse einen Keil leer.
#define RING_MIN_LANG  56

#define LINIE_MAX 40
#define ECKEN_MAX 110
#define TEILE_MAX 5

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

typedef struct {
  int32_t x, y;
  int32_t a;  // Fahrtrichtung: 0 = nach oben, im Uhrzeigersinn wachsend
} Stift;

typedef struct {
  int32_t x, y;
} Ecke;

// Alles, was beim Bauen eines Zeichens anfaellt: die Mittellinie des gerade
// gebauten Teils, alle Umrisse hintereinander (teil_ende sagt, wo einer
// aufhoert - "abseits der Route" hat mehrere) und die Punkte fuer den Fueller.
//
// AUF DEM APP-HEAP, NUR WAEHREND DES ZEICHNENS. Als statische Felder laegen
// die 1,8 KB dauerhaft im RAM der Firmware, auch wenn die App nie laeuft; auf
// dem Stapel des Layer-Zeichnens waeren sie zu viel.
typedef struct {
  Stift linie[LINIE_MAX];
  int nl;
  Ecke ecken[ECKEN_MAX];
  int ne;
  int teil_ende[TEILE_MAX];
  int nt;
  // Was ausser den Umrissen noch Platz braucht (der Ring), fuer die Mitte.
  int32_t min_x, min_y, max_x, max_y;
  GPoint punkte[ECKEN_MAX];
} Bau;

static Bau *s_bau;

void pfeil_zuruecksetzen(void) {
  // Ein Zeiger aus dem vorigen Lauf zeigte in dessen App-Heap.
  s_bau = NULL;
}

bool pfeil_kennt(int art) { return art >= TT_C && art <= TT_RNLB; }

bool pfeil_zeigt_nummer(int art, int ausfahrt) {
  return (art == TT_RNDB || art == TT_RNLB) && ausfahrt > 0 && ausfahrt < 100;
}

// --- Rechnen ---

static int32_t prv_grad(int32_t grad) { return grad * TRIG_MAX_ANGLE / 360; }

static int32_t prv_sin(int32_t a) { return sin_lookup((int32_t)normalize_angle(a)); }
static int32_t prv_cos(int32_t a) { return cos_lookup((int32_t)normalize_angle(a)); }

static int32_t prv_mal(int32_t laenge, int32_t verhaeltnis) {
  return (int32_t)(((int64_t)laenge * verhaeltnis) / TRIG_MAX_RATIO);
}

/** Der Winkel, unter dem eine Sehne im Abstand `halb` vom Mittelpunkt einen Kreis trifft. */
static int32_t prv_asin(int32_t halb, int32_t radius) {
  const int32_t rest = integer_sqrt((int64_t)radius * radius - (int64_t)halb * halb);
  return atan2_lookup((int16_t)halb, (int16_t)rest);
}

static void prv_ecke(int32_t x, int32_t y) {
  if (s_bau->ne >= ECKEN_MAX) return;
  s_bau->ecken[s_bau->ne++] = (Ecke){ x, y };
  if (x < s_bau->min_x) s_bau->min_x = x;
  if (x > s_bau->max_x) s_bau->max_x = x;
  if (y < s_bau->min_y) s_bau->min_y = y;
  if (y > s_bau->max_y) s_bau->max_y = y;
}

static void prv_teil_fertig(void) {
  if (s_bau->nt < TEILE_MAX) s_bau->teil_ende[s_bau->nt++] = s_bau->ne;
}

// --- Der Stift ---

static void prv_start(int32_t x, int32_t y) {
  s_bau->nl = 0;
  s_bau->linie[s_bau->nl++] = (Stift){ x, y, 0 };
}

static void prv_gerade(int32_t laenge) {
  const Stift p = s_bau->linie[s_bau->nl - 1];
  if (s_bau->nl >= LINIE_MAX) return;
  s_bau->linie[s_bau->nl++] = (Stift){ p.x + prv_mal(laenge, prv_sin(p.a)),
                             p.y - prv_mal(laenge, prv_cos(p.a)), p.a };
}

/** Ein Bogen: `grad` > 0 nach rechts, < 0 nach links. */
static void prv_bogen(int32_t radius, int32_t grad) {
  const Stift p = s_bau->linie[s_bau->nl - 1];
  const int32_t s = grad > 0 ? 1 : -1;
  // Der Mittelpunkt liegt auf der Seite, zu der der Bogen dreht. Die rechte
  // Normale einer Fahrtrichtung a ist (cos a, sin a).
  const int32_t mx = p.x + s * prv_mal(radius, prv_cos(p.a));
  const int32_t my = p.y + s * prv_mal(radius, prv_sin(p.a));
  // Alle neun Grad ein Punkt - darunter sieht man am Rand keine Ecken mehr.
  const int32_t schritte = (grad * s) / 9 + 1;
  for (int32_t i = 1; i <= schritte && s_bau->nl < LINIE_MAX; i++) {
    const int32_t a = p.a + prv_grad(grad) * i / schritte;
    s_bau->linie[s_bau->nl++] = (Stift){ mx - s * prv_mal(radius, prv_cos(a)),
                               my - s * prv_mal(radius, prv_sin(a)), a };
  }
}

/**
 * Aus der Mittellinie einen Umriss: rechte Kante hinauf, die Spitze, linke
 * Kante zurueck. Ein einfaches Vieleck - der Fueller der Firmware kennt keine
 * Ueberschneidungen.
 */
static void prv_band(bool mit_spitze) {
  for (int i = 0; i < s_bau->nl; i++) {
    const Stift p = s_bau->linie[i];
    prv_ecke(p.x + prv_mal(SCHAFT_HALB, prv_cos(p.a)), p.y + prv_mal(SCHAFT_HALB, prv_sin(p.a)));
  }
  if (mit_spitze) {
    const Stift p = s_bau->linie[s_bau->nl - 1];
    const int32_t nx = prv_cos(p.a), ny = prv_sin(p.a);
    prv_ecke(p.x + prv_mal(SPITZE_HALB, nx), p.y + prv_mal(SPITZE_HALB, ny));
    prv_ecke(p.x + prv_mal(SPITZE_LANG, prv_sin(p.a)), p.y - prv_mal(SPITZE_LANG, prv_cos(p.a)));
    prv_ecke(p.x - prv_mal(SPITZE_HALB, nx), p.y - prv_mal(SPITZE_HALB, ny));
  }
  for (int i = s_bau->nl - 1; i >= 0; i--) {
    const Stift p = s_bau->linie[i];
    prv_ecke(p.x - prv_mal(SCHAFT_HALB, prv_cos(p.a)), p.y - prv_mal(SCHAFT_HALB, prv_sin(p.a)));
  }
  prv_teil_fertig();
}

// --- Die Zeichen ---

/**
 * Die gewoehnlichen Pfeile. `seite` ist +1 fuer rechts, -1 fuer links: die
 * linke Hand ist das Spiegelbild der rechten.
 */
static void prv_pfeil(int art) {
  int32_t seite = 1;
  switch (art) {
    case TT_TL: case TT_TSLL: case TT_TSHL: case TT_KL: case TT_TU:
      seite = -1;
      break;
    default:
      break;
  }

  prv_start(0, 0);
  switch (art) {
    case TT_TR: case TT_TL:
      prv_gerade(E(34));
      prv_bogen(E(22), seite * 90);
      prv_gerade(E(12));
      break;
    case TT_TSLR: case TT_TSLL:
      prv_gerade(E(38));
      prv_bogen(E(34), seite * 45);
      prv_gerade(E(8));
      break;
    case TT_TSHR: case TT_TSHL:
      // Hinauf und scharf zurueck: die Spitze zeigt schraeg nach unten.
      prv_gerade(E(48));
      prv_bogen(E(16), seite * 135);
      prv_gerade(E(4));
      break;
    case TT_KR: case TT_KL:
      // Ein Versatz zur Seite, dann weiter geradeaus - wie ein Spurwechsel.
      // Gut 1,5 Schaftbreiten: weniger, und er saehe aus wie "leicht links".
      prv_gerade(E(16));
      prv_bogen(E(26), seite * 60);
      prv_bogen(E(26), -seite * 60);
      prv_gerade(E(6));
      break;
    case TT_TU: case TT_TRU:
      prv_gerade(E(40));
      prv_bogen(E(20), seite * 180);
      prv_gerade(E(12));
      break;
    default:  // TT_C
      prv_gerade(E(62));
      break;
  }
  prv_band(true);
}

/** Abseits der Route: ein gestrichelter Pfeil geradeaus, wie bei OsmAnd. */
static void prv_abseits(void) {
  for (int i = 0; i < 3; i++) {
    prv_start(0, -E(i * 19));
    prv_gerade(E(12));
    prv_band(false);
  }
  prv_start(0, -E(57));
  prv_gerade(E(6));
  prv_band(true);
}

static int32_t prv_px(int32_t v, int32_t k) { return prv_mal(v, prv_sin(k)); }
static int32_t prv_py(int32_t v, int32_t k) { return -prv_mal(v, prv_cos(k)); }

/**
 * Der Kreisverkehr: Zulauf von unten, im Ring bis zur Ausfahrt, hinaus.
 *
 * Gebaut fuer Rechtsverkehr (gegen den Uhrzeigersinn); Linksverkehr ist das
 * Spiegelbild. Die Lage im Ring heisst `phi`: 0 oben, im Uhrzeigersinn - die
 * Einfahrt liegt also bei 180 Grad.
 *
 * `fahrt` ist, wie weit man im Ring faehrt (Grad), oder < 0, wenn es niemand
 * weiss: dann nur Zulauf und Ring, keine geratene Ausfahrt.
 */
static void prv_kreisel(int32_t fahrt) {
  const int32_t unten = RING_AUSSEN + RING_ZULAUF;
  const int32_t a_aussen = prv_asin(SCHAFT_HALB, RING_AUSSEN);
  const int32_t a_innen = prv_asin(SCHAFT_HALB, RING_INNEN);
  const int32_t halb_grad = TRIG_MAX_ANGLE / 2;

  prv_ecke(SCHAFT_HALB, unten);
  if (fahrt < 0) {
    const int32_t y = integer_sqrt((int64_t)RING_AUSSEN * RING_AUSSEN -
                                   (int64_t)SCHAFT_HALB * SCHAFT_HALB);
    prv_ecke(SCHAFT_HALB, y);
    prv_ecke(-SCHAFT_HALB, y);
    prv_ecke(-SCHAFT_HALB, unten);
    prv_teil_fertig();
    return;
  }

  const int32_t ziel = halb_grad - prv_grad(fahrt);  // phi der Ausfahrt
  // Aussen am Ring entlang, von der Einfahrt zur Ausfahrt.
  const int32_t von = halb_grad - a_aussen, bis = ziel + a_aussen;
  const int32_t schritte = (von - bis) / prv_grad(9) + 1;
  for (int32_t i = 0; i <= schritte; i++) {
    const int32_t k = von - (von - bis) * i / schritte;
    prv_ecke(prv_px(RING_AUSSEN, k), prv_py(RING_AUSSEN, k));
  }
  // Hinaus, die Spitze, und wieder herein. u zeigt nach aussen, t quer dazu.
  const int32_t ux = prv_sin(ziel), uy = -prv_cos(ziel);
  const int32_t tx = prv_cos(ziel), ty = prv_sin(ziel);
  const int32_t fuss = RING_AUSSEN + RING_ABLAUF;
  prv_ecke(prv_mal(fuss, ux) + prv_mal(SCHAFT_HALB, tx), prv_mal(fuss, uy) + prv_mal(SCHAFT_HALB, ty));
  prv_ecke(prv_mal(fuss, ux) + prv_mal(SPITZE_HALB, tx), prv_mal(fuss, uy) + prv_mal(SPITZE_HALB, ty));
  prv_ecke(prv_mal(fuss + SPITZE_LANG, ux), prv_mal(fuss + SPITZE_LANG, uy));
  prv_ecke(prv_mal(fuss, ux) - prv_mal(SPITZE_HALB, tx), prv_mal(fuss, uy) - prv_mal(SPITZE_HALB, ty));
  prv_ecke(prv_mal(fuss, ux) - prv_mal(SCHAFT_HALB, tx), prv_mal(fuss, uy) - prv_mal(SCHAFT_HALB, ty));
  // Innen zurueck zur Einfahrt.
  const int32_t von_i = ziel - a_innen, bis_i = halb_grad + a_innen;
  const int32_t schritte_i = (bis_i - von_i) / prv_grad(9) + 1;
  for (int32_t i = 0; i <= schritte_i; i++) {
    const int32_t k = von_i + (bis_i - von_i) * i / schritte_i;
    prv_ecke(prv_px(RING_INNEN, k), prv_py(RING_INNEN, k));
  }
  prv_ecke(-SCHAFT_HALB, unten);
  prv_teil_fertig();
}

/**
 * Wie weit im Ring gefahren wird, aus OsmAnds turn_angle.
 *
 * Nach OsmAnds eigener Zeichnung: der Winkel zeigt, wohin die Ausfahrt
 * fuehrt (0 geradeaus, 90 rechts). Ganz nah an der Einfahrt entscheidet die
 * Nummer, ob es die erste Ausfahrt ist oder eine Runde fast ganz herum.
 */
static int32_t prv_fahrt(int winkel, int ausfahrt, bool links) {
  if (winkel == WEG_KEIN_WINKEL) return -1;
  int32_t fahrt = links ? 180 + winkel : 180 - winkel;
  if (fahrt < RING_MIN_GRAD) {
    fahrt = ausfahrt > 2 ? 360 - RING_MIN_LANG : RING_MIN_GRAD;
  } else if (fahrt > 360 - RING_MIN_LANG) {
    fahrt = ausfahrt == 1 ? RING_MIN_GRAD : 360 - RING_MIN_LANG;
  }
  return fahrt;
}

// --- Zeichnen ---

typedef struct {
  GPoint mitte;
  int32_t cx, cy;   // Mitte des Entwurfs (E)
  int32_t zaehler, nenner;
} Abbildung;

static int32_t prv_runde(int32_t z, int32_t n) {
  return z >= 0 ? (z + n / 2) / n : -((-z + n / 2) / n);
}

static GPoint prv_bild(const Abbildung *ab, int32_t x, int32_t y) {
  return GPoint((int16_t)(ab->mitte.x + prv_runde((x - ab->cx) * ab->zaehler, ab->nenner)),
                (int16_t)(ab->mitte.y + prv_runde((y - ab->cy) * ab->zaehler, ab->nenner)));
}

static void prv_zahl(GContext *ctx, GPoint m, int16_t r, int ausfahrt) {
  char text[4];
  snprintf(text, sizeof(text), "%d", ausfahrt);
  const GFont f = fonts_get_system_font(r >= 17 ? FONT_KEY_GOTHIC_28_BOLD
                                        : r >= 12 ? FONT_KEY_GOTHIC_24_BOLD
                                                  : FONT_KEY_GOTHIC_18_BOLD);
  const GRect feld = GRect((int16_t)(m.x - r), (int16_t)(m.y - r), (int16_t)(2 * r),
                           (int16_t)(2 * r));
  const GSize g = graphics_text_layout_get_content_size(
      text, f, feld, GTextOverflowModeFill, GTextAlignmentCenter);
  // GOTHIC laesst ueber den Ziffern ein Viertel der Zeile frei; ohne den
  // Ausgleich saesse die Zahl zu tief im Ring.
  const int16_t y = (int16_t)(m.y - g.h / 2 - g.h / 6);
  graphics_context_set_text_color(ctx, KST_FARBE_TEXT);
  graphics_draw_text(ctx, text, f, GRect(feld.origin.x, y, feld.size.w, (int16_t)(g.h + 4)),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

static void prv_zeichne(GContext *ctx, GRect kasten, int art, int ausfahrt, int winkel) {
  s_bau->ne = 0;
  s_bau->nt = 0;
  s_bau->min_x = s_bau->min_y = INT32_MAX;
  s_bau->max_x = s_bau->max_y = INT32_MIN;

  const bool kreisel = (art == TT_RNDB || art == TT_RNLB);
  const bool links = (art == TT_RNLB);
  if (kreisel) {
    // Der ganze Ring braucht Platz, auch wo der Weg nicht hinfuehrt.
    prv_ecke(-RING_AUSSEN, -RING_AUSSEN);
    prv_ecke(RING_AUSSEN, RING_AUSSEN);
    s_bau->ne = 0;
    prv_kreisel(prv_fahrt(winkel, ausfahrt, links));
    if (links) {
      for (int i = 0; i < s_bau->ne; i++) s_bau->ecken[i].x = -s_bau->ecken[i].x;
      const int32_t t = s_bau->min_x;
      s_bau->min_x = -s_bau->max_x;
      s_bau->max_x = -t;
    }
  } else if (art == TT_OFFR) {
    prv_abseits();
  } else {
    prv_pfeil(art);
  }
  if (s_bau->nt == 0) return;

  // Auf den Kasten: die Mitte des Entwurfs in die Mitte des Kastens, und so
  // gross, dass er ihn fuellt. Nur ein ganz kleiner Entwurf (ein Kreisel mit
  // seitlicher Ausfahrt) waechst nicht ueber 88 Einheiten hinaus - sonst
  // haette sein Schaft eine andere Breite als der aller anderen Pfeile.
  const int32_t seite = MIN(kasten.size.w, kasten.size.h);
  const int32_t gross = MAX(s_bau->max_x - s_bau->min_x, s_bau->max_y - s_bau->min_y);
  const Abbildung ab = {
    .mitte = GPoint((int16_t)(kasten.origin.x + kasten.size.w / 2),
                    (int16_t)(kasten.origin.y + kasten.size.h / 2)),
    .cx = (s_bau->min_x + s_bau->max_x) / 2,
    .cy = (s_bau->min_y + s_bau->max_y) / 2,
    .zaehler = seite,
    .nenner = MAX(E(88), gross),
  };

  const bool aa = graphics_context_get_antialiased(ctx);
  graphics_context_set_antialiased(ctx, true);

  if (kreisel) {
    const GPoint m = prv_bild(&ab, 0, 0);
    const int16_t ra = (int16_t)prv_runde(RING_AUSSEN * ab.zaehler, ab.nenner);
    const int16_t ri = (int16_t)prv_runde(RING_INNEN * ab.zaehler, ab.nenner);
    graphics_context_set_fill_color(ctx, KST_FARBE_RING);
    graphics_fill_circle(ctx, m, (uint16_t)ra);
    graphics_context_set_fill_color(ctx, KST_FARBE_GRUND);
    graphics_fill_circle(ctx, m, (uint16_t)ri);
    graphics_context_set_stroke_color(ctx, KST_FARBE_PFEIL_RAND);
    graphics_context_set_stroke_width(ctx, 2);
    graphics_draw_circle(ctx, m, (uint16_t)ra);
    graphics_draw_circle(ctx, m, (uint16_t)ri);
  }

  GPoint *punkte = s_bau->punkte;
  int anfang = 0;
  for (int t = 0; t < s_bau->nt; t++) {
    const int n = s_bau->teil_ende[t] - anfang;
    for (int i = 0; i < n; i++) {
      punkte[i] = prv_bild(&ab, s_bau->ecken[anfang + i].x, s_bau->ecken[anfang + i].y);
    }
    GPath pfad = { .num_points = (uint32_t)n, .points = punkte };
    graphics_context_set_fill_color(ctx, KST_FARBE_PFEIL);
    gpath_draw_filled(ctx, &pfad);
#if PBL_COLOR
    // Der dunkle Rand traegt das Gruen auf dem hellen Grund - ohne ihn
    // verschwimmt die Spitze bei Sonne. Schwarz auf Weiss braucht keinen.
    graphics_context_set_stroke_color(ctx, KST_FARBE_PFEIL_RAND);
    graphics_context_set_stroke_width(ctx, 2);
    gpath_draw_outline(ctx, &pfad);
#endif
    anfang = s_bau->teil_ende[t];
  }
  graphics_context_set_stroke_width(ctx, 1);
  graphics_context_set_antialiased(ctx, aa);

  if (kreisel && pfeil_zeigt_nummer(art, ausfahrt)) {
    const int16_t ri = (int16_t)prv_runde(RING_INNEN * ab.zaehler, ab.nenner);
    prv_zahl(ctx, prv_bild(&ab, 0, 0), ri, ausfahrt);
  }
}

void pfeil_zeichne(GContext *ctx, GRect kasten, int art, int ausfahrt, int winkel) {
  if (!pfeil_kennt(art)) return;
  s_bau = app_malloc(sizeof(Bau));
  // Ohne Speicher kein Pfeil - Zahl und Strasse stehen trotzdem da.
  if (!s_bau) return;
  prv_zeichne(ctx, kasten, art, ausfahrt, winkel);
  app_free(s_bau);
  s_bau = NULL;
}
