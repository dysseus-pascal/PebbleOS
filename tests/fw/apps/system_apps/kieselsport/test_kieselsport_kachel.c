/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "stubs_logging.h"
#include "stubs_passert.h"

#include "apps/system/launcher/default/app_glance_kieselsport.h"
#include "apps/system/launcher/default/app_glance_structured.h"
#include "apps/system/kieselsport/kachel.h"

// Die Kachel von Kieselsport im Startmenue (app_glance_kieselsport.c): wann
// sie jede Sekunde nachfragt. Nachgebaut sind der Dienst, die Wecker und das
// Startmenue darum herum - die Kachel selbst ist die echte.

// --- Der Dienst ---

static KsStand s_stand;
void kieselsport_stand(KsStand *aus) { *aus = s_stand; }
bool kieselsport_laeuft(void) { return s_stand.zustand != LaufAus; }

// --- Die App ---

const char *art_name(Sportart art) { return "Run"; }
void strings_refresh(void) {}

// --- Wecker ---

static AppTimerCallback s_takt_cb;
static void *s_takt_daten;
static int s_takte;
AppTimer *app_timer_register_repeatable(uint32_t timeout_ms, AppTimerCallback cb, void *daten,
                                        bool repeating) {
  cl_assert(repeating);
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
  return NULL;
}
static int s_bild;
KinoReel *kino_reel_create_with_resource(uint32_t resource_id) {
  return (KinoReel *)&s_bild;
}
void kino_reel_destroy(KinoReel *reel) {}
void *app_zalloc_check(size_t bytes) { return calloc(1, bytes); }
void app_free(void *ptr) { free(ptr); }

static AppMenuNode s_knoten = { .name = "Kieselsport" };

void test_kieselsport_kachel__initialize(void) {
  memset(&s_stand, 0, sizeof(s_stand));
  s_takt_cb = NULL;
  s_takte = 0;
  s_geaendert = 0;
}

void test_kieselsport_kachel__cleanup(void) {
  if (s_kachel.impl) s_kachel.impl->destructor(&s_kachel);
  s_kachel.impl = NULL;
}

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
