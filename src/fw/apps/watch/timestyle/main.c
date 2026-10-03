/* SPDX-FileCopyrightText: Dan Tilden (freakified) and the TimeStyle contributors */
/* SPDX-License-Identifier: MIT */
/* TimeStyle, https://github.com/freakified/TimeStylePebble (e2be725), LICENSE
 * in this folder. Boulder: ported into the firmware as a system watch face;
 * changes are marked "Boulder:". */

#include <pebble.h>
#include "clock_area.h"
#include "settings.h"
#include "weather.h"
#include "sidebar.h"
#include "util.h"
// Boulder: no messaging.h - no AppMessage, no PebbleKit JS (see weather.c).
#include "timestyle.h"
#include "applib/event_service_client.h"
#include "kernel/events.h"

// windows and layers
static Window* mainWindow;
static Layer* windowLayer;

// current bluetooth state
static bool isPhoneConnected;

// current time service subscription
static bool updatingEverySecond;

// Boulder: no weatherRefreshMinute - the watch never asks the phone for
// weather; Boulder fills the firmware's store and an event tells us.
static EventServiceInfo s_wetter_ereignis;
// Boulder: new settings from Boulder (watch settings sync, key tsBedrock).
static EventServiceInfo s_einstellung_ereignis;

void update_clock();
void redrawScreen();
void tick_handler(struct tm *tick_time, TimeUnits units_changed);
void bluetoothStateChanged(bool newConnectionState);


void update_clock() {
  time_t rawTime;
  struct tm* timeInfo;

#if defined(USE_FAKE_TIME) || defined(TS_DEMO)
  // Boulder: TS_DEMO (test build for the emulator picture) shows the time of
  // the user's photo: Saturday, 3 October 2026, 15:45.
  struct tm fakeTime = {
    .tm_hour = 15,
    .tm_min  = 45,
    .tm_sec  = 0,
    .tm_mday = 3,
    .tm_mon  = 9,
    .tm_year = 126,
    .tm_wday = 6,
    .tm_yday = 275,
  };
  (void)rawTime;
  timeInfo = &fakeTime;
#else
  time(&rawTime);
  timeInfo = localtime(&rawTime);
#endif

  ClockArea_update_time(timeInfo);
  Sidebar_updateTime(timeInfo);
}

/* forces everything on screen to be redrawn -- perfect for keeping track of settings! */
void redrawScreen() {

  // check if the tick handler frequency should be changed
  if(dynamicSettings.updateScreenEverySecond != updatingEverySecond) {
    tick_timer_service_unsubscribe();

    if(dynamicSettings.updateScreenEverySecond) {
      tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
      updatingEverySecond = true;
    } else {
      tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
      updatingEverySecond = false;
    }
  }

  window_set_background_color(mainWindow, settings.timeBgColor);

  // maybe the language changed!
  update_clock();

  // update the sidebar
  Sidebar_redraw();

  ClockArea_redraw();
}

static void main_window_load(Window *window) {
  window_set_background_color(window, settings.timeBgColor);

  // create the sidebar
  Sidebar_init(window);

  ClockArea_init(window);

  // Make sure the time is displayed from the start
  redrawScreen();
}

static void main_window_unload(Window *window) {
  ClockArea_deinit();
  Sidebar_deinit();
}


void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  // Boulder: every 15 minutes, re-read the firmware's weather store - new
  // data comes with an event, but data that grew too old, and sunrise and
  // sunset (day or night icon), do not.
  if(!dynamicSettings.disableWeather) {
    if(tick_time->tm_min % 15 == 0 && tick_time->tm_sec == 0) {
      Weather_refresh();
    }
  }

  // every hour, if requested, vibrate
  if(!quiet_time_is_active() && tick_time->tm_sec == 0) {
    if(settings.hourlyVibe == VIBE_EVERY_HOUR) { // hourly vibes only
      if(tick_time->tm_min == 0) {
        vibes_double_pulse();
      }
    } else if(settings.hourlyVibe == VIBE_EVERY_HALF_HOUR) {  // hourly and half-hourly
      if(tick_time->tm_min == 0) {
        vibes_double_pulse();
      } else if(tick_time->tm_min == 30) {
        vibes_short_pulse();
      }
    }
  }

  update_clock();

  // redraw all screen
  Sidebar_redraw();
  ClockArea_redraw();
}

void bluetoothStateChanged(bool newConnectionState) {
  // if the phone was connected but isn't anymore and the user has opted in,
  // trigger a vibration
  if(!quiet_time_is_active() && isPhoneConnected && !newConnectionState && settings.btVibe) {
    static uint32_t const segments[] = { 200, 100, 100, 100, 500 };
    VibePattern pat = {
      .durations = segments,
      .num_segments = ARRAY_LENGTH(segments),
      };
    vibes_enqueue_custom_pattern(pat);
  }

  // Boulder: no weather request on reconnect - Boulder writes the store and
  // the weather event follows.

  isPhoneConnected = newConnectionState;

  Sidebar_redraw();
}

// force the sidebar to redraw any time the battery state changes
void batteryStateChanged(BatteryChargeState charge_state) {
  Sidebar_redraw();
}

// Boulder: the phone wrote new weather into the firmware's store.
static void prv_wetter_neu(PebbleEvent *event, void *context) {
  if(dynamicSettings.disableWeather) {
    return;
  }
  Weather_refresh();
  Sidebar_redraw();
}

// Boulder: Boulder sent new settings. Applied at once, without restarting
// the face - the same steps as TimeStyle after its config page.
static void prv_einstellungen_neu(PebbleEvent *event, void *context) {
  const char *key = event->pref_change.key;
  if(!key || strcmp(key, TS_BEDROCK_PREF_SCHLUESSEL) != 0) {
    return;
  }
  Settings_loadFromStorage();

  // a weather widget may just have been chosen - its data may be old
  if(!dynamicSettings.disableWeather) {
    Weather_refresh();
  }

  // also switches the tick between seconds and minutes if needed
  redrawScreen();
}

// fixes for disappearing elements after notifications
// (from http://codecorner.galanter.net/2016/01/08/solved-issue-with-pebble-framebuffer-after-notification-is-dismissed/)
static void app_focus_changing(bool focusing) {
  if (focusing) {
     layer_set_hidden(windowLayer, true);
  }
}

static void app_focus_changed(bool focused) {
  if (focused) {
     layer_set_hidden(windowLayer, false);
     layer_mark_dirty(windowLayer);
  }
}

static void init() {
  // Boulder: no setlocale/srand (no random weather minute) and no
  // messaging_init. A system app keeps its statics after it ends, so the
  // state of this file starts from scratch.
  mainWindow = NULL;
  windowLayer = NULL;
  isPhoneConnected = false;
  updatingEverySecond = false;

  // init settings
  Settings_init();

  // init weather system
  Weather_init();

  // Boulder: new weather from the phone arrives as an event.
  s_wetter_ereignis = (EventServiceInfo) {
    .type = PEBBLE_WEATHER_EVENT,
    .handler = prv_wetter_neu,
  };
  event_service_client_subscribe(&s_wetter_ereignis);

  // Boulder: and new settings as a pref change.
  s_einstellung_ereignis = (EventServiceInfo) {
    .type = PEBBLE_PREF_CHANGE_EVENT,
    .handler = prv_einstellungen_neu,
  };
  event_service_client_subscribe(&s_einstellung_ereignis);

  // Create main Window element and assign to pointer
  mainWindow = window_create();

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(mainWindow, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  // Show the Window on the watch, with animated=true
  window_stack_push(mainWindow, true);

  windowLayer = window_get_root_layer(mainWindow);

  // Register with TickTimerService
  if(dynamicSettings.updateScreenEverySecond) {
    tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
    updatingEverySecond = true;
  } else {
    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
    updatingEverySecond = false;
  }

  bool connected = bluetooth_connection_service_peek();
  bluetoothStateChanged(connected);
  bluetooth_connection_service_subscribe(bluetoothStateChanged);

  // register with battery service
  battery_state_service_subscribe(batteryStateChanged);

  // set up focus change handlers
  app_focus_service_subscribe_handlers((AppFocusHandlers){
    .did_focus = app_focus_changed,
    .will_focus = app_focus_changing
  });
}

static void deinit() {
  // Destroy Window
  window_destroy(mainWindow);

  // Boulder: stop listening for weather and settings before their data goes away.
  event_service_client_unsubscribe(&s_wetter_ereignis);
  event_service_client_unsubscribe(&s_einstellung_ereignis);

  // unload weather stuff
  Weather_deinit();
  Settings_deinit();

  tick_timer_service_unsubscribe();
  bluetooth_connection_service_unsubscribe();
  battery_state_service_unsubscribe();
}

// Boulder: main() of the SDK app, as the main function of a system process.
static void prv_main(void) {
  init();
  app_event_loop();
  deinit();
}

const PebbleProcessMd *timestyle_get_app_info(void) {
  static const PebbleProcessMdSystem s_app_md = {
    .common = {
      .uuid = TIMESTYLE_UUID_INIT,
      .main_func = prv_main,
      .process_type = ProcessTypeWatchface,
    },
    .icon_resource_id = RESOURCE_ID_MENU_ICON_TIMESTYLE,
    .name = "TimeStyle Bedrock",
  };
  return (const PebbleProcessMd *)&s_app_md;
}
