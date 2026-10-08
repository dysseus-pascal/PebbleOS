/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "stubs_logging.h"
#include "stubs_passert.h"

#include "apps/system/launcher/default/app_glance_kieselsport.h"
#include "apps/system/launcher/default/app_glance_structured.h"
#include "apps/system/kieselsport/kachel.h"
#include "resource/resource_ids.auto.h"
#include "pbl/util/size.h"

// Die Kachel von Kieselsport im Startmenue (app_glance_kieselsport.c, P6): wann sie jede Sekunde
// nachfragt (die zwei Tests aus b25, main tests/fw/apps/system_apps/kieselsport/) und was sie
// zeigt (neu: die Zeile und das Bild). Nachgebaut sind der Dienst, die Wecker und das Startmenue
// darum herum - die Kachel selbst ist die echte. Dass Pebbles Startmenue sie fuer Kieselsport
// nimmt, prueft test_kieselsport_kachel_haken.

// --- Der Dienst ---

static KsStand s_stand;
void kieselsport_stand(KsStand *aus) { *aus = s_stand; }
bool kieselsport_laeuft(void) { return s_stand.zustand != LaufAus; }

// --- Die App ---

const char *art_name(Sportart art) {
  switch (art) {
    case ArtLaufen:
      return "Run";
    case ArtBike:
      return "Bike";
    case ArtWandern:
      return "Hike";
    default:
      return "Andere";
  }
}
void strings_refresh(void) {}

// --- Wecker ---

static AppTimerCallback s_takt_cb;
static void *s_takt_daten;
static int s_takte;
AppTimer *app_timer_register_repeatable(uint32_t timeout_ms, AppTimerCallback cb, void *daten,
                                        bool repeating) {
  cl_assert(repeating);
  cl_assert_equal_i(timeout_ms, 1000);
  cl_assert(!s_takt_cb);
  s_takt_cb = cb;
  s_takt_daten = daten;
  s_takte++;
  return (AppTimer *)(uintptr_t)s_takte;
}
void app_timer_cancel(AppTimer *timer) {
  cl_assert(timer);
  s_takt_cb = NULL;
}

// --- Das Startmenue ---

static LauncherAppGlanceStructured s_kachel;
static int s_geaendert;
static GTextNodeTextDynamicUpdate s_zeile_fuellen;
LauncherAppGlanceStructured *launcher_app_glance_structured_create(
    const Uuid *uuid, const LauncherAppGlanceStructuredImpl *impl, bool should_consider_slices,
    void *data) {
  memset(&s_kachel, 0, sizeof(s_kachel));
  s_kachel.impl = impl;
  s_kachel.data = data;
  return &s_kachel;
}
void *launcher_app_glance_structured_get_data(LauncherAppGlanceStructured *structured_glance) {
  return structured_glance->data;
}
void launcher_app_glance_structured_notify_service_glance_changed(
    LauncherAppGlanceStructured *structured_glance) {
  s_geaendert++;
}
GTextNode *launcher_app_glance_structured_create_subtitle_text_node(
    LauncherAppGlanceStructured *structured_glance, GTextNodeTextDynamicUpdate update) {
  s_zeile_fuellen = update;
  return NULL;
}
static int s_bild;
static uint32_t s_bild_id;
KinoReel *kino_reel_create_with_resource(uint32_t resource_id) {
  s_bild_id = resource_id;
  return (KinoReel *)&s_bild;
}
void kino_reel_destroy(KinoReel *reel) {}
void *app_zalloc_check(size_t bytes) { return calloc(1, bytes); }
void app_free(void *ptr) { free(ptr); }

static AppMenuNode s_knoten = { .name = "Kieselsport" };

// Die Zeile unter dem Namen, wie Pebbles Startmenue sie beim Zeichnen holt
// (app_glance_structured.c: der Rueckruf mit der Kachel als user_data).
static const char *prv_zeile(void) {
  static char zeile[64];
  memset(zeile, 'x', sizeof(zeile));
  cl_assert(s_kachel.impl->create_subtitle_node);
  s_kachel.impl->create_subtitle_node(&s_kachel);
  cl_assert(s_zeile_fuellen);
  s_zeile_fuellen(NULL, NULL, NULL, NULL, true, zeile, sizeof(zeile), &s_kachel);
  return zeile;
}

void test_kieselsport_kachel__initialize(void) {
  memset(&s_stand, 0, sizeof(s_stand));
  s_takt_cb = NULL;
  s_takte = 0;
  s_geaendert = 0;
  s_zeile_fuellen = NULL;
  s_bild_id = 0;
}

void test_kieselsport_kachel__cleanup(void) {
  if (s_kachel.impl) s_kachel.impl->destructor(&s_kachel);
  s_kachel.impl = NULL;
}

// --- Aus b25 ---

void test_kieselsport_kachel__ohne_training_kein_takt(void) {
  launcher_app_glance_kieselsport_create(&s_knoten);
  cl_assert_equal_i(s_takte, 0);
}

void test_kieselsport_kachel__waehrend_des_trainings_jede_sekunde(void) {
  s_stand.zustand = LaufLaeuft;
  s_stand.art = ArtLaufen;
  s_stand.t.dauer_s = 65;
  launcher_app_glance_kieselsport_create(&s_knoten);
  cl_assert_equal_i(s_takte, 1);
  cl_assert(s_takt_cb);
  const int vorher = s_geaendert;
  s_stand.t.dauer_s = 66;
  s_takt_cb(s_takt_daten);
  cl_assert_equal_i(s_geaendert, vorher + 1);
  // Das Training endet (auch von selbst): der Takt hoert auf.
  s_stand.zustand = LaufAus;
  s_takt_cb(s_takt_daten);
  cl_assert(!s_takt_cb);
}

// --- Neu: was die Kachel zeigt (Entscheid 3 vom 08.10.2026, wie b25) ---

// Im Training: Art und laufende Zeit h:mm:ss; in der Pause steht die Zeit, davor "|| ".
void test_kieselsport_kachel__zeile_mit_der_laufzeit(void) {
  s_stand.zustand = LaufLaeuft;
  s_stand.art = ArtLaufen;
  s_stand.t.dauer_s = 65;
  launcher_app_glance_kieselsport_create(&s_knoten);
  cl_assert_equal_s(s_kachel.impl->get_title(&s_kachel), "Kieselsport");
  cl_assert_equal_s(prv_zeile(), "Run 0:01:05");
  s_stand.t.dauer_s = 3600 + 2 * 60 + 5;
  s_takt_cb(s_takt_daten);
  cl_assert_equal_s(prv_zeile(), "Run 1:02:05");
  s_stand.zustand = LaufPause;
  const int vorher = s_geaendert;
  s_takt_cb(s_takt_daten);
  cl_assert_equal_i(s_geaendert, vorher + 1);
  cl_assert_equal_s(prv_zeile(), "|| Run 1:02:05");
  // Die Pause dauert, die Zeit steht: keine Aenderung, kein Neuzeichnen.
  s_takt_cb(s_takt_daten);
  cl_assert_equal_i(s_geaendert, vorher + 1);
  s_stand.zustand = LaufLaeuft;
  s_stand.t.dauer_s += 1;
  s_takt_cb(s_takt_daten);
  cl_assert_equal_s(prv_zeile(), "Run 1:02:06");
  // Nach dem Ende: keine Zeile mehr.
  s_stand.zustand = LaufAus;
  s_takt_cb(s_takt_daten);
  cl_assert_equal_s(prv_zeile(), "");
}

// Laeuft nichts, hat die Kachel nur Namen und Bild, keine Zeile.
void test_kieselsport_kachel__ohne_training_keine_zeile(void) {
  launcher_app_glance_kieselsport_create(&s_knoten);
  cl_assert_equal_s(prv_zeile(), "");
  cl_assert_equal_i(s_bild_id, RESOURCE_ID_ACTIVITY_TINY);
}

// Das Bild: Laufen und Wandern wie Pebbles Workout-Kachel, alles andere das Herz.
void test_kieselsport_kachel__bild_nach_der_art(void) {
  static const struct {
    Sportart art;
    uint32_t bild;
  } s_faelle[] = {
    {ArtLaufen, RESOURCE_ID_WORKOUT_APP_RUN_TINY},
    {ArtWandern, RESOURCE_ID_WORKOUT_APP_WALK_TINY},
    {ArtBike, RESOURCE_ID_WORKOUT_APP_HEART},
    {ArtSchwimmen, RESOURCE_ID_WORKOUT_APP_HEART},
  };
  for (size_t i = 0; i < ARRAY_LENGTH(s_faelle); i++) {
    test_kieselsport_kachel__initialize();
    s_stand.zustand = LaufLaeuft;
    s_stand.art = s_faelle[i].art;
    launcher_app_glance_kieselsport_create(&s_knoten);
    cl_assert_equal_i(s_bild_id, s_faelle[i].bild);
    cl_assert(s_kachel.impl->get_icon(&s_kachel) == (KinoReel *)&s_bild);
    test_kieselsport_kachel__cleanup();
  }
}
