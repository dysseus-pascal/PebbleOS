/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "applib/ui/window.h"
#include "process_management/pebble_process_md.h"
#include "resource/resource_ids.auto.h"

#include <stdio.h>

// DIE APP, WIE BOULDER UND DER PERSIST SIE KENNEN. Boulder 0.52 erkennt Kieselsport an der UUID
// der b25- und Store-App, und der Persist liegt in "ps" + UUID (Spezifikation K6). Die UUID steht
// hier ausgeschrieben und nicht aus dem Header, damit ein vertipptes Byte dort auffaellt.

const PebbleProcessMd *kieselsport_app_get_info(void);

// Das leere Fenster der App; im Test laeuft prv_main nicht.
Window *window_create(void) { return NULL; }
void window_destroy(Window *window) {}
void app_window_stack_push(Window *window, bool animated) {}
void app_event_loop(void) {}

void test_kieselsport_app__kennung_wie_b25(void) {
  const uint8_t *b = (const uint8_t *)&kieselsport_app_get_info()->uuid;
  char hex[2 * sizeof(Uuid) + 1];
  for (size_t i = 0; i < sizeof(Uuid); i++) {
    snprintf(&hex[2 * i], 3, "%02x", b[i]);
  }
  cl_assert_equal_s(hex, "6c386250a6d74a19a48552541b5d7c0d");
}

// Eine sichtbare System-App im Startmenue und in der Quick-Launch-Auswahl (K6).
void test_kieselsport_app__sichtbare_system_app(void) {
  const PebbleProcessMd *md = kieselsport_app_get_info();
  cl_assert_equal_i(md->process_storage, ProcessStorageBuiltin);
  cl_assert_equal_i(md->process_type, ProcessTypeApp);
  cl_assert_equal_i(md->visibility, ProcessVisibilityShown);
  cl_assert(md->main_func != NULL);
  const PebbleProcessMdSystem *system = (const PebbleProcessMdSystem *)md;
  cl_assert_equal_s(system->name, "Kieselsport");
  cl_assert_equal_i(system->icon_resource_id, RESOURCE_ID_ACTIVITY_TINY);
}
