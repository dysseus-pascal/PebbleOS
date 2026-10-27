/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// NUR MIT DER VARIANTE (boulder_schlaf.h). Der Ordner wird fuer jedes Board gebaut (GLOB_RECURSE in
// fw/apps/system/CMakeLists.txt); ohne CONFIG_SERVICE_ACTIVITY_ALGORITHM_BOULDER bleibt die Datei
// leer, und die Registry fuehrt den Schalter dann auch nicht.
#ifdef CONFIG_SERVICE_ACTIVITY_ALGORITHM_BOULDER

#include "boulder_schlaf.h"

#include <pbl/services/activity/boulder/schlafmodus.h>
#include <pbl/services/i18n/i18n.h>

#include <applib/app.h>
#include <applib/ui/action_toggle.h>
#include <process_management/app_manager.h>
#include <shell/system_app_ids.auto.h>

#define BOULDER_SCHLAF_UUID \
  {0xcd, 0x87, 0x9a, 0x52, 0x3f, 0x5f, 0x43, 0xc6, 0x9d, 0x93, 0xbf, 0x6c, 0x6f, 0xc4, 0xed, 0x77}

static bool prv_get_state(void *context) {
  return boulder_schlafmodus_aktiv();
}

static void prv_set_state(bool enabled, void *context) {
  boulder_schlafmodus_schalten(enabled);
}

static const ActionToggleImpl s_boulder_schlaf_action_toggle_impl = {
  .window_name = "Boulder Sleep Mode Toggle",
  .prompt_icon = RESOURCE_ID_SLEEP_LARGE,
  .result_icon = RESOURCE_ID_SLEEP_LARGE,
  .prompt_enable_message = i18n_noop("Start Sleep Mode?"),
  .prompt_disable_message = i18n_noop("End Sleep Mode?"),
  .result_enable_message = i18n_noop("Sleep Mode\nStarted"),
  .result_disable_message = i18n_noop("Sleep Mode\nEnded"),
  .callbacks = {
    .get_state = prv_get_state,
    .set_state = prv_set_state,
  },
};

static void prv_main(void) {
  action_toggle_push(&(ActionToggleConfig){
    .impl = &s_boulder_schlaf_action_toggle_impl,
    .set_exit_reason = true,
  });
  app_event_loop();
}

const PebbleProcessMd *boulder_schlaf_get_app_info(void);

const PebbleProcessMd *boulder_schlaf_get_app_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common =
        {
          .main_func = &prv_main,
          .uuid = BOULDER_SCHLAF_UUID,
          .visibility = ProcessVisibilityQuickLaunch,
        },
    /// The Quick Launch action that toggles Boulder's sleep mode.
    .name = i18n_noop("Toggle Sleep Mode"),
  };
  return &s_app_info.common;
}

void boulder_schlaf_oeffnen(void) {
  app_manager_put_launch_app_event(&(AppLaunchEventConfig){
    .id = APP_ID_BOULDER_SCHLAF,
    .common.reason = APP_LAUNCH_USER,
  });
}

#endif  // CONFIG_SERVICE_ACTIVITY_ALGORITHM_BOULDER
