/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Kieselsport: die System-App (Schritt 2, aus b25 src/fw/apps/system/kieselsport/).
//
// NUR MIT DER VARIANTE. Der Ordner wird fuer jedes Board gebaut (GLOB_RECURSE in
// fw/apps/system/CMakeLists.txt); ohne den Schalter bleibt die Datei leer, und die Registry
// (Eintrag KIESELSPORT, -103) fuehrt die App dann auch nicht.
//
// Das Geruest ist ein leeres Fenster. Laufschirm, Menue und Telefon kommen mit dem Port.

#ifdef CONFIG_BOULDER_KIESELSPORT

#include "pbl/services/activity/boulder/sport/kieselsport.h"

#include "applib/app.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/window.h"
#include "process_management/pebble_process_md.h"
#include "resource/resource_ids.auto.h"

const PebbleProcessMd *kieselsport_app_get_info(void);

static void prv_main(void) {
  Window *fenster = window_create();
  app_window_stack_push(fenster, true);
  app_event_loop();
  window_destroy(fenster);
}

const PebbleProcessMd *kieselsport_app_get_info(void) {
  static const PebbleProcessMdSystem s_info = {
    .common = {
      .main_func = prv_main,
      .uuid = KIESELSPORT_UUID_INIT,
    },
    .name = "Kieselsport",
    .icon_resource_id = RESOURCE_ID_ACTIVITY_TINY,
  };
  return &s_info.common;
}

#endif
