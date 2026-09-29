/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>
#include "hrv_window.h"
#include "telefon.h"
#include "thema.h"
#include "texte.h"

// Eine Minute: lang genug fuer gut sechzig Intervalle, kurz genug, um still
// zu sitzen. Dieselbe Laenge wie bisher in Herzintervall - so bleiben die
// Werte vergleichbar.
#define HRV_DAUER_S 60

typedef enum { HrvBereit, HrvMisst, HrvFertig } HrvPhase;

static Window *s_fenster;
static Layer *s_flaeche;
static AppTimer *s_takt;
static HrvPhase s_phase;
static int s_rest;
static uint16_t s_ergebnis;

static uint16_t prv_anzahl(void) {
  uint16_t rmssd, anzahl;
  kieselsport_hrv_stand(&rmssd, &anzahl);
  return anzahl;
}

static void prv_text(GContext *ctx, const char *text, const char *schrift, GRect wo, GTextAlignment wie) {
  graphics_draw_text(ctx, text, fonts_get_system_font(schrift), wo,
                     GTextOverflowModeWordWrap, wie, NULL);
}

static void prv_zeichne(Layer *layer, GContext *ctx) {
  const GRect b = layer_get_unobstructed_bounds(layer);
  const int16_t w = b.size.w - KS_LEISTE_B;
  const int16_t rand = PBL_IF_ROUND_ELSE(44, KS_RAND);
  const int16_t breite = w - rand - 4;
  int16_t y = PBL_IF_ROUND_ELSE(40, 18);
  char zahl[16];

  graphics_context_set_text_color(ctx, KS_FARBE_TEXT);
  prv_text(ctx, "HRV", FONT_KEY_GOTHIC_18_BOLD, GRect(rand, y - 2, breite, 20), GTextAlignmentLeft);
  y += 20;

  switch (s_phase) {
    case HrvBereit:
      prv_text(ctx, S(STR_HRV_BEREIT), KS_BREIT ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD,
               GRect(rand, y, breite, 90), GTextAlignmentLeft);
      break;
    case HrvMisst:
      snprintf(zahl, sizeof(zahl), "0:%02d", s_rest);
      prv_text(ctx, zahl, KS_BREIT ? FONT_KEY_LECO_42_NUMBERS : FONT_KEY_LECO_36_BOLD_NUMBERS,
               GRect(rand, y - 6, breite, 50), GTextAlignmentLeft);
      y += KS_BREIT ? 46 : 40;
      graphics_context_set_text_color(ctx, KS_FARBE_NEBEN);
      snprintf(zahl, sizeof(zahl), S(STR_HRV_SCHLAEGE), (unsigned)prv_anzahl());
      prv_text(ctx, zahl, FONT_KEY_GOTHIC_18, GRect(rand, y, breite, 22), GTextAlignmentLeft);
      y += 22;
      prv_text(ctx, S(STR_HRV_STILL), FONT_KEY_GOTHIC_14, GRect(rand, y, breite, 40), GTextAlignmentLeft);
      break;
    case HrvFertig:
      if (s_ergebnis > 0) {
        snprintf(zahl, sizeof(zahl), "%u", (unsigned)s_ergebnis);
        prv_text(ctx, zahl, KS_BREIT ? FONT_KEY_LECO_42_NUMBERS : FONT_KEY_LECO_36_BOLD_NUMBERS,
                 GRect(rand, y - 6, breite, 50), GTextAlignmentLeft);
        prv_text(ctx, "ms RMSSD", FONT_KEY_GOTHIC_18_BOLD,
                 GRect(rand, y + (KS_BREIT ? 42 : 36), breite, 22), GTextAlignmentLeft);
        y += KS_BREIT ? 68 : 60;
        graphics_context_set_text_color(ctx, KS_FARBE_NEBEN);
        prv_text(ctx, telefon_hrv_offen() ? S(STR_TEL_WARTET) : S(STR_TEL_ANGEKOMMEN),
                 FONT_KEY_GOTHIC_14, GRect(rand, y, breite, 34), GTextAlignmentLeft);
      } else {
        prv_text(ctx, S(STR_HRV_ZUWENIG), KS_BREIT ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD,
                 GRect(rand, y, breite, 90), GTextAlignmentLeft);
      }
      break;
  }

  thema_leiste(ctx, b, SymbolKeins,
               s_phase == HrvMisst ? SymbolKeins : SymbolStart, SymbolKeins);
}

static void prv_aus(void) {
  kieselsport_hrv_messen(false);
}

static void prv_takt(void *data) {
  s_takt = NULL;
  if (s_phase == HrvMisst) {
    if (--s_rest <= 0) {
      uint16_t anzahl;
      kieselsport_hrv_stand(&s_ergebnis, &anzahl);
      prv_aus();
      s_phase = HrvFertig;
      if (s_ergebnis > 0) telefon_hrv(s_ergebnis, time(NULL));
      vibes_short_pulse();
    }
  }
  if (s_flaeche) layer_mark_dirty(s_flaeche);
  // Nach dem Ergebnis nur noch nachsehen, ob das Telefon es hat.
  if (s_phase == HrvMisst || (s_phase == HrvFertig && telefon_hrv_offen())) {
    s_takt = app_timer_register(1000, prv_takt, NULL);
  }
}

static void prv_select(ClickRecognizerRef anlass, void *context) {
  if (s_phase == HrvMisst) return;
  // Waehrend eines Trainings gehoert der Sensor dem Training.
  if (kieselsport_laeuft()) return;
  kieselsport_hrv_messen(true);
  s_phase = HrvMisst;
  s_rest = HRV_DAUER_S;
  s_ergebnis = 0;
  if (s_takt) app_timer_cancel(s_takt);
  s_takt = app_timer_register(1000, prv_takt, NULL);
  layer_mark_dirty(s_flaeche);
}

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
  // ZURUECK MITTEN IN DER MESSUNG bricht sie ab - und schaltet den Sensor
  // wieder auf normal. Das ist die Zeile, die man sonst vergisst.
  if (s_phase == HrvMisst) prv_aus();
  if (s_takt) { app_timer_cancel(s_takt); s_takt = NULL; }
  layer_destroy(s_flaeche);
  s_flaeche = NULL;
  window_destroy(s_fenster);
  s_fenster = NULL;
}

void hrv_window_zeige(void) {
  s_phase = HrvBereit;
  s_ergebnis = 0;
  s_fenster = window_create();
  window_set_background_color(s_fenster, KS_FARBE_GRUND);
  window_set_click_config_provider(s_fenster, prv_tasten);
  window_set_window_handlers(s_fenster, (WindowHandlers) { .load = prv_laden, .unload = prv_entladen });
  window_stack_push(s_fenster, true);
}

void hrv_window_zuruecksetzen(void) {
  memset(&s_fenster, 0, sizeof(s_fenster));
  memset(&s_flaeche, 0, sizeof(s_flaeche));
  memset(&s_takt, 0, sizeof(s_takt));
  memset(&s_phase, 0, sizeof(s_phase));
  memset(&s_rest, 0, sizeof(s_rest));
  memset(&s_ergebnis, 0, sizeof(s_ergebnis));
}
