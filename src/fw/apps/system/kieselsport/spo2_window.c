/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>
#include "spo2_window.h"
#include "telefon.h"
#include "thema.h"
#include "texte.h"

// So lange wartet der Schirm auf Kiesel-Helper. Die Antwort braucht eine
// Abfrage in Health Connect - eine Sekunde, selten drei.
#define SPO2_WARTEN_MS 10000

typedef enum { Spo2Fragt, Spo2Da, Spo2Keiner, Spo2KeinTelefon } Spo2Phase;

static Window *s_fenster;
static Layer *s_flaeche;
static AppTimer *s_warten;
static AppTimer *s_takt;
static Spo2Phase s_phase;
static Spo2Antwort s_antwort;

static void prv_text(GContext *ctx, const char *text, const char *schrift, GRect wo, GTextAlignment wie) {
  graphics_draw_text(ctx, text, fonts_get_system_font(schrift), wo,
                     GTextOverflowModeWordWrap, wie, NULL);
}

static void prv_zeichne(Layer *layer, GContext *ctx) {
  const GRect b = layer_get_unobstructed_bounds(layer);
  const int16_t w = b.size.w;
  const int16_t rand = PBL_IF_ROUND_ELSE(34, 6);
  const int16_t breite = w - 2 * rand;
  const char *gross = KS_BREIT ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD;
  int16_t y = PBL_IF_ROUND_ELSE(34, 10);
  char zeile[48];

  graphics_context_set_text_color(ctx, KS_FARBE_TEXT);
  prv_text(ctx, "SpO2", gross, GRect(rand, y - 4, breite, 30), GTextAlignmentCenter);
  y += KS_BREIT ? 30 : 24;

  switch (s_phase) {
    case Spo2Fragt:
      graphics_context_set_text_color(ctx, KS_FARBE_NEBEN);
      prv_text(ctx, S(STR_SPO2_FRAGT), gross, GRect(rand, b.size.h / 2 - 16, breite, 60), GTextAlignmentCenter);
      break;
    case Spo2KeinTelefon:
      prv_text(ctx, S(STR_SPO2_KEIN_TEL), gross, GRect(rand, b.size.h / 2 - 16, breite, 60), GTextAlignmentCenter);
      break;
    case Spo2Keiner:
      prv_text(ctx, S(STR_SPO2_KEINER), gross, GRect(rand, y + 10, breite, 110), GTextAlignmentCenter);
      break;
    case Spo2Da: {
      const int16_t zh = KS_BREIT ? 50 : 42;
      snprintf(zeile, sizeof(zeile), "%u", (unsigned)s_antwort.wert);
      const GFont zahl = fonts_get_system_font(KS_BREIT ? FONT_KEY_LECO_42_NUMBERS : FONT_KEY_LECO_36_BOLD_NUMBERS);
      const GFont prozent = fonts_get_system_font(gross);
      const int16_t zb = graphics_text_layout_get_content_size(zeile, zahl, GRect(0, 0, 200, 60),
                                                               GTextOverflowModeWordWrap, GTextAlignmentLeft).w;
      const int16_t pb = graphics_text_layout_get_content_size("%", prozent, GRect(0, 0, 60, 30),
                                                               GTextOverflowModeWordWrap, GTextAlignmentLeft).w;
      const int16_t x = (w - zb - 4 - pb) / 2;
      graphics_draw_text(ctx, zeile, zahl, GRect(x, y, zb + 4, zh), GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
      graphics_draw_text(ctx, "%", prozent, GRect(x + zb + 4, y + zh - (KS_BREIT ? 30 : 24), pb + 4, 30),
                         GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
      y += zh + 2;

      // Wie alt: ein Wert von heute Nacht ist eine andere Auskunft als einer
      // von eben.
      const int32_t alter = (int32_t)(time(NULL) - (time_t)s_antwort.zeit);
      if (alter < 3600) snprintf(zeile, sizeof(zeile), S(STR_SPO2_VOR_MIN), (unsigned)(alter > 0 ? alter / 60 : 0));
      else snprintf(zeile, sizeof(zeile), S(STR_SPO2_VOR_H), (unsigned)(alter / 3600));
      graphics_context_set_text_color(ctx, KS_FARBE_NEBEN);
      prv_text(ctx, zeile, FONT_KEY_GOTHIC_18, GRect(rand, y, breite, 22), GTextAlignmentCenter);
      y += 26;

      if (s_antwort.nacht_mittel > 0) {
        graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorLightGray, GColorBlack));
        graphics_draw_line(ctx, GPoint(rand + 10, y), GPoint(w - rand - 10, y));
        y += 4;
        graphics_context_set_text_color(ctx, KS_FARBE_TEXT);
        prv_text(ctx, S(STR_SPO2_NACHT), FONT_KEY_GOTHIC_14_BOLD, GRect(rand, y, breite, 18), GTextAlignmentCenter);
        y += 16;
        snprintf(zeile, sizeof(zeile), S(STR_SPO2_NACHT_WERTE),
                 (unsigned)s_antwort.nacht_mittel, (unsigned)s_antwort.nacht_tief);
        prv_text(ctx, zeile, FONT_KEY_GOTHIC_18_BOLD, GRect(rand, y, breite, 22), GTextAlignmentCenter);
      }
      break;
    }
  }
}

static void prv_antwort(const Spo2Antwort *a) {
  if (!s_fenster) return;
  if (s_warten) { app_timer_cancel(s_warten); s_warten = NULL; }
  s_antwort = *a;
  s_phase = a->wert > 0 ? Spo2Da : Spo2Keiner;
  if (s_flaeche) layer_mark_dirty(s_flaeche);
}

static void prv_zu_lange(void *data) {
  s_warten = NULL;
  s_phase = Spo2KeinTelefon;
  if (s_flaeche) layer_mark_dirty(s_flaeche);
}

// Jede Minute neu zeichnen: "vor 3 min" soll nicht stehen bleiben.
static void prv_takt(void *data) {
  s_takt = app_timer_register(60000, prv_takt, NULL);
  if (s_flaeche) layer_mark_dirty(s_flaeche);
}

static void prv_fragen(void) {
  s_phase = Spo2Fragt;
  if (s_flaeche) layer_mark_dirty(s_flaeche);
  if (!connection_service_peek_pebble_app_connection()) {
    s_phase = Spo2KeinTelefon;
    return;
  }
  telefon_spo2_frage(prv_antwort);
  if (s_warten) app_timer_cancel(s_warten);
  s_warten = app_timer_register(SPO2_WARTEN_MS, prv_zu_lange, NULL);
}

// Select fragt noch einmal.
static void prv_select(ClickRecognizerRef anlass, void *context) { prv_fragen(); }

static void prv_tasten(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select);
}

static void prv_laden(Window *fenster) {
  Layer *wurzel = window_get_root_layer(fenster);
  s_flaeche = layer_create(layer_get_bounds(wurzel));
  layer_set_update_proc(s_flaeche, prv_zeichne);
  layer_add_child(wurzel, s_flaeche);
}

static void prv_entladen(Window *fenster) {
  telefon_spo2_frage(NULL);
  if (s_warten) { app_timer_cancel(s_warten); s_warten = NULL; }
  if (s_takt) { app_timer_cancel(s_takt); s_takt = NULL; }
  layer_destroy(s_flaeche);
  s_flaeche = NULL;
  window_destroy(s_fenster);
  s_fenster = NULL;
}

void spo2_window_zeige(void) {
  memset(&s_antwort, 0, sizeof(s_antwort));
  s_fenster = window_create();
  window_set_background_color(s_fenster, KS_FARBE_GRUND);
  window_set_click_config_provider(s_fenster, prv_tasten);
  window_set_window_handlers(s_fenster, (WindowHandlers) { .load = prv_laden, .unload = prv_entladen });
  window_stack_push(s_fenster, true);
  s_takt = app_timer_register(60000, prv_takt, NULL);
  prv_fragen();
}

void spo2_window_zuruecksetzen(void) {
  memset(&s_fenster, 0, sizeof(s_fenster));
  memset(&s_flaeche, 0, sizeof(s_flaeche));
  memset(&s_warten, 0, sizeof(s_warten));
  memset(&s_takt, 0, sizeof(s_takt));
  memset(&s_phase, 0, sizeof(s_phase));
  memset(&s_antwort, 0, sizeof(s_antwort));
}
