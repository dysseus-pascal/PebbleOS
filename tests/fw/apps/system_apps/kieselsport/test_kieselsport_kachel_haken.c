/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "apps/system/launcher/default/app_glance_service.h"
#include "apps/system/launcher/default/app_glance.h"
#include "pbl/services/activity/boulder/sport/kieselsport.h"

#include "stubs_app_timer.h"
#include "stubs_event_service_client.h"
#include "stubs_kino_player.h"
#include "stubs_logging.h"
#include "stubs_passert.h"

#include <stdlib.h>

// P6 (Entscheid 3 vom 08.10.2026): Pebbles Startmenue nimmt fuer Kieselsport die eigene Kachel
// (app_glance_kieselsport.c) statt der allgemeinen. Echt ist app_glance_service.c, wie es auf dem
// Board gebaut wird: mit CONFIG_BOULDER_KIESELSPORT wie obelix und qemu_emery, ohne wie asterix
// (CMakeLists.txt). Die Kacheln selbst sind hier nur Zeugen, wer gerufen wurde; was die
// Kieselsport-Kachel zeigt, prueft test_kieselsport_kachel.

typedef enum {
  KachelKeine,
  KachelAllgemein,
  KachelWorkout,
  KachelKieselsport,
  KachelAndere,
} Kachel;

typedef struct {
  LauncherAppGlance glance;
  Kachel art;
} Zeuge;

static Kachel s_gebaut;
static Kachel s_gezeichnet;
static int s_abgebaut;

static LauncherAppGlance *prv_zeuge(const AppMenuNode *node, Kachel art) {
  Zeuge *z = calloc(1, sizeof(*z));
  z->glance.uuid = node->uuid;
  z->art = art;
  s_gebaut = art;
  return &z->glance;
}

#define ANDERE(fn) \
  LauncherAppGlance *fn(const AppMenuNode *node) { return prv_zeuge(node, KachelAndere); }
ANDERE(launcher_app_glance_settings_create)
ANDERE(launcher_app_glance_music_create)
ANDERE(launcher_app_glance_weather_create)
ANDERE(launcher_app_glance_notifications_create)
ANDERE(launcher_app_glance_alarms_create)
ANDERE(launcher_app_glance_watchfaces_create)

LauncherAppGlance *launcher_app_glance_workout_create(const AppMenuNode *node) {
  return prv_zeuge(node, KachelWorkout);
}

// Auch ohne den Schalter definiert: Dann darf ihn niemand rufen.
LauncherAppGlance *launcher_app_glance_kieselsport_create(const AppMenuNode *node) {
  return prv_zeuge(node, KachelKieselsport);
}

LauncherAppGlance *launcher_app_glance_generic_create(const AppMenuNode *node,
                                                      const KinoReel *fallback_icon,
                                                      uint32_t fallback_icon_resource_id) {
  return prv_zeuge(node, KachelAllgemein);
}

void launcher_app_glance_draw(GContext *ctx, const GRect *frame, LauncherAppGlance *glance,
                              bool is_highlighted) {
  s_gezeichnet = glance ? ((Zeuge *)glance)->art : KachelKeine;
}

void launcher_app_glance_destroy(LauncherAppGlance *glance) {
  if (glance) {
    s_abgebaut++;
    free(glance);
  }
}

void launcher_app_glance_update_current_slice(LauncherAppGlance *glance) {}

// --- Umfeld ---

static int s_symbol;
KinoReel *kino_reel_create_with_resource(uint32_t resource_id) { return (KinoReel *)&s_symbol; }
void kino_reel_destroy(KinoReel *reel) {}
void *app_zalloc_check(size_t bytes) { return calloc(1, bytes); }
void app_free(void *ptr) { free(ptr); }
time_t rtc_get_time(void) { return 1790445600; }

static LauncherAppGlanceService s_dienst;

static Kachel prv_kachel_fuer(const Uuid *uuid) {
  AppMenuNode node = {.uuid = *uuid, .name = "App"};
  s_gebaut = KachelKeine;
  s_gezeichnet = KachelKeine;
  launcher_app_glance_service_draw_glance_for_app_node(&s_dienst, NULL, &GRect(0, 0, 200, 40),
                                                       false, 0, &node);
  cl_assert_equal_i(s_gezeichnet, s_gebaut);
  return s_gezeichnet;
}

void test_kieselsport_kachel_haken__initialize(void) {
  s_abgebaut = 0;
  launcher_app_glance_service_init(&s_dienst, 1);
}

void test_kieselsport_kachel_haken__cleanup(void) {
  launcher_app_glance_service_deinit(&s_dienst);
}

void test_kieselsport_kachel_haken__kieselsport_bekommt_seine_kachel(void) {
  const Uuid kieselsport = KIESELSPORT_UUID_INIT;
#ifdef CONFIG_BOULDER_KIESELSPORT
  cl_assert_equal_i(prv_kachel_fuer(&kieselsport), KachelKieselsport);
#else
  // Ohne Variante gibt es keine System-App Kieselsport; eine Store-App gleichen Namens bekommt
  // Pebbles allgemeine Kachel wie vor Boulder.
  cl_assert_equal_i(prv_kachel_fuer(&kieselsport), KachelAllgemein);
#endif
}

// Pebbles eigene Kacheln bleiben, wie sie sind.
void test_kieselsport_kachel_haken__andere_unveraendert(void) {
  const Uuid workout = {0xfe, 0xf8, 0x2c, 0x82, 0x71, 0x76, 0x4e, 0x22,
                        0x88, 0xde, 0x35, 0xa3, 0xfc, 0x18, 0xd4, 0x3f};
  const Uuid fremd = {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
                      0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0};
  cl_assert_equal_i(prv_kachel_fuer(&workout), KachelWorkout);
  cl_assert_equal_i(prv_kachel_fuer(&fremd), KachelAllgemein);
}
