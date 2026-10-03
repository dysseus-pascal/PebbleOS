/* SPDX-FileCopyrightText: Dan Tilden (freakified) and the TimeStyle contributors */
/* SPDX-License-Identifier: MIT */
/* TimeStyle, https://github.com/freakified/TimeStylePebble (e2be725), LICENSE
 * in this folder. Boulder: ported into the firmware as a system watch face;
 * changes are marked "Boulder:". */

#pragma once
#include <pebble.h>
// Boulder: WeatherCondition moved to wetter_symbol.h (pure, host-tested).
#include "wetter_symbol.h"

// Boulder: no WEATHER_PERSIST_KEY - the data comes from the firmware's
// weather store on every refresh (wetter_quelle.c).

typedef struct {
  // current weather
  int currentTemp;
  uint32_t currentIconResourceID;
  int currentUVIndex;

  // today's forecast
  int todaysHighTemp;
  int todaysLowTemp;
  uint32_t forecastIconResourceID;
} WeatherInfo;

extern WeatherInfo Weather_weatherInfo;

extern GDrawCommandImage* Weather_currentWeatherIcon;
extern GDrawCommandImage* Weather_forecastWeatherIcon;

void Weather_setCurrentCondition(int conditionCode);
void Weather_setForecastCondition(int conditionCode);
void Weather_saveData();
void Weather_init();
void Weather_deinit();
// Boulder: re-reads the firmware's weather store (replaces messaging.c).
void Weather_refresh();
