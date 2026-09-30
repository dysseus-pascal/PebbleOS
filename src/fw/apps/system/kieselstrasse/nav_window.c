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
//   [Pfeil]  250 m         <- Zeichen und Zahl als Gruppe, mittig
//   ==================     <- Balken, erst unter 300 m
//   Bahnhofstrasse         <- wohin es geht
//   12 km · an 20:11       <- leise, was noch bleibt
//
// DIE UHRZEIT STEHT OBEN, WEIL DAS DING EINE UHR IST. Wer aufs Handgelenk
// schaut, will oft genug bloss wissen, wie spaet es ist - und soll dafuer keine
// App verlassen muessen. Sie ist durch einen Strich abgesetzt: darunter steht,
// was die Navigation sagt, darueber, was die Uhr immer sagt.
//
// Die Gruppe aus Pfeil und Zahl steht MITTIG und nicht linksbuendig: sonst
// wandert die Zeile, sobald aus "980 m" ein "1,2 km" wird, und am Steuer
// springt einem die Anzeige unter dem Blick weg.

static Window *s_window;
static Layer *s_leinwand;

#define BALKEN_H (KST_BREIT ? 10 : 7)

// Kopfzeile: Uhrzeit und der Strich darunter.
#define KOPF_H   (KST_BREIT ? 30 : 22)

static GFont prv_font_uhr(void) {
  return fonts_get_system_font(KST_BREIT ? FONT_KEY_GOTHIC_28_BOLD
                                        : FONT_KEY_GOTHIC_18_BOLD);
}

static GFont prv_font_zahl(void) {
  return fonts_get_system_font(KST_BREIT ? FONT_KEY_LECO_42_NUMBERS
                                        : FONT_KEY_LECO_32_BOLD_NUMBERS);
}

static GFont prv_font_name(void) {
  return fonts_get_system_font(KST_BREIT ? FONT_KEY_GOTHIC_28_BOLD
                                        : FONT_KEY_GOTHIC_18_BOLD);
}

static GFont prv_font_zart(void) {
  return fonts_get_system_font(KST_BREIT ? FONT_KEY_GOTHIC_24 : FONT_KEY_GOTHIC_14);
}

/**
 * Die Entfernung als Zahl und Einheit.
 *
 * ZWEIGETEILT, WEIL LECO KEINE BUCHSTABEN HAT. Es hat ein Komma - nachgesehen,
 * nicht angenommen -, aber kein m und kein k. Die Zahl steht also in LECO, die
 * Einheit daneben in GOTHIC.
 */
static void prv_zerlege(int32_t meter, char *zahl, size_t zl, char *einheit, size_t el) {
  if (meter < 1000) {
    snprintf(zahl, zl, "%d", (int)meter);
    snprintf(einheit, el, "%s", "m");
  } else {
    const int32_t zehntel = (meter + 50) / 100;   // auf 100 m gerundet
    snprintf(zahl, zl, "%d,%d", (int)(zehntel / 10), (int)(zehntel % 10));
    snprintf(einheit, el, "%s", "km");
  }
}

/** Die Kopfzeile mit der Uhrzeit. Rueckgabe: wo es darunter weitergeht. */
static int16_t prv_kopf(GContext *ctx, GRect b) {
  char uhr[10];
  clock_copy_time_string(uhr, sizeof(uhr));
  const int16_t y = PBL_IF_ROUND_ELSE(16, 2);
  graphics_context_set_text_color(ctx, KST_FARBE_TEXT);
  graphics_draw_text(ctx, uhr, prv_font_uhr(),
                     GRect(KST_RAND, y, (int16_t)(b.size.w - 2 * KST_RAND), KOPF_H),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  const int16_t unter = (int16_t)(y + KOPF_H);
  graphics_context_set_fill_color(ctx, KST_FARBE_TEXT);
  graphics_fill_rect(ctx, GRect(KST_RAND, unter,
                                (int16_t)(b.size.w - 2 * KST_RAND), 2), 0, GCornerNone);
  return (int16_t)(unter + (KST_BREIT ? 8 : 5));
}

static void prv_zeichne(Layer *layer, GContext *ctx) {
  const GRect b = layer_get_bounds(layer);
  const Weg *w = weg_stand();
  const int16_t breite = (int16_t)(b.size.w - 2 * KST_RAND);

  graphics_context_set_text_color(ctx, KST_FARBE_TEXT);
  int16_t y = prv_kopf(ctx, b);

  // --- Kein gueltiger Stand ---
  // Die Uhr laedt beim Oeffnen den letzten Stand. Einer von gestern saehe aus
  // wie eine gueltige Anweisung, und man faehrt danach.
  if (weg_veraltet() || !weg_hat_zahl()) {
    if (!weg_veraltet() && pfeil_kennt(w->abbiegeart)) {
      // Kein Meterwert, aber eine Richtung: der Augenblick des Abbiegens.
      const int16_t ph = pfeil_hoehe();
      pfeil_zeichne(ctx, GPoint((int16_t)(b.size.w / 2), (int16_t)(y + ph / 2 + 6)),
                    w->abbiegeart);
      y = (int16_t)(y + ph + (KST_BREIT ? 18 : 12));
      if (w->strasse[0]) {
        graphics_draw_text(ctx, w->strasse, prv_font_name(),
                           GRect(KST_RAND, y, breite, (KST_BREIT ? 32 : 24) * 2),
                           GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
      }
      return;
    }
    y = (int16_t)(y + (KST_BREIT ? 20 : 12));
    graphics_draw_text(ctx, ST(STR_KEINE_NAVIGATION), prv_font_name(),
                       GRect(KST_RAND, y, breite, (KST_BREIT ? 32 : 24) * 2),
                       GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    char wann[28];
    weg_alter_text(wann, sizeof(wann));
    char zeile[40];
    snprintf(zeile, sizeof(zeile), ST(STR_ZULETZT), wann);
    graphics_context_set_text_color(ctx, KST_FARBE_ZART);
    graphics_draw_text(ctx, zeile, prv_font_zart(),
                       GRect(KST_RAND, (int16_t)(y + (KST_BREIT ? 66 : 48)), breite, 48),
                       GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    return;
  }

  // --- Pfeil und Zahl als Gruppe, mittig ---
  char zahl[12], einheit[4];
  prv_zerlege(w->entfernung_m, zahl, sizeof(zahl), einheit, sizeof(einheit));
  const GFont fz = prv_font_zahl();
  const GFont fzart = prv_font_zart();
  const GSize mz = graphics_text_layout_get_content_size(
      zahl, fz, GRect(0, 0, breite, 70), GTextOverflowModeTrailingEllipsis,
      GTextAlignmentLeft);
  const GSize me = graphics_text_layout_get_content_size(
      einheit, fzart, GRect(0, 0, breite, 34), GTextOverflowModeTrailingEllipsis,
      GTextAlignmentLeft);

  const bool mit_pfeil = pfeil_kennt(w->abbiegeart);
  const int16_t pb = mit_pfeil ? (int16_t)(pfeil_breite() + (KST_BREIT ? 12 : 7)) : 0;
  const int16_t gesamt = (int16_t)(pb + mz.w + 3 + me.w);
  int16_t x = (int16_t)((b.size.w - gesamt) / 2);
  if (x < KST_RAND) x = KST_RAND;

  if (mit_pfeil) {
    pfeil_zeichne(ctx, GPoint((int16_t)(x + pfeil_breite() / 2),
                              (int16_t)(y + mz.h / 2)), w->abbiegeart);
    x = (int16_t)(x + pb);
  }
  graphics_context_set_text_color(ctx, KST_FARBE_TEXT);
  graphics_draw_text(ctx, zahl, fz, GRect(x, y, (int16_t)(mz.w + 6), (int16_t)(mz.h + 6)),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, einheit, fzart,
                     GRect((int16_t)(x + mz.w + 3),
                           (int16_t)(y + mz.h - (KST_BREIT ? 30 : 18)),
                           (int16_t)(me.w + 6), 32),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  y = (int16_t)(y + mz.h + (KST_BREIT ? 6 : 3));

  // --- Balken ---
  const int proz = weg_balken_prozent();
  if (proz >= 0) {
    const int16_t voll = (int16_t)((int32_t)proz * breite / 100);
    graphics_context_set_fill_color(ctx, KST_FARBE_BALKEN_BG);
    graphics_fill_rect(ctx, GRect(KST_RAND, y, breite, BALKEN_H), 0, GCornerNone);
    graphics_context_set_fill_color(ctx, KST_FARBE_BALKEN);
    graphics_fill_rect(ctx, GRect(KST_RAND, y, voll, BALKEN_H), 0, GCornerNone);
    y = (int16_t)(y + BALKEN_H + (KST_BREIT ? 8 : 5));
  }

  // --- Wohin es geht ---
  // Der Platz bis zur unteren Zeile gehoert ihr ganz: ein Strassenname ist
  // laenger als ein Wort, und abgeschnitten nuetzt er nichts.
  const int16_t unten_h = (int16_t)(KST_BREIT ? 30 : 20);
  const int16_t platz = (int16_t)(b.size.h - y - unten_h - PBL_IF_ROUND_ELSE(16, 4));
  if (w->strasse[0] && platz > 10) {
    const GFont fn = prv_font_name();
    // MITTIG IM RESTLICHEN RAUM, nicht oben angeschlagen. Sonst klebt ein
    // einzeiliger Name unter dem Balken und darunter steht ein Loch bis zur
    // Fusszeile - und bei zwei Zeilen saehe es wieder anders aus. So sitzt der
    // Name immer an derselben Stelle, egal wie lang er ist.
    const GSize mn = graphics_text_layout_get_content_size(
        w->strasse, fn, GRect(0, 0, breite, platz),
        GTextOverflowModeWordWrap, GTextAlignmentCenter);
    int16_t oben = (int16_t)(y + (platz - mn.h) / 2);
    if (oben < y) oben = y;
    graphics_context_set_text_color(ctx, KST_FARBE_TEXT);
    graphics_draw_text(ctx, w->strasse, fn,
                       GRect(KST_RAND, oben, breite, platz),
                       GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  }

  // --- Was noch bleibt ---
  // Restweg und Ankunft in einer Zeile, leise und ganz unten. Sie sind kein
  // Fahrbefehl, sondern Auskunft - und sollen den Blick nicht von der Zahl
  // oben wegziehen.
  char rest[40] = "";
  if (w->rest_m > 0) {
    if (w->rest_m < 1000) snprintf(rest, sizeof(rest), "%d m", (int)w->rest_m);
    else snprintf(rest, sizeof(rest), "%d km", (int)((w->rest_m + 500) / 1000));
  }
  if (w->ankunft > 0) {
    struct tm *an = localtime(&w->ankunft);
    char hhmm[10];
    strftime(hhmm, sizeof(hhmm), clock_is_24h_style() ? "%H:%M" : "%I:%M", an);
    const size_t n = strlen(rest);
    if (n > 0) snprintf(rest + n, sizeof(rest) - n, "%s", " · ");
    const size_t m = strlen(rest);
    snprintf(rest + m, sizeof(rest) - m, ST(STR_ANKUNFT), hhmm);
  }
  if (rest[0]) {
    graphics_context_set_text_color(ctx, KST_FARBE_ZART);
    graphics_draw_text(ctx, rest, fzart,
                       GRect(KST_RAND,
                             (int16_t)(b.size.h - unten_h - PBL_IF_ROUND_ELSE(14, 2)),
                             breite, unten_h),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
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
  pfeil_init();
  tick_timer_service_subscribe(MINUTE_UNIT, prv_tick);
}

static void prv_unload(Window *window) {
  tick_timer_service_unsubscribe();
  pfeil_deinit();
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
