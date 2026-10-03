/* SPDX-FileCopyrightText: Dan Tilden (freakified) and the TimeStyle contributors */
/* SPDX-License-Identifier: MIT */
/* TimeStyle, https://github.com/freakified/TimeStylePebble (e2be725), LICENSE
 * in this folder. Boulder: ported into the firmware as a system watch face;
 * changes are marked "Boulder:". */

#pragma once
#include <pebble.h>
#include "time.h"


/*
 * For the specified GDrawCommandImage, recolors it with
 * the specified fill and stroke colors
 */
extern void gdraw_command_image_recolor(GDrawCommandImage *img, GColor fill_color, GColor stroke_color);

/*
 * Boulder: loads a PDC resource as a writable copy in the app heap.
 * System resources may be memory-mapped read-only flash, and recoloring
 * writes into the image.
 */
extern GDrawCommandImage *ts_pdc_laden(uint32_t resource_id);

/*
 * Returns the current time in Swatch Internet Time "beats"
 */
extern int time_get_beats(const struct tm *tm);

#ifdef PBL_HEALTH
  /*
   * Checks if any of the specified health activites exist in the specified time range
   */
  extern bool is_health_metric_accessible(HealthMetric metric);

  /*
   * Returns true if the user is sleeping, false otherwise
   */
  extern bool is_user_sleeping();
#endif
