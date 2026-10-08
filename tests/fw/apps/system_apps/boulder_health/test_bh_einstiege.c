/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "apps/system/boulder_health/einstieg.h"
#include "apps/system_app_ids.h"
#include "applib/app_launch_button.h"
#include "applib/app_launch_reason.h"
#include "kernel/events.h"
#include "process_management/app_install_manager.h"
#include "process_management/app_manager.h"
#include "process_management/app_run_state.h"
#include "process_management/process_manager.h"
#include "pbl/services/compositor/compositor_transitions.h"
#include "pbl/services/compositor/default/compositor_launcher_app_transitions.h"
#include "pbl/services/compositor/default/compositor_peek_transitions.h"
#include "pbl/services/compositor/default/compositor_port_hole_transitions.h"
#include "pbl/services/compositor/default/compositor_shutter_transitions.h"
#include "pbl/services/compositor/default/compositor_slide_transitions.h"
#include "pbl/services/timeline/timeline.h"
#include "shell/normal/watchface.h"
#include "shell/system_shell.h"

#include <stdio.h>
#include <string.h>

#include "fake_app_timer.h"
#include "fake_pebble_tasks.h"

#include "stubs_accel_service.h"
#include "stubs_analytics.h"
#include "stubs_animation_service.h"
#include "stubs_app_cache.h"
#include "stubs_app_state.h"
#include "stubs_comm_session.h"
#include "stubs_dls.h"
#include "stubs_evented_timer.h"
#include "stubs_expandable_dialog.h"
#include "stubs_heap.h"
#include "stubs_i18n.h"
#include "stubs_irq.h"
#include "stubs_logging.h"
#include "stubs_low_power.h"
#include "stubs_modal_manager.h"
#include "stubs_mutex.h"
#include "stubs_new_timer.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_process_md.h"
#include "stubs_persist.h"
#include "stubs_resources.h"
#include "stubs_system_task.h"
#include "stubs_thread.h"
#include "stubs_tick.h"
#include "stubs_timeline_peek.h"
#include "stubs_watchface.h"
#include "stubs_worker_manager.h"
#include "stubs_worker_state.h"

// Die Einstiege durch Pebbles echten Code (T3.2, T3.3 und T3.4 auf dem Host): Quick Launch mit
// jeder Geste ueber das Zifferblatt (shell/normal/watchface.c mit applib/ui/click.c), der Start vom
// Telefon ueber das Protokoll (process_management/app_run_state.c), die Start-Syscalls der App
// (process_management/process_manager.c mit applib/app_launch_*.c) und der Uebergang der Shell
// (shell/normal/shell.c). Am Ende steht Boulders einstieg.c: Karte, Kreis, Spiegel, und welche
// Taste zum Zifferblatt fuehrt.
//
// Nachgebaut ist nur das Umfeld: die Quick-Launch-Einstellungen (alle Gesten auf Health, wie sie
// ein Nutzer in Settings -> Quick Launch oder Boulder setzt), die Uebergaenge als Merkmale und die
// eine Zeile von app_manager.c, die den Start an den Prozess weiterreicht (s_next_app.common).
//
// Die Pins der Insights laufen in QEMU ganz durch (Pebbles Pruefhilfe -> Timeline -> "Open App",
// belege-schritt3/ap7-qemu-ablauf.txt); hier prueft test_bh_einstieg ihre Kartentypen.

#define ZIFFERBLATT ((AppInstallId)-69) //< TicToc, Pebbles Vorgabe

// Der Empfaenger des Endpunkts 0x34 (protocol_endpoints_table.json:42), ohne eigenen Kopf.
void app_run_state_protocol_msg_callback(CommSession *session, const uint8_t *data, size_t length);

// --- Quick Launch: jede Geste auf Health ---

static bool s_belegt;

bool quick_launch_is_enabled(ButtonId button) { return s_belegt; }
AppInstallId quick_launch_get_app(ButtonId button) { return APP_ID_HEALTH_APP; }
bool quick_launch_single_click_is_enabled(ButtonId button) { return s_belegt; }
AppInstallId quick_launch_single_click_get_app(ButtonId button) { return APP_ID_HEALTH_APP; }
bool quick_launch_combo_back_up_is_enabled(void) { return s_belegt; }
AppInstallId quick_launch_combo_back_up_get_app(void) { return APP_ID_HEALTH_APP; }
bool quick_launch_combo_up_down_is_enabled(void) { return s_belegt; }
AppInstallId quick_launch_combo_up_down_get_app(void) { return APP_ID_HEALTH_APP; }

// --- Was gestartet wird ---

static AppLaunchEventConfig s_start;
static int s_starts;

void app_manager_put_launch_app_event(const AppLaunchEventConfig *config) {
  s_start = *config;
  s_starts++;
}

// --- Prozess der App ---
//
// process_manager.c liest Grund, Taste und Argument aus dem Kontext der laufenden App. Den legt
// process_manager_init_context an; Grund und Taste holt es sich bei app_manager.c, das sie aus
// dem Start uebernimmt (app_manager.c:701-704, s_next_app.common = config->common).

static ProcessContext s_kontext;

ProcessContext *app_manager_get_task_context(void) { return &s_kontext; }
AppLaunchReason app_manager_get_launch_reason(void) { return s_start.common.reason; }
ButtonId app_manager_get_launch_button(void) { return s_start.common.button; }
WakeupInfo app_manager_get_app_wakeup_state(void) { return (WakeupInfo){0}; }

static const PebbleProcessMdSystem s_health_md = {
    .common = {.uuid = UUID_HEALTH_DATA_SOURCE, .is_unprivileged = true}, .name = "Health"};
static const Uuid s_health_uuid = UUID_HEALTH_DATA_SOURCE;

AppInstallId app_install_get_id_for_uuid(const Uuid *uuid) {
  return uuid_equal(uuid, &s_health_uuid) ? APP_ID_HEALTH_APP : INSTALL_ID_INVALID;
}

// Was die App ueber die Applib vom Start erfaehrt.
static AppQuickLaunchAction s_aktion;
static uint32_t s_argument;

// Die App startet: Kontext anlegen wie KernelMain es tut, dann laeuft die App-Task.
static BhEinstieg prv_app_startet(void) {
  cl_assert_equal_i(s_starts, 1);
  cl_assert_equal_i(s_start.id, APP_ID_HEALTH_APP);
  memset(&s_kontext, 0, sizeof(s_kontext));
  stub_pebble_tasks_set_current(PebbleTask_KernelMain);
  process_manager_init_context(&s_kontext, &s_health_md.common, s_start.common.args);
  stub_pebble_tasks_set_current(PebbleTask_App);
  // Wie boulder_health.c (prv_start): die vier Start-Syscalls der Applib.
  s_aktion = app_launch_get_quick_launch_action();
  s_argument = app_launch_get_args();
  return bh_einstieg(app_launch_reason(), app_launch_button(), s_aktion, s_argument);
}

// --- Uebergaenge der Shell als Merkmale ---

typedef struct {
  CompositorTransition objekt;
  char name[32];
} Uebergang;
static Uebergang s_uebergaenge[32];
static int s_anzahl_uebergaenge;

static const char *const s_richtung[] = {"oben", "unten", "links", "rechts", "keine"};

static const CompositorTransition *prv_uebergang(const char *name) {
  cl_assert(s_anzahl_uebergaenge < (int)(sizeof(s_uebergaenge) / sizeof(s_uebergaenge[0])));
  Uebergang *u = &s_uebergaenge[s_anzahl_uebergaenge++];
  snprintf(u->name, sizeof(u->name), "%s", name);
  return &u->objekt;
}

static const char *prv_name(const CompositorTransition *t) {
  for (int i = 0; i < s_anzahl_uebergaenge; i++) {
    if (t == &s_uebergaenge[i].objekt) {
      return s_uebergaenge[i].name;
    }
  }
  return t ? "?" : "keiner";
}

const CompositorTransition *compositor_shutter_transition_get(
    CompositorTransitionDirection direction, GColor color) {
  char name[32];
  snprintf(name, sizeof(name), "Shutter %s %s", s_richtung[direction],
           gcolor_equal(color, GColorBlack) ? "schwarz" : "andere");
  return prv_uebergang(name);
}
const CompositorTransition *compositor_slide_transition_app_get(bool slide_up) {
  return prv_uebergang(slide_up ? "App schiebt nach oben" : "App schiebt nach unten");
}
const CompositorTransition *compositor_port_hole_transition_app_get(
    CompositorTransitionDirection direction) {
  return prv_uebergang("rund");
}
const CompositorTransition *compositor_launcher_app_transition_get(bool app_is_destination) {
  return prv_uebergang("Startmenue");
}
const CompositorTransition *compositor_slide_transition_timeline_get(
    bool timeline_is_future, bool timeline_is_destination, bool timeline_is_empty) {
  return prv_uebergang("Timeline");
}
const CompositorTransition *compositor_peek_transition_timeline_get(void) {
  return prv_uebergang("Timeline");
}

// shell.c fragt, ob das Ziel ein Zifferblatt ist.
bool app_install_get_entry_for_install_id(AppInstallId install_id, AppInstallEntry *entry) {
  *entry = (AppInstallEntry){
    .install_id = install_id,
    .process_type = (install_id == ZIFFERBLATT) ? ProcessTypeWatchface : ProcessTypeApp,
  };
  return true;
}
bool app_install_entry_is_watchface(const AppInstallEntry *entry) {
  return entry->process_type == ProcessTypeWatchface;
}

// --- Tasten am Zifferblatt ---
//
// Ein Druck und ein Loslassen gehen als Ereignisse in watchface.c wie vom Tastentreiber. "Halten"
// laesst Pebbles Zeitgeber ablaufen (400 ms, watchface.c:22): den fuer langes Druecken einer
// Taste oder den der Kombination.

static void prv_taste(ButtonId knopf, bool gedrueckt) {
  PebbleEvent e = {
    .type = gedrueckt ? PEBBLE_BUTTON_DOWN_EVENT : PEBBLE_BUTTON_UP_EVENT,
    .button.button_id = knopf,
  };
  watchface_handle_button_event(&e);
}

static void prv_halten_ablaufen(void) {
  // Der zuletzt gestellte Zeitgeber ist der fuer das Halten.
  cl_assert(s_fake_app_timer_head != NULL);
  cl_assert(app_timer_trigger((AppTimer *)(uintptr_t)s_fake_app_timer_head->timer_id));
}

static void prv_tippen(ButtonId knopf) {
  prv_taste(knopf, true);
  prv_taste(knopf, false);
}

static void prv_halten(ButtonId knopf) {
  prv_taste(knopf, true);
  prv_halten_ablaufen();
  prv_taste(knopf, false);
}

static void prv_kombination(ButtonId erste, ButtonId zweite) {
  prv_taste(erste, true);
  prv_taste(zweite, true);
  prv_halten_ablaufen();
  prv_taste(zweite, false);
  prv_taste(erste, false);
}

void test_bh_einstiege__initialize(void) {
  fake_app_timer_init();
  s_belegt = true;
  s_start = (AppLaunchEventConfig){0};
  s_starts = 0;
  memset(&s_kontext, 0, sizeof(s_kontext));
  s_aktion = APP_QUICK_LAUNCH_ACTION_NONE;
  s_argument = 0xffffffff;
  s_anzahl_uebergaenge = 0;
  stub_pebble_tasks_set_current(PebbleTask_KernelMain);
  watchface_init();
  // Das Zifferblatt laeuft: watchface.c nimmt Tasten nur, solange es nicht schliesst.
  s_kontext.closing_state = ProcessRunState_Running;
}

void test_bh_einstiege__cleanup(void) {
  fake_app_timer_deinit();
}

// Was ein Start ergibt: Karte, Kreis, Spiegel, die Uebergaenge beim Oeffnen und Schliessen und
// welche Taste auf der ersten Karte zum Zifferblatt fuehrt ("-" wenn keine).
static void prv_pruefe(const char *soll_karte, bool soll_kreis, bool soll_spiegel,
                       const char *soll_auf, const char *soll_zu, const char *soll_zurueck) {
  // Pebbles Shell waehlt den Uebergang, bevor die App laeuft (app_manager.c:701-702) ...
  const CompositorTransition *auf =
      shell_get_open_compositor_animation(ZIFFERBLATT, APP_ID_HEALTH_APP, &s_start.common);
  const BhEinstieg e = prv_app_startet();
  cl_assert_equal_s(bh_karte_titel(e.karte), soll_karte);
  cl_assert_equal_b(e.kreis, soll_kreis);
  cl_assert_equal_b(e.gespiegelt, soll_spiegel);
  cl_assert_equal_s(prv_name(auf), soll_auf);
  // ... und beim Schliessen nach dem Kontext der laufenden App (shell.c:48-54, :87-90).
  stub_pebble_tasks_set_current(PebbleTask_KernelMain);
  const CompositorTransition *zu = shell_get_close_compositor_animation(APP_ID_HEALTH_APP,
                                                                        ZIFFERBLATT);
  cl_assert_equal_s(prv_name(zu), soll_zu);
  // Welche Taste auf der ersten Karte zum Zifferblatt fuehrt.
  const char *zurueck = "-";
  BhKarte karte = BhKarteToday;
  if (bh_blaettern(&e, &karte, BUTTON_ID_UP) == BhSchrittZifferblatt) {
    zurueck = "oben";
  }
  karte = BhKarteToday;
  if (bh_blaettern(&e, &karte, BUTTON_ID_DOWN) == BhSchrittZifferblatt) {
    zurueck = "unten";
  }
  cl_assert_equal_s(zurueck, soll_zurueck);
}

// --- Quick Launch (T3.2) und Uebergang (T3.3) ---

// Tippen Oben: Today ohne Kreis. Der Shutter oeffnet nach oben und faellt beim Schliessen nach
// unten; dorthin fuehrt Unten auf Today.
void test_bh_einstiege__tippen_oben(void) {
  prv_tippen(BUTTON_ID_UP);
  cl_assert_equal_i(s_start.common.reason, APP_LAUNCH_QUICK_LAUNCH);
  prv_pruefe("Today", false, false, "Shutter oben schwarz", "Shutter unten schwarz", "unten");
  cl_assert_equal_i(s_aktion, APP_QUICK_LAUNCH_ACTION_TAP);
  // Das Argument bleibt der App verborgen: Pebble reicht es nur fuer "Open App" durch
  // (process_manager.c:712-718), hier traegt es die Geste.
  cl_assert_equal_i(s_argument, 0);
}

void test_bh_einstiege__halten_oben(void) {
  prv_halten(BUTTON_ID_UP);
  prv_pruefe("Today", false, false, "Shutter oben schwarz", "Shutter unten schwarz", "unten");
  cl_assert_equal_i(s_aktion, APP_QUICK_LAUNCH_ACTION_HOLD);
}

// Unten: Die App schiebt sich von unten herein, ohne Shutter, und schliesst gespiegelt nach oben;
// dorthin fuehrt Oben auf Today.
void test_bh_einstiege__tippen_unten(void) {
  prv_tippen(BUTTON_ID_DOWN);
  prv_pruefe("Today", false, true, "App schiebt nach oben", "Shutter oben schwarz", "oben");
  cl_assert_equal_i(s_aktion, APP_QUICK_LAUNCH_ACTION_TAP);
}

void test_bh_einstiege__halten_unten(void) {
  prv_halten(BUTTON_ID_DOWN);
  prv_pruefe("Today", false, true, "App schiebt nach oben", "Shutter oben schwarz", "oben");
  cl_assert_equal_i(s_aktion, APP_QUICK_LAUNCH_ACTION_HOLD);
}

// Select- und Back-Halten: im Kreis, kein Weg zum Zifferblatt ueber die Karten.
void test_bh_einstiege__halten_select(void) {
  prv_halten(BUTTON_ID_SELECT);
  prv_pruefe("Today", true, false, "Shutter oben schwarz", "Shutter unten schwarz", "-");
  cl_assert_equal_i(s_aktion, APP_QUICK_LAUNCH_ACTION_HOLD);
}

void test_bh_einstiege__halten_zurueck(void) {
  prv_halten(BUTTON_ID_BACK);
  prv_pruefe("Today", true, false, "Shutter oben schwarz", "Shutter unten schwarz", "-");
  cl_assert_equal_i(s_aktion, APP_QUICK_LAUNCH_ACTION_HOLD);
}

// Kombinationen: im Kreis. Up+Down meldet Pebble mit der Taste Oben (watchface.c:97-98).
void test_bh_einstiege__zurueck_und_oben(void) {
  prv_kombination(BUTTON_ID_BACK, BUTTON_ID_UP);
  cl_assert_equal_i(s_start.common.button, BUTTON_ID_BACK);
  prv_pruefe("Today", true, false, "Shutter oben schwarz", "Shutter unten schwarz", "-");
  cl_assert_equal_i(s_aktion, APP_QUICK_LAUNCH_ACTION_COMBO);
}

void test_bh_einstiege__oben_und_unten(void) {
  prv_kombination(BUTTON_ID_UP, BUTTON_ID_DOWN);
  cl_assert_equal_i(s_start.common.button, BUTTON_ID_UP);
  prv_pruefe("Today", true, false, "Shutter oben schwarz", "Shutter unten schwarz", "-");
  cl_assert_equal_i(s_aktion, APP_QUICK_LAUNCH_ACTION_COMBO);
}

// Ohne Belegung startet keine Geste Health (Pebbles Vorgabe: nur Tippen Oben).
void test_bh_einstiege__unbelegt(void) {
  s_belegt = false;
  prv_tippen(BUTTON_ID_DOWN);
  prv_kombination(BUTTON_ID_BACK, BUTTON_ID_UP);
  prv_kombination(BUTTON_ID_UP, BUTTON_ID_DOWN);
  cl_assert_equal_i(s_starts, 0);
}

// --- Start vom Telefon (T3.4) ---

// AppRunState "Run" mit der UUID, Byte fuer Byte wie vom Telefon (Endpunkt 0x34): Today im Kreis.
void test_bh_einstiege__vom_telefon(void) {
  uint8_t nachricht[1 + sizeof(Uuid)] = {APP_RUN_STATE_RUN_COMMAND};
  memcpy(&nachricht[1], &s_health_uuid, sizeof(Uuid));
  app_run_state_protocol_msg_callback(NULL, nachricht, sizeof(nachricht));
  cl_assert_equal_i(s_start.common.reason, APP_LAUNCH_PHONE);
  prv_pruefe("Today", true, false, "Shutter oben schwarz", "Shutter unten schwarz", "-");
  cl_assert_equal_i(s_aktion, APP_QUICK_LAUNCH_ACTION_NONE);
}
