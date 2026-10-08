/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// NUR IM QEMU-PRUEFBAU (Spezifikation Schritt 3, AP1). Ohne CONFIG_BOULDER_PRUEFLAUF bleibt die
// Datei leer; probe.h macht bh_probe() dann zu false.
#if defined(CONFIG_BOULDER_HEALTH_APP) && defined(CONFIG_BOULDER_PRUEFLAUF)

#include "probe.h"

#include "applib/app.h"
#include "applib/app_launch_reason.h"
#include "applib/app_logging.h"
#include "applib/app_timer.h"
#include "applib/fonts/fonts.h"
#include "applib/graphics/gdraw_command_image.h"
#include "applib/graphics/graphics.h"
#include "applib/graphics/graphics_bitmap.h"
#include "applib/graphics/gtypes.h"
#include "applib/graphics/text.h"
#include "applib/health_service.h"
#include "applib/tick_timer_service.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/layer.h"
#include "applib/ui/menu_layer.h"
#include "applib/ui/scroll_layer.h"
#include "applib/ui/text_layer.h"
#include "applib/ui/window.h"
#include "font_resource_keys.auto.h"
#include "kernel/pbl_malloc.h"
#include "pbl/services/activity/activity.h"
#include "pbl/services/clock.h"
#include "process_state/app_state/app_state.h"
#include "resource/resource_ids.auto.h"
#include "shell/prefs_syscalls.h"
#include "syscall/syscall.h"

#include <time.h>

// Die Probe der Bausteine (T2.5) und die Fehlerprobe (T2.1).
//
// WARUM JEDER BAUSTEIN EINMAL: Was die App spaeter ruft, laeuft ohne Systemrechte. Ein Syscall,
// der dabei Kern-Speicher braucht, den die App nicht sehen darf, oder der tiefer stapelt als die
// 2 KiB des Syscall-Stapels, faellt hier auf und nicht erst in AP6. Gebraucht wird jeder Baustein
// aus 3.6 und jeder Helfer aus 3.7.5 der Spezifikation, mit den Werten, die die Karten brauchen
// werden (Metriken nur mit Verlauf 1, jede Schrift der Karten, ein PNG und ein PDC aus dem
// Systempaket). Nach frischem Boot liest "boulder stapel" dann den Tiefstand.
//
// Jeder Baustein schreibt beim ersten Gebrauch "bh probe <name> <wert>" ins Protokoll; sind alle
// benutzt, folgt "bh probe fertig". Das QEMU-Skript vergleicht die Namen mit der Liste der
// Spezifikation.

typedef enum {
  // 3.6 Activity
  BhMetrik,
  BhSitzungen,
  BhActivityBereit,
  // 3.6 Health
  BhPulsJetzt,
  BhEreignisse,
  BhPulstakt,
  // 3.6 Einstellungen und Zeit
  BhEinheit,
  Bh24h,
  BhUhrzeit,
  BhOrtszeit,
  BhTicks,
  // 3.6 Start
  BhStartgrund,
  BhStarttaste,
  BhStartargument,
  BhQuickLaunch,
  // 3.6 Oberflaeche
  BhFenster,
  BhEbene,
  BhMenue,
  BhRolle,
  BhText,
  BhGrafik,
  BhSchriften,
  BhAppTimer,
  BhTickTimer,
  // 3.6 Ressourcen und Protokoll
  BhBild,
  BhZeichnung,
  BhProtokoll,
  // 3.7.5 Helfer ausserhalb des SDK
  BhZustandSetzen,
  BhZustandLesen,
  BhWindowInit,
  BhLayerInit,
  BhTextLayerInit,
  BhMallocCheck,
  BhZallocCheck,
  BhBausteinAnzahl,
} BhBaustein;

// Die Namen sind die Funktionen, die der Baustein ruft, damit das QEMU-Skript sie mit 3.6 und
// 3.7.5 vergleichen kann.
static const char *const s_namen[BhBausteinAnzahl] = {
  [BhMetrik] = "sys_activity_get_metric",
  [BhSitzungen] = "sys_activity_get_sessions",
  [BhActivityBereit] = "sys_activity_is_initialized",
  [BhPulsJetzt] = "health_service_peek_current_value",
  [BhEreignisse] = "health_service_events_subscribe",
  [BhPulstakt] = "health_service_set_heart_rate_sample_period",
  [BhEinheit] = "sys_shell_prefs_get_units_distance",
  [Bh24h] = "clock_is_24h_style",
  [BhUhrzeit] = "sys_get_time",
  [BhOrtszeit] = "sys_localtime_r",
  [BhTicks] = "sys_get_ticks",
  [BhStartgrund] = "app_launch_reason",
  [BhStarttaste] = "app_launch_button",
  [BhStartargument] = "app_launch_get_args",
  [BhQuickLaunch] = "app_launch_get_quick_launch_action",
  [BhFenster] = "window",
  [BhEbene] = "layer",
  [BhMenue] = "menu_layer",
  [BhRolle] = "scroll_layer",
  [BhText] = "text_layer",
  [BhGrafik] = "graphics",
  [BhSchriften] = "fonts_get_system_font",
  [BhAppTimer] = "app_timer",
  [BhTickTimer] = "tick_timer_service",
  [BhBild] = "gbitmap_create_with_resource",
  [BhZeichnung] = "gdraw_command_image_create_with_resource",
  [BhProtokoll] = "app_log",
  [BhZustandSetzen] = "app_state_set_user_data",
  [BhZustandLesen] = "app_state_get_user_data",
  [BhWindowInit] = "window_init",
  [BhLayerInit] = "layer_init",
  [BhTextLayerInit] = "text_layer_init_with_parameters",
  [BhMallocCheck] = "app_malloc_check",
  [BhZallocCheck] = "app_zalloc_check",
};

// Die Metriken der Karten (2.6.2 Phase 0), jede mit Verlauf 1: Ein laengerer Verlauf oeffnet die
// Activity-Datei auf dem Syscall-Stapel (3.6).
static const ActivityMetric s_metriken[] = {
  ActivityMetricStepCount,
  ActivityMetricDistanceMeters,
  ActivityMetricActiveSeconds,
  ActivityMetricRestingKCalories,
  ActivityMetricActiveKCalories,
  ActivityMetricHeartRateFilteredBPM,
  ActivityMetricHeartRateFilteredUpdatedTimeUTC,
};

// Die Schriften der Karten (2.1): Labels GOTHIC 14, Kopf GOTHIC 18 fett, Zahlen LECO 26-38.
// Eine Schrift, die noch niemand geladen hat, liest der Syscall aus dem Ressourcenspeicher; das
// ist der tiefste Weg auf dem Syscall-Stapel (vgl. Pebbles Demo "Syscall near limit").
static const char *const s_schriften[] = {
  FONT_KEY_GOTHIC_14,
  FONT_KEY_GOTHIC_14_BOLD,
  FONT_KEY_GOTHIC_18,
  FONT_KEY_GOTHIC_18_BOLD,
  FONT_KEY_GOTHIC_24_BOLD,
  FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM,
  FONT_KEY_LECO_28_LIGHT_NUMBERS,
  FONT_KEY_LECO_32_BOLD_NUMBERS,
  FONT_KEY_LECO_36_BOLD_NUMBERS,
  FONT_KEY_LECO_38_BOLD_NUMBERS,
};
#define SCHRIFTEN (sizeof(s_schriften) / sizeof(s_schriften[0]))
#define METRIKEN  (sizeof(s_metriken) / sizeof(s_metriken[0]))

// Health-Ereignisse kommen im 1-s-Takt; ins Protokoll gehen nur die ersten.
#define EREIGNISSE_IM_PROTOKOLL 3

typedef struct {
  // Eingebettet, damit die Helfer window_init, layer_init und text_layer_init_with_parameters
  // benutzt sind. Sie gehen mit dem Heap der App, wenn die Probe endet; die Helfer fuer ihren
  // Abbau stehen nicht auf der Liste in 3.7.5, und keiner von ihnen haelt Kern-Speicher.
  Window fenster;
  Layer flaeche;
  TextLayer titel;
  MenuLayer *menue;
  ScrollLayer *rolle;
  TextLayer *rollen_text;
  GBitmap *bild;
  GDrawCommandImage *zeichnung;
  ActivitySession *sitzungen;
  GFont schriften[SCHRIFTEN];
  uint64_t benutzt; //< ein Bit je BhBaustein
  uint8_t ereignisse;
  bool fertig;
} BhProbe;

static void prv_benutzt(BhProbe *probe, BhBaustein baustein, int32_t wert) {
  const uint64_t bit = (uint64_t)1 << baustein;
  if (probe->benutzt & bit) {
    return;
  }
  probe->benutzt |= bit;
  APP_LOG(APP_LOG_LEVEL_INFO, "bh probe %s %ld", s_namen[baustein], (long)wert);
  if (!probe->fertig && probe->benutzt == ((uint64_t)1 << BhBausteinAnzahl) - 1) {
    probe->fertig = true;
    APP_LOG(APP_LOG_LEVEL_INFO, "bh probe fertig %d", (int)BhBausteinAnzahl);
  }
}

static BhProbe *prv_probe(void) {
  BhProbe *probe = app_state_get_user_data();
  prv_benutzt(probe, BhZustandLesen, 0);
  return probe;
}

// --- Daten: Activity, Health, Einstellungen, Zeit, Start ---

static void prv_ereignis(HealthEventType typ, void *kontext) {
  BhProbe *probe = kontext;
  if (probe->ereignisse < EREIGNISSE_IM_PROTOKOLL) {
    probe->ereignisse++;
    APP_LOG(APP_LOG_LEVEL_INFO, "bh probe ereignis %d", (int)typ);
  }
}

static void prv_daten(BhProbe *probe) {
  prv_benutzt(probe, BhActivityBereit, sys_activity_is_initialized());

  int32_t gelesen = 0;
  for (unsigned i = 0; i < METRIKEN; i++) {
    int32_t wert = 0;
    if (sys_activity_get_metric(s_metriken[i], 1, &wert)) {
      gelesen++;
    }
  }
  prv_benutzt(probe, BhMetrik, gelesen);

  // Wie die App spaeter: der Puffer im Heap (32 x 16 B), die Anzahl auf dem Stapel der App.
  probe->sitzungen =
      app_malloc_check(ACTIVITY_MAX_ACTIVITY_SESSIONS_COUNT * sizeof(ActivitySession));
  prv_benutzt(probe, BhMallocCheck, ACTIVITY_MAX_ACTIVITY_SESSIONS_COUNT * sizeof(ActivitySession));
  uint32_t sitzungen = ACTIVITY_MAX_ACTIVITY_SESSIONS_COUNT;
  const bool ok = sys_activity_get_sessions(&sitzungen, probe->sitzungen);
  prv_benutzt(probe, BhSitzungen, ok ? (int32_t)sitzungen : -1);

  prv_benutzt(probe, BhPulsJetzt, health_service_peek_current_value(HealthMetricHeartRateBPM));
  prv_benutzt(probe, BhEreignisse, health_service_events_subscribe(prv_ereignis, probe));
  prv_benutzt(probe, BhPulstakt, health_service_set_heart_rate_sample_period(1));

  prv_benutzt(probe, BhEinheit, sys_shell_prefs_get_units_distance());
  prv_benutzt(probe, Bh24h, clock_is_24h_style());
  const time_t jetzt = sys_get_time();
  prv_benutzt(probe, BhUhrzeit, (int32_t)jetzt);
  struct tm ortszeit = {0};
  sys_localtime_r(&jetzt, &ortszeit);
  prv_benutzt(probe, BhOrtszeit, ortszeit.tm_hour * 100 + ortszeit.tm_min);
  prv_benutzt(probe, BhTicks, (int32_t)sys_get_ticks());

  prv_benutzt(probe, BhStartgrund, app_launch_reason());
  prv_benutzt(probe, BhStarttaste, app_launch_button());
  prv_benutzt(probe, BhStartargument, (int32_t)app_launch_get_args());
  prv_benutzt(probe, BhQuickLaunch, app_launch_get_quick_launch_action());
}

// --- Oberflaeche ---

static void prv_flaeche_zeichnen(Layer *flaeche, GContext *ctx) {
  BhProbe *probe = prv_probe();
  const GRect rahmen = layer_get_bounds_by_value(flaeche);

  graphics_context_set_fill_color(ctx, GColorLightGray);
  graphics_fill_round_rect_by_value(ctx, GRect(0, 0, rahmen.size.w, 2), 0, GCornerNone);
  graphics_context_set_stroke_color(ctx, GColorRed);
  graphics_draw_line(ctx, GPoint(0, rahmen.size.h - 1), GPoint(rahmen.size.w, rahmen.size.h - 1));
  prv_benutzt(probe, BhGrafik, 0);

  // Links die fuenf GOTHIC untereinander, rechts die fuenf LECO, jede in einem Feld, in das "54"
  // ganz passt (sonst zeigte das Bild nur die Auslassungspunkte).
  static const GRect s_leco[] = {
    {{60, 0}, {44, 34}},   {{104, 0}, {48, 34}}, {{60, 36}, {56, 38}},
    {{116, 36}, {62, 38}}, {{60, 74}, {66, 42}},
  };
  graphics_context_set_text_color(ctx, GColorBlack);
  for (unsigned i = 0; i < SCHRIFTEN; i++) {
    const bool leco = (i >= 5);
    graphics_draw_text(ctx, leco ? "54" : "Steps", probe->schriften[i],
                       leco ? s_leco[i - 5] : GRect(2, 2 + (int16_t)i * 22, 58, 30),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
  prv_benutzt(probe, BhSchriften, SCHRIFTEN);

  graphics_draw_bitmap_in_rect_by_value(ctx, probe->bild, GRect(166, 4, 25, 25));
  prv_benutzt(probe, BhBild, probe->bild != NULL);
  gdraw_command_image_draw(ctx, probe->zeichnung, GPoint(150, 82));
  prv_benutzt(probe, BhZeichnung, probe->zeichnung != NULL);

  prv_benutzt(probe, BhEbene, 0);
}

static uint16_t prv_menue_zeilen(MenuLayer *menue, uint16_t abschnitt, void *kontext) {
  return 2;
}

// Eigene Zeilenhoehe und eigenes Zeichnen statt Pebbles Vorgabe und menu_cell_basic_draw: Fuer
// eine App mit System-ID (-82) lesen beide die Textgroesse des Systems direkt aus Kernel-RAM
// (menu_layer_system_cells.c:69-78 -> s_system_text_size in shell/normal/prefs.c:1909-1911). Ohne
// Systemrechte endete die App daran; der erste Lauf dieser Probe hat es gezeigt (AP1).
static int16_t prv_menue_hoehe(MenuLayer *menue, MenuIndex *index, void *kontext) {
  return 25;
}

static void prv_menue_zeile(GContext *ctx, const Layer *zelle, MenuIndex *index, void *kontext) {
  BhProbe *probe = kontext;
  graphics_draw_text(ctx, index->row ? "Light" : "Balanced", probe->schriften[2],
                     layer_get_bounds_by_value(zelle), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
  prv_benutzt(probe, BhMenue, index->row);
}

static void prv_app_timer(void *daten) {
  prv_benutzt(prv_probe(), BhAppTimer, 0);
}

static void prv_tick(struct tm *zeit, TimeUnits einheiten) {
  prv_benutzt(prv_probe(), BhTickTimer, zeit->tm_sec);
}

static void prv_oberflaeche(BhProbe *probe) {
  window_init(&probe->fenster, "bh probe");
  prv_benutzt(probe, BhWindowInit, 0);
  window_set_background_color(&probe->fenster, GColorWhite);
  Layer *wurzel = window_get_root_layer(&probe->fenster);
  const GRect rahmen = layer_get_bounds_by_value(wurzel);

  for (unsigned i = 0; i < SCHRIFTEN; i++) {
    probe->schriften[i] = fonts_get_system_font(s_schriften[i]);
  }

  text_layer_init_with_parameters(&probe->titel, &GRect(0, 0, rahmen.size.w, 24), "Probe",
                                  probe->schriften[3], GColorWhite, GColorBlue,
                                  GTextAlignmentCenter, GTextOverflowModeTrailingEllipsis);
  prv_benutzt(probe, BhTextLayerInit, 0);
  layer_add_child(wurzel, text_layer_get_layer(&probe->titel));

  probe->bild = gbitmap_create_with_resource(RESOURCE_ID_WORKOUT_APP_WALK_TINY);
  probe->zeichnung = gdraw_command_image_create_with_resource(RESOURCE_ID_HEART_TINY);
  layer_init(&probe->flaeche, &GRect(0, 24, rahmen.size.w, 116));
  prv_benutzt(probe, BhLayerInit, 0);
  layer_set_update_proc(&probe->flaeche, prv_flaeche_zeichnen);
  layer_add_child(wurzel, &probe->flaeche);

  probe->menue = menu_layer_create(GRect(0, 140, rahmen.size.w, 50));
  menu_layer_set_callbacks_by_value(probe->menue, probe,
                                    (MenuLayerCallbacks){
                                      .get_num_rows = prv_menue_zeilen,
                                      .get_cell_height = prv_menue_hoehe,
                                      .draw_row = prv_menue_zeile,
                                    });
  layer_add_child(wurzel, menu_layer_get_layer(probe->menue));

  probe->rolle = scroll_layer_create(GRect(0, 190, rahmen.size.w, rahmen.size.h - 190));
  scroll_layer_set_content_size(probe->rolle, GSize(rahmen.size.w, 100));
  probe->rollen_text = text_layer_create(GRect(4, 0, rahmen.size.w - 8, 100));
  text_layer_set_text(probe->rollen_text, "ScrollLayer");
  text_layer_set_font(probe->rollen_text, probe->schriften[0]);
  scroll_layer_add_child(probe->rolle, text_layer_get_layer(probe->rollen_text));
  layer_add_child(wurzel, scroll_layer_get_layer(probe->rolle));
  prv_benutzt(probe, BhRolle, 0);
  prv_benutzt(probe, BhText, 0);

  app_window_stack_push(&probe->fenster, true /* animiert */);
  prv_benutzt(probe, BhFenster, 0);

  app_timer_register(100, prv_app_timer, NULL);
  tick_timer_service_subscribe(SECOND_UNIT, prv_tick);
}

static void prv_bausteine(void) {
  BhProbe *probe = app_zalloc_check(sizeof(BhProbe));
  app_state_set_user_data(probe);
  // Die erste Zeile im Protokoll belegt APP_LOG selbst.
  prv_benutzt(probe, BhProtokoll, 1);
  prv_benutzt(probe, BhZallocCheck, sizeof(BhProbe));
  prv_benutzt(probe, BhZustandSetzen, 0);

  prv_daten(probe);
  prv_oberflaeche(probe);
  app_event_loop();

  // Abbau mit dem SDK: Abos zuerst, damit nach dem Ende kein Pulstakt bleibt.
  tick_timer_service_unsubscribe();
  health_service_set_heart_rate_sample_period(0);
  health_service_events_unsubscribe();
  menu_layer_destroy(probe->menue);
  text_layer_destroy(probe->rollen_text);
  scroll_layer_destroy(probe->rolle);
  gbitmap_destroy(probe->bild);
  gdraw_command_image_destroy(probe->zeichnung);
  APP_LOG(APP_LOG_LEVEL_INFO, "bh probe ende %s",
          probe->fertig ? "vollstaendig" : "UNVOLLSTAENDIG");
  task_free(probe->sitzungen);
  task_free(probe);
}

// --- Fehlerprobe ---

// Kehrt nur mit Systemrechten zurueck (Gegenprobe); ohne endet die App in bh_fehlerprobe_schreiben.
static void prv_fehler_jetzt(void *text) {
  const uint32_t gelesen = bh_fehlerprobe_schreiben();
  APP_LOG(APP_LOG_LEVEL_INFO, "bh fehlerprobe ueberlebt %08lx", (unsigned long)gelesen);
  text_layer_set_text(text, "survived");
}

static void prv_fehler(void) {
  Window *fenster = window_create();
  window_set_background_color(fenster, GColorWhite);
  Layer *wurzel = window_get_root_layer(fenster);
  TextLayer *text = text_layer_create(layer_get_bounds_by_value(wurzel));
  text_layer_set_text(text, "Fault probe");
  text_layer_set_font(text, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(text, GTextAlignmentCenter);
  layer_add_child(wurzel, text_layer_get_layer(text));
  app_window_stack_push(fenster, true /* animiert */);
  // 5 s statt Pebbles 300 ms: Das QEMU-Skript nimmt vorher ein Bild, das die laufende App zeigt.
  app_timer_register(5000, prv_fehler_jetzt, text);
  app_event_loop();
  text_layer_destroy(text);
  window_destroy(fenster);
}

bool bh_probe(void) {
  if (app_launch_reason() != APP_LAUNCH_TIMELINE_ACTION) {
    return false;
  }
  switch (app_launch_get_args()) {
    case BH_PROBE_BAUSTEINE:
      prv_bausteine();
      return true;
    case BH_PROBE_FEHLER:
      prv_fehler();
      return true;
    default:
      return false;
  }
}

#endif // CONFIG_BOULDER_HEALTH_APP && CONFIG_BOULDER_PRUEFLAUF
