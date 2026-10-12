/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "apps/system/boulder_health/daten.h"
#include "apps/system/boulder_health/details.h"
#include "apps/system/boulder_health/inhalt.h"
#include "apps/system/boulder_health/zeichnen.h"

#include "applib/app_launch_button.h"
#include "applib/app_launch_reason.h"
#include "applib/app_timer.h"
#include "applib/health_service.h"
#include "applib/tick_timer_service.h"
#include "applib/fonts/fonts.h"
#include "applib/graphics/graphics.h"
#include "applib/graphics/text.h"
#include "applib/ui/click.h"
#include "applib/ui/layer.h"
#include "applib/ui/scroll_layer.h"
#include "applib/ui/window.h"
#include "font_resource_keys.auto.h"
#include "pbl/services/timeline/health_layout.h"
#include "process_management/pebble_process_md.h"
#include "resource/resource_ids.auto.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Boulders Health-App beim Start und an den Tasten (Schritt 3, AP0 und AP7): wie die Einstiege
// sie finden, dass sie ohne Systemrechte laeuft, dass ihr Zustand im Heap der App liegt und alles
// wieder freigibt, auf welcher Karte sie oeffnet, wohin Oben und Unten fuehren, und die
// Zeitstempel des Pruefbaus (nur mit CONFIG_BOULDER_HEALTH_ZEITEN, test_bh_app_zeiten).
// Nachgebaut sind Pebbles Fenster, Ebenen, Grafik, Timer, Minutentakt, Health-Abo, Start-Syscalls,
// Heap und Protokoll, dazu das Laden (daten.c) und das Zeichnen von Inhalt und Details: Die
// zeichnet test_bh_karten mit echter Grafik. Echt sind die App, ihr Einstieg (einstieg.c), ihr
// Kartenrahmen mit der Zuordnung der Details (karten.c) und die Texte (texte.c).

const PebbleProcessMd *health_app_get_info(void);

// --- Was die App ruft, mitgezaehlt ---

static char s_reihe[512]; //< die Aufrufe in ihrer Reihenfolge, je ein Buchstabe
static Layer *s_kind;     //< was layer_add_child an die Wurzel des ersten Fensters haengt
static GColor s_hintergrund;
static bool s_animiert;
static int s_details_animiert; //< Detailfenster, die mit Uebergang geschoben wurden
static void *s_zustand;        //< app_state_set_user_data
static void *s_heap;           //< app_zalloc_check
static int s_offen;            //< Bloecke im Heap der App
static bool s_neu_zeichnen;    //< layer_mark_dirty seit dem letzten Zeichnen
static bool s_alle_weg;        //< app_window_stack_pop_all
static char s_protokoll[64][96];
static int s_zeilen;
static unsigned long s_ticks;

// Fenster: das der Karten und hoechstens ein Detail darueber.
#define FENSTER_MAX 2
static Window s_fenster[FENSTER_MAX];
static bool s_fenster_belegt[FENSTER_MAX];
static int s_fenster_offen;
static ClickConfigProvider s_tasten_anmelden[FENSTER_MAX];
static WindowHandlers s_handler[FENSTER_MAX];
static ClickHandler s_klick[FENSTER_MAX][NUM_BUTTONS];
static int s_anmelden; //< Fenster, dessen Tasten gerade angemeldet werden
static int s_stapel[FENSTER_MAX];
static int s_stapel_n;
static int s_letztes_fenster; //< zuletzt geschaffenes Fenster: dorthin gehoeren neue Ebenen

// Ebenen: die Flaeche der Karten zuerst, dann Kopf und Inhalt eines Details.
#define EBENEN_MAX 4
static Layer s_ebenen[EBENEN_MAX];
static bool s_ebene_belegt[EBENEN_MAX];
static int s_ebene_fenster[EBENEN_MAX];
static LayerUpdateProc s_ebene_zeichnen[EBENEN_MAX];
static int s_flaechen_offen;
#define s_flaeche (s_ebenen[0])
static ScrollLayer s_rolle;
static int s_rollen_offen;

// Timer, Minutentakt, Health-Abo
#define TIMER_MAX 4
static struct {
  bool an;
  uint32_t ms;
  AppTimerCallback rueckruf;
  void *daten;
} s_timer[TIMER_MAX];
static TickHandler s_minute;
static HealthEventHandler s_ereignis;
static uint32_t s_pulstakt;   //< zuletzt gesetzter Takt (0 = Vorgabe)
static int s_pulstakt_setzen; //< wie oft gesetzt

// Laden: was das nachgebaute daten.c liefert.
static uint8_t s_offen_beim_laden; //< BhQuelle-Bits nach Phase 1
static int s_nachfragen_bis_da;    //< nach so vielen Nachfragen ist alles da
static uint8_t s_gesetzte_stufe;
static BhKarte s_inhalt_karte;
static BhDetail s_detail_gezeichnet;
static uint8_t s_auswahl_gezeichnet;
static char s_detail_titel[32];

// Der Start, wie Pebbles Syscalls ihn der App melden.
static AppLaunchReason s_grund;
static ButtonId s_knopf;
static AppQuickLaunchAction s_aktion;
static uint32_t s_argument;

// Was ein Zeichnen hinterlaesst: der Titel im Kopf, seine Schrift und Farbe, die Mitte des
// gefuellten Punktes, und wie oft gezeichnet wurde.
static char s_titel[32];
static const char *s_titel_schrift;
static GColor s_textfarbe;
static GColor s_fuellfarbe;
static int16_t s_punkt_x;
static int s_offene_punkte;
static int s_bilder;

// Die Eingaben, die die Ereignisschleife zwischen den Bildern macht: 'o' Oben, 'u' Unten,
// 's' Select, 'x' Zurueck, 'm' eine Minute, 'M' ein Takt ohne Minute, 'n' der Timer des Nachladens,
// 'q' der Timer, der den Puls live beendet.
static const char *s_druecke;

static void prv_merke(char c) {
  const size_t n = strlen(s_reihe);
  cl_assert(n + 1 < sizeof(s_reihe));
  s_reihe[n] = c;
}

void *app_zalloc_check(size_t size) {
  prv_merke('a');
  s_heap = calloc(1, size);
  s_offen++;
  return s_heap;
}
void task_free(void *ptr) {
  prv_merke('f');
  cl_assert(ptr == s_heap);
  free(ptr);
  s_offen--;
}
void app_state_set_user_data(void *data) {
  s_zustand = data;
}
void *app_state_get_user_data(void) {
  return s_zustand;
}

AppLaunchReason app_launch_reason(void) {
  prv_merke('R');
  return s_grund;
}
ButtonId app_launch_button(void) {
  prv_merke('B');
  return s_knopf;
}
AppQuickLaunchAction app_launch_get_quick_launch_action(void) {
  prv_merke('Q');
  return s_aktion;
}
uint32_t app_launch_get_args(void) {
  prv_merke('A');
  return s_argument;
}

static int prv_fenster_nr(const Window *window) {
  const int i = (int)(window - s_fenster);
  cl_assert(i >= 0 && i < FENSTER_MAX && s_fenster_belegt[i]);
  return i;
}

Window *window_create(void) {
  prv_merke('w');
  for (int i = 0; i < FENSTER_MAX; i++) {
    if (!s_fenster_belegt[i]) {
      s_fenster_belegt[i] = true;
      memset(&s_fenster[i], 0, sizeof(s_fenster[i]));
      s_tasten_anmelden[i] = NULL;
      memset(&s_handler[i], 0, sizeof(s_handler[i]));
      memset(s_klick[i], 0, sizeof(s_klick[i]));
      s_fenster_offen++;
      s_letztes_fenster = i;
      return &s_fenster[i];
    }
  }
  cl_fail("mehr als zwei Fenster");
  return NULL;
}
void window_destroy(Window *window) {
  prv_merke('W');
  s_fenster_belegt[prv_fenster_nr(window)] = false;
  s_fenster_offen--;
}
void window_set_background_color(Window *window, GColor color) {
  if (prv_fenster_nr(window) == 0) {
    s_hintergrund = color;
  }
}
void window_set_click_config_provider(Window *window, ClickConfigProvider provider) {
  prv_merke('c');
  s_tasten_anmelden[prv_fenster_nr(window)] = provider;
}
void window_set_window_handlers_by_value(Window *window, WindowHandlers handlers) {
  s_handler[prv_fenster_nr(window)] = handlers;
}
void window_single_click_subscribe(ButtonId button_id, ClickHandler handler) {
  s_klick[s_anmelden][button_id] = handler;
}
void window_single_repeating_click_subscribe(ButtonId button_id, uint16_t repeat_interval_ms,
                                             ClickHandler handler) {
  s_klick[s_anmelden][button_id] = handler;
}
ButtonId click_recognizer_get_button_id(ClickRecognizerRef recognizer) {
  return *(ButtonId *)recognizer;
}
Layer *window_get_root_layer(const Window *window) {
  return &((Window *)window)->layer;
}
GRect layer_get_bounds_by_value(const Layer *layer) {
  return GRect(0, 0, 200, 228);
}
Layer *layer_create(GRect frame) {
  prv_merke('l');
  for (int i = 0; i < EBENEN_MAX; i++) {
    if (!s_ebene_belegt[i]) {
      s_ebene_belegt[i] = true;
      s_ebene_fenster[i] = s_letztes_fenster;
      s_ebene_zeichnen[i] = NULL;
      s_flaechen_offen++;
      return &s_ebenen[i];
    }
  }
  cl_fail("zu viele Ebenen");
  return NULL;
}
static int prv_ebene_nr(const Layer *layer) {
  const int i = (int)(layer - s_ebenen);
  cl_assert(i >= 0 && i < EBENEN_MAX && s_ebene_belegt[i]);
  return i;
}
void layer_destroy(Layer *layer) {
  prv_merke('L');
  s_ebene_belegt[prv_ebene_nr(layer)] = false;
  s_flaechen_offen--;
}
void layer_set_update_proc(Layer *layer, LayerUpdateProc update_proc) {
  s_ebene_zeichnen[prv_ebene_nr(layer)] = update_proc;
}
void layer_add_child(Layer *parent, Layer *child) {
  if (parent == &s_fenster[0].layer) {
    s_kind = child;
  }
}
void layer_mark_dirty(Layer *layer) {
  prv_merke('d');
  prv_ebene_nr(layer);
  s_neu_zeichnen = true;
}

ScrollLayer *scroll_layer_create(GRect frame) {
  prv_merke('y');
  s_rollen_offen++;
  return &s_rolle;
}
void scroll_layer_destroy(ScrollLayer *scroll_layer) {
  prv_merke('Y');
  s_rollen_offen--;
}
void scroll_layer_set_shadow_hidden(ScrollLayer *scroll_layer, bool hidden) {
}
void scroll_layer_set_content_size(ScrollLayer *scroll_layer, GSize size) {
}
void scroll_layer_add_child(ScrollLayer *scroll_layer, Layer *child) {
}
// Die ScrollLayer rollt mit Oben und Unten selbst; die App meldet dafuer nichts an.
void scroll_layer_set_click_config_onto_window(ScrollLayer *scroll_layer, struct Window *window) {
}
Layer *scroll_layer_get_layer(const ScrollLayer *scroll_layer) {
  return &((ScrollLayer *)scroll_layer)->layer;
}

static void prv_anmelden(int nr) {
  if (s_tasten_anmelden[nr]) {
    s_anmelden = nr;
    s_tasten_anmelden[nr](&s_fenster[nr]);
  }
}
void app_window_stack_push(Window *window, bool animated) {
  prv_merke('p');
  const int nr = prv_fenster_nr(window);
  cl_assert(s_stapel_n < FENSTER_MAX);
  s_stapel[s_stapel_n++] = nr;
  if (nr == 0) {
    s_animiert = animated;
  } else if (animated) {
    s_details_animiert++;
  }
  prv_anmelden(nr);
  s_neu_zeichnen = true;
}
// Wie Pebble: Das oberste Fenster geht vom Stapel, sein unload laeuft.
Window *app_window_stack_pop(bool animated) {
  prv_merke('O');
  cl_assert(s_stapel_n > 1);
  const int nr = s_stapel[--s_stapel_n];
  if (s_handler[nr].unload) {
    s_handler[nr].unload(&s_fenster[nr]);
  }
  s_neu_zeichnen = true;
  return &s_fenster[nr];
}
void app_window_stack_pop_all(const bool animated) {
  prv_merke('z');
  cl_assert(animated);
  s_alle_weg = true;
}

// Timer, Minutentakt, Health-Abo
AppTimer *app_timer_register(uint32_t timeout_ms, AppTimerCallback callback, void *callback_data) {
  prv_merke('r');
  for (int i = 0; i < TIMER_MAX; i++) {
    if (!s_timer[i].an) {
      s_timer[i].an = true;
      s_timer[i].ms = timeout_ms;
      s_timer[i].rueckruf = callback;
      s_timer[i].daten = callback_data;
      return (AppTimer *)(uintptr_t)(i + 1);
    }
  }
  cl_fail("zu viele Timer");
  return NULL;
}
void app_timer_cancel(AppTimer *timer) {
  prv_merke('k');
  const int i = (int)(uintptr_t)timer - 1;
  cl_assert(i >= 0 && i < TIMER_MAX && s_timer[i].an);
  s_timer[i].an = false;
}
static int prv_timer_offen(void) {
  int n = 0;
  for (int i = 0; i < TIMER_MAX; i++) {
    n += s_timer[i].an;
  }
  return n;
}
// Laesst den Timer mit dieser Laufzeit ablaufen.
static void prv_timer_ab(uint32_t ms) {
  for (int i = 0; i < TIMER_MAX; i++) {
    if (s_timer[i].an && s_timer[i].ms == ms) {
      s_timer[i].an = false;
      s_timer[i].rueckruf(s_timer[i].daten);
      return;
    }
  }
  cl_fail("kein Timer mit dieser Laufzeit");
}
void tick_timer_service_subscribe(TimeUnits tick_units, TickHandler handler) {
  prv_merke('T');
  cl_assert_equal_i(tick_units, MINUTE_UNIT);
  s_minute = handler;
}
void tick_timer_service_unsubscribe(void) {
  prv_merke('U');
  s_minute = NULL;
}
bool health_service_events_subscribe(HealthEventHandler handler, void *context) {
  prv_merke('E');
  s_ereignis = handler;
  return true;
}
bool health_service_events_unsubscribe(void) {
  prv_merke('X');
  s_ereignis = NULL;
  return true;
}
bool health_service_set_heart_rate_sample_period(uint16_t interval_sec) {
  prv_merke('H');
  s_pulstakt = interval_sec;
  s_pulstakt_setzen++;
  return true;
}
bool sys_boulder_messstufe_setzen(uint8_t stufe) {
  prv_merke('S');
  s_gesetzte_stufe = stufe;
  return stufe >= 1 && stufe <= 5;
}

// Laden (daten.c): Phase 1 laesst offen, was der Test will; Phase 2 holt es nach einigen
// Nachfragen, mit derselben Grenze wie daten.c.
void bh_daten_laden(BhDaten *d) {
  prv_merke('D');
  memset(d, 0, sizeof(*d));
  d->offen = s_offen_beim_laden;
  d->lage.messstufe = 3;
}
bool bh_daten_nachladen(BhDaten *d) {
  prv_merke('N');
  if (!d->offen || d->nachfragen >= BH_NACHFRAGEN) {
    return false;
  }
  d->nachfragen++;
  if (d->nachfragen >= s_nachfragen_bis_da) {
    d->offen = 0;
  }
  return d->offen && d->nachfragen < BH_NACHFRAGEN;
}
void bh_daten_auffrischen(BhDaten *d) {
  prv_merke('F');
}
void bh_daten_puls(BhDaten *d) {
  prv_merke('P');
}

// Zeichnen von Inhalt und Details: nur, was gezeichnet wird (test_bh_karten zeichnet echt).
void bh_inhalt_zeichnen(GContext *ctx, GRect flaeche, BhKarte karte, const BhDaten *d) {
  s_inhalt_karte = karte;
}
int16_t bh_detail_hoehe(BhDetail detail, const BhDaten *d) {
  return 300;
}
void bh_detail_zeichnen(GContext *ctx, int16_t breite, BhDetail detail, const BhDaten *d,
                        uint8_t auswahl) {
  s_detail_gezeichnet = detail;
  s_auswahl_gezeichnet = auswahl;
}
void bh_kopf_detail(GContext *ctx, int16_t breite, const char *titel, GColor farbe) {
  snprintf(s_detail_titel, sizeof(s_detail_titel), "%s", titel);
}
GFont bh_schrift(BhSchriftart art) {
  return (GFont)((art == BhSchriftG18Fett) ? FONT_KEY_GOTHIC_18_BOLD : "andere");
}

// Grafik: Es zaehlt, was der Kopfbalken zeigt.
GFont fonts_get_system_font(const char *font_key) {
  return (GFont)font_key;
}
void graphics_context_set_fill_color(GContext *ctx, GColor color) {
  s_fuellfarbe = color;
}
void graphics_context_set_text_color(GContext *ctx, GColor color) {
  s_textfarbe = color;
}
void graphics_context_set_stroke_color(GContext *ctx, GColor color) {
}
void graphics_fill_round_rect_by_value(GContext *ctx, GRect rect, uint16_t corner_radius,
                                       GCornerMask corner_mask) {
  // Der Kopfbalken: 0-27 px ueber die ganze Breite.
  cl_assert_equal_i(rect.origin.y, 0);
  cl_assert_equal_i(rect.size.h, 28);
  cl_assert_equal_i(rect.size.w, 200);
}
void graphics_draw_text(GContext *ctx, const char *text, GFont const font, const GRect box,
                        const GTextOverflowMode overflow_mode, const GTextAlignment alignment,
                        GTextAttributes *text_attributes) {
  snprintf(s_titel, sizeof(s_titel), "%s", text);
  s_titel_schrift = (const char *)font;
}
void graphics_fill_circle(GContext *ctx, GPoint p, uint16_t radius) {
  cl_assert(gcolor_equal(s_fuellfarbe, GColorWhite));
  s_punkt_x = p.x;
}
void graphics_draw_circle(GContext *ctx, GPoint p, uint16_t radius) {
  s_offene_punkte++;
}

uint64_t sys_get_ticks(void) {
  prv_merke('t');
  return s_ticks;
}
void app_log(uint8_t log_level, const char *src_filename, int src_line_number, const char *fmt,
             ...) {
  prv_merke('g');
  cl_assert(s_zeilen < 64);
  va_list args;
  va_start(args, fmt);
  vsnprintf(s_protokoll[s_zeilen++], sizeof(s_protokoll[0]), fmt, args);
  va_end(args);
}

// Pebble zeichnet die Ebenen des obersten Fensters.
static void prv_bild(void) {
  s_neu_zeichnen = false;
  s_titel[0] = '\0';
  s_offene_punkte = 0;
  const int oben = s_stapel[s_stapel_n - 1];
  for (int i = 0; i < EBENEN_MAX; i++) {
    if (s_ebene_belegt[i] && s_ebene_fenster[i] == oben && s_ebene_zeichnen[i]) {
      s_ebene_zeichnen[i](&s_ebenen[i], NULL);
    }
  }
  s_bilder++;
}

// Die Ereignisschleife: Pebble zeichnet das Fenster, dann macht der Nutzer die Eingaben aus
// s_druecke - nach jeder zeichnet Pebble neu, wenn die App es verlangt -, und schliesslich
// zeichnet Pebble noch einmal und der Nutzer schliesst die App. Fuehrt eine Taste zum
// Zifferblatt, endet die Schleife dort; Zurueck auf der Karte schliesst die App.
void app_event_loop(void) {
  prv_merke('e');
  cl_assert(s_stapel_n == 1);
  prv_bild();
  for (const char *t = s_druecke; t && *t; t++) {
    prv_merke(*t);
    const int oben = s_stapel[s_stapel_n - 1];
    if (*t == 'm' || *t == 'M') {
      // 'M': der Aufruf, den Pebble beim Anmelden und bei 12/24 h macht, ohne MINUTE_UNIT.
      cl_assert(s_minute != NULL);
      struct tm zeit = {0};
      s_minute(&zeit, (*t == 'm') ? MINUTE_UNIT : 0);
    } else if (*t == 'n') {
      prv_timer_ab(BH_NACHLADEN_MS);
    } else if (*t == 'q') {
      prv_timer_ab(2 * 60 * 1000);
    } else if (*t == 'x') {
      if (s_stapel_n == 1) {
        return;
      }
      app_window_stack_pop(true);
    } else {
      ButtonId knopf = (*t == 'o') ? BUTTON_ID_UP : (*t == 'u') ? BUTTON_ID_DOWN : BUTTON_ID_SELECT;
      if (s_klick[oben][knopf]) {
        s_klick[oben][knopf](&knopf, &s_fenster[oben]);
      }
    }
    if (s_alle_weg) {
      return;
    }
    if (s_neu_zeichnen) {
      prv_bild();
    }
  }
  prv_bild();
  // Schliesst die App mit einem offenen Detail, nimmt Pebble es vorher vom Stapel.
  while (s_stapel_n > 1) {
    app_window_stack_pop(false);
  }
}

// --- Hilfen ---

static void prv_starten(void) {
  health_app_get_info()->main_func();
}

static void prv_start_setzen(AppLaunchReason grund, ButtonId knopf, AppQuickLaunchAction aktion,
                             uint32_t argument) {
  s_grund = grund;
  s_knopf = knopf;
  s_aktion = aktion;
  s_argument = argument;
}

// Die Mitte des Punktes der Karte i (0 = Today): rechts der letzte, je 10 px nach links.
static int16_t prv_punkt_x(int karte) {
  return 200 - 8 - 3 - (4 - karte) * 10;
}

void test_bh_app__initialize(void) {
  memset(s_reihe, 0, sizeof(s_reihe));
  memset(s_fenster, 0, sizeof(s_fenster));
  memset(s_fenster_belegt, 0, sizeof(s_fenster_belegt));
  memset(s_tasten_anmelden, 0, sizeof(s_tasten_anmelden));
  memset(s_klick, 0, sizeof(s_klick));
  s_fenster_offen = 0;
  s_stapel_n = 0;
  s_letztes_fenster = 0;
  memset(s_ebenen, 0, sizeof(s_ebenen));
  memset(s_ebene_belegt, 0, sizeof(s_ebene_belegt));
  s_flaechen_offen = 0;
  s_rollen_offen = 0;
  memset(s_timer, 0, sizeof(s_timer));
  s_minute = NULL;
  s_ereignis = NULL;
  s_pulstakt = 0;
  s_pulstakt_setzen = 0;
  s_offen_beim_laden = 0;
  s_nachfragen_bis_da = 1;
  s_gesetzte_stufe = 0;
  s_inhalt_karte = BhKartenAnzahl;
  s_detail_gezeichnet = BhDetailKeins;
  s_auswahl_gezeichnet = 0xFF;
  s_detail_titel[0] = '\0';
  s_kind = NULL;
  s_hintergrund = GColorClear;
  s_animiert = false;
  s_details_animiert = 0;
  s_zustand = NULL;
  s_heap = NULL;
  s_offen = 0;
  s_neu_zeichnen = false;
  s_alle_weg = false;
  memset(s_protokoll, 0, sizeof(s_protokoll));
  s_zeilen = 0;
  s_ticks = 4711;
  s_titel[0] = '\0';
  s_titel_schrift = NULL;
  s_textfarbe = GColorClear;
  s_fuellfarbe = GColorClear;
  s_punkt_x = -1;
  s_offene_punkte = 0;
  s_bilder = 0;
  s_druecke = NULL;
  // Wie aus dem Startmenue (launcher/default/menu_layer.c:87-92).
  prv_start_setzen(APP_LAUNCH_USER, BUTTON_ID_SELECT, APP_QUICK_LAUNCH_ACTION_NONE, 0);
}

void test_bh_app__cleanup(void) {
}

// DIE APP, DIE PEBBLES EINSTIEGE FINDEN. Quick Launch und die Pins der Insights suchen die UUID
// von Pebbles Health-App (Spezifikation 4.2). Sie steht hier ausgeschrieben und nicht aus dem
// Kopf, damit ein vertipptes Byte dort auffaellt.
void test_bh_app__kennung_wie_pebble(void) {
  const uint8_t *b = (const uint8_t *)&health_app_get_info()->uuid;
  char hex[2 * sizeof(Uuid) + 1];
  for (size_t i = 0; i < sizeof(Uuid); i++) {
    snprintf(&hex[2 * i], 3, "%02x", b[i]);
  }
  cl_assert_equal_s(hex, "36d8c6ed4c834fa1a9e28f12dc941f8c");
}

// OHNE SYSTEMRECHTE (Kriterium 2): Pebble nimmt der App-Task die Rechte vor main, wenn die App es
// so traegt. Ein Fehler beendet dann nur die App.
void test_bh_app__ohne_systemrechte(void) {
  cl_assert(health_app_get_info()->is_unprivileged);
}

// Name, Symbol und Sichtbarkeit wie Pebbles App: einmal "Health" im Startmenue und in der
// Quick-Launch-Auswahl (T3.1).
void test_bh_app__sichtbar_wie_pebbles_app(void) {
  const PebbleProcessMd *md = health_app_get_info();
  cl_assert_equal_i(md->process_storage, ProcessStorageBuiltin);
  cl_assert_equal_i(md->process_type, ProcessTypeApp);
  cl_assert_equal_i(md->visibility, ProcessVisibilityShown);
  cl_assert(md->main_func != NULL);
  const PebbleProcessMdSystem *system = (const PebbleProcessMdSystem *)md;
  cl_assert_equal_s(system->name, "Health");
  cl_assert_equal_i(system->icon_resource_id, RESOURCE_ID_MENU_ICON_HEALTH);
}

// Ein weisses Fenster mit Pebbles Uebergang; der Zustand liegt im Heap der App (ein beschreibbares
// static laege im Kernel-RAM). Beim Schliessen ist alles wieder frei.
void test_bh_app__start_und_ende(void) {
  prv_starten();
  cl_assert(s_zustand != NULL);
  cl_assert(s_zustand == s_heap);
  cl_assert(s_kind == &s_flaeche);
  cl_assert_equal_i(s_hintergrund.argb, GColorWhiteARGB8);
  cl_assert(s_animiert);
  cl_assert_equal_i(s_offen, 0);
  cl_assert_equal_i(s_fenster_offen, 0);
  cl_assert_equal_i(s_flaechen_offen, 0);
}

// Der Kopfbalken (2.1): Titel in GOTHIC 18 fett und weiss, von fuenf Punkten der der Karte
// gefuellt, die anderen vier offen.
void test_bh_app__kopfbalken(void) {
  prv_starten();
  cl_assert_equal_s(s_titel, "Today");
  cl_assert_equal_s(s_titel_schrift, FONT_KEY_GOTHIC_18_BOLD);
  cl_assert(gcolor_equal(s_textfarbe, GColorWhite));
  cl_assert_equal_i(s_punkt_x, prv_punkt_x(0));
  cl_assert_equal_i(s_offene_punkte, 4);
}

// T3.4: die erste Karte je Einstieg, aus Pebbles Start-Syscalls gelesen, vor dem ersten Bild.
void test_bh_app__erste_karte(void) {
  static const struct {
    AppLaunchReason grund;
    ButtonId knopf;
    AppQuickLaunchAction aktion;
    uint32_t argument;
    const char *titel;
    int platz;
  } faelle[] = {
    {APP_LAUNCH_USER, BUTTON_ID_SELECT, APP_QUICK_LAUNCH_ACTION_NONE, 0, "Today", 0},
    {APP_LAUNCH_QUICK_LAUNCH, BUTTON_ID_UP, APP_QUICK_LAUNCH_ACTION_TAP, 0, "Today", 0},
    {APP_LAUNCH_QUICK_LAUNCH, BUTTON_ID_DOWN, APP_QUICK_LAUNCH_ACTION_HOLD, 0, "Today", 0},
    {APP_LAUNCH_PHONE, BUTTON_ID_BACK, APP_QUICK_LAUNCH_ACTION_NONE, 0, "Today", 0},
    {APP_LAUNCH_TIMELINE_ACTION, BUTTON_ID_BACK, APP_QUICK_LAUNCH_ACTION_NONE, HealthCardType_Sleep,
     "Night", 1},
    {APP_LAUNCH_TIMELINE_ACTION, BUTTON_ID_BACK, APP_QUICK_LAUNCH_ACTION_NONE,
     HealthCardType_Activity, "Activity", 3},
  };
  for (size_t i = 0; i < sizeof(faelle) / sizeof(faelle[0]); i++) {
    test_bh_app__initialize();
    prv_start_setzen(faelle[i].grund, faelle[i].knopf, faelle[i].aktion, faelle[i].argument);
    prv_starten();
    cl_assert_equal_s(s_titel, faelle[i].titel);
    cl_assert_equal_i(s_punkt_x, prv_punkt_x(faelle[i].platz));
    // Der Start ist gelesen, bevor das Fenster entsteht.
    cl_assert(strchr(s_reihe, 'R') < strchr(s_reihe, 'w'));
  }
}

// Oben und Unten zeichnen die neue Karte; nach Quick Launch Oben fuehrt Unten auf Today zum
// Zifferblatt, ohne noch etwas zu zeichnen.
void test_bh_app__tasten(void) {
  prv_start_setzen(APP_LAUNCH_QUICK_LAUNCH, BUTTON_ID_UP, APP_QUICK_LAUNCH_ACTION_TAP, 0);
  s_druecke = "oo";
  prv_starten();
  cl_assert_equal_s(s_titel, "Heart");
  cl_assert_equal_i(s_punkt_x, prv_punkt_x(2));
  cl_assert_equal_i(s_bilder, 4);
  cl_assert(!s_alle_weg);

  test_bh_app__initialize();
  prv_start_setzen(APP_LAUNCH_QUICK_LAUNCH, BUTTON_ID_UP, APP_QUICK_LAUNCH_ACTION_TAP, 0);
  s_druecke = "ouu";
  prv_starten();
  cl_assert(s_alle_weg);
  cl_assert_equal_i(s_bilder, 3);
  cl_assert_equal_s(s_titel, "Today");
  cl_assert_equal_i(s_offen, 0);

  // Gespiegelt nach Quick Launch Unten.
  test_bh_app__initialize();
  prv_start_setzen(APP_LAUNCH_QUICK_LAUNCH, BUTTON_ID_DOWN, APP_QUICK_LAUNCH_ACTION_TAP, 0);
  s_druecke = "uoo";
  prv_starten();
  cl_assert(s_alle_weg);
  cl_assert_equal_i(s_bilder, 3);

  // Im Kreis aus dem Startmenue: Unten auf Today fuehrt zu Settings.
  test_bh_app__initialize();
  s_druecke = "u";
  prv_starten();
  cl_assert_equal_s(s_titel, "Settings");
  cl_assert_equal_i(s_punkt_x, prv_punkt_x(4));
}

// L3 (2.6.2): Eine Taste liest nichts neu. Zwischen dem ersten Bild und dem Ende ruft die App
// keinen Start-Syscall mehr, laedt nichts nach und frischt nichts auf - ohne den Schalter des
// Pruefbaus ruft sie dort ueberhaupt keinen Syscall. Gelesen wird einmal vor dem ersten Bild.
void test_bh_app__tasten_ohne_syscall(void) {
  s_druecke = "ouuo";
  prv_starten();
  const char *nach_dem_start = strchr(s_reihe, 'e');
  cl_assert(nach_dem_start != NULL);
  cl_assert(strpbrk(nach_dem_start, "RBQADNFP") == NULL);
  cl_assert(strchr(s_reihe, 'D') < strchr(s_reihe, 'w'));
#ifndef CONFIG_BOULDER_HEALTH_ZEITEN
  cl_assert(strpbrk(nach_dem_start, "tg") == NULL);
  cl_assert_equal_s(s_reihe, "aRBQADwclpTEeodududodXULWf");
#endif
}

// L4 (2.6.2): Gelesen wird danach nur im Minutentakt, und jede Minute zeichnet die Karte neu.
void test_bh_app__auffrischen_im_minutentakt(void) {
  s_druecke = "omum";
  prv_starten();
  const char *nach_dem_start = strchr(s_reihe, 'e');
  cl_assert(strpbrk(nach_dem_start, "DN") == NULL);
  // Erst die Minute, dann das Auffrischen, dann das neue Bild (Zeitstempel und Protokoll des
  // Pruefbaus, 't' und 'g', dazwischen zaehlen nicht).
  char ohne_zeit[sizeof(s_reihe)] = {0};
  for (const char *c = nach_dem_start, *ende = c + strlen(c); c < ende; c++) {
    if (*c != 't' && *c != 'g') {
      ohne_zeit[strlen(ohne_zeit)] = *c;
    }
  }
  cl_assert_equal_s(ohne_zeit, "eodmFdudmFdXULWf");
}

// Pebbles erster Takt nach dem Anmelden (ohne MINUTE_UNIT) liest nichts: Phase 1 hat eben gelesen.
void test_bh_app__erster_takt_liest_nichts(void) {
  s_druecke = "M";
  prv_starten();
  cl_assert(strchr(s_reihe, 'F') == NULL);
}

// Phase 2: Fehlt nach dem Start etwas, fragt ein Timer im Sekundenabstand nach, bis es da ist,
// hoechstens BH_NACHFRAGEN mal. Danach laeuft kein Timer mehr.
void test_bh_app__nachladen(void) {
  s_offen_beim_laden = BhQuelleNacht | BhQuelleHerz;
  s_nachfragen_bis_da = 2;
  s_druecke = "nn";
  prv_starten();
  cl_assert(strchr(s_reihe, 'r') < strchr(s_reihe, 'e'));
  cl_assert_equal_i(prv_timer_offen(), 0);
  int nachfragen = 0;
  for (const char *c = s_reihe; *c; c++) {
    nachfragen += (*c == 'N');
  }
  cl_assert_equal_i(nachfragen, 2);

  // Kommt es nie, hoert die App nach BH_NACHFRAGEN auf.
  test_bh_app__initialize();
  s_offen_beim_laden = BhQuelleNacht;
  s_nachfragen_bis_da = 99;
  s_druecke = "nnnnn";
  prv_starten();
  cl_assert_equal_i(prv_timer_offen(), 0);

  // Ist alles da, gibt es keinen Timer.
  test_bh_app__initialize();
  prv_starten();
  cl_assert(strchr(s_reihe, 'r') == NULL);
}

// Ein Timer, der beim Schliessen noch laeuft, wird abgemeldet.
void test_bh_app__nachladen_beim_schliessen(void) {
  s_offen_beim_laden = BhQuelleNaechte;
  s_nachfragen_bis_da = 99;
  prv_starten();
  cl_assert(strchr(s_reihe, 'k') != NULL);
  cl_assert_equal_i(prv_timer_offen(), 0);
}

// Select oeffnet das Detail der Karte in einem eigenen Fenster mit dem Titel des Details; Zurueck
// fuehrt zur Karte, und Fenster, Ebenen und ScrollLayer sind wieder frei. Today hat keins (F12).
void test_bh_app__details(void) {
  static const struct {
    const char *weg;
    BhDetail detail;
    const char *titel;
  } faelle[] = {
    {"os", BhDetailNight, "Night"},
    {"oos", BhDetailHeart, "Heart"},
    {"ooos", BhDetailSessions, "Sessions"},
    {"oooos", BhDetailPrecision, "Precision"},
  };
  for (size_t i = 0; i < sizeof(faelle) / sizeof(faelle[0]); i++) {
    test_bh_app__initialize();
    char druecke[16];
    snprintf(druecke, sizeof(druecke), "%sx", faelle[i].weg);
    s_druecke = druecke;
    prv_starten();
    cl_assert_equal_i(s_detail_gezeichnet, faelle[i].detail);
    cl_assert_equal_s(s_detail_titel, faelle[i].titel);
    // L2: ohne Uebergang, sonst kommt das erste Bild erst nach Pebbles Animation.
    cl_assert_equal_i(s_details_animiert, 0);
    // Nach Zurueck zeichnet wieder die Karte.
    cl_assert_equal_i(s_inhalt_karte, (BhKarte)(strlen(faelle[i].weg) - 1));
    cl_assert_equal_i(s_fenster_offen, 0);
    cl_assert_equal_i(s_flaechen_offen, 0);
    cl_assert_equal_i(s_rollen_offen, 0);
    cl_assert_equal_i(s_offen, 0);
  }
  // Precision rollt nicht, die anderen schon.
  test_bh_app__initialize();
  s_druecke = "oooos";
  prv_starten();
  cl_assert(strchr(s_reihe, 'y') == NULL);
  test_bh_app__initialize();
  s_druecke = "os";
  prv_starten();
  cl_assert(strchr(s_reihe, 'y') != NULL);

  // Today: kein Fenster, kein Bild.
  test_bh_app__initialize();
  s_druecke = "s";
  prv_starten();
  cl_assert_equal_i(s_detail_gezeichnet, BhDetailKeins);
  cl_assert_equal_i(s_bilder, 2);
}

// Schliesst die App mit offenem Detail, ist danach trotzdem alles frei.
void test_bh_app__schliessen_mit_detail(void) {
  s_druecke = "oos";
  prv_starten();
  cl_assert_equal_i(s_fenster_offen, 0);
  cl_assert_equal_i(s_flaechen_offen, 0);
  cl_assert_equal_i(s_rollen_offen, 0);
  cl_assert_equal_i(s_offen, 0);
}

// Precision (F4): markiert die geltende Stufe, Oben und Unten waehlen ohne Kreis, Select setzt sie
// ueber S4 und schliesst die Liste; die Karte zeigt sie sofort.
void test_bh_app__precision(void) {
  s_druecke = "oooos";
  prv_starten();
  cl_assert_equal_i(s_auswahl_gezeichnet, 2); // Stufe 3, wie die Lage sagt

  test_bh_app__initialize();
  s_druecke = "oooosuuuus";
  prv_starten();
  cl_assert_equal_i(s_gesetzte_stufe, 5);
  cl_assert_equal_i(s_inhalt_karte, BhKarteSettings);
  cl_assert(strchr(s_reihe, 'O') != NULL);
  cl_assert_equal_i(s_fenster_offen, 0);

  test_bh_app__initialize();
  s_druecke = "oooosoooooos";
  prv_starten();
  cl_assert_equal_i(s_gesetzte_stufe, 1);
}

// V3: Puls im Sekundentakt nur auf Heart und hoechstens zwei Minuten; zurueck auf die Vorgabe,
// sobald die App Heart verlaesst, die Zeit um ist oder die App endet.
void test_bh_app__puls_live(void) {
  s_druecke = "oo";
  prv_starten();
  cl_assert_equal_i(s_pulstakt_setzen, 2); // an auf Heart, aus beim Schliessen
  cl_assert_equal_i(s_pulstakt, 0);

  test_bh_app__initialize();
  s_druecke = "oou";
  prv_starten();
  cl_assert_equal_i(s_pulstakt_setzen, 2); // an auf Heart, aus auf Night
  cl_assert_equal_i(s_pulstakt, 0);
  cl_assert_equal_i(prv_timer_offen(), 0);

  test_bh_app__initialize();
  s_druecke = "ooq";
  prv_starten();
  cl_assert_equal_i(s_pulstakt_setzen, 2); // an, nach zwei Minuten aus; das Ende setzt nichts
  cl_assert_equal_i(s_pulstakt, 0);

  // Today, Settings und Activity lassen den Takt in Ruhe.
  test_bh_app__initialize();
  s_druecke = "uu";
  prv_starten();
  cl_assert_equal_i(s_pulstakt_setzen, 0);
}

// Ein Health-Ereignis frischt nur auf Heart den Puls auf, ohne die anderen Quellen zu lesen.
void test_bh_app__puls_ereignis(void) {
  s_druecke = "oo";
  prv_starten();
  cl_assert(s_ereignis == NULL); // abgemeldet
  test_bh_app__initialize();
  prv_starten();
  cl_assert(strchr(s_reihe, 'E') != NULL && strchr(s_reihe, 'X') != NULL);
  cl_assert(strchr(s_reihe, 'T') != NULL && strchr(s_reihe, 'U') != NULL);
}

// Die Zeitstempel (2.6.3): mit dem Schalter "main" vor allem anderen, der Start mit seinen
// Werten und "bild" am Ende des ersten Zeichnens; nach jeder Taste "taste", wohin sie fuehrt, und
// "bild" am Ende des Zeichnens danach. Ohne den Schalter kein Aufruf davon.
void test_bh_app__zeitstempel(void) {
  prv_start_setzen(APP_LAUNCH_QUICK_LAUNCH, BUTTON_ID_UP, APP_QUICK_LAUNCH_ACTION_TAP, 0);
  s_druecke =
      "oooooo"
      "uuuuuu";
  prv_starten();
#ifdef CONFIG_BOULDER_HEALTH_ZEITEN
  static const char *const soll[] = {
    "bh t=4711 main",
    "bh t=4711 start grund=5 knopf=1 aktion=2 argument=0 Today kreis=0 spiegel=0",
    "bh t=4711 daten",
    "bh t=4711 bild",
    "bh t=4711 taste",
    "bh t=4711 karte Night",
    "bh t=4711 bild",
  };
  cl_assert_equal_i(APP_LAUNCH_QUICK_LAUNCH, 5);
  cl_assert_equal_i(BUTTON_ID_UP, 1);
  cl_assert_equal_i(APP_QUICK_LAUNCH_ACTION_TAP, 2);
  cl_assert(s_zeilen >= (int)(sizeof(soll) / sizeof(soll[0])));
  for (size_t i = 0; i < sizeof(soll) / sizeof(soll[0]); i++) {
    cl_assert_equal_s(s_protokoll[i], soll[i]);
  }
  // Settings ist die letzte Karte: Oben tut dort nichts. Zuletzt Unten auf Today.
  cl_assert_equal_s(s_protokoll[s_zeilen - 1], "bh t=4711 zifferblatt");
  bool nichts = false;
  for (int i = 0; i < s_zeilen; i++) {
    nichts |= (strcmp(s_protokoll[i], "bh t=4711 nichts") == 0);
  }
  cl_assert(nichts);
  // main zuerst, die Daten vor dem Fenster, bild nach dem Schieben des Fensters.
  cl_assert_equal_i(strncmp(s_reihe, "tgaRBQAtgDtgwclpTEetg", 21), 0);
#else
  cl_assert_equal_i(s_zeilen, 0);
#endif
}
