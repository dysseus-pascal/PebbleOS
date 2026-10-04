/* SPDX-FileCopyrightText: Dan Tilden (freakified) and the TimeStyle contributors */
/* SPDX-License-Identifier: MIT */
/* TimeStyle, https://github.com/freakified/TimeStylePebble (e2be725), LICENSE
 * in this folder. Boulder: ported into the firmware as a system watch face;
 * changes are marked "Boulder:". */

#include <pebble.h>
#include "weather.h"
#include "util.h"
#include "wetter_quelle.h"

WeatherInfo Weather_weatherInfo;

GDrawCommandImage* Weather_currentWeatherIcon;
GDrawCommandImage* Weather_forecastWeatherIcon;

uint32_t getConditionIcon(WeatherCondition conditionCode) {
  uint32_t iconToLoad;

  switch(conditionCode) {
    case CLEAR_DAY:
      iconToLoad = RESOURCE_ID_WEATHER_CLEAR_DAY;
      break;
    case CLEAR_NIGHT:
      iconToLoad = RESOURCE_ID_WEATHER_CLEAR_NIGHT;
      break;
    case CLOUDY_DAY:
      iconToLoad = RESOURCE_ID_WEATHER_CLOUDY;
      break;
    case HEAVY_RAIN:
      iconToLoad = RESOURCE_ID_WEATHER_HEAVY_RAIN;
      break;
    case HEAVY_SNOW:
      iconToLoad = RESOURCE_ID_WEATHER_HEAVY_SNOW;
      break;
    case LIGHT_RAIN:
      iconToLoad = RESOURCE_ID_WEATHER_LIGHT_RAIN;
      break;
    case LIGHT_SNOW:
      iconToLoad = RESOURCE_ID_WEATHER_LIGHT_SNOW;
      break;
    case PARTLY_CLOUDY_NIGHT:
      iconToLoad = RESOURCE_ID_WEATHER_PARTLY_CLOUDY_NIGHT;
      break;
    case PARTLY_CLOUDY:
      iconToLoad = RESOURCE_ID_WEATHER_PARTLY_CLOUDY;
      break;
    case RAINING_AND_SNOWING:
      iconToLoad = RESOURCE_ID_WEATHER_RAINING_AND_SNOWING;
      break;
    case THUNDERSTORM:
      iconToLoad = RESOURCE_ID_WEATHER_THUNDERSTORM;
      break;
    default:
      iconToLoad = RESOURCE_ID_WEATHER_GENERIC;
      break;
  }

  return iconToLoad;
}

void Weather_setCurrentCondition(int conditionCode) {

  uint32_t currentWeatherIcon = getConditionIcon(conditionCode);

  // Boulder: refreshed every 15 minutes - keep the icon if it did not change.
  if (Weather_currentWeatherIcon &&
      Weather_weatherInfo.currentIconResourceID == currentWeatherIcon) {
    return;
  }

  // ok, now load the new icon:
  gdraw_command_image_destroy(Weather_currentWeatherIcon);
  // Boulder: a writable copy, the system resource may be read-only flash.
  Weather_currentWeatherIcon = ts_pdc_laden(currentWeatherIcon);

  Weather_weatherInfo.currentIconResourceID = currentWeatherIcon;
}

void Weather_setForecastCondition(int conditionCode) {
  uint32_t forecastWeatherIcon = getConditionIcon(conditionCode);

  // Boulder: see Weather_setCurrentCondition.
  if (Weather_forecastWeatherIcon &&
      Weather_weatherInfo.forecastIconResourceID == forecastWeatherIcon) {
    return;
  }

  gdraw_command_image_destroy(Weather_forecastWeatherIcon);
  Weather_forecastWeatherIcon = ts_pdc_laden(forecastWeatherIcon);

  Weather_weatherInfo.forecastIconResourceID = forecastWeatherIcon;
}

// Boulder: no data or data too old - back to how TimeStyle starts without
// weather ("..." and no icon).
static void prv_ohne_wetter(void) {
  Weather_weatherInfo.currentTemp = INT32_MIN;
  Weather_weatherInfo.currentUVIndex = INT32_MIN;
  Weather_weatherInfo.todaysHighTemp = INT32_MIN;
  Weather_weatherInfo.todaysLowTemp = INT32_MIN;
  Weather_weatherInfo.currentIconResourceID = 0;
  Weather_weatherInfo.forecastIconResourceID = 0;
  gdraw_command_image_destroy(Weather_currentWeatherIcon);
  gdraw_command_image_destroy(Weather_forecastWeatherIcon);
  Weather_currentWeatherIcon = NULL;
  Weather_forecastWeatherIcon = NULL;
}

// Boulder: replaces messaging.c - the same fields, read from the firmware's
// weather store instead of an AppMessage from PebbleKit JS.
void Weather_refresh() {
  WetterStand stand;
  const time_t jetzt = time(NULL);
#ifdef TS_DEMO
  // Boulder: test build for the emulator picture - the weather of the
  // user's photo (22 degrees, cloudy), as if Boulder had just sent it.
  stand = (WetterStand) {
    .temperatur = 22, .hoch = 24, .tief = 12, .uv_tag_x10 = 30,
    .uv_stunden_da = false, .ort_versatz_min = INT16_MIN,
    .art = 1 /* WeatherType_CloudyDay */, .art_heute = 1,
    .breite_e2 = 4737, .laenge_e2 = 854, .stand_utc = jetzt,
  };
  const bool gelesen = true;
#else
  const bool gelesen = wetter_quelle_lesen(&stand);
#endif
  if (!gelesen || !wetter_frisch(stand.stand_utc, jetzt) ||
      stand.temperatur == WETTER_UNBEKANNT) {
    prv_ohne_wetter();
    return;
  }

  struct tm *utc = gmtime(&jetzt);
  const bool nacht = wetter_ist_nacht(stand.breite_e2, stand.laenge_e2, utc->tm_yday + 1,
                                      utc->tm_hour * 60 + utc->tm_min);

  Weather_weatherInfo.currentTemp = stand.temperatur;
  Weather_setCurrentCondition(wetter_symbol(stand.art, nacht));
  // Boulder: der UV-Index der Stunde jetzt (wetter_stand_uv), mit der
  // Zeitzone der Uhr, falls der Satz die des Ortes nicht kennt.
  const struct tm *lokal = localtime(&jetzt);
  const int uv = wetter_stand_uv(&stand, (int32_t)lokal->tm_gmtoff, jetzt);
  Weather_weatherInfo.currentUVIndex = (uv >= 0) ? uv : INT32_MIN;

  if (stand.hoch != WETTER_UNBEKANNT && stand.tief != WETTER_UNBEKANNT) {
    Weather_weatherInfo.todaysHighTemp = stand.hoch;
    Weather_weatherInfo.todaysLowTemp = stand.tief;
  } else {
    Weather_weatherInfo.todaysHighTemp = INT32_MIN;
    Weather_weatherInfo.todaysLowTemp = INT32_MIN;
  }
  // The forecast icon is the day's weather - always the day symbol, as the
  // phone side of TimeStyle sent it.
  Weather_setForecastCondition(wetter_symbol(stand.art_heute, false));
}

void Weather_init() {
  // Boulder: a system app keeps its statics after it ends; the icons of the
  // previous run belonged to its (gone) app heap.
  Weather_currentWeatherIcon = NULL;
  Weather_forecastWeatherIcon = NULL;
  memset(&Weather_weatherInfo, 0, sizeof(Weather_weatherInfo));

  // initialize weather with default values
  Weather_weatherInfo.currentTemp = INT32_MIN;
  Weather_weatherInfo.currentUVIndex = INT32_MIN;
  Weather_weatherInfo.todaysHighTemp = INT32_MIN;
  Weather_weatherInfo.todaysLowTemp = INT32_MIN;

  // Boulder: instead of the persisted copy, the firmware's weather store.
  Weather_refresh();
}

void Weather_saveData() {
  // Boulder: nothing to persist - the firmware keeps the weather.
}

void Weather_deinit() {
  // free memory
  gdraw_command_image_destroy(Weather_currentWeatherIcon);
  gdraw_command_image_destroy(Weather_forecastWeatherIcon);
  Weather_currentWeatherIcon = NULL;
  Weather_forecastWeatherIcon = NULL;
}
