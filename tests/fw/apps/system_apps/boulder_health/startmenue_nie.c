/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#include <stdlib.h>

// Fuer test_bh_startmenue: Von boulder_health.c (mit dem Schalter) und Pebbles health/health.c
// (ohne) zaehlt dort nur der Prozess-Eintrag. Ihre App-Rumpfe laufen nie; was sie rufen, steht hier
// ohne Koepfe, damit es bindet. Laeuft doch etwas davon, bricht der Test ab - der Test waere dann
// falsch gebaut.

#define NIE(fn)                                              \
  void fn(void) {                                            \
    fprintf(stderr, "test_bh_startmenue: %s\n", #fn);        \
    abort();                                                 \
  }

// Boulders App (boulder_health.c, einstieg.c, karten.c, texte.c); Laden, Inhalt und Details
// (daten.c, inhalt.c, details.c, zeichnen.c) sind nicht gebunden.
NIE(app_event_loop)
NIE(app_timer_cancel)
NIE(app_timer_register)
NIE(app_window_stack_pop)
NIE(bh_daten_auffrischen)
NIE(bh_daten_laden)
NIE(bh_daten_nachladen)
NIE(bh_daten_puls)
NIE(bh_detail_hoehe)
NIE(bh_detail_zeichnen)
NIE(bh_inhalt_zeichnen)
NIE(bh_kopf_detail)
NIE(bh_schrift)
NIE(health_service_events_unsubscribe)
NIE(scroll_layer_add_child)
NIE(scroll_layer_create)
NIE(scroll_layer_destroy)
NIE(scroll_layer_get_layer)
NIE(scroll_layer_set_click_config_onto_window)
NIE(scroll_layer_set_content_size)
NIE(scroll_layer_set_shadow_hidden)
NIE(sys_boulder_messstufe_setzen)
NIE(tick_timer_service_unsubscribe)
NIE(window_set_window_handlers_by_value)
NIE(window_single_repeating_click_subscribe)
NIE(app_launch_button)
NIE(app_launch_get_args)
NIE(app_launch_get_quick_launch_action)
NIE(app_launch_reason)
NIE(app_window_stack_pop_all)
NIE(app_window_stack_push)
NIE(click_recognizer_get_button_id)
NIE(graphics_draw_circle)
NIE(graphics_draw_text)
NIE(graphics_fill_circle)
NIE(graphics_fill_round_rect_by_value)
NIE(layer_add_child)
NIE(layer_create)
NIE(layer_destroy)
NIE(layer_get_bounds_by_value)
NIE(layer_mark_dirty)
NIE(layer_set_update_proc)
NIE(window_create)
NIE(window_destroy)
NIE(window_get_root_layer)
NIE(window_set_background_color)
NIE(window_set_click_config_provider)
NIE(window_single_click_subscribe)
// Pebbles App (health/health.c)
NIE(activity_prefs_activity_insights_are_enabled)
NIE(activity_prefs_get_health_app_opened_version)
NIE(activity_prefs_set_health_app_opened_version)
NIE(activity_prefs_sleep_insights_are_enabled)
NIE(activity_prefs_tracking_is_enabled)
NIE(app_expandable_dialog_push)
NIE(app_focus_service_subscribe_handlers)
NIE(app_focus_service_unsubscribe)
NIE(expandable_dialog_close_cb)
NIE(expandable_dialog_create_with_params)
NIE(health_card_view_create)
NIE(health_card_view_destroy)
NIE(health_card_view_mark_dirty)
NIE(health_card_view_push)
NIE(health_data_create)
NIE(health_data_destroy)
NIE(health_data_update)
NIE(health_data_update_current_bpm)
NIE(health_data_update_quick)
NIE(health_data_update_sleep)
NIE(health_data_update_step_derived_metrics)
NIE(health_data_update_steps)
NIE(health_service_events_subscribe)
NIE(health_service_set_heart_rate_sample_period)
NIE(health_service_sum_today)
NIE(health_tracking_ui_show_message)
NIE(tick_timer_service_subscribe)
