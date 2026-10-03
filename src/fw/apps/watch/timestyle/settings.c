/* SPDX-FileCopyrightText: Dan Tilden (freakified) and the TimeStyle contributors */
/* SPDX-License-Identifier: MIT */
/* TimeStyle, https://github.com/freakified/TimeStylePebble (e2be725), LICENSE
 * in this folder. Boulder: ported into the firmware as a system watch face;
 * changes are marked "Boulder:". */

#include "settings.h"
#include "languages.h"  // Boulder: LANGUAGE_EN for the fixed settings
#include <pebble.h>

Settings settings;
DynamicSettings dynamicSettings;

void Settings_init() {
  // Boulder: no persisted settings and no emery font migration - the
  // configuration is fixed (see Settings_loadFromStorage).
  Settings_loadFromStorage();
}

void Settings_deinit() {}

void Settings_loadFromStorage() {
  // Boulder: FESTE EINSTELLUNGEN statt Persist und Konfigseite. Sie ergeben
  // das Zifferblatt vom Foto des Nutzers (Pebble Time 2, 03.10.2026):
  // schwarzer Grund, weisse LECO-Ziffern, weisse Leiste rechts mit schwarzer
  // Schrift; oben das aktuelle Wetter, in der Mitte die Schritte, unten
  // Wochentag, Kalenderblatt und Monat, Englisch. Der Rest sind TimeStyles
  // Vorgaben. Eine System-App behaelt ihre statischen Variablen ueber ihr
  // Ende hinaus - darum zuerst alles auf null.
  memset(&settings, 0, sizeof(settings));

  settings.timeColor = GColorWhite;
  settings.timeBgColor = GColorBlack;
  settings.sidebarColor = GColorWhite;
  settings.sidebarTextColor = GColorBlack;

  settings.languageId = LANGUAGE_EN;
  settings.showLeadingZero = false;
  settings.clockFontId = FONT_SETTING_LECO;

  settings.btVibe = false;
  settings.hourlyVibe = NO_VIBE;

  settings.widgets[0] = WEATHER_CURRENT;
  settings.widgets[1] = STEP_COUNTER;
  settings.widgets[2] = DATE;
  settings.sidebarOnLeft = false;
  settings.useLargeFonts = false;
  settings.activateDisconnectIcon = true;

  // Boulder: not read - the phone already sends temperatures in the user's unit.
  settings.useMetric = true;

  settings.showBatteryPct = true;
  settings.disableAutobattery = false;

  strncpy(settings.altclockName, "ALT", sizeof(settings.altclockName));
  settings.altclockOffset = 0;

  settings.healthUseDistance = false;
  settings.healthUseRestfulSleep = false;
  settings.decimalSeparator = '.';

  Settings_updateDynamicSettings();
}

void Settings_saveToStorage() {
  // Boulder: nothing to store, the settings are fixed.
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
