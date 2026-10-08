/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "flash_region/flash_region.h"
#include "process_management/app_install_manager.h"
#include "process_management/app_menu_data_source.h"
#include "process_management/pebble_process_md.h"
#include "resource/resource.h"
#include "resource/resource_storage.h"
#include "resource/resource_storage_file.h"
#include "pbl/services/activity/boulder/sport/kieselsport.h"
#include "pbl/services/app_cache.h"
#include "pbl/services/blob_db/app_db.h"
#include "pbl/services/filesystem/pfs.h"
#include "pbl/util/size.h"
#include "fixtures/load_test_resources.h"

// Die Registry, wie generate.py sie aus der echten system_app_registry_list.json fuer das Board
// erzeugt (CMakeLists.txt: mit CONFIG_BOULDER_KIESELSPORT wie obelix und qemu_emery, ohne wie
// asterix).
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

// T6.1 (Kriterium 6, Spezifikation 6.1): Was Startmenue und Quick-Launch-Auswahl zeigen. Echt
// sind die Registry aus der JSON (P3), Pebbles Workout-App mit P2 (workout.c), Kieselsports
// Prozess-Eintrag (kieselsport.c), der App-Speicher (app_install_manager.c, app_db.c) und die
// Liste des Startmenues (app_menu_data_source.c). Die Filter stehen in launcher.c:62 und
// quick_launch_app_menu.c:57-71 (static); beide verwerfen, was app_install_entry_is_hidden()
// meldet - so filtern auch Pebbles eigene Tests (test_app_menu_data_source.c).
//
// Gerechnet wird mit dem Board, nicht mit dem Test: Mit dem Schalter (obelix, qemu_emery) zeigt
// das Startmenue Kieselsport genau einmal und Workout nicht; ohne (asterix, getafix, qemu_flint,
// qemu_gabbro) Workout und kein Kieselsport.

const PebbleProcessMd *workout_app_get_info(void);

static const Uuid s_workout_uuid = {0xfe, 0xf8, 0x2c, 0x82, 0x71, 0x76, 0x4e, 0x22,
                                    0x88, 0xde, 0x35, 0xa3, 0xfc, 0x18, 0xd4, 0x3f};
static const Uuid s_kieselsport_uuid = KIESELSPORT_UUID_INIT;

// --- Die anderen System-Apps der Registry ---
//
// Fuer sie zaehlt hier nur, dass es sie gibt: ein sichtbarer Eintrag ohne eigene Kennung. Schwach,
// damit eine echte Definition (workout.c, kieselsport.c) vorgeht. Fehlt ein Name, den die JSON
// neu bekommt, bricht das Linken - laut, nicht still.
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
ANDERE(battery_critical_get_app_info)
ANDERE(health_app_get_info)
ANDERE(send_text_app_get_info)
ANDERE(reminder_app_get_info)
ANDERE(quiet_time_toggle_get_app_info)
ANDERE(backlight_state_toggle_get_app_info)
ANDERE(motion_backlight_toggle_get_app_info)
ANDERE(airplane_mode_toggle_get_app_info)
// Sports steht in Pebbles Liste ganz vorn (app_menu_data_source.c, s_override_table), ist aber nur
// sichtbar, solange eine Sport-App mit der Uhr spricht (workout/sports.c:339). So hier auch -
// sonst stuende es im Test immer ueber allem.
static const PebbleProcessMdSystem s_sports = {
    .common = {.visibility = ProcessVisibilityShownOnCommunication}, .name = "Sports"};
const PebbleProcessMd *sports_app_get_info(void) { return &s_sports.common; }
ANDERE(timeline_full_get_app_info)
ANDERE(notifications_clear_history_app_get_info)
ANDERE(quick_launch_nothing_get_app_info)
// Ohne den Schalter fuehrt die Registry Kieselsport nicht, und kieselsport.c bleibt leer.
ANDERE(kieselsport_app_get_info)

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
  int workout;
  int kieselsport;
  AppInstallId kieselsport_ids[4];
} Zaehlung;

static Zaehlung prv_zaehle(AppMenuFilterCallback filter) {
  static MenuLayer menue;
  AppMenuDataSource liste;
  app_menu_data_source_init(&liste, &(AppMenuDataSourceCallbacks){.filter = filter}, &menue);
  Zaehlung z = {0};
  const uint16_t n = app_menu_data_source_get_count(&liste);
  for (uint16_t i = 0; i < n; i++) {
    AppMenuNode *node = app_menu_data_source_get_node_at_index(&liste, i);
    cl_assert(node);
    z.workout += uuid_equal(&node->uuid, &s_workout_uuid);
    if (uuid_equal(&node->uuid, &s_kieselsport_uuid)) {
      cl_assert(z.kieselsport < (int)ARRAY_LENGTH(z.kieselsport_ids));
      z.kieselsport_ids[z.kieselsport++] = node->install_id;
    }
  }
  app_menu_data_source_deinit(&liste);
  return z;
}

// Die alte Store-App mit der Kennung der System-App, wie Boulder sie in den App-Speicher der Uhr
// gelegt hat (V9). Die Werte stehen im Kopf von Kieselsport 0.18.1 (9261244,
// build/emery/pebble-app.bin): SDK 5.106, Fassung 0.18, Flags 0x158 (JS, Worker, emery).
static AppInstallId prv_store_app_installieren(void) {
  AppDBEntry store = {
    .name = "Kieselsport",
    .uuid = KIESELSPORT_UUID_INIT,
    .info_flags = PROCESS_INFO_ALLOW_JS | PROCESS_INFO_HAS_WORKER | PROCESS_INFO_PLATFORM_EMERY,
    .app_version = {.major = 0, .minor = 18},
    .sdk_version = {.major = 5, .minor = 106},
  };
  cl_assert_equal_i(app_db_insert((uint8_t *)&store.uuid, sizeof(Uuid), (uint8_t *)&store,
                                  sizeof(store)), S_SUCCESS);
  const AppInstallId id = app_db_get_install_id_for_uuid(&store.uuid);
  app_cache_add_entry(id, 10701);
  return id;
}

void test_kieselsport_startmenue__initialize(void) {
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

void test_kieselsport_startmenue__cleanup(void) {
  s_head_callback_node_list = NULL;
  app_install_manager_flush_recent_communication_timestamps();
}

// Die Registry des Boards: Workout (-95) immer, Kieselsport (-103) nur mit der Variante.
void test_kieselsport_startmenue__registry(void) {
  int workout = 0, kieselsport = 0;
  for (size_t i = 0; i < ARRAY_LENGTH(APP_RECORDS); i++) {
    workout += APP_RECORDS[i].id == -95;
    kieselsport += APP_RECORDS[i].id == -103;
  }
  cl_assert_equal_i(workout, 1);
#ifdef CONFIG_BOULDER_KIESELSPORT
  cl_assert_equal_i(kieselsport, 1);
  cl_assert_equal_i(app_install_get_id_for_uuid(&s_kieselsport_uuid), -103);
#else
  cl_assert_equal_i(kieselsport, 0);
  cl_assert_equal_i(app_install_get_id_for_uuid(&s_kieselsport_uuid), INSTALL_ID_INVALID);
#endif
}

// T6.1: Startmenue und Quick-Launch-Auswahl.
void test_kieselsport_startmenue__nur_kieselsport_mit_variante(void) {
  const Zaehlung start = prv_zaehle(prv_startmenue);
  const Zaehlung schnell = prv_zaehle(prv_quick_launch);
#ifdef CONFIG_BOULDER_KIESELSPORT
  cl_assert_equal_i(start.workout, 0);
  cl_assert_equal_i(start.kieselsport, 1);
  cl_assert_equal_i(start.kieselsport_ids[0], -103);
  cl_assert_equal_i(schnell.workout, 0);
  cl_assert_equal_i(schnell.kieselsport, 1);
#else
  cl_assert_equal_i(start.workout, 1);
  cl_assert_equal_i(start.kieselsport, 0);
  cl_assert_equal_i(schnell.workout, 1);
  cl_assert_equal_i(schnell.kieselsport, 0);
#endif
}

// P2 versteckt nur: Workout bleibt startbar, etwa ueber Pebbles Erinnerung "End/Open Workout"
// (apps/system/workout/utils.c:81), und ohne Variante bleibt es die sichtbare Trainings-App.
void test_kieselsport_startmenue__workout_bleibt_startbar(void) {
  AppInstallEntry e;
  cl_assert(app_install_get_entry_for_install_id(-95, &e));
  cl_assert(uuid_equal(&e.uuid, &s_workout_uuid));
  cl_assert_equal_p(app_install_get_md(-95, false), workout_app_get_info());
#ifdef CONFIG_BOULDER_KIESELSPORT
  cl_assert(app_install_entry_is_hidden(&e));
#else
  cl_assert(!app_install_entry_is_hidden(&e));
#endif
}

// V9 (bestaetigt: nicht ausblenden, nur pruefen): Liegt die alte Store-App mit derselben Kennung
// im App-Speicher, steht Kieselsport mit der Variante zweimal im Startmenue und in der
// Quick-Launch-Auswahl - die System-App (-103) und die Store-App. Gestartet ueber die Kennung
// (Boulder, Pin, Meldung) wird die System-App, denn die Registry geht vor
// (app_install_manager.c:300-301); der zweite Eintrag startet die alte Store-App. Darum die
// Release-Notiz: die Store-App einmal loeschen. Ohne Variante ist es der eine Eintrag der
// Store-App, wie vor b25.
void test_kieselsport_startmenue__alte_store_app_doppelt(void) {
  const AppInstallId store = prv_store_app_installieren();
  cl_assert(store > 0);
  const Zaehlung start = prv_zaehle(prv_startmenue);
  const Zaehlung schnell = prv_zaehle(prv_quick_launch);
#ifdef CONFIG_BOULDER_KIESELSPORT
  cl_assert_equal_i(start.kieselsport, 2);
  cl_assert_equal_i(schnell.kieselsport, 2);
  cl_assert(start.kieselsport_ids[0] == -103 || start.kieselsport_ids[1] == -103);
  cl_assert(start.kieselsport_ids[0] == store || start.kieselsport_ids[1] == store);
  cl_assert_equal_i(app_install_get_id_for_uuid(&s_kieselsport_uuid), -103);
#else
  cl_assert_equal_i(start.kieselsport, 1);
  cl_assert_equal_i(start.kieselsport_ids[0], store);
  cl_assert_equal_i(app_install_get_id_for_uuid(&s_kieselsport_uuid), store);
#endif
  cl_assert_equal_i(start.workout, schnell.workout);
}

// Der Platz einer App im Startmenue (0 = ganz oben), -1 = nicht da.
static int prv_platz(const Uuid *uuid) {
  static MenuLayer menue;
  AppMenuDataSource liste;
  app_menu_data_source_init(&liste, &(AppMenuDataSourceCallbacks){.filter = prv_startmenue},
                            &menue);
  int platz = -1;
  const uint16_t n = app_menu_data_source_get_count(&liste);
  for (uint16_t i = 0; i < n && platz < 0; i++) {
    AppMenuNode *node = app_menu_data_source_get_node_at_index(&liste, i);
    cl_assert(node);
    if (uuid_equal(&node->uuid, uuid)) {
      platz = i;
    }
  }
  app_menu_data_source_deinit(&liste);
  return platz;
}

// P5 (Entscheid 3 vom 08.10.2026): Mit dem Start markiert der Dienst Kieselsport ohne Ablauf
// (plattform.c, ks_startmenue_oben), und das Startmenue stellt es ganz nach oben - wie Pebble es
// mit Workout tut. Auch nach Stunden. Nach dem Ende (mit Ablauf) bleibt es Pebbles Frist von fuenf
// Minuten oben, dann steht es wieder an seinem Platz. Ohne Variante gilt das fuer Workout wie bei
// Pebble.
void test_kieselsport_startmenue__im_training_oben(void) {
#ifdef CONFIG_BOULDER_KIESELSPORT
  const Uuid *app = &s_kieselsport_uuid;
#else
  const Uuid *app = &s_workout_uuid;
#endif
  const AppInstallId id = app_install_get_id_for_uuid(app);
  const int platz = prv_platz(app);
  cl_assert(platz > 0);
  app_install_mark_prioritized(id, false /* can_expire: das Training laeuft */);
  cl_assert_equal_i(prv_platz(app), 0);
  rtc_set_time(rtc_get_time() + 3 * 60 * 60);
  cl_assert_equal_i(prv_platz(app), 0);
  app_install_mark_prioritized(id, true /* can_expire: das Training ist vorbei */);
  rtc_set_time(rtc_get_time() + 5 * 60 - 1);
  cl_assert_equal_i(prv_platz(app), 0);
  rtc_set_time(rtc_get_time() + 1);
  cl_assert_equal_i(prv_platz(app), platz);
}
