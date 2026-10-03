/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "apps/watch/timestyle/timestyle.h"
#include "pbl/services/filesystem/pfs.h"
#include "pbl/util/uuid.h"
#include "process_management/app_install_manager.h"
#include "shell/normal/watchface.h"
#include "shell/prefs.h"
#include "shell/prefs_private.h"

#include <string.h>

// Ohne Kopfzeile, wie in shell/normal/shell_event_loop.c.
extern void shell_prefs_init(void);

// Die einmalige Umstellung auf das System-TimeStyle (prefs.c,
// prv_boulder_timestyle_einmal): wann sie greift, dass sie nur einmal greift
// und dass eine eigene Wahl danach bleibt - auch ueber einen Neustart.

// Fakes
////////////////////////////////////////////////////////////////
#include "fake_spi_flash.h"
#include "fake_system_task.h"
#include "fake_kernel_services_notifications.h"

// Stubs (ohne stubs_app_install_manager.h: die App-Liste ist unten nachgebaut)
////////////////////////////////////////////////////////////////
#include "stubs_analytics.h"
#include "stubs_event_loop.h"
#include "stubs_hexdump.h"
#include "stubs_logging.h"
#include "stubs_mfg_info.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_prompt.h"
#include "stubs_rand_ptr.h"
#include "stubs_sleep.h"
#include "stubs_system_theme.h"
#include "stubs_task_watchdog.h"
#include "stubs_timeline_peek.h"
#include "stubs_ambient_light.h"
#include "stubs_activity.h"

void prefs_sync_init(void) {
}

void event_put(PebbleEvent *event) {
}

void i18n_enable(bool enable) {
}

// --- Nachgebaute App-Liste ---

#define ID_TIMESTYLE (-104)
#define ID_TICTOC (-69)
#define ID_STORE_TIMESTYLE 7
#define ID_EIGENES 8
#define ID_APP 9

static const Uuid s_uuid_timestyle = TIMESTYLE_UUID_INIT;
static const Uuid s_uuid_store = TIMESTYLE_STORE_UUID_INIT;
// apps/watch/tictoc/tictoc.c
static const Uuid s_uuid_tictoc = {0x8f, 0x3c, 0x86, 0x86, 0x31, 0xa1, 0x4f, 0x5f,
                                   0x91, 0xf5, 0x01, 0x60, 0x0c, 0x9b, 0xdc, 0x59};
static const Uuid s_uuid_eigenes = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
                                    0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x01};
static const Uuid s_uuid_app = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};
// Ein Zifferblatt, das einmal Standard war und inzwischen geloescht ist.
static const Uuid s_uuid_geloescht = {0xde, 0xad, 0xbe, 0xef, 0, 0, 0, 0,
                                      0, 0, 0, 0, 0, 0, 0, 0x42};

typedef struct {
  AppInstallId id;
  const Uuid *uuid;
  bool zifferblatt;
} FakeApp;

// Reihenfolge wie app_install_enumerate_entries: System-Apps nach der
// Registry (TimeStyle steht dort vorn), dann die aus dem Store.
static const FakeApp s_apps_alle[] = {
  {ID_TIMESTYLE, &s_uuid_timestyle, true},
  {ID_TICTOC, &s_uuid_tictoc, true},
  {ID_STORE_TIMESTYLE, &s_uuid_store, true},
  {ID_EIGENES, &s_uuid_eigenes, true},
  {ID_APP, &s_uuid_app, false},
};
static size_t s_anzahl_apps;
static bool s_store_installiert;

static const FakeApp *prv_app(AppInstallId id) {
  for (size_t i = 0; i < s_anzahl_apps; i++) {
    if (s_apps_alle[i].id == id) {
      if (id == ID_STORE_TIMESTYLE && !s_store_installiert) {
        return NULL;
      }
      return &s_apps_alle[i];
    }
  }
  return NULL;
}

AppInstallId app_install_get_id_for_uuid(const Uuid *uuid) {
  for (size_t i = 0; i < s_anzahl_apps; i++) {
    if (uuid_equal(uuid, s_apps_alle[i].uuid) && prv_app(s_apps_alle[i].id)) {
      return s_apps_alle[i].id;
    }
  }
  return INSTALL_ID_INVALID;
}

bool app_install_get_entry_for_install_id(AppInstallId install_id, AppInstallEntry *entry) {
  const FakeApp *app = prv_app(install_id);
  if (!app) {
    return false;
  }
  *entry = (AppInstallEntry){
    .install_id = app->id,
    .uuid = *app->uuid,
    .process_type = app->zifferblatt ? ProcessTypeWatchface : ProcessTypeApp,
    .visibility = ProcessVisibilityShown,
  };
  return true;
}

bool app_install_get_uuid_for_install_id(AppInstallId install_id, Uuid *uuid_out) {
  AppInstallEntry entry;
  if (app_install_get_entry_for_install_id(install_id, &entry)) {
    *uuid_out = entry.uuid;
    return true;
  }
  *uuid_out = UUID_INVALID;
  return false;
}

bool app_install_entry_is_watchface(const AppInstallEntry *entry) {
  return entry->process_type == ProcessTypeWatchface;
}

bool app_install_entry_is_hidden(const AppInstallEntry *entry) {
  return false;
}

// false: TimeStyle kommt beim Aufzaehlen zuletzt. So zeigt sich, ob die
// Umstellung auch ohne den Rueckfall auf "das erste Zifferblatt" greift.
static bool s_timestyle_vorn;

void app_install_enumerate_entries(AppInstallEnumerateCb cb, void *data) {
  for (size_t n = 0; n < s_anzahl_apps; n++) {
    const size_t i = s_timestyle_vorn ? n : (n + 1) % s_anzahl_apps;
    AppInstallEntry entry;
    if (app_install_get_entry_for_install_id(s_apps_alle[i].id, &entry) && !cb(&entry, data)) {
      return;
    }
  }
}

bool app_install_id_from_system(AppInstallId id) {
  return id < INSTALL_ID_INVALID;
}

bool app_install_id_from_app_db(AppInstallId id) {
  return id > INSTALL_ID_INVALID;
}

// --- Hilfen ---

#define KEY_ZIFFERBLATT "watchface"
#define KEY_MERKER "boulderTimeStyle"

// Legt den gespeicherten Stand an und "startet die Uhr neu": shell_prefs_init
// liest die Einstellungen aus dem Flash. Beide Schluessel werden immer
// geschrieben, weil die statischen Werte in prefs.c sonst aus dem vorigen
// Test stehen blieben; UUID_INVALID und 0 sind genau das, was eine frische
// Uhr ohne gespeicherte Werte hat.
static void prv_gespeichert(const Uuid *zifferblatt, uint8_t merker) {
  cl_assert(prefs_private_write_backing((const uint8_t *)KEY_ZIFFERBLATT,
                                        strlen(KEY_ZIFFERBLATT), zifferblatt, sizeof(Uuid)));
  cl_assert(prefs_private_write_backing((const uint8_t *)KEY_MERKER, strlen(KEY_MERKER), &merker,
                                        sizeof(merker)));
  shell_prefs_init();
}

static void prv_neustart(void) {
  shell_prefs_init();
}

static Uuid prv_gespeichertes_zifferblatt(void) {
  Uuid uuid;
  cl_assert(prefs_private_read_backing((const uint8_t *)KEY_ZIFFERBLATT, strlen(KEY_ZIFFERBLATT),
                                       &uuid, sizeof(uuid)));
  return uuid;
}

static uint8_t prv_gespeicherter_merker(void) {
  uint8_t merker = 0xff;
  cl_assert(prefs_private_read_backing((const uint8_t *)KEY_MERKER, strlen(KEY_MERKER), &merker,
                                       sizeof(merker)));
  return merker;
}

#define cl_assert_uuid(a, b) cl_assert(uuid_equal((a), (b)))

void test_timestyle_standard__initialize(void) {
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  s_anzahl_apps = sizeof(s_apps_alle) / sizeof(s_apps_alle[0]);
  s_store_installiert = true;
  s_timestyle_vorn = true;
}

void test_timestyle_standard__cleanup(void) {
}

// --- Wann umgestellt wird ---

void test_timestyle_standard__frische_uhr_bekommt_timestyle(void) {
  prv_gespeichert(&UUID_INVALID, 0);

  cl_assert_equal_i(watchface_get_default_install_id(), ID_TIMESTYLE);
  const Uuid gespeichert = prv_gespeichertes_zifferblatt();
  cl_assert_uuid(&gespeichert, &s_uuid_timestyle);
  cl_assert_equal_i(prv_gespeicherter_merker(), 1);
}

void test_timestyle_standard__tictoc_wird_timestyle(void) {
  prv_gespeichert(&s_uuid_tictoc, 0);

  cl_assert_equal_i(watchface_get_default_install_id(), ID_TIMESTYLE);
  const Uuid gespeichert = prv_gespeichertes_zifferblatt();
  cl_assert_uuid(&gespeichert, &s_uuid_timestyle);
  cl_assert_equal_i(prv_gespeicherter_merker(), 1);
}

// Der Fall des Nutzers: die Store-Fassung (mit PebbleKit JS) ist Standard.
void test_timestyle_standard__store_timestyle_wird_system_timestyle(void) {
  prv_gespeichert(&s_uuid_store, 0);

  cl_assert_equal_i(watchface_get_default_install_id(), ID_TIMESTYLE);
  const Uuid gespeichert = prv_gespeichertes_zifferblatt();
  cl_assert_uuid(&gespeichert, &s_uuid_timestyle);
  cl_assert_equal_i(prv_gespeicherter_merker(), 1);
}

void test_timestyle_standard__store_timestyle_schon_entfernt(void) {
  s_store_installiert = false;
  prv_gespeichert(&s_uuid_store, 0);

  cl_assert_equal_i(watchface_get_default_install_id(), ID_TIMESTYLE);
  const Uuid gespeichert = prv_gespeichertes_zifferblatt();
  cl_assert_uuid(&gespeichert, &s_uuid_timestyle);
}

void test_timestyle_standard__geloeschtes_zifferblatt_wird_timestyle(void) {
  prv_gespeichert(&s_uuid_geloescht, 0);

  cl_assert_equal_i(watchface_get_default_install_id(), ID_TIMESTYLE);
  const Uuid gespeichert = prv_gespeichertes_zifferblatt();
  cl_assert_uuid(&gespeichert, &s_uuid_timestyle);
}

// Fehlt das gespeicherte Zifferblatt, wird es TimeStyle - auch wenn TimeStyle
// nicht vorn in der Registry stuende (sonst naehme der Rueckfall der Firmware
// das erste Zifferblatt, hier TicToc).
void test_timestyle_standard__fehlend_wird_timestyle_unabhaengig_von_der_reihenfolge(void) {
  s_timestyle_vorn = false;

  prv_gespeichert(&s_uuid_geloescht, 0);
  cl_assert_equal_i(watchface_get_default_install_id(), ID_TIMESTYLE);

  prv_gespeichert(&UUID_INVALID, 0);
  cl_assert_equal_i(watchface_get_default_install_id(), ID_TIMESTYLE);
}

// --- Wann nicht ---

void test_timestyle_standard__eigenes_zifferblatt_bleibt(void) {
  prv_gespeichert(&s_uuid_eigenes, 0);

  cl_assert_equal_i(watchface_get_default_install_id(), ID_EIGENES);
  const Uuid gespeichert = prv_gespeichertes_zifferblatt();
  cl_assert_uuid(&gespeichert, &s_uuid_eigenes);
  // Auch hier gilt die Umstellung als erledigt: ein spaeterer Wechsel auf
  // TicToc ist dann eine Wahl des Nutzers.
  cl_assert_equal_i(prv_gespeicherter_merker(), 1);
}

void test_timestyle_standard__nach_dem_merker_bleibt_tictoc(void) {
  prv_gespeichert(&s_uuid_tictoc, 1);

  cl_assert_equal_i(watchface_get_default_install_id(), ID_TICTOC);
  const Uuid gespeichert = prv_gespeichertes_zifferblatt();
  cl_assert_uuid(&gespeichert, &s_uuid_tictoc);
}

void test_timestyle_standard__nach_dem_merker_bleibt_store_timestyle(void) {
  prv_gespeichert(&s_uuid_store, 1);

  cl_assert_equal_i(watchface_get_default_install_id(), ID_STORE_TIMESTYLE);
}

// --- Nur einmal ---

void test_timestyle_standard__spaetere_wahl_bleibt_auch_nach_neustart(void) {
  prv_gespeichert(&s_uuid_tictoc, 0);
  cl_assert_equal_i(watchface_get_default_install_id(), ID_TIMESTYLE);

  // Der Nutzer waehlt in der Zifferblatt-Liste wieder TicToc.
  watchface_set_default_install_id(ID_TICTOC);
  cl_assert_equal_i(watchface_get_default_install_id(), ID_TICTOC);

  prv_neustart();
  cl_assert_equal_i(watchface_get_default_install_id(), ID_TICTOC);
  const Uuid gespeichert = prv_gespeichertes_zifferblatt();
  cl_assert_uuid(&gespeichert, &s_uuid_tictoc);
  cl_assert_equal_i(prv_gespeicherter_merker(), 1);
}

// Faellt das gewaehlte Zifferblatt spaeter weg (geloescht), nimmt die Firmware
// das erste der Liste - das ist TimeStyle, ohne die Umstellung noch einmal.
void test_timestyle_standard__rueckfall_nach_dem_merker_trifft_timestyle(void) {
  prv_gespeichert(&s_uuid_geloescht, 1);

  cl_assert_equal_i(watchface_get_default_install_id(), ID_TIMESTYLE);
  const Uuid gespeichert = prv_gespeichertes_zifferblatt();
  cl_assert_uuid(&gespeichert, &s_uuid_timestyle);
}
