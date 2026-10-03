/* SPDX-FileCopyrightText: Dan Tilden (freakified) and the TimeStyle contributors */
/* SPDX-License-Identifier: MIT */
/* TimeStyle, https://github.com/freakified/TimeStylePebble (e2be725), LICENSE
 * in this folder. Boulder: ported into the firmware as a system watch face;
 * changes are marked "Boulder:". */

#include <pebble.h>
#include "util.h"


bool recolor_iterator_cb(GDrawCommand *command, uint32_t index, void *context) {
  GColor *colors = (GColor *)context;

  gdraw_command_set_fill_color(command, colors[0]);
  gdraw_command_set_stroke_color(command, colors[1]);

  return true;
}

void gdraw_command_image_recolor(GDrawCommandImage *img, GColor fill_color, GColor stroke_color) {
  GColor colors[2];
  colors[0] = fill_color;
  colors[1] = stroke_color;

  gdraw_command_list_iterate(gdraw_command_image_get_command_list(img),
                             recolor_iterator_cb, &colors);
}

// Boulder: Ressourcen einer System-App koennen direkt im Flash liegen (nur
// lesbar, siehe applib_resource_mmap_or_load); das Umfaerben schreibt aber in
// das Bild. Darum eine Kopie im Speicher der App, wie die Wetter-App es macht.
GDrawCommandImage *ts_pdc_laden(uint32_t resource_id) {
  GDrawCommandImage *roh = gdraw_command_image_create_with_resource(resource_id);
  if (!roh) {
    return NULL;
  }
  GDrawCommandImage *kopie = gdraw_command_image_clone(roh);
  gdraw_command_image_destroy(roh);
  return kopie;
}

int time_get_beats(const struct tm *tm) {
  // get utc time
  time_t utc_time = mktime((struct tm *)tm);

  // convert to "biel mean time" (BMT) by adding an hour
  time_t bmt_time = utc_time + 3600;

  // use gmtime to get the BMT components
  struct tm *bmt_tm = gmtime(&bmt_time);

  // how many seconds have passed since midnight?
  int seconds_since_midnight = bmt_tm->tm_hour * 3600 + bmt_tm->tm_min * 60 + bmt_tm->tm_sec;
  int beats = (seconds_since_midnight * 1000 + 43200) / 86400;

  return beats % 1000;
}

#ifdef PBL_HEALTH
bool is_health_metric_accessible(HealthMetric metric) {
  time_t start = time_start_of_today();
  time_t end = time(NULL);

  // Check step data is available
     HealthServiceAccessibilityMask mask = health_service_metric_accessible(metric, start, end);
  bool result = mask & HealthServiceAccessibilityMaskAvailable;

  return result;
}

bool is_user_sleeping() {
  // currently disabled
  return false;
  //
  //  time_t now = time(NULL);
  //
    //  HealthActivityMask activities = HealthActivitySleep | HealthActivityRestfulSleep;
  //
    //  HealthServiceAccessibilityMask mask = health_service_any_activity_accessible(activities, now, now);
    //  bool sleep_access_available = mask & HealthServiceAccessibilityMaskAvailable;
  //
  //  if(sleep_access_available) {
  //    uint32_t current_activities = health_service_peek_current_activities();
    //    bool sleeping = current_activities & HealthActivitySleep || current_activities & HealthActivityRestfulSleep;
  //
  //    return sleeping;
  //  } else {
  //    return false;
  //  }
}
#endif
