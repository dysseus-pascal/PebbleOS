/* SPDX-FileCopyrightText: Dan Tilden (freakified) and the TimeStyle contributors */
/* SPDX-License-Identifier: MIT */
/* TimeStyle, https://github.com/freakified/TimeStylePebble (e2be725), LICENSE
 * in this folder. Boulder: ported into the firmware as a system watch face;
 * changes are marked "Boulder:". */

#ifndef PBL_PLATFORM_APLITE
#include <pebble.h>

#include "clock_area.h"
#include "settings.h"
#include "schriftwahl.h"
#include "sidebar.h"

#include <pebble-fctx/fctx.h>
#include <pebble-fctx/ffont.h>
// Boulder: fpath is not used and not taken over.

#define ROUND_VERTICAL_PADDING 15

char time_hours[3];
char time_minutes[3];

Layer* clock_area_layer;
FFont* hours_font;
FFont* minutes_font;

// just allocate all the fonts at startup because i don't feel like
// dealing with allocating and deallocating things
// Boulder: Kumbh Sans (SIL OFL) instead of Avenir Next (a commercial typeface,
// not taken over) - the free replacement for "Default", "Bold", "Bold Hour"
// and "Bold Minute", built to Avenir's digit height and width so the clock
// keeps the original layout; see resources/normal/base/timestyle/LICENSE.
FFont* avenir;
FFont* avenir_bold;
FFont* leco;

// Boulder: static - sidebar.c has its own screen_rect (no -fcommon here).
static GRect screen_rect;

// "private" functions
// Boulder: die Zuordnung steht in schriftwahl.c (host-getestet); hier nur,
// welche geladene Schrift dazu gehoert.
static FFont *prv_schrift(TsSchrift schrift) {
  switch (schrift) {
    case TS_SCHRIFT_SANS:
      return avenir;
    case TS_SCHRIFT_SANS_FETT:
      return avenir_bold;
    case TS_SCHRIFT_LECO:
    default:
      return leco;
  }
}

void update_fonts() {
  TsSchrift stunden;
  TsSchrift minuten;
  ts_schriftwahl(settings.clockFontId, &stunden, &minuten);
  hours_font = prv_schrift(stunden);
  minutes_font = prv_schrift(minuten);
  // Boulder: a font that did not load falls back to LECO (and LECO itself is
  // checked before drawing) - a missing resource must not crash the face.
  if (!hours_font) {
    hours_font = leco;
  }
  if (!minutes_font) {
    minutes_font = leco;
  }
}

void update_clock_area_layer(Layer *l, GContext* ctx) {
  // check layer bounds
  GRect bounds = layer_get_unobstructed_bounds(l);

  #if PBL_ROUND  // Boulder: the firmware defines PBL_ROUND as 0 or 1
    bounds = GRect(0, ROUND_VERTICAL_PADDING, screen_rect.size.w, screen_rect.size.h - ROUND_VERTICAL_PADDING * 2);
  #endif

  // Boulder: choose the renderer BEFORE fctx_init_context. Upstream switches
  // after it, so the first frame allocated the 8-bit flag buffer of the
  // antialiased renderer (200x228 bytes) just to draw LECO without it.
  #if PBL_COLOR
    fctx_enable_aa(settings.clockFontId != FONT_SETTING_LECO);
  #endif

  // initialize FCTX, the fancy 3rd party drawing library that all the cool kids use
  FContext fctx;
  // Boulder: zeroed - fctx_init_context leaves it untouched without a frame buffer.
  memset(&fctx, 0, sizeof(fctx));

  fctx_init_context(&fctx, ctx);
  // Boulder: no frame buffer or no flag buffer - nothing to draw into. The
  // antialiased renderer needs a flag buffer of 200x228 bytes in the app heap;
  // if that ever fails, say so instead of silently showing no time.
  if (!fctx.gctx || !fctx.flag_buffer || !hours_font || !minutes_font) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "TimeStyle: no clock buffer or font (aa %d, heap free %d)",
            (int)fctx_is_aa_enabled(), (int)heap_bytes_free());
    fctx_deinit_context(&fctx);
    return;
  }
  fctx_set_color_bias(&fctx, 0);
  fctx_set_fill_color(&fctx, settings.timeColor);


  // calculate font size
  int font_size = 4 * bounds.size.h / 7;

  // avenir + avenir bold metrics
  int v_padding = bounds.size.h / 16;
  int h_adjust = 0;
  int v_adjust = 0;

  // alternate metrics for LECO
  if(settings.clockFontId == FONT_SETTING_LECO) {
    font_size = 4 * bounds.size.h / 7 + 6;
    v_padding = bounds.size.h / 20;
    h_adjust = -4;
    v_adjust = 0;

    // leco looks awful with antialiasing
    // Boulder: fctx_enable_aa moved before fctx_init_context (see above).
  }

  // if it's a round watch, EVERYTHING CHANGES
  #if PBL_ROUND  // Boulder: #if, see above
    v_adjust = ROUND_VERTICAL_PADDING;

    if(settings.clockFontId != FONT_SETTING_LECO) {
      h_adjust = -1;
    }
  #else
    // for rectangular watches, adjust X position based on sidebar position
    if(settings.sidebarOnLeft) {
      h_adjust += sidebarWidth / 2;
    } else {
      h_adjust -= sidebarWidth / 2 + 1;
    }
  #endif

  FPoint time_pos;
  fctx_begin_fill(&fctx);
  fctx_set_text_em_height(&fctx, hours_font, font_size);
  fctx_set_text_em_height(&fctx, minutes_font, font_size);

  // draw hours
  time_pos.x = INT_TO_FIXED(bounds.size.w / 2 + h_adjust);
  time_pos.y = INT_TO_FIXED(v_padding + v_adjust);
  fctx_set_offset(&fctx, time_pos);
  fctx_draw_string(&fctx, time_hours, hours_font, GTextAlignmentCenter, FTextAnchorTop);

  //draw minutes
  time_pos.y = INT_TO_FIXED(bounds.size.h - v_padding + v_adjust);
  fctx_set_offset(&fctx, time_pos);
  fctx_draw_string(&fctx, time_minutes, minutes_font, GTextAlignmentCenter, FTextAnchorBaseline);
  fctx_end_fill(&fctx);

  fctx_deinit_context(&fctx);
}


void ClockArea_init(Window* window) {
  // record the screen size, since we NEVER GET IT AGAIN
  screen_rect = layer_get_bounds(window_get_root_layer(window));

  GRect bounds;
  bounds = GRect(0, 0, screen_rect.size.w, screen_rect.size.h);

  // init the clock area layer
  clock_area_layer = layer_create(bounds);
  layer_add_child(window_get_root_layer(window), clock_area_layer);
  layer_set_update_proc(clock_area_layer, update_clock_area_layer);

  // allocate fonts
  avenir =      ffont_create_from_resource(RESOURCE_ID_SANS_REGULAR_FFONT);
  avenir_bold = ffont_create_from_resource(RESOURCE_ID_SANS_BOLD_FFONT);
  leco =        ffont_create_from_resource(RESOURCE_ID_LECO_REGULAR_FFONT);

  // select fonts based on settings
  update_fonts();
}

void ClockArea_deinit() {
  layer_destroy(clock_area_layer);

  ffont_destroy(avenir);
  ffont_destroy(avenir_bold);
  ffont_destroy(leco);
  // Boulder: a system app keeps its statics after it ends.
  avenir = NULL;
  avenir_bold = NULL;
  leco = NULL;
  hours_font = NULL;
  minutes_font = NULL;
  clock_area_layer = NULL;
}

void ClockArea_redraw() {
  // check if the fonts need to be switched
  update_fonts();

  layer_mark_dirty(clock_area_layer);
}

void ClockArea_update_time(struct tm* time_info) {

  // hours
  if (clock_is_24h_style()) {
    strftime(time_hours, sizeof(time_hours), (settings.showLeadingZero) ? "%H" : "%k", time_info);
  } else {
    strftime(time_hours, sizeof(time_hours), (settings.showLeadingZero) ? "%I" : "%l", time_info);
  }

  // minutes
  strftime(time_minutes, sizeof(time_minutes), "%M", time_info);
}

#endif