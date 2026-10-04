/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder: TimeStyle und pebble-fctx kamen als SDK-Code mit <pebble.h>. Diese
// Datei steht an seiner Stelle - nur fuer die Bibliothek dieses Ordners, die
// ihn als eigenen Suchpfad hat. So bleiben die Kopfzeilen des uebernommenen
// Codes unveraendert. Sie bindet die Koepfe der Firmware ein und bildet die
// SDK-Namen ab, wo die Firmware andere Unterschriften hat (Muster:
// apps/system/kieselstrasse/pebble_compat.h).
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "applib/app.h"
#include "applib/app_focus_service.h"
#include "applib/app_heap_util.h"
#include "applib/app_logging.h"
#include "applib/applib_resource.h"
#include "applib/battery_state_service.h"
#include "applib/connection_service.h"
#include "applib/fonts/fonts.h"
#include "applib/graphics/gdraw_command.h"
#include "applib/graphics/gdraw_command_image.h"
#include "applib/graphics/gdraw_command_list.h"
#include "applib/graphics/graphics.h"
#include "applib/graphics/gtypes.h"
#include "applib/graphics/text.h"
#include "applib/health_service.h"
#include "applib/pbl_std/pbl_std.h"
#include "applib/tick_timer_service.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/ui.h"
#include "applib/ui/vibes.h"
#include "kernel/pbl_malloc.h"
#include "pbl/services/clock.h"
#include "pbl/util/size.h"
#include "pbl/util/trig.h"
#include "resource/resource_ids.auto.h"
#include "syscall/syscall.h"
#include "util/time/time.h"

#include "ressourcen.h"

// Boulder: TS_DEMO zeigt feste Werte (Uhrzeit, Wetter, Schritte, Puls, Schlaf,
// Akku) fuer die Bilder im Emulator - in einer ausgelieferten Firmware waere
// das ein falsches Zifferblatt. Darum bricht ein Release-Bau mit TS_DEMO ab
// (ein alter CMake-Cache kann das Flag sonst unbemerkt mitnehmen).
#if defined(TS_DEMO) && defined(CONFIG_RELEASE)
#error "TS_DEMO ist ein Pruefbau fuer den Emulator und darf nie in ein Release (CONFIG_RELEASE)"
#endif

// Plattform: das SDK setzt diese Namen nur, wenn es sie gibt (#ifdef). Die
// Firmware setzt PBL_COLOR/PBL_ROUND immer, als 0 oder 1 - darum fragt der
// uebernommene Code dort mit #if (Aenderungen mit "Boulder:" markiert).
#if defined(CONFIG_PLATFORM_EMERY)
#define PBL_PLATFORM_EMERY 1
#endif
#if defined(CONFIG_PLATFORM_GABBRO)
#define PBL_PLATFORM_GABBRO 1
#endif
#define PBL_HEALTH 1

// Die SDK-Namen auf die Firmware-Fassungen, die GRect als Wert nehmen und
// zurueckgeben. Zuletzt definiert, nach allen Koepfen der Firmware.
#define layer_get_bounds(layer)              layer_get_bounds_by_value(layer)
#define layer_get_unobstructed_bounds(layer) layer_get_unobstructed_bounds_by_value(layer)
#define layer_set_frame(layer, ...)          layer_set_frame_by_value((layer), __VA_ARGS__)
#define window_set_window_handlers(w, ...)   window_set_window_handlers_by_value((w), __VA_ARGS__)
#define window_stack_push(window, animated)  app_window_stack_push((window), (animated))
#define graphics_fill_rect(ctx, rect, rad, mask) \
  graphics_fill_round_rect_by_value((ctx), (rect), (rad), (mask))

// Ressourcen: die nackten resource_* der Firmware nehmen eine ResAppNum.
#define resource_get_handle(id)         applib_resource_get_handle(id)
#define resource_size(handle)           applib_resource_size(handle)
#define resource_load(handle, buf, max) applib_resource_load((handle), (buf), (max))

// Speicher der App. Die _check-Fassungen beenden die App, statt NULL zu
// liefern - wie jedes SDK-malloc, das ins Leere greift, nur deutlich.
#define malloc(sz) app_malloc_check(sz)
#define free(p)    app_free(p)

// Zeit: time/localtime/gmtime/mktime sind fuer Apps nicht gelinkt. time()
// muss auch schreiben, wohin der Zeiger zeigt - der Code ruft time(&t).
#define time(p)      pbl_override_time(p)
#define localtime(t) pbl_override_localtime(t)
#define gmtime(t)    pbl_override_gmtime(t)
#define mktime(t)    pbl_override_mktime(t)

// Ruhezeit: in der Firmware der Syscall hinter "Nicht stoeren".
#define quiet_time_is_active() sys_do_not_disturb_is_active()
