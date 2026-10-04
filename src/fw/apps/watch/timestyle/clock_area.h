/* SPDX-FileCopyrightText: Dan Tilden (freakified) and the TimeStyle contributors */
/* SPDX-License-Identifier: MIT */
/* TimeStyle, https://github.com/freakified/TimeStylePebble (e2be725), LICENSE
 * in this folder. Boulder: ported into the firmware as a system watch face;
 * changes are marked "Boulder:". */

#pragma once
#include <pebble.h>

// "public" functions
void ClockArea_init(Window* window);
void ClockArea_deinit();
void ClockArea_redraw();
void ClockArea_update_time(struct tm* time_info);

// Boulder: das Zeichnen der Uhrzeit (die update_proc der Schicht) - und fuer
// den Host-Test, wie oft fctx dafuer rechnete und wie gross die Kopie ist.
void update_clock_area_layer(Layer *l, GContext* ctx);
uint32_t ClockArea_gerechnet(void);
size_t ClockArea_kopie_bytes(void);
