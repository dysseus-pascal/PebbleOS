/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#include <stdlib.h>

// Fuer test_bh_einstiege: Was watchface.c, shell.c, process_manager.c, app_run_state.c und
// karten.c ausserhalb der geprueften Wege rufen, steht hier ohne Koepfe, damit es bindet. Laeuft
// doch etwas davon, bricht der Test ab - dann ginge ein Einstieg einen Weg, den der Test nicht
// kennt.

#define NIE(fn)                                            \
  void fn(void) {                                          \
    fprintf(stderr, "test_bh_einstiege: %s\n", #fn);       \
    abort();                                               \
  }

// Der Speicher, den process_manager.c fuer App und Worker kennt (wie test_process_manager.c).
char __APP_RAM__[1024];
char *__APP_RAM_end__ = &__APP_RAM__[1024];
char __WORKER_RAM__[1024];
char *__WORKER_RAM_end__ = &__WORKER_RAM__[1024];

// app_run_state.c merkt die App beim Start vom Telefon im Startmenue vor (Pebbles Frist); fuer den
// Start selbst ohne Belang.
void app_install_mark_prioritized(int install_id, int can_expire) {}

NIE(app_db_get_app_entry_for_install_id)
NIE(app_install_entry_is_SDK_compatible)
NIE(app_install_get_md)
NIE(app_install_id_from_app_db)
NIE(app_install_is_app_running)
NIE(app_install_release_md)
NIE(app_install_unmark_prioritized)
NIE(app_manager_close_current_app)
NIE(app_manager_get_current_app_md)
NIE(app_manager_is_app_supported)
NIE(app_manager_launch_new_app)
NIE(app_manager_set_minimum_run_level)
NIE(comm_session_get_system_session)
NIE(comm_session_has_capability)
NIE(comm_session_send_data)
NIE(event_put)
NIE(event_put_from_process)
NIE(event_queue_cleanup_and_reset)
NIE(event_reset_from_process_queue)
NIE(event_service_clear_process_subscriptions)
NIE(bh_schrift)
NIE(fonts_get_system_font)
NIE(graphics_context_set_fill_color)
NIE(graphics_context_set_stroke_color)
NIE(graphics_context_set_text_color)
NIE(graphics_draw_circle)
NIE(graphics_draw_text)
NIE(graphics_fill_circle)
NIE(graphics_fill_round_rect_by_value)
NIE(launcher_app_message_send_app_state_deprecated)
NIE(launcher_task_add_callback)
NIE(pbl_msgq_get)
NIE(pbl_msgq_num_used)
NIE(pbl_msgq_put)
NIE(pebble_task_unregister)
NIE(process_metadata_get_app_sdk_platform)
NIE(quick_launch_setup_get_app_info)
NIE(timeline_peek_dismiss)
NIE(timeline_peek_get_item_id)
NIE(timeline_peek_is_first_event)
NIE(timeline_peek_is_future_empty)
