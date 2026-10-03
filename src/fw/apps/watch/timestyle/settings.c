/* SPDX-FileCopyrightText: Dan Tilden (freakified) and the TimeStyle contributors */
/* SPDX-License-Identifier: MIT */
/* TimeStyle, https://github.com/freakified/TimeStylePebble (e2be725), LICENSE
 * in this folder. Boulder: ported into the firmware as a system watch face;
 * changes are marked "Boulder:". */

#include "settings.h"
#include "timestyle.h"  // Boulder: the value from Boulder
#include "ts_format.h"  // Boulder: its layout and the defaults
#include <pebble.h>

Settings settings;
DynamicSettings dynamicSettings;

void Settings_init() {
  // Boulder: no persisted settings and no emery font migration - the
  // configuration comes from Boulder (see Settings_loadFromStorage).
  Settings_loadFromStorage();
}

void Settings_deinit() {}

void Settings_loadFromStorage() {
  // Boulder: DIE EINSTELLUNGEN KOMMEN AUS BOULDER - ein Wert von 32 Byte in
  // den Uhreinstellungen (prefs.c), statt Persist und Konfigseite. Fehlt er
  // (alter Boulder) oder ist er ungueltig, gelten die Vorgaben vom Foto des
  // Nutzers (ts_format_vorgaben). Eine System-App behaelt ihre statischen
  // Variablen ueber ihr Ende hinaus - darum zuerst alles auf null.
  memset(&settings, 0, sizeof(settings));

  uint8_t wert[TS_BEDROCK_PREF_LAENGE];
  timestyle_bedrock_blob_lesen(wert);
  ts_format_dekodieren(wert, sizeof(wert), &settings);

  Settings_updateDynamicSettings();
}

void Settings_saveToStorage() {
  // Boulder: nothing to store - only Boulder writes the settings.
  Settings_updateDynamicSettings();
}

void Settings_updateDynamicSettings() {
  dynamicSettings.disableWeather = true;
  dynamicSettings.updateScreenEverySecond = false;
  dynamicSettings.enableAutoBatteryWidget = true;
  dynamicSettings.enableBeats = false;
  dynamicSettings.enableAltTimeZone = false;

  for (int i = 0; i < 3; i++) {
    // if there are any weather widgets, enable weather checking
    if (settings.widgets[i] == WEATHER_CURRENT ||
        settings.widgets[i] == WEATHER_FORECAST_TODAY ||
        settings.widgets[i] == WEATHER_UV_INDEX) {
      dynamicSettings.disableWeather = false;
    }

    // if any widget is "seconds", we'll need to update the sidebar every second
    if (settings.widgets[i] == SECONDS) {
      dynamicSettings.updateScreenEverySecond = true;
    }

    // if any widget is "battery", disable the automatic battery indication
    if (settings.widgets[i] == BATTERY_METER) {
      dynamicSettings.enableAutoBatteryWidget = false;
    }

    // if any widget is "beats", enable the beats calculation
    if (settings.widgets[i] == BEATS) {
      dynamicSettings.enableBeats = true;
    }

    // if any widget is "alt_time_zone", enable the alternative time calculation
    if (settings.widgets[i] == ALT_TIME_ZONE) {
      dynamicSettings.enableAltTimeZone = true;
    }
  }

  // if the sidebar is black, use inverted colors for icons
  if (gcolor_equal(settings.sidebarColor, GColorBlack)) {
    dynamicSettings.iconFillColor = GColorBlack;
    dynamicSettings.iconStrokeColor = settings.sidebarTextColor;
  } else {
    dynamicSettings.iconFillColor = GColorWhite;
    dynamicSettings.iconStrokeColor = GColorBlack;
  }
}
