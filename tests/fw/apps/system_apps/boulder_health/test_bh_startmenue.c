/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "flash_region/flash_region.h"
#include "process_management/app_install_manager.h"
#include "process_management/app_menu_data_source.h"
#include "process_management/pebble_process_md.h"
#include "resource/resource.h"
#include "resource/resource_ids.auto.h"
#include "resource/resource_storage.h"
#include "resource/resource_storage_file.h"
#include "pbl/services/app_cache.h"
#include "pbl/services/blob_db/app_db.h"
#include "pbl/services/filesystem/pfs.h"
#include "pbl/services/timeline/timeline.h"
#include "pbl/util/size.h"
#include "fixtures/load_test_resources.h"

// Die Registry, wie generate.py sie aus der echten system_app_registry_list.json fuer das Board
// erzeugt (CMakeLists.txt: mit den Schaltern von obelix oder ohne wie asterix).
#include "shell/system_app_registry_list.auto.h"

#include "fake_spi_flash.h"
#include "pbl/services/time.h"

#include "stubs_activity.h"
#include "stubs_analytics.h"
#include "stubs_app_custom_icon.h"
#include "stubs_app_fetch_endpoint.h"
#include "stubs_app_manager.h"
#include "stubs_app_state.h"
#include "stubs_bootbits.h"
#include "stubs_build_id.h"
#include "stubs_comm_session.h"
#include "stubs_event_loop.h"
#include "stubs_event_service_client.h"
#include "stubs_events.h"
#include "stubs_fonts.h"
#include "stubs_gbitmap.h"
#include "stubs_graphics.h"
#include "stubs_graphics_context.h"
#include "stubs_heap.h"
#include "stubs_hexdump.h"
#include "stubs_i18n.h"
#include "stubs_kino_reel.h"
#include "stubs_logging.h"
#include "stubs_memory_layout.h"
#include "stubs_menu_layer.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_tasks.h"
#include "stubs_persist.h"
#include "stubs_pin_db.h"
#include "stubs_process_loader.h"
#include "stubs_process_manager.h"
#include "stubs_put_bytes.h"
#include "stubs_quick_launch.h"
#include "stubs_rand_ptr.h"
#include "stubs_serial.h"
#include "stubs_shell_prefs.h"
#include "stubs_sleep.h"
#include "stubs_system_task.h"
#include "stubs_task_wdt.h"
#include "stubs_watchface.h"
#include "stubs_worker_manager.h"

// T3.1 (Kriterium 3): Registry und Startmenue auf obelix und asterix. Echt sind die Registry aus
// der JSON (generate.py), der App-Speicher (app_install_manager.c, app_db.c), die Liste des
// Startmenues (app_menu_data_source.c) und die App hinter health_app_get_info: mit
// CONFIG_BOULDER_HEALTH_APP Boulders boulder_health.c, ohne Pebbles health/health.c - genau die
// Datei, die fw/apps/system/CMakeLists.txt (P7) fuer das Board baut. Die Filter der zwei Listen
// stehen in launcher.c:62 und quick_launch_app_menu.c:57-71 (static); beide verwerfen, was
// app_install_entry_is_hidden() meldet, wie Pebbles eigene Tests (test_app_menu_data_source.c).
//
// Rot, wenn Health fehlt, doppelt da ist oder auf emery Pebbles App hinter der Kennung steht.

const PebbleProcessMd *health_app_get_info(void);

static const Uuid s_health_uuid = UUID_HEALTH_DATA_SOURCE;

// --- Die anderen System-Apps der Registry ---
//
// Fuer sie zaehlt hier nur, dass es sie gibt: ein sichtbarer Eintrag ohne eigene Kennung. Schwach,
// damit eine echte Definition vorgeht. Fehlt ein Name, den die JSON neu bekommt, bricht das
// Linken - laut, nicht still. health_app_get_info steht nicht hier: Es kommt immer aus einer
// echten Datei.
static const PebbleProcessMdSystem s_andere = {.common = {.visibility = ProcessVisibilityShown},
                                               .name = "Andere"};
#define ANDERE(fn) \
  __attribute__((weak)) const PebbleProcessMd *fn(void) { return &s_andere.common; }
ANDERE(tictoc_get_app_info)
ANDERE(kickstart_get_app_info)
ANDERE(low_power_face_get_app_info)
ANDERE(settings_get_app_info)
ANDERE(music_app_get_info)
ANDERE(notifications_app_get_info)
ANDERE(alarms_app_get_info)
ANDERE(watchfaces_get_app_info)
ANDERE(quick_launch_setup_get_app_info)
ANDERE(timeline_get_app_info)
ANDERE(timeline_past_get_app_info)
ANDERE(launcher_menu_app_get_app_info)
ANDERE(weather_app_get_info)
ANDERE(workout_app_get_info)
ANDERE(battery_critical_get_app_info)
ANDERE(send_text_app_get_info)
ANDERE(reminder_app_get_info)
ANDERE(quiet_time_toggle_get_app_info)
ANDERE(backlight_state_toggle_get_app_info)
ANDERE(motion_backlight_toggle_get_app_info)
ANDERE(airplane_mode_toggle_get_app_info)
ANDERE(timeline_full_get_app_info)
ANDERE(notifications_clear_history_app_get_info)
ANDERE(quick_launch_nothing_get_app_info)
ANDERE(kieselsport_app_get_info)
ANDERE(touch_toggle_get_app_info)
// Sports ist nur sichtbar, solange eine Sport-App mit der Uhr spricht (workout/sports.c:339).
static const PebbleProcessMdSystem s_sports = {
    .common = {.visibility = ProcessVisibilityShownOnCommunication}, .name = "Sports"};
const PebbleProcessMd *sports_app_get_info(void) { return &s_sports.common; }

// --- Umfeld ---

const uint32_t g_num_file_resource_stores = 0;
const FileResourceData g_file_resource_stores[] = {};

uint32_t time_get_uptime_seconds(void) {
  return rtc_get_time();
}

extern void app_order_storage_reset_for_tests(void);
extern ListNode *s_head_callback_node_list;

// --- Die zwei Listen ---

// Startmenue (launcher.c:62): keine Zifferblaetter, nichts Verstecktes.
static bool prv_startmenue(AppMenuDataSource *source, AppInstallEntry *entry) {
  return !app_install_entry_is_watchface(entry) && !app_install_entry_is_hidden(entry);
}

// Quick-Launch-Auswahl, Rubrik Apps (quick_launch_app_menu.c:57-71).
static bool prv_quick_launch(AppMenuDataSource *source, AppInstallEntry *entry) {
  return !app_install_entry_is_watchface(entry) &&
         !app_install_entry_is_quick_launch_visible_only(entry) &&
         !app_install_entry_is_hidden(entry);
}

typedef struct {
  int health;            //< Zeilen mit der UUID der Health-App
  int namens_health;     //< Zeilen einer App mit dem Namen "Health"
  AppInstallId id;
} Zaehlung;

static Zaehlung prv_zaehle(AppMenuFilterCallback filter) {
  static MenuLayer menue;
  AppMenuDataSource liste;
  app_menu_data_source_init(&liste, &(AppMenuDataSourceCallbacks){.filter = filter}, &menue);
  Zaehlung z = {.id = INSTALL_ID_INVALID};
  const uint16_t n = app_menu_data_source_get_count(&liste);
  for (uint16_t i = 0; i < n; i++) {
    AppMenuNode *node = app_menu_data_source_get_node_at_index(&liste, i);
    cl_assert(node);
    if (uuid_equal(&node->uuid, &s_health_uuid)) {
      z.health++;
      z.id = node->install_id;
    }
    // Der Name aus dem Eintrag der App: Die Zeile selbst nimmt ihn aus einer Attrappe
    // (stubs_app_custom_icon.h).
    AppInstallEntry e;
    cl_assert(app_install_get_entry_for_install_id(node->install_id, &e));
    z.namens_health += (strcmp(e.name, "Health") == 0);
  }
  app_menu_data_source_deinit(&liste);
  return z;
}

void test_bh_startmenue__initialize(void) {
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  pfs_format(false);
  app_order_storage_reset_for_tests();
  app_install_manager_init();
  app_db_init();
  app_cache_init();
  load_resource_fixture_in_flash(RESOURCES_FIXTURE_PATH, SYSTEM_RESOURCES_FIXTURE_NAME, false);
  resource_init();
  rtc_set_time(100);
}

void test_bh_startmenue__cleanup(void) {
  s_head_callback_node_list = NULL;
  app_install_manager_flush_recent_communication_timestamps();
}

// Die Registry des Boards fuehrt -82 genau einmal, mit health_app_get_info, und die UUID der
// Health-App fuehrt dorthin - so findet Quick Launch sie bei jedem Druck (prefs.c:1751-1765).
void test_bh_startmenue__registry(void) {
  int eintraege = 0;
  for (size_t i = 0; i < ARRAY_LENGTH(APP_RECORDS); i++) {
    if (APP_RECORDS[i].id == APP_ID_HEALTH_APP) {
      eintraege++;
      cl_assert(APP_RECORDS[i].md_fn == health_app_get_info);
    }
  }
  cl_assert_equal_i(eintraege, 1);
  cl_assert_equal_i(APP_ID_HEALTH_APP, -82);
  cl_assert_equal_i(app_install_get_id_for_uuid(&s_health_uuid), APP_ID_HEALTH_APP);
}

// Hinter der Kennung steht auf emery Boulders App ohne Systemrechte, sonst Pebbles App.
void test_bh_startmenue__welche_app(void) {
  const PebbleProcessMd *md = app_install_get_md(APP_ID_HEALTH_APP, false);
  cl_assert(md == health_app_get_info());
  cl_assert(uuid_equal(&md->uuid, &s_health_uuid));
#ifdef CONFIG_BOULDER_HEALTH_APP
  cl_assert(md->is_unprivileged);
#else
  cl_assert(!md->is_unprivileged);
#endif
  app_install_release_md(md);
}

// Name, Symbol und Farbe wie bisher: aus der Registry (Farbe) und dem Eintrag der App.
void test_bh_startmenue__eintrag(void) {
  AppInstallEntry e;
  cl_assert(app_install_get_entry_for_install_id(APP_ID_HEALTH_APP, &e));
  cl_assert_equal_s(e.name, "Health");
  cl_assert_equal_i(e.icon_resource_id, RESOURCE_ID_MENU_ICON_HEALTH);
  cl_assert_equal_i(e.color.argb, GColorSunsetOrangeARGB8);
  cl_assert(!app_install_entry_is_hidden(&e));
  cl_assert(!app_install_entry_is_watchface(&e));
}

// T3.1: Startmenue und Quick-Launch-Auswahl zeigen Health genau einmal, mit der Kennung -82.
void test_bh_startmenue__einmal(void) {
  const Zaehlung start = prv_zaehle(prv_startmenue);
  cl_assert_equal_i(start.health, 1);
  cl_assert_equal_i(start.namens_health, 1);
  cl_assert_equal_i(start.id, APP_ID_HEALTH_APP);
  const Zaehlung schnell = prv_zaehle(prv_quick_launch);
  cl_assert_equal_i(schnell.health, 1);
  cl_assert_equal_i(schnell.namens_health, 1);
  cl_assert_equal_i(schnell.id, APP_ID_HEALTH_APP);
}
