/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "messen_window.h"
#include "hrv_window.h"
#include "spo2_window.h"
#include "symbole.h"
#include "thema.h"
#include "texte.h"

static Window *s_fenster;
static MenuLayer *s_liste;

static uint16_t prv_zeilen(MenuLayer *liste, uint16_t abschnitt, void *ctx) { return 2; }

static int16_t prv_hoehe(MenuLayer *liste, MenuIndex *index, void *ctx) {
  return PBL_IF_ROUND_ELSE(60, KS_BREIT ? 56 : 46);
}

static void prv_zeichne_zeile(GContext *ctx, const Layer *zelle, MenuIndex *index, void *daten) {
  const GRect b = layer_get_bounds(zelle);
  const bool hell = menu_cell_layer_is_highlighted(zelle);
  const GColor farbe = hell ? KS_FARBE_AUF_LEISTE : KS_FARBE_TEXT;
  const int16_t g = KS_BREIT ? 32 : 26;
  const int16_t links = PBL_IF_ROUND_ELSE(30, 6);
  symbol_herz(ctx, GPoint(links + g / 2, b.size.h / 2), g, farbe);

  const int16_t tx = links + g + 8;
  const bool hrv = index->row == 0;
  graphics_context_set_text_color(ctx, farbe);
  const int16_t nh = KS_BREIT ? 28 : 22;
  graphics_draw_text(ctx, hrv ? "HRV" : "SpO2",
                     fonts_get_system_font(KS_BREIT ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD),
                     GRect(tx, b.size.h / 2 - nh + 4, b.size.w - tx - 2, nh),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, hrv ? S(STR_HRV_UNTER) : S(STR_SPO2_UNTER), fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(tx, b.size.h / 2 + 2, b.size.w - tx - 2, 18),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void prv_gewaehlt(MenuLayer *liste, MenuIndex *index, void *daten) {
  if (index->row == 0) hrv_window_zeige();
  else spo2_window_zeige();
}

static void prv_laden(Window *fenster) {
  Layer *wurzel = window_get_root_layer(fenster);
  s_liste = menu_layer_create(layer_get_bounds(wurzel));
  menu_layer_set_callbacks(s_liste, NULL, (MenuLayerCallbacks) {
    .get_num_rows = prv_zeilen,
    .get_cell_height = prv_hoehe,
    .draw_row = prv_zeichne_zeile,
    .select_click = prv_gewaehlt,
  });
  menu_layer_set_normal_colors(s_liste, KS_FARBE_GRUND, KS_FARBE_TEXT);
  menu_layer_set_highlight_colors(s_liste, KS_FARBE_LEISTE, KS_FARBE_AUF_LEISTE);
  menu_layer_set_click_config_onto_window(s_liste, fenster);
  layer_add_child(wurzel, menu_layer_get_layer(s_liste));
}

static void prv_entladen(Window *fenster) {
  menu_layer_destroy(s_liste);
  s_liste = NULL;
  window_destroy(s_fenster);
  s_fenster = NULL;
}

void messen_window_zeige(void) {
  s_fenster = window_create();
  window_set_background_color(s_fenster, KS_FARBE_GRUND);
  window_set_window_handlers(s_fenster, (WindowHandlers) { .load = prv_laden, .unload = prv_entladen });
  window_stack_push(s_fenster, true);
}
