/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder: P6, die Kachel aus b25 (main src/fw/apps/system/launcher/default/, 8e8748f5f), hier
// unveraendert bis auf die Pfade von V2. Sie steht in Pebbles Ordner, weil sie dessen Koepfe
// braucht; app_glance_service.c ruft sie nur mit der Variante (Entscheid 3 vom 08.10.2026).
// fw/apps/system/ baut jedes Board: ohne den Schalter bleibt die Datei leer.
#ifdef CONFIG_BOULDER_KIESELSPORT

#include "app_glance_kieselsport.h"

#include "app_glance_structured.h"

#include "apps/system/kieselsport/kachel.h"

#include "kernel/pbl_malloc.h"
#include "process_management/app_install_manager.h"
#include "resource/resource_ids.auto.h"
#include "system/passert.h"
#include "pbl/kernel/compiler.h"
#include "pbl/util/struct.h"

#include <stdio.h>

// Die Zeile unter Kieselsport im Startmenue, solange ein Training laeuft.
//
// WARUM EINE EIGENE KACHEL: der Startmenue-Code nimmt die Glance-Scheiben,
// die eine App selbst schreibt, nur von Store-Apps. Eine System-App bekommt
// die Zeile so nie zu sehen. Also fragt die Kachel den Dienst direkt, jede
// Sekunde - wie die Kachel der Workout-App es mit ihrem Dienst tut.
//
// NUR SOLANGE EIN TRAINING LAEUFT. Beginnen kann eines nur in der App, und
// dafuer geht das Startmenue zu; beim naechsten Oeffnen entsteht die Kachel
// neu. Ohne Training fragte sie sonst jede Sekunde umsonst.

#define UNTERTITEL_MAX (32)

typedef struct LauncherAppGlanceKieselsport {
  char titel[APP_NAME_SIZE_BYTES];
  char untertitel[UNTERTITEL_MAX];
  KinoReel *bild;
  uint32_t bild_id;
  AppTimer *takt;
} LauncherAppGlanceKieselsport;

static KinoReel *prv_bild(LauncherAppGlanceStructured *structured_glance) {
  LauncherAppGlanceKieselsport *k = launcher_app_glance_structured_get_data(structured_glance);
  return NULL_SAFE_FIELD_ACCESS(k, bild, NULL);
}

static const char *prv_titel(LauncherAppGlanceStructured *structured_glance) {
  LauncherAppGlanceKieselsport *k = launcher_app_glance_structured_get_data(structured_glance);
  return NULL_SAFE_FIELD_ACCESS(k, titel, NULL);
}

static void prv_untertitel_fuellen([[maybe_unused]] GContext *ctx, [[maybe_unused]] GTextNode *node,
                                   [[maybe_unused]] const GRect *box,
                                   [[maybe_unused]] const GTextNodeDrawConfig *config,
                                   [[maybe_unused]] bool render, char *buffer, size_t buffer_size,
                                   void *user_data) {
  LauncherAppGlanceStructured *structured_glance = user_data;
  LauncherAppGlanceKieselsport *k = launcher_app_glance_structured_get_data(structured_glance);
  if (k) {
    strncpy(buffer, k->untertitel, buffer_size);
    buffer[buffer_size - 1] = '\0';
  }
}

static GTextNode *prv_untertitel_knoten(LauncherAppGlanceStructured *structured_glance) {
  return launcher_app_glance_structured_create_subtitle_text_node(structured_glance,
                                                                   prv_untertitel_fuellen);
}

static void prv_abbau(LauncherAppGlanceStructured *structured_glance) {
  LauncherAppGlanceKieselsport *k = launcher_app_glance_structured_get_data(structured_glance);
  if (k) {
    kino_reel_destroy(k->bild);
    if (k->takt) app_timer_cancel(k->takt);
  }
  app_free(k);
}

static bool prv_bild_setzen(LauncherAppGlanceKieselsport *k, uint32_t id) {
  if (k->bild_id == id) {
    return false;
  }
  kino_reel_destroy(k->bild);
  k->bild = kino_reel_create_with_resource(id);
  PBL_ASSERTN(k->bild);
  k->bild_id = id;
  return true;
}

// Die Bilder der Workout-App: Laufen und Wandern haben eines, alles andere
// ist das Herz.
static uint32_t prv_bild_fuer(Sportart art) {
  switch (art) {
    case ArtLaufen:
      return RESOURCE_ID_WORKOUT_APP_RUN_TINY;
    case ArtWandern:
      return RESOURCE_ID_WORKOUT_APP_WALK_TINY;
    default:
      return RESOURCE_ID_WORKOUT_APP_HEART;
  }
}

static void prv_takt(void *data) {
  LauncherAppGlanceStructured *structured_glance = data;
  LauncherAppGlanceKieselsport *k = launcher_app_glance_structured_get_data(structured_glance);
  PBL_ASSERTN(k);

  KsStand stand;
  kieselsport_stand(&stand);
  char neu[UNTERTITEL_MAX] = "";
  uint32_t bild = RESOURCE_ID_ACTIVITY_TINY;
  if (stand.zustand != LaufAus) {
    bild = prv_bild_fuer(stand.art);
    const uint32_t s = stand.t.dauer_s;
    // In der Pause steht die Zeit - das sagen die zwei Striche davor.
    snprintf(neu, sizeof(neu), "%s%s %u:%02u:%02u", stand.zustand == LaufPause ? "|| " : "",
             art_name(stand.art), (unsigned)(s / 3600), (unsigned)((s / 60) % 60),
             (unsigned)(s % 60));
  }
  const bool bild_neu = prv_bild_setzen(k, bild);
  if (bild_neu || strcmp(neu, k->untertitel) != 0) {
    strncpy(k->untertitel, neu, sizeof(k->untertitel));
    launcher_app_glance_structured_notify_service_glance_changed(structured_glance);
  }
  // Das Training ist zu Ende (gespeichert, verworfen oder von selbst nach
  // langer Pause): die Zeile ist leer, der Takt hat nichts mehr zu tun.
  if (stand.zustand == LaufAus && k->takt) {
    app_timer_cancel(k->takt);
    k->takt = NULL;
  }
}

static const LauncherAppGlanceStructuredImpl s_kieselsport_impl = {
  .get_icon = prv_bild,
  .get_title = prv_titel,
  .create_subtitle_node = prv_untertitel_knoten,
  .destructor = prv_abbau,
};

LauncherAppGlance *launcher_app_glance_kieselsport_create(const AppMenuNode *node) {
  PBL_ASSERTN(node);
  // Die Namen der Arten in der Sprache der Uhr - das Startmenue hat sie nie
  // gelesen.
  strings_refresh();

  LauncherAppGlanceKieselsport *k = app_zalloc_check(sizeof(*k));
  strncpy(k->titel, node->name, sizeof(k->titel));
  k->titel[sizeof(k->titel) - 1] = '\0';

  LauncherAppGlanceStructured *structured_glance = launcher_app_glance_structured_create(
      &node->uuid, &s_kieselsport_impl, false /* should_consider_slices */, k);
  PBL_ASSERTN(structured_glance);

  if (kieselsport_laeuft()) {
    k->takt = app_timer_register_repeatable(1000, prv_takt, structured_glance, true /* repeating */);
  }
  prv_takt(structured_glance);
  return &structured_glance->glance;
}

#endif  // CONFIG_BOULDER_KIESELSPORT
