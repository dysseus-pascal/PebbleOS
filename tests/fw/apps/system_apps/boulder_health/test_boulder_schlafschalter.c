/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "apps/system/boulder_schlaf/boulder_schlaf.h"

#include "applib/ui/action_toggle.h"
#include "process_management/app_manager.h"
#include "process_management/pebble_process_md.h"
#include "resource/resource_ids.auto.h"
#include "shell/system_app_ids.auto.h"

#include <string.h>

// Der Schalter fuer Quick Launch (F53, boulder_schlaf.c): gebaut wie Pebbles Schalter fuer Quiet
// Time, verdrahtet mit dem Schlafmodus, und S8 der Health-App oeffnet ihn mit seiner Kennung aus der
// Registry von obelix. Pebbles ActionToggle selbst ist nachgebaut: Dialog, Summen und Ende der App
// sind Pebbles Code (action_toggle.c).

const PebbleProcessMd *boulder_schlaf_get_app_info(void);

static ActionToggleConfig s_config;
static int s_gezeigt;
static bool s_aktiv;
static int s_geschaltet;
static bool s_wunsch;
static AppLaunchEventConfig s_start;
static int s_starts;

void action_toggle_push(const ActionToggleConfig *config) {
  s_config = *config;
  s_gezeigt++;
}
void app_event_loop(void) {
}
bool boulder_schlafmodus_aktiv(void) {
  return s_aktiv;
}
void boulder_schlafmodus_schalten(bool an) {
  s_geschaltet++;
  s_wunsch = an;
}
void app_manager_put_launch_app_event(const AppLaunchEventConfig *config) {
  s_start = *config;
  s_starts++;
}

void test_boulder_schlafschalter__initialize(void) {
  memset(&s_config, 0, sizeof(s_config));
  s_gezeigt = 0;
  s_aktiv = false;
  s_geschaltet = 0;
  s_wunsch = false;
  memset(&s_start, 0, sizeof(s_start));
  s_starts = 0;
}

void test_boulder_schlafschalter__cleanup(void) {
}

// Nur in Quick Launch, ohne Frage wie Quiet Time (ActionTogglePrompt_Auto), mit Pebbles Symbol fuer
// den Schlaf; das Ergebnis beendet die App zum Zifferblatt.
void test_boulder_schlafschalter__wie_pebbles_schalter(void) {
  const PebbleProcessMdSystem *md = (const PebbleProcessMdSystem *)boulder_schlaf_get_app_info();
  cl_assert_equal_i(md->common.visibility, ProcessVisibilityQuickLaunch);
  cl_assert(!md->common.is_unprivileged);
  cl_assert_equal_s(md->name, "Toggle Sleep Mode");
  md->common.main_func();
  cl_assert_equal_i(s_gezeigt, 1);
  cl_assert(s_config.set_exit_reason);
  cl_assert_equal_i(s_config.prompt, ActionTogglePrompt_Auto);
  const ActionToggleImpl *impl = s_config.impl;
  cl_assert_equal_s(impl->result_enable_message, "Sleep Mode\nStarted");
  cl_assert_equal_s(impl->result_disable_message, "Sleep Mode\nEnded");
  cl_assert_equal_i(impl->result_icon, RESOURCE_ID_SLEEP_LARGE);
  cl_assert_equal_i(impl->prompt_icon, RESOURCE_ID_SLEEP_LARGE);
}

// Der Zustand kommt vom Schlafmodus, das Umschalten geht an ihn (er arbeitet auf KernelBG).
void test_boulder_schlafschalter__verdrahtet_mit_dem_schlafmodus(void) {
  boulder_schlaf_get_app_info()->main_func();
  const ActionToggleImpl *impl = s_config.impl;
  cl_assert(!impl->callbacks.get_state(s_config.context));
  s_aktiv = true;
  cl_assert(impl->callbacks.get_state(s_config.context));
  impl->callbacks.set_state(false, s_config.context);
  cl_assert_equal_i(s_geschaltet, 1);
  cl_assert(!s_wunsch);
  impl->callbacks.set_state(true, s_config.context);
  cl_assert_equal_i(s_geschaltet, 2);
  cl_assert(s_wunsch);
}

// S8: wie ein Start aus dem Startmenue, mit der Kennung aus der Registry von obelix.
void test_boulder_schlafschalter__oeffnen(void) {
  boulder_schlaf_oeffnen();
  cl_assert_equal_i(s_starts, 1);
  cl_assert_equal_i(APP_ID_BOULDER_SCHLAF, -105);
  cl_assert_equal_i(s_start.id, APP_ID_BOULDER_SCHLAF);
  cl_assert_equal_i(s_start.common.reason, APP_LAUNCH_USER);
}
