/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pebble_compat.h"
#include "nav_window.h"
#include "pfeil.h"
#include "texte.h"
#include "thema.h"
#include "weg.h"

// Der Schirm, von oben nach unten:
//
//        19:46             <- die Uhrzeit. Steht IMMER da.
//   ──────────────────
//   ╭──────╮   250         <- Pfeil und Zahl, so gross es geht
//   │  ↱   │    m
//   ╰──────╯
//   ==================     <- Balken, erst unter 300 m
//   Bahnhofstrasse         <- wohin es geht, bis zwei Zeilen
//   12 km · an 20:11       <- was noch bleibt
//
// DIE UHRZEIT STEHT OBEN, WEIL DAS DING EINE UHR IST. Wer aufs Handgelenk
// schaut, will oft genug bloss wissen, wie spaet es ist - und soll dafuer keine
// App verlassen muessen.
//
// ALLES DAZWISCHEN GEHOERT DEM PFEIL. Erst werden Fuss- und Strassenzeile
// gemessen, der Rest ist sein Kasten. Fehlt die Strasse, waechst er, statt ein
// Loch zu lassen; ist der Kasten hoch genug, steht die Zahl unter dem Pfeil
// statt daneben, in der grossen Schrift.
//
// Der Platz fuer den Balken ist IMMER frei: sonst schrumpfte der Pfeil genau
// in dem Augenblick, in dem der Balken bei 300 m erscheint, und die Anzeige
// spraenge einem unter dem Blick weg.

static Window *s_window;
static Layer *s_leinwand;

#define BALKEN_H     (KST_BREIT ? 8 : 6)
#define BALKEN_PLATZ (BALKEN_H + (KST_BREIT ? 8 : 5))

// Kopfzeile: Uhrzeit und der Strich darunter.
#define KOPF_H   (KST_BREIT ? 26 : 20)

// Abstand zwischen Pfeil und Zahl, wenn sie nebeneinander stehen.
#define LUECKE   (KST_BREIT ? 6 : 4)

static GFont prv_font_uhr(void) {
  return fonts_get_system_font(KST_BREIT ? FONT_KEY_GOTHIC_24_BOLD
                                        : FONT_KEY_GOTHIC_18_BOLD);
}

static GFont prv_font_name(void) {
  return fonts_get_system_font(KST_BREIT ? FONT_KEY_GOTHIC_28_BOLD
                                        : FONT_KEY_GOTHIC_18_BOLD);
}

static GFont prv_font_einheit(void) {
  return fonts_get_system_font(KST_BREIT ? FONT_KEY_GOTHIC_28_BOLD
                                        : FONT_KEY_GOTHIC_18_BOLD);
}

static GFont prv_font_fuss(void) {
  return fonts_get_system_font(KST_BREIT ? FONT_KEY_GOTHIC_24_BOLD
                                        : FONT_KEY_GOTHIC_18_BOLD);
}

static GFont prv_font_zart(void) {
  return fonts_get_system_font(KST_BREIT ? FONT_KEY_GOTHIC_24 : FONT_KEY_GOTHIC_14);
}

/**
 * Die Ziffernschriften, von gross nach klein, mit dem, was LECO ueber den
 * Ziffern frei laesst (`leer`) und wie hoch sie selbst sind (`hoch`) -
 * nachgemessen in den pbf-Dateien. Gebraucht wird nur die Ziffer; der leere
 * Streifen darueber wuerde sonst als Abstand mitgerechnet.
 */
typedef struct {
  const char *schluessel;
  int16_t leer, hoch;
} Ziffern;

static const Ziffern s_ziffern[] = {
  { FONT_KEY_LECO_60_BOLD_NUMBERS_AM_PM, 18, 42 },
  // LECO 42 gibt es nur duenn. Neben dem Pfeil stuende "99 m" dann zarter als
  // "100 m" - gerade die Zahl kurz vor dem Abbiegen.
  { FONT_KEY_LECO_38_BOLD_NUMBERS, 11, 27 },
  { FONT_KEY_LECO_32_BOLD_NUMBERS, 10, 22 },
};

/**
 * Die Entfernung als Zahl und Einheit.
 *
 * ZWEIGETEILT, WEIL LECO KEINE KLEINBUCHSTABEN HAT. Es hat ein Komma -
 * nachgesehen, nicht angenommen -, aber kein m. Die Zahl steht also in LECO,
 * die Einheit daneben in GOTHIC.
 */
static void prv_zerlege(int32_t meter, char *zahl, size_t zl, char *einheit, size_t el) {
  if (meter < 1000) {
    snprintf(zahl, zl, "%d", (int)meter);
    snprintf(einheit, el, "%s", "m");
  } else {
    const int32_t zehntel = (meter + 50) / 100;   // auf 100 m gerundet
    snprintf(zahl, zl, "%d%s%d", (int)(zehntel / 10), ST(STR_DEZIMAL), (int)(zehntel % 10));
    snprintf(einheit, el, "%s", "km");
  }
}

static GSize prv_mass(const char *text, GFont f, int16_t breite) {
  return graphics_text_layout_get_content_size(
      text, f, GRect(0, 0, breite, 400), GTextOverflowModeWordWrap, GTextAlignmentLeft);
}

static int16_t prv_zahl_breite(const char *zahl, int zf) {
  return prv_mass(zahl, fonts_get_system_font(s_ziffern[zf].schluessel), 400).w;
}

/**
 * Die groesste Ziffernschrift, in der `zahl` hoechstens `breite` braucht.
 * Passt sie in keine, die kleinste - wer das zeichnet, muss Platz schaffen.
 */
static int prv_ziffern_fuer(const char *zahl, int16_t breite, int ab) {
  const int n = (int)ARRAY_LENGTH(s_ziffern);
  for (int i = ab; i < n; i++) {
    if (prv_zahl_breite(zahl, i) <= breite) return i;
  }
  return n - 1;
}

/** Die Kopfzeile mit der Uhrzeit. Rueckgabe: wo es darunter weitergeht. */
static int16_t prv_kopf(GContext *ctx, GRect b) {
  char uhr[10];
  clock_copy_time_string(uhr, sizeof(uhr));
  const int16_t y = PBL_IF_ROUND_ELSE(16, 0);
  graphics_context_set_text_color(ctx, KST_FARBE_TEXT);
  graphics_draw_text(ctx, uhr, prv_font_uhr(),
                     GRect(KST_RAND, y, (int16_t)(b.size.w - 2 * KST_RAND), KOPF_H),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  const int16_t unter = (int16_t)(y + KOPF_H + 2);
  graphics_context_set_fill_color(ctx, KST_FARBE_TEXT);
  graphics_fill_rect(ctx, GRect(KST_RAND, unter,
                                (int16_t)(b.size.w - 2 * KST_RAND), 2), 0, GCornerNone);
  return (int16_t)(unter + (KST_BREIT ? 6 : 4));
}

/** Restweg und Ankunft in `rest`; leer, wenn beides fehlt. */
static void prv_rest_text(const Weg *w, char *rest, size_t len) {
  rest[0] = '\0';
  if (w->rest_m > 0) {
    if (w->rest_m < 1000) snprintf(rest, len, "%d m", (int)w->rest_m);
    else snprintf(rest, len, "%d km", (int)((w->rest_m + 500) / 1000));
  }
  if (w->ankunft != 0) {
    // Die Uhr rechnet in ihrem time_t; der Zeitpunkt kommt vorzeichenlos (weg.h).
    const time_t ankunft = (time_t)w->ankunft;
    struct tm *an = localtime(&ankunft);
    char hhmm[10];
    // 12 h ohne fuehrende Null, wie die Kopfzeile; %l fuellt mit Leerzeichen.
    strftime(hhmm, sizeof(hhmm), clock_is_24h_style() ? "%H:%M" : "%l:%M", an);
    const char *zeit = (hhmm[0] == ' ') ? hhmm + 1 : hhmm;
    const size_t n = strlen(rest);
    if (n > 0) snprintf(rest + n, len - n, "%s", " · ");
    const size_t m = strlen(rest);
    snprintf(rest + m, len - m, ST(STR_ANKUNFT), zeit);
  }
}

/**
 * Die Strasse ohne vorangestelltes "Exit 2 · ", wenn die Nummer schon im
 * Ring steht.
 *
 * Boulder stellt sie voran, damit eine Uhr ohne AUSFAHRT sie auch sieht.
 * Doppelt kostet sie hier die Zeile, die man im Vorbeifahren liest: der Name
 * faellt eine Schriftgroesse kleiner oder wird abgeschnitten. Der Text ist
 * fest, Boulder spricht nur Englisch.
 */
static const char *prv_ohne_ausfahrt(const char *strasse, int ausfahrt) {
  char vorsilbe[12];
  snprintf(vorsilbe, sizeof(vorsilbe), "Exit %d", ausfahrt);
  const size_t n = strlen(vorsilbe);
  if (strncmp(strasse, vorsilbe, n) != 0) return strasse;
  const char *rest = strasse + n;
  if (rest[0] == '\0') return rest;
  static const char trenner[] = " · ";
  if (strncmp(rest, trenner, sizeof(trenner) - 1) == 0) return rest + sizeof(trenner) - 1;
  // "Exit 12" bei Ausfahrt 1 - eine andere Nummer, also nichts anruehren.
  return strasse;
}

/** Kein gueltiger Stand: gross, was los ist, klein, seit wann. */
static void prv_keine_navigation(GContext *ctx, int16_t oben, int16_t unten, int16_t breite) {
  const GFont fn = prv_font_name();
  const GFont fz = prv_font_zart();
  char wann[28];
  weg_alter_text(wann, sizeof(wann));
  char zeile[40];
  snprintf(zeile, sizeof(zeile), ST(STR_ZULETZT), wann);
  const GSize m1 = prv_mass(ST(STR_KEINE_NAVIGATION), fn, breite);
  const GSize m2 = prv_mass(zeile, fz, breite);
  // Mittig im freien Raum - oben angeschlagen stuende darunter ein Loch.
  int16_t y = (int16_t)(oben + (unten - oben - m1.h - 6 - m2.h) / 2);
  if (y < oben) y = oben;
  graphics_context_set_text_color(ctx, KST_FARBE_TEXT);
  graphics_draw_text(ctx, ST(STR_KEINE_NAVIGATION), fn,
                     GRect(KST_RAND, y, breite, (int16_t)(m1.h + 4)),
                     GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  graphics_context_set_text_color(ctx, KST_FARBE_ZART);
  graphics_draw_text(ctx, zeile, fz,
                     GRect(KST_RAND, (int16_t)(y + m1.h + 6), breite, (int16_t)(m2.h + 4)),
                     GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

/**
 * Zahl und Einheit, die Zahl in LECO mit ihrer Oberkante (nicht der ihres
 * leeren Streifens) bei `y`. `untereinander`: Einheit unter statt neben der
 * Zahl. Rueckgabe: die Hoehe der Gruppe; mit `zeichnen` false nur gemessen.
 */
static int16_t prv_zahl(GContext *ctx, const char *zahl, const char *einheit, int zf,
                        GRect feld, bool untereinander, bool zeichnen) {
  const GFont fz = fonts_get_system_font(s_ziffern[zf].schluessel);
  const GFont fe = prv_font_einheit();
  const GSize mz = prv_mass(zahl, fz, 400);
  const GSize me = prv_mass(einheit, fe, 400);
  // GOTHIC traegt ueber den Buchstaben rund ein Drittel seiner Hoehe leer.
  const int16_t e_leer = (int16_t)(KST_BREIT ? 10 : 6);
  const int16_t e_hoch = (int16_t)(me.h - e_leer);
  const int16_t hoch = s_ziffern[zf].hoch;
  const int16_t gesamt = untereinander ? (int16_t)(hoch + 4 + e_hoch) : hoch;
  if (!zeichnen) return gesamt;

  graphics_context_set_text_color(ctx, KST_FARBE_TEXT);
  if (untereinander) {
    const int16_t mitte = (int16_t)(feld.origin.x + feld.size.w / 2);
    graphics_draw_text(ctx, zahl, fz,
                       GRect((int16_t)(mitte - mz.w / 2 - 2),
                             (int16_t)(feld.origin.y - s_ziffern[zf].leer),
                             (int16_t)(mz.w + 4), (int16_t)(mz.h + 4)),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    graphics_draw_text(ctx, einheit, fe,
                       GRect((int16_t)(mitte - me.w / 2 - 2),
                             (int16_t)(feld.origin.y + hoch + 4 - e_leer),
                             (int16_t)(me.w + 4), (int16_t)(me.h + 4)),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  } else {
    // Nebeneinander, gemeinsam mittig; die Einheit auf der Grundlinie.
    const int16_t breit = (int16_t)(mz.w + 4 + me.w);
    const int16_t x = (int16_t)(feld.origin.x + (feld.size.w - breit) / 2);
    graphics_draw_text(ctx, zahl, fz,
                       GRect(x, (int16_t)(feld.origin.y - s_ziffern[zf].leer),
                             (int16_t)(mz.w + 4), (int16_t)(mz.h + 4)),
                       GTextOverflowModeFill, GTextAlignmentLeft, NULL);
    graphics_draw_text(ctx, einheit, fe,
                       GRect((int16_t)(x + mz.w + 4),
                             (int16_t)(feld.origin.y + hoch - e_hoch - e_leer),
                             (int16_t)(me.w + 4), (int16_t)(me.h + 4)),
                       GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  }
  return gesamt;
}

static void prv_zeichne(Layer *layer, GContext *ctx) {
  const GRect b = layer_get_bounds(layer);
  const Weg *w = weg_stand();
  const int16_t breite = (int16_t)(b.size.w - 2 * KST_RAND);

  const int16_t oben = prv_kopf(ctx, b);

  // --- Kein gueltiger Stand ---
  // Die Uhr laedt beim Oeffnen den letzten Stand. Einer von gestern saehe aus
  // wie eine gueltige Anweisung, und man faehrt danach.
  const bool mit_pfeil = !weg_veraltet() && pfeil_kennt(w->abbiegeart);
  const bool mit_zahl = !weg_veraltet() && weg_hat_zahl();
  if (!mit_pfeil && !mit_zahl) {
    prv_keine_navigation(ctx, oben, (int16_t)(b.size.h - PBL_IF_ROUND_ELSE(30, 8)), breite);
    return;
  }

  // --- Von unten her: was noch bleibt ---
  // Restweg und Ankunft, eine Zeile ganz unten. Sie sind kein Fahrbefehl,
  // sondern Auskunft; fehlen sie, gehoert die Zeile dem Pfeil.
  int16_t unten = (int16_t)(b.size.h - PBL_IF_ROUND_ELSE(14, 2));
  char rest[40];
  prv_rest_text(w, rest, sizeof(rest));
  if (rest[0]) {
    const GFont ff = prv_font_fuss();
    const int16_t fh = (int16_t)(KST_BREIT ? 26 : 20);
    unten = (int16_t)(unten - fh);
    graphics_context_set_text_color(ctx, KST_FARBE_TEXT);
    graphics_draw_text(ctx, rest, ff, GRect(KST_RAND, (int16_t)(unten - 2), breite, fh),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }

  // --- Wohin es geht ---
  // Eine Zeile in der grossen Schrift, wenn der Name hineinpasst; sonst zwei
  // in der naechstkleineren. Laenger wird er abgeschnitten - drei Zeilen
  // liest am Steuer niemand.
  const int ausfahrt = weg_ausfahrt();
  const int winkel = weg_winkel();
  const char *strasse = w->strasse;
  if (mit_pfeil && pfeil_zeigt_nummer(w->abbiegeart, ausfahrt)) {
    strasse = prv_ohne_ausfahrt(strasse, ausfahrt);
  }
  if (strasse[0]) {
    GFont fn = prv_font_name();
    const int16_t zeile = prv_mass("Ag", fn, breite).h;
    GSize mn = prv_mass(strasse, fn, breite);
    if (mn.h > zeile + 4) {
      fn = fonts_get_system_font(KST_BREIT ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_14_BOLD);
      const int16_t zeile2 = prv_mass("Ag", fn, breite).h;
      mn = prv_mass(strasse, fn, breite);
      if (mn.h > 2 * zeile2 + 4) mn.h = (int16_t)(2 * zeile2);
    }
    unten = (int16_t)(unten - mn.h - (KST_BREIT ? 2 : 1));
    graphics_context_set_text_color(ctx, KST_FARBE_TEXT);
    graphics_draw_text(ctx, strasse, fn,
                       GRect(KST_RAND, (int16_t)(unten - (KST_BREIT ? 4 : 2)), breite,
                             (int16_t)(mn.h + 4)),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }

  // --- Balken ---
  if (mit_zahl) {
    unten = (int16_t)(unten - BALKEN_PLATZ);
    const int proz = weg_balken_prozent();
    if (proz >= 0) {
      const int16_t y = (int16_t)(unten + (BALKEN_PLATZ - BALKEN_H) / 2);
      const int16_t voll = (int16_t)((int32_t)proz * breite / 100);
#if PBL_COLOR
      graphics_context_set_fill_color(ctx, KST_FARBE_BALKEN_BG);
      graphics_fill_rect(ctx, GRect(KST_RAND, y, breite, BALKEN_H), 0, GCornerNone);
#else
      // Ohne Grau zeigt ein Rahmen die Spur - sonst sieht man nicht, wie viel
      // schon weg ist.
      graphics_context_set_stroke_color(ctx, KST_FARBE_BALKEN);
      graphics_draw_rect(ctx, GRect(KST_RAND, y, breite, BALKEN_H));
#endif
      graphics_context_set_fill_color(ctx, KST_FARBE_BALKEN);
      graphics_fill_rect(ctx, GRect(KST_RAND, y, voll, BALKEN_H), 0, GCornerNone);
    }
  }

  // --- Pfeil und Zahl: der ganze Rest ---
  const int16_t hoehe = (int16_t)(unten - oben);
  if (!mit_zahl) {
    // Kein Meterwert, aber eine Richtung: der Augenblick des Abbiegens.
    pfeil_zeichne(ctx, GRect(KST_RAND, oben, breite, hoehe), w->abbiegeart, ausfahrt, winkel);
    return;
  }

  char zahl[12], einheit[4];
  prv_zerlege(w->entfernung_m, zahl, sizeof(zahl), einheit, sizeof(einheit));

  if (!mit_pfeil) {
    const int zf = prv_ziffern_fuer(zahl, (int16_t)(breite - 40), 0);
    const int16_t h = prv_zahl(ctx, zahl, einheit, zf, GRectZero, false, false);
    prv_zahl(ctx, zahl, einheit, zf,
             GRect(KST_RAND, (int16_t)(oben + (hoehe - h) / 2), breite, h), false, true);
    return;
  }

  // Untereinander: Pfeil oben, Zahl mit Einheit in der groessten Schrift
  // darunter. Nebeneinander: der Pfeil links, die Zahl rechts daneben.
  const int zf_unter = prv_ziffern_fuer(zahl, (int16_t)(breite - 40), 0);
  const int16_t h_unter = prv_zahl(ctx, zahl, einheit, zf_unter, GRectZero, false, false);
  const int16_t luecke_unter = (int16_t)(KST_BREIT ? 8 : 4);
  const int16_t p_unter = (int16_t)MIN(hoehe - h_unter - luecke_unter, breite);
  int16_t p_neben = (int16_t)MIN(hoehe, breite * 58 / 100);

  // Untereinander, sobald der Pfeil dort nicht kleiner wird: die Zahl steht
  // dann in der grossen Schrift.
  if (p_unter + (KST_BREIT ? 6 : 4) >= p_neben) {
    const int16_t y = (int16_t)(oben + (hoehe - p_unter - luecke_unter - h_unter) / 2);
    pfeil_zeichne(ctx, GRect(KST_RAND, y, breite, p_unter), w->abbiegeart, ausfahrt, winkel);
    prv_zahl(ctx, zahl, einheit, zf_unter,
             GRect(KST_RAND, (int16_t)(y + p_unter + luecke_unter), breite, h_unter),
             false, true);
    return;
  }

  int16_t spalte = (int16_t)(breite - p_neben - LUECKE);
  const int zf = prv_ziffern_fuer(zahl, spalte, 1);
  // Passt die Zahl selbst in der kleinsten Schrift nicht (flint: 50 px fuer
  // "800"), weicht der Pfeil - eine Zahl, die in ihn hineinragt, liest keiner.
  const int16_t noetig = prv_zahl_breite(zahl, zf);
  if (noetig > spalte) {
    p_neben = (int16_t)(p_neben - (noetig - spalte));
    spalte = noetig;
  }
  const int16_t h = prv_zahl(ctx, zahl, einheit, zf, GRectZero, true, false);
  pfeil_zeichne(ctx, GRect(KST_RAND, (int16_t)(oben + (hoehe - p_neben) / 2), p_neben, p_neben),
                w->abbiegeart, ausfahrt, winkel);
  prv_zahl(ctx, zahl, einheit, zf,
           GRect((int16_t)(KST_RAND + p_neben + LUECKE), (int16_t)(oben + (hoehe - h) / 2),
                 spalte, h),
           true, true);
}

/** Jede Minute neu zeichnen - sonst stuende die Uhrzeit still. */
static void prv_tick(struct tm *jetzt, TimeUnits einheiten) {
  nav_window_refresh();
}

static void prv_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_leinwand = layer_create(layer_get_bounds(root));
  // Ohne Leinwand kein Schirm; prv_unload kommt mit NULL zurecht.
  if (!s_leinwand) return;
  layer_set_update_proc(s_leinwand, prv_zeichne);
  layer_add_child(root, s_leinwand);
  tick_timer_service_subscribe(MINUTE_UNIT, prv_tick);
}

static void prv_unload(Window *window) {
  tick_timer_service_unsubscribe();
  layer_destroy(s_leinwand);
  s_leinwand = NULL;
  window_destroy(s_window);
  s_window = NULL;
}

void nav_window_push(void) {
  if (s_window) return;
  s_window = window_create();
  if (!s_window) return;
  window_set_background_color(s_window, KST_FARBE_GRUND);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_load, .unload = prv_unload,
  });
  window_stack_push(s_window, true);
}

void nav_window_refresh(void) {
  if (s_leinwand) layer_mark_dirty(s_leinwand);
}

void nav_window_zuruecksetzen(void) {
  // Die Zeiger zeigten in den App-Speicher des vorigen Laufs. Ohne das hier
  // hielte nav_window_push das Fenster fuer offen und zeigte nichts.
  s_window = NULL;
  s_leinwand = NULL;
}
