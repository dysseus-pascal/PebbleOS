/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder: DIE GLOBALEN NAMEN VON TIMESTYLE BEKOMMEN DAS PRAEFIX ts_.
// TimeStyle war eine eigene App mit eigenem Namensraum; in der Firmware teilt
// sie den Gesamtlink mit allem anderen. Namen wie settings, mod, tick_handler
// oder gdraw_command_image_recolor stiessen dort beim naechsten Upstream-
// Stand, der denselben Namen bringt, zusammen. Umbenannt wird hier statt im
// uebernommenen Code: die Quellen bleiben mit TimeStyle, pebble-fctx und
// pebble-utf8 vergleichbar. CMakeLists.txt stellt diese Datei jeder Quelle
// des Zifferblatts voran (-include); fuer den Rest der Firmware gibt es nur
// die Namen mit ts_, fctx_, ffont_ oder timestyle_. pruefe_ts_symbole.py
// laesst den Bau scheitern, wenn ein neuer globaler Name ohne sie dazukommt -
// dann gehoert er hier dazu.
//
// Die Host-Tests bauen ohne diese Datei und sehen die alten Namen.
#pragma once

#define altClock                       ts_altClock
#define AltTime_draw                   ts_AltTime_draw
#define AltTime_getHeight              ts_AltTime_getHeight
#define altTimeWidget                  ts_altTimeWidget
#define avenir                         ts_avenir
#define avenir_bold                    ts_avenir_bold
#define batteryChargeImage             ts_batteryChargeImage
#define batteryFont                    ts_batteryFont
#define batteryImage                   ts_batteryImage
#define BatteryMeter_draw              ts_BatteryMeter_draw
#define BatteryMeter_getHeight         ts_BatteryMeter_getHeight
#define batteryMeterWidget             ts_batteryMeterWidget
#define batteryStateChanged            ts_batteryStateChanged
#define Beats_draw                     ts_Beats_draw
#define Beats_getHeight                ts_Beats_getHeight
#define beatsWidget                    ts_beatsWidget
#define bluetoothStateChanged          ts_bluetoothStateChanged
#define BTDisconnect_draw              ts_BTDisconnect_draw
#define BTDisconnect_getHeight         ts_BTDisconnect_getHeight
#define btDisconnectWidget             ts_btDisconnectWidget
#define checkObject                    ts_checkObject
#define clock_area_layer               ts_clock_area_layer
#define ClockArea_deinit               ts_ClockArea_deinit
#define ClockArea_gerechnet            ts_ClockArea_gerechnet
#define ClockArea_init                 ts_ClockArea_init
#define ClockArea_kopie_bytes          ts_ClockArea_kopie_bytes
#define ClockArea_redraw               ts_ClockArea_redraw
#define ClockArea_update_time          ts_ClockArea_update_time
#define countBits                      ts_countBits
#define currentBeats                   ts_currentBeats
#define currentDayName                 ts_currentDayName
#define currentDayNum                  ts_currentDayNum
#define currentMonth                   ts_currentMonth
#define currentSecondsNum              ts_currentSecondsNum
#define currentSidebarFont             ts_currentSidebarFont
#define CurrentWeather_draw            ts_CurrentWeather_draw
#define CurrentWeather_getHeight       ts_CurrentWeather_getHeight
#define currentWeatherWidget           ts_currentWeatherWidget
#define currentWeekNum                 ts_currentWeekNum
#define dateImage                      ts_dateImage
#define dateLgImage                    ts_dateLgImage
#define dateWidget                     ts_dateWidget
#define DateWidget_draw                ts_DateWidget_draw
#define DateWidget_getHeight           ts_DateWidget_getHeight
#define dayNames                       ts_dayNames
#define disconnectImage                ts_disconnectImage
#define distanz_text                   ts_distanz_text
#define drawRoundSidebar               ts_drawRoundSidebar
#define dynamicSettings                ts_dynamicSettings
#define edge_init                      ts_edge_init
#define edge_init_aa                   ts_edge_init_aa
#define edge_step                      ts_edge_step
#define emptyWidget                    ts_emptyWidget
#define EmptyWidget_draw               ts_EmptyWidget_draw
#define EmptyWidget_getHeight          ts_EmptyWidget_getHeight
#define fceil                          ts_fceil
#define fceil_aa                       ts_fceil_aa
#define floorDivMod                    ts_floorDivMod
#define gdraw_command_image_recolor    ts_gdraw_command_image_recolor
#define getConditionIcon               ts_getConditionIcon
#define getReplacableWidget            ts_getReplacableWidget
#define getSidebarWidgetByType         ts_getSidebarWidgetByType
#define heartImage                     ts_heartImage
#define HeartRate_draw                 ts_HeartRate_draw
#define HeartRate_getHeight            ts_HeartRate_getHeight
#define heartRateWidget                ts_heartRateWidget
#define hours_font                     ts_hours_font
#define is_health_metric_accessible    ts_is_health_metric_accessible
#define is_user_sleeping               ts_is_user_sleeping
#define isAutoBatteryShown             ts_isAutoBatteryShown
#define leco                           ts_leco
#define lgSidebarFont                  ts_lgSidebarFont
#define mdSidebarFont                  ts_mdSidebarFont
#define minutes_font                   ts_minutes_font
#define mod                            ts_mod
#define monthNames                     ts_monthNames
#define recolor_iterator_cb            ts_recolor_iterator_cb
#define redrawScreen                   ts_redrawScreen
#define schritte_text                  ts_schritte_text
#define Seconds_draw                   ts_Seconds_draw
#define Seconds_getHeight              ts_Seconds_getHeight
#define secondsWidget                  ts_secondsWidget
#define settings                       ts_settings
#define Settings_deinit                ts_Settings_deinit
#define Settings_init                  ts_Settings_init
#define Settings_loadFromStorage       ts_Settings_loadFromStorage
#define Settings_saveToStorage         ts_Settings_saveToStorage
#define Settings_updateDynamicSettings ts_Settings_updateDynamicSettings
#define Sidebar_deinit                 ts_Sidebar_deinit
#define Sidebar_init                   ts_Sidebar_init
#define Sidebar_redraw                 ts_Sidebar_redraw
#define Sidebar_updateTime             ts_Sidebar_updateTime
#define sidebarLayer                   ts_sidebarLayer
#define sidebarLayer2                  ts_sidebarLayer2
#define SidebarWidgets_deinit          ts_SidebarWidgets_deinit
#define SidebarWidgets_init            ts_SidebarWidgets_init
#define SidebarWidgets_updateFonts     ts_SidebarWidgets_updateFonts
#define SidebarWidgets_updateTime      ts_SidebarWidgets_updateTime
#define SidebarWidgets_useCompactMode  ts_SidebarWidgets_useCompactMode
#define SidebarWidgets_xOffset         ts_SidebarWidgets_xOffset
#define sidebarWidth                   ts_sidebarWidth
#define sleepImage                     ts_sleepImage
#define SleepTimer_draw                ts_SleepTimer_draw
#define SleepTimer_getHeight           ts_SleepTimer_getHeight
#define sleepTimerWidget               ts_sleepTimerWidget
#define smSidebarFont                  ts_smSidebarFont
#define StepCounter_draw               ts_StepCounter_draw
#define StepCounter_getHeight          ts_StepCounter_getHeight
#define stepCounterWidget              ts_stepCounterWidget
#define stepsImage                     ts_stepsImage
#define tick_handler                   ts_tick_handler
#define time_get_beats                 ts_time_get_beats
#define time_hours                     ts_time_hours
#define time_minutes                   ts_time_minutes
#define update_clock                   ts_update_clock
#define update_clock_area_layer        ts_update_clock_area_layer
#define update_fonts                   ts_update_fonts
#define updateRectSidebar              ts_updateRectSidebar
#define updateRoundSidebarLeft         ts_updateRoundSidebarLeft
#define updateRoundSidebarRight        ts_updateRoundSidebarRight
#define utf8_decode_byte               ts_utf8_decode_byte
#define utf8_str_to_lower              ts_utf8_str_to_lower
#define utf8_str_to_upper              ts_utf8_str_to_upper
#define UVIndex_draw                   ts_UVIndex_draw
#define UVIndex_getHeight              ts_UVIndex_getHeight
#define uvIndexWidget                  ts_uvIndexWidget
#define Weather_currentWeatherIcon     ts_Weather_currentWeatherIcon
#define Weather_deinit                 ts_Weather_deinit
#define Weather_forecastWeatherIcon    ts_Weather_forecastWeatherIcon
#define Weather_init                   ts_Weather_init
#define Weather_refresh                ts_Weather_refresh
#define Weather_saveData               ts_Weather_saveData
#define Weather_setCurrentCondition    ts_Weather_setCurrentCondition
#define Weather_setForecastCondition   ts_Weather_setForecastCondition
#define Weather_weatherInfo            ts_Weather_weatherInfo
#define WeatherForecast_draw           ts_WeatherForecast_draw
#define WeatherForecast_getHeight      ts_WeatherForecast_getHeight
#define weatherForecastWidget          ts_weatherForecastWidget
#define WeekNumber_draw                ts_WeekNumber_draw
#define WeekNumber_getHeight           ts_WeekNumber_getHeight
#define weekNumberWidget               ts_weekNumberWidget
#define wetter_frisch                  ts_wetter_frisch
#define wetter_ist_nacht               ts_wetter_ist_nacht
#define wetter_quelle_lesen            ts_wetter_quelle_lesen
#define wetter_stand_uv                ts_wetter_stand_uv
#define wetter_symbol                  ts_wetter_symbol
#define wetter_uv_jetzt                ts_wetter_uv_jetzt
#define wordForWeek                    ts_wordForWeek
#define zweitzeit_text                 ts_zweitzeit_text
