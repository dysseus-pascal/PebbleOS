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
#include "applib/ui/animation.h"
#include "applib/ui/click.h"
#include "applib/ui/layer.h"
#include "applib/ui/property_animation.h"
#include "applib/ui/scroll_layer.h"
#include "applib/ui/window.h"
#include "font_resource_keys.auto.h"
#include "pbl/services/hrm/hrm_manager.h"
#include "pbl/services/timeline/health_layout.h"
#include "process_management/pebble_process_md.h"
#include "resource/resource_ids.auto.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Boulders Health-App beim Start und an den Tasten (Schritt 3, AP0 und AP7): wie die Einstiege
// sie finden, dass sie ohne Systemrechte laeuft, dass ihr Zustand im Heap der App liegt und alles
// wieder freigibt, auf welcher Karte sie oeffnet, wohin Oben und Unten fuehren und wie die Karten
// dabei gleiten (F40), und die Zeitstempel des Pruefbaus (nur mit CONFIG_BOULDER_HEALTH_ZEITEN,
// test_bh_app_zeiten). Nachgebaut sind Pebbles Fenster, Ebenen, Animationen, Grafik, Timer,
// Minutentakt, Health-Abo, Start-Syscalls, Heap und Protokoll, dazu das Laden (daten.c) und das
// Zeichnen von Inhalt und Details: Die zeichnet test_bh_karten mit echter Grafik. Echt sind die
// App, ihr Einstieg (einstieg.c), ihr Kartenrahmen mit der Zuordnung der Details (karten.c) und
// die Texte (texte.c).

const PebbleProcessMd *health_app_get_info(void);

// --- Was die App ruft, mitgezaehlt ---

static char s_reihe[512]; //< die Aufrufe in ihrer Reihenfolge, je ein Buchstabe
static GColor s_hintergrund;
static bool s_animiert;
static int s_details_animiert; //< Detailfenster, die mit Uebergang geschoben wurden
static void *s_zustand;        //< app_state_set_user_data
static void *s_heap;           //< app_zalloc_check
static int s_offen;            //< Bloecke im Heap der App
static bool s_neu_zeichnen;    //< layer_mark_dirty seit dem letzten Zeichnen
static bool s_alle_weg;        //< app_window_stack_pop_all
#define ZEILEN_MAX 128
static char s_protokoll[ZEILEN_MAX][96];
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
static ClickHandler s_lang[FENSTER_MAX][NUM_BUTTONS]; //< window_long_click_subscribe
static int s_anmelden; //< Fenster, dessen Tasten gerade angemeldet werden
static int s_stapel[FENSTER_MAX];
static int s_stapel_n;
static int s_letztes_fenster; //< zuletzt geschaffenes Fenster: dorthin gehoeren neue Ebenen

// Ebenen: die fuenf Karten zuerst, dann Kopf und Inhalt eines Details. Pebble zeichnet nur, was
// nicht versteckt ist und im Bild liegt (layer.c, layer_render_tree: leerer Ausschnitt).
#define EBENEN_MAX (BhKartenAnzahl + 2)
static Layer s_ebenen[EBENEN_MAX];
static bool s_ebene_belegt[EBENEN_MAX];
static int s_ebene_fenster[EBENEN_MAX];
static LayerUpdateProc s_ebene_zeichnen[EBENEN_MAX];
static GRect s_ebene_rahmen[EBENEN_MAX];
static bool s_ebene_versteckt[EBENEN_MAX];
static int s_flaechen_offen;
static Layer *s_kinder[EBENEN_MAX]; //< was layer_add_child an die Wurzel des ersten Fensters haengt
static int s_kinder_n;
static unsigned s_karten_im_bild;   //< je Karte ein Bit: im letzten Bild gezeichnet
static int s_weiss_gefuellt;        //< Inhalt einer Karte weiss gefuellt

// Animationen: je Gleiten zwei Teile (eine Karte hinaus, eine herein) in einem Spawn.
typedef struct {
  Layer *ebene;
  GRect von;
  GRect bis;
  uint32_t dauer;
  InterpolateInt64Function kurve;
} Teil;
typedef struct {
  Teil *raus;
  Teil *rein;
  AnimationHandlers handler;
  void *kontext;
  bool geplant; //< geplant und nicht zu Ende
  bool begonnen;
  unsigned karten_mitte; //< die Karten im Bild in der Mitte des Gleitens
} Spawn;
#define TEILE_MAX 64
#define SPAWNS_MAX 32
static Teil s_teile[TEILE_MAX];
static int s_teile_n;
static Spawn s_spawns[SPAWNS_MAX];
static int s_spawns_n;
static bool s_gleiten_stehen_lassen; //< ein Gleiten endet erst mit der naechsten Taste
static int32_t s_kurve_mittel;       //< zuletzt an interpolate_moook_soft gegeben
static int32_t s_dauer_mittel;       //< zuletzt an interpolate_moook_soft_duration gegeben
// Pebbles Dauer der Kurve: drei Bilder Anlauf, vier Federn und die Mitte, je 33 ms.
#define MOOOK_DAUER(mitte) ((3 + 4 + (mitte)) * 33)
static ScrollLayer s_rolle;
static int s_rollen_offen;
// Wie Pebble: Die Inhaltsebene haengt an der ScrollLayer (scroll_layer.c, content_sublayer), und
// layer_destroy loest eine Ebene von ihrer Mutter. Ist die Mutter schon frei, liest und schreibt
// das freigegebenen Speicher.
static int s_rollen_kind;      //< Ebene an der ScrollLayer, -1 = keine
static int s_kind_ohne_mutter; //< Ebene, deren ScrollLayer frei ist, -1 = keine
static GSize s_rollen_inhalt;  //< zuletzt gesetzte Inhaltsgroesse

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
static uint32_t s_pulstakt_an; //< zuletzt gesetzter Takt ausser der Vorgabe
static uint16_t s_puls_ablauf; //< Ablauf, den die App dem Abo im Kern gab (s)

// Laden: was das nachgebaute daten.c liefert.
static uint8_t s_offen_beim_laden; //< BhQuelle-Bits nach Phase 1
static bool s_tracking;            //< Pebbles Schalter, wie die Lage ihn meldet
static int s_nachfragen_bis_da;    //< nach so vielen Nachfragen ist alles da
static uint8_t s_gesetzte_stufe;
static uint32_t s_schlafmodus_seit; //< F53: wie die Lage ihn meldet, 0 = keiner
static bool s_umschalten_geht;      //< Antwort von S8
static int s_umgeschaltet;          //< Aufrufe von S8
static bool s_umschalten_laeuft;    //< was die App S8 zuletzt als Stand gab
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
// 's' Select, 'h' Select lange, 'x' Zurueck, 'm' eine Minute, 'M' ein Takt ohne Minute, 'n' der Timer des Nachladens,
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
      memset(s_lang[i], 0, sizeof(s_lang[i]));
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
void window_long_click_subscribe(ButtonId button_id, uint16_t delay_ms, ClickHandler down_handler,
                                 ClickHandler up_handler) {
  cl_assert(up_handler == NULL);
  s_lang[s_anmelden][button_id] = down_handler;
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
      s_ebene_rahmen[i] = frame;
      s_ebene_versteckt[i] = false;
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
  const int nr = prv_ebene_nr(layer);
  if (nr == s_kind_ohne_mutter) {
    cl_fail("Ebene nach ihrer ScrollLayer freigegeben: layer_destroy laese freien Speicher");
  }
  if (nr == s_rollen_kind) {
    s_rollen_kind = -1;
  }
  s_ebene_belegt[nr] = false;
  s_flaechen_offen--;
}
void layer_set_frame_by_value(Layer *layer, GRect frame) {
  s_ebene_rahmen[prv_ebene_nr(layer)] = frame;
}
void layer_set_hidden(Layer *layer, bool hidden) {
  s_ebene_versteckt[prv_ebene_nr(layer)] = hidden;
}
void layer_set_update_proc(Layer *layer, LayerUpdateProc update_proc) {
  s_ebene_zeichnen[prv_ebene_nr(layer)] = update_proc;
}
void layer_add_child(Layer *parent, Layer *child) {
  if (parent == &s_fenster[0].layer) {
    cl_assert(s_kinder_n < EBENEN_MAX);
    s_kinder[s_kinder_n++] = child;
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
  s_kind_ohne_mutter = s_rollen_kind;
  s_rollen_kind = -1;
  s_rollen_offen--;
}
void scroll_layer_set_shadow_hidden(ScrollLayer *scroll_layer, bool hidden) {
}
void scroll_layer_set_content_size(ScrollLayer *scroll_layer, GSize size) {
  s_rollen_inhalt = size;
}
void scroll_layer_add_child(ScrollLayer *scroll_layer, Layer *child) {
  s_rollen_kind = prv_ebene_nr(child);
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
  if (s_handler[nr].appear) {
    s_handler[nr].appear(&s_fenster[nr]);
  }
  s_neu_zeichnen = true;
}
// Wie Pebble: Das oberste Fenster geht vom Stapel, sein unload laeuft, und das darunter erscheint.
Window *app_window_stack_pop(bool animated) {
  prv_merke('O');
  cl_assert(s_stapel_n > 1);
  const int nr = s_stapel[--s_stapel_n];
  if (s_handler[nr].unload) {
    s_handler[nr].unload(&s_fenster[nr]);
  }
  const int darunter = s_stapel[s_stapel_n - 1];
  if (s_handler[darunter].appear) {
    s_handler[darunter].appear(&s_fenster[darunter]);
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
  if (interval_sec) {
    s_pulstakt_an = interval_sec;
  }
  return true;
}
// Das Abo der App im Kern: es gibt eins, solange ein Takt gesetzt ist.
AppInstallId sys_app_manager_get_current_app_id(void) {
  return -82;
}
HRMSessionRef sys_hrm_manager_get_app_subscription(AppInstallId app_id) {
  cl_assert_equal_i(app_id, -82);
  return s_pulstakt ? 7 : HRM_INVALID_SESSION_REF;
}
bool sys_hrm_manager_set_update_interval(HRMSessionRef session, uint32_t update_interval_s,
                                         uint16_t expire_s) {
  prv_merke('V');
  cl_assert_equal_i(session, 7);
  cl_assert_equal_i(update_interval_s, s_pulstakt);
  s_puls_ablauf = expire_s;
  return true;
}
bool sys_boulder_messstufe_setzen(uint8_t stufe) {
  prv_merke('S');
  s_gesetzte_stufe = stufe;
  return stufe >= 1 && stufe <= 5;
}
bool sys_boulder_schlafmodus_umschalten(bool laeuft) {
  prv_merke('Z');
  s_umgeschaltet++;
  s_umschalten_laeuft = laeuft;
  return s_umschalten_geht;
}

// Animationen. Die Kurve rechnet hier geradlinig; welche Pebble-Kurve die App setzt, zeigt das
// Argument fuer die Mitte.
PropertyAnimation *property_animation_create_layer_frame(struct Layer *layer, GRect *from_frame,
                                                         GRect *to_frame) {
  prv_ebene_nr(layer);
  cl_assert(s_teile_n < TEILE_MAX);
  Teil *t = &s_teile[s_teile_n++];
  *t = (Teil){.ebene = layer, .von = *from_frame, .bis = *to_frame};
  return (PropertyAnimation *)t;
}
bool animation_set_duration(Animation *animation, uint32_t duration_ms) {
  ((Teil *)animation)->dauer = duration_ms;
  return true;
}
bool animation_set_custom_interpolation(Animation *animation,
                                        InterpolateInt64Function interpolate_function) {
  ((Teil *)animation)->kurve = interpolate_function;
  return true;
}
uint32_t interpolate_moook_soft_duration(int32_t num_frames_mid) {
  s_dauer_mittel = num_frames_mid;
  return MOOOK_DAUER(num_frames_mid);
}
int64_t interpolate_moook_soft(int32_t normalized, int64_t from, int64_t to,
                               int32_t num_frames_mid) {
  s_kurve_mittel = num_frames_mid;
  return from + (to - from) * normalized / ANIMATION_NORMALIZED_MAX;
}
Animation *animation_spawn_create(Animation *animation_a, Animation *animation_b,
                                  Animation *animation_c, ...) {
  cl_assert(animation_c == NULL);
  cl_assert(s_spawns_n < SPAWNS_MAX);
  Spawn *s = &s_spawns[s_spawns_n++];
  *s = (Spawn){.raus = (Teil *)animation_a, .rein = (Teil *)animation_b};
  return (Animation *)s;
}
bool animation_set_handlers(Animation *animation, AnimationHandlers callbacks, void *context) {
  Spawn *s = (Spawn *)animation;
  cl_assert(!s->geplant);
  s->handler = callbacks;
  s->kontext = context;
  return true;
}
bool animation_schedule(Animation *animation) {
  prv_merke('G');
  ((Spawn *)animation)->geplant = true;
  return true;
}
// Wie Pebble: Ein beendetes Gleiten gibt es nicht mehr (auto_destroy); ein laufendes endet mit
// stopped(finished = false), ein noch nicht begonnenes ohne.
bool animation_unschedule(Animation *animation) {
  prv_merke('J');
  Spawn *s = (Spawn *)animation;
  if (!s || !s->geplant) {
    return false;
  }
  s->geplant = false;
  if (s->begonnen && s->handler.stopped) {
    s->handler.stopped(animation, false, s->kontext);
  }
  return true;
}

// Laden (daten.c): Phase 1 laesst offen, was der Test will; Phase 2 holt es nach einigen
// Nachfragen, mit derselben Grenze wie daten.c.
void bh_daten_laden(BhDaten *d) {
  prv_merke('D');
  memset(d, 0, sizeof(*d));
  d->offen = s_offen_beim_laden;
  d->lage.messstufe = 3;
  d->lage.tracking = s_tracking;
  d->lage.schlafmodus_seit = s_schlafmodus_seit;
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
// Wie details.c: Night ohne Nacht ist kurz.
int16_t bh_detail_hoehe(BhDetail detail, const BhDaten *d) {
  return (detail == BhDetailNight && (d->offen & BhQuelleNacht)) ? 60 : 332;
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
// Die Karte deckt ihre Ebene: unter dem Kopfbalken weiss.
void bh_rechteck(GContext *ctx, GRect r, GColor farbe) {
  cl_assert(gcolor_equal(farbe, GColorWhite));
  cl_assert_equal_i(r.origin.x, 0);
  cl_assert_equal_i(r.origin.y, 28);
  cl_assert_equal_i(r.size.w, 200);
  cl_assert_equal_i(r.size.h, 228 - 28);
  s_weiss_gefuellt++;
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
  cl_assert(s_zeilen < ZEILEN_MAX);
  va_list args;
  va_start(args, fmt);
  vsnprintf(s_protokoll[s_zeilen++], sizeof(s_protokoll[0]), fmt, args);
  va_end(args);
}

static bool prv_zu_sehen(int i) {
  const GRect r = s_ebene_rahmen[i];
  return !s_ebene_versteckt[i] && r.size.w > 0 && r.size.h > 0 && r.origin.x < 200 &&
         r.origin.x + r.size.w > 0 && r.origin.y < 228 && r.origin.y + r.size.h > 0;
}

// Pebble zeichnet die Ebenen des obersten Fensters, die zu sehen sind, in ihrer Reihenfolge.
static void prv_bild(void) {
  s_neu_zeichnen = false;
  s_titel[0] = '\0';
  s_offene_punkte = 0;
  s_karten_im_bild = 0;
  const int oben = s_stapel[s_stapel_n - 1];
  for (int i = 0; i < EBENEN_MAX; i++) {
    if (s_ebene_belegt[i] && s_ebene_fenster[i] == oben && s_ebene_zeichnen[i] &&
        prv_zu_sehen(i)) {
      if (i < BhKartenAnzahl) {
        s_karten_im_bild |= 1u << i;
      }
      s_ebene_zeichnen[i](&s_ebenen[i], NULL);
    }
  }
  s_bilder++;
}

// Pebbles Animationsdienst, verkuerzt auf drei Bilder je Gleiten: am Anfang, in der Mitte und am
// Ende, die Rahmen mit der Kurve, die die App gesetzt hat.
static void prv_teil_setzen(const Teil *t, int32_t fortschritt) {
  GRect r = t->von;
  r.origin.x = (int16_t)t->kurve(fortschritt, t->von.origin.x, t->bis.origin.x);
  r.origin.y = (int16_t)t->kurve(fortschritt, t->von.origin.y, t->bis.origin.y);
  layer_set_frame_by_value(t->ebene, r);
}
static void prv_gleiten_bild(Spawn *s, int32_t fortschritt) {
  if (!s->begonnen) {
    s->begonnen = true;
    if (s->handler.started) {
      s->handler.started((Animation *)s, s->kontext);
    }
  }
  prv_teil_setzen(s->raus, fortschritt);
  prv_teil_setzen(s->rein, fortschritt);
  prv_bild();
}
static void prv_gleiten_laufen(bool bis_zum_ende) {
  for (int i = 0; i < s_spawns_n; i++) {
    Spawn *s = &s_spawns[i];
    if (!s->geplant) {
      continue;
    }
    if (!s->begonnen) {
      prv_gleiten_bild(s, 0);
      prv_gleiten_bild(s, ANIMATION_NORMALIZED_MAX / 2);
      s->karten_mitte = s_karten_im_bild;
    }
    if (bis_zum_ende) {
      prv_teil_setzen(s->raus, ANIMATION_NORMALIZED_MAX);
      prv_teil_setzen(s->rein, ANIMATION_NORMALIZED_MAX);
      s->geplant = false;
      if (s->handler.stopped) {
        s->handler.stopped((Animation *)s, true, s->kontext);
      }
      prv_bild();
    }
  }
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
      struct pbl_tm zeit = {0};
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
    } else if (*t == 'h') {
      ButtonId knopf = BUTTON_ID_SELECT;
      if (s_lang[oben][knopf]) {
        s_lang[oben][knopf](&knopf, &s_fenster[oben]);
      }
    } else {
      ButtonId knopf = (*t == 'o') ? BUTTON_ID_UP : (*t == 'u') ? BUTTON_ID_DOWN : BUTTON_ID_SELECT;
      if (s_klick[oben][knopf]) {
        s_klick[oben][knopf](&knopf, &s_fenster[oben]);
      }
    }
    if (s_alle_weg) {
      return;
    }
    prv_gleiten_laufen(!s_gleiten_stehen_lassen);
    if (s_neu_zeichnen) {
      prv_bild();
    }
  }
  prv_gleiten_laufen(!s_gleiten_stehen_lassen);
  prv_bild();
  // Schliesst die App mit einem offenen Detail, nimmt Pebble alle Fenster vom Stapel, ohne dass
  // eins erscheint (app.c, prv_handle_deinit_event; window_stack_pop_all).
  while (s_stapel_n > 1) {
    const int nr = s_stapel[--s_stapel_n];
    if (s_handler[nr].unload) {
      s_handler[nr].unload(&s_fenster[nr]);
    }
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
  memset(s_lang, 0, sizeof(s_lang));
  s_fenster_offen = 0;
  s_stapel_n = 0;
  s_letztes_fenster = 0;
  memset(s_ebenen, 0, sizeof(s_ebenen));
  memset(s_ebene_belegt, 0, sizeof(s_ebene_belegt));
  memset(s_ebene_rahmen, 0, sizeof(s_ebene_rahmen));
  memset(s_ebene_versteckt, 0, sizeof(s_ebene_versteckt));
  s_flaechen_offen = 0;
  memset(s_kinder, 0, sizeof(s_kinder));
  s_kinder_n = 0;
  s_karten_im_bild = 0;
  s_weiss_gefuellt = 0;
  memset(s_teile, 0, sizeof(s_teile));
  s_teile_n = 0;
  memset(s_spawns, 0, sizeof(s_spawns));
  s_spawns_n = 0;
  s_gleiten_stehen_lassen = false;
  s_kurve_mittel = -1;
  s_dauer_mittel = -1;
  s_rollen_offen = 0;
  s_rollen_kind = -1;
  s_kind_ohne_mutter = -1;
  s_rollen_inhalt = GSize(0, 0);
  memset(s_timer, 0, sizeof(s_timer));
  s_minute = NULL;
  s_ereignis = NULL;
  s_pulstakt = 0;
  s_pulstakt_setzen = 0;
  s_pulstakt_an = 0;
  s_puls_ablauf = 0;
  s_offen_beim_laden = 0;
  s_tracking = true;
  s_nachfragen_bis_da = 1;
  s_gesetzte_stufe = 0;
  s_schlafmodus_seit = 0;
  s_umschalten_geht = true;
  s_umgeschaltet = 0;
  s_umschalten_laeuft = false;
  s_inhalt_karte = BhKartenAnzahl;
  s_detail_gezeichnet = BhDetailKeins;
  s_auswahl_gezeichnet = 0xFF;
  s_detail_titel[0] = '\0';
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

static bool prv_rahmen(GRect r, int16_t x, int16_t y, int16_t w, int16_t h) {
  return r.origin.x == x && r.origin.y == y && r.size.w == w && r.size.h == h;
}

// Ein weisses Fenster mit Pebbles Uebergang; der Zustand liegt im Heap der App (ein beschreibbares
// static laege im Kernel-RAM). Wie bei Pebble je Karte eine Ebene, die erste im ganzen Fenster,
// die anderen ohne Flaeche (card_view.c:350-367). Beim Schliessen ist alles wieder frei.
void test_bh_app__start_und_ende(void) {
  prv_starten();
  cl_assert(s_zustand != NULL);
  cl_assert(s_zustand == s_heap);
  cl_assert_equal_i(s_kinder_n, BhKartenAnzahl);
  for (int k = 0; k < BhKartenAnzahl; k++) {
    cl_assert(s_kinder[k] == &s_ebenen[k]);
    cl_assert(k == BhKarteToday ? prv_rahmen(s_ebene_rahmen[k], 0, 0, 200, 228)
                                : prv_rahmen(s_ebene_rahmen[k], 0, 0, 0, 0));
  }
  cl_assert_equal_i(s_karten_im_bild, 1u << BhKarteToday);
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

// Oben und Unten zeigen die neue Karte; nach Quick Launch fuehrt Oben auf Today zum
// Zifferblatt, ohne noch etwas zu zeichnen (F23: Unten blaettert vorwaerts). Je Gleiten drei
// Bilder (prv_gleiten_laufen), dazu das erste und das letzte der App.
void test_bh_app__tasten(void) {
  prv_start_setzen(APP_LAUNCH_QUICK_LAUNCH, BUTTON_ID_UP, APP_QUICK_LAUNCH_ACTION_TAP, 0);
  s_druecke = "uu";
  prv_starten();
  cl_assert_equal_s(s_titel, "Heart");
  cl_assert_equal_i(s_punkt_x, prv_punkt_x(2));
  cl_assert_equal_i(s_bilder, 1 + 3 + 3 + 1);
  cl_assert_equal_i(s_karten_im_bild, 1u << BhKarteHeart);
  cl_assert(!s_alle_weg);

  test_bh_app__initialize();
  prv_start_setzen(APP_LAUNCH_QUICK_LAUNCH, BUTTON_ID_UP, APP_QUICK_LAUNCH_ACTION_TAP, 0);
  s_druecke = "uoo";
  prv_starten();
  cl_assert(s_alle_weg);
  cl_assert_equal_i(s_bilder, 1 + 3 + 3);
  cl_assert_equal_s(s_titel, "Today");
  cl_assert_equal_i(s_spawns_n, 2); // zum Zifferblatt gleitet nichts
  cl_assert_equal_i(s_offen, 0);

  // Nach Quick Launch Unten genauso.
  test_bh_app__initialize();
  prv_start_setzen(APP_LAUNCH_QUICK_LAUNCH, BUTTON_ID_DOWN, APP_QUICK_LAUNCH_ACTION_TAP, 0);
  s_druecke = "uoo";
  prv_starten();
  cl_assert(s_alle_weg);
  cl_assert_equal_i(s_bilder, 1 + 3 + 3);

  // Unten auf der letzten Karte ohne Kreis: nichts, auch kein Gleiten.
  test_bh_app__initialize();
  prv_start_setzen(APP_LAUNCH_QUICK_LAUNCH, BUTTON_ID_UP, APP_QUICK_LAUNCH_ACTION_TAP, 0);
  s_druecke = "uuuuu";
  prv_starten();
  cl_assert_equal_s(s_titel, "Settings");
  cl_assert_equal_i(s_spawns_n, 4);

  // Im Kreis aus dem Startmenue: Oben auf Today fuehrt zu Settings.
  test_bh_app__initialize();
  s_druecke = "o";
  prv_starten();
  cl_assert_equal_s(s_titel, "Settings");
  cl_assert_equal_i(s_punkt_x, prv_punkt_x(4));
}

// L3 (2.6.2): Eine Taste liest nichts neu. Zwischen dem ersten Bild und dem Ende ruft die App
// keinen Start-Syscall mehr, laedt nichts nach und frischt nichts auf - ohne den Schalter des
// Pruefbaus ruft sie dort ueberhaupt keinen Syscall. Gelesen wird einmal vor dem ersten Bild.
void test_bh_app__tasten_ohne_syscall(void) {
  s_druecke = "uoou";
  prv_starten();
  const char *nach_dem_start = strchr(s_reihe, 'e');
  cl_assert(nach_dem_start != NULL);
  cl_assert(strpbrk(nach_dem_start, "RBQADNFP") == NULL);
  cl_assert(strchr(s_reihe, 'D') < strchr(s_reihe, 'w'));
#ifndef CONFIG_BOULDER_HEALTH_ZEITEN
  cl_assert(strpbrk(nach_dem_start, "tg") == NULL);
  // Je Taste nur das Gleiten: das laufende abbrechen ('J'), das neue planen ('G').
  cl_assert_equal_s(s_reihe, "aRBQADwclllllpTEeuJGoJGoJGuJGXULLLLLWf");
#endif
}

// L4 (2.6.2): Gelesen wird danach nur im Minutentakt, und jede Minute zeichnet die Karte neu.
void test_bh_app__auffrischen_im_minutentakt(void) {
  s_druecke = "umom";
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
  cl_assert_equal_s(ohne_zeit, "euJGmFdoJGmFdXULLLLLWf");
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
    {"us", BhDetailNight, "Night"},
    {"uus", BhDetailHeart, "Heart"},
    {"uuus", BhDetailSessions, "Sessions"},
    {"uuuus", BhDetailPrecision, "Precision"},
  };
  for (size_t i = 0; i < sizeof(faelle) / sizeof(faelle[0]); i++) {
    test_bh_app__initialize();
    char druecke[16];
    snprintf(druecke, sizeof(druecke), "%sx", faelle[i].weg);
    s_druecke = druecke;
    prv_starten();
    cl_assert_equal_i(s_detail_gezeichnet, faelle[i].detail);
    cl_assert_equal_s(s_detail_titel, faelle[i].titel);
    // Mit Pebbles Uebergang wie Pebbles Details (activity_summary_card.c:196).
    cl_assert_equal_i(s_details_animiert, 1);
    // Nach Zurueck zeichnet wieder die Karte.
    cl_assert_equal_i(s_inhalt_karte, (BhKarte)(strlen(faelle[i].weg) - 1));
    cl_assert_equal_i(s_fenster_offen, 0);
    cl_assert_equal_i(s_flaechen_offen, 0);
    cl_assert_equal_i(s_rollen_offen, 0);
    cl_assert_equal_i(s_offen, 0);
  }
  // Precision rollt nicht, die anderen schon.
  test_bh_app__initialize();
  s_druecke = "uuuus";
  prv_starten();
  cl_assert(strchr(s_reihe, 'y') == NULL);
  test_bh_app__initialize();
  s_druecke = "us";
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
  s_druecke = "uus";
  prv_starten();
  cl_assert_equal_i(s_fenster_offen, 0);
  cl_assert_equal_i(s_flaechen_offen, 0);
  cl_assert_equal_i(s_rollen_offen, 0);
  cl_assert_equal_i(s_offen, 0);
}

// Precision (F4): markiert die geltende Stufe, Oben und Unten waehlen ohne Kreis, Select setzt sie
// ueber S4 und schliesst die Liste; die Karte zeigt sie sofort.
void test_bh_app__precision(void) {
  s_druecke = "uuuus";
  prv_starten();
  cl_assert_equal_i(s_auswahl_gezeichnet, 2); // Stufe 3, wie die Lage sagt

  test_bh_app__initialize();
  s_druecke = "uuuusuuuus";
  prv_starten();
  cl_assert_equal_i(s_gesetzte_stufe, 5);
  cl_assert_equal_i(s_inhalt_karte, BhKarteSettings);
  cl_assert(strchr(s_reihe, 'O') != NULL);
  cl_assert_equal_i(s_fenster_offen, 0);

  test_bh_app__initialize();
  s_druecke = "uuuusoooooos";
  prv_starten();
  cl_assert_equal_i(s_gesetzte_stufe, 1);
}

// V3: Puls im Sekundentakt nur auf Heart und hoechstens zwei Minuten; zurueck auf die Vorgabe,
// sobald die App Heart verlaesst, die Zeit um ist oder die App endet.
void test_bh_app__puls_live(void) {
  s_druecke = "uu";
  prv_starten();
  cl_assert_equal_i(s_pulstakt_setzen, 2); // an auf Heart, aus beim Schliessen
  cl_assert_equal_i(s_pulstakt, 0);
  cl_assert_equal_i(s_pulstakt_an, 1); // Sekundentakt
  // Das Abo im Kern laeuft nach zwei Minuten ab, auch wenn die App nicht ueber ihr Ende geht.
  cl_assert_equal_i(s_puls_ablauf, 2 * 60);

  test_bh_app__initialize();
  s_druecke = "uuo";
  prv_starten();
  cl_assert_equal_i(s_pulstakt_setzen, 2); // an auf Heart, aus auf Night
  cl_assert_equal_i(s_pulstakt, 0);
  cl_assert_equal_i(prv_timer_offen(), 0);

  test_bh_app__initialize();
  s_druecke = "uuq";
  prv_starten();
  cl_assert_equal_i(s_pulstakt_setzen, 2); // an, nach zwei Minuten aus; das Ende setzt nichts
  cl_assert_equal_i(s_pulstakt, 0);

  // Today, Settings und Activity lassen den Takt in Ruhe.
  test_bh_app__initialize();
  s_druecke = "oo";
  prv_starten();
  cl_assert_equal_i(s_pulstakt_setzen, 0);
}

// Ein Health-Ereignis frischt nur auf Heart den Puls auf, ohne die anderen Quellen zu lesen.
void test_bh_app__puls_ereignis(void) {
  s_druecke = "uu";
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
      "uuuuuu"
      "oooooo";
  prv_starten();
#ifdef CONFIG_BOULDER_HEALTH_ZEITEN
  // Beim Gleiten: "bild" im ersten Bild danach, "neu", sobald die neue Karte zu sehen ist, "ruhe"
  // am Ende.
  static const char *const soll[] = {
    "bh t=4711 main",
    "bh t=4711 start grund=5 knopf=1 aktion=2 argument=0 Today kreis=0",
    "bh t=4711 daten",
    "bh t=4711 bild",
    "bh t=4711 taste",
    "bh t=4711 karte Night",
    "bh t=4711 bild",
    "bh t=4711 neu",
    "bh t=4711 ruhe",
  };
  cl_assert_equal_i(APP_LAUNCH_QUICK_LAUNCH, 5);
  cl_assert_equal_i(BUTTON_ID_UP, 1);
  cl_assert_equal_i(APP_QUICK_LAUNCH_ACTION_TAP, 2);
  cl_assert(s_zeilen >= (int)(sizeof(soll) / sizeof(soll[0])));
  for (size_t i = 0; i < sizeof(soll) / sizeof(soll[0]); i++) {
    cl_assert_equal_s(s_protokoll[i], soll[i]);
  }
  // Settings ist die letzte Karte: Unten tut dort nichts. Zuletzt Oben auf Today.
  cl_assert_equal_s(s_protokoll[s_zeilen - 1], "bh t=4711 zifferblatt");
  bool nichts = false;
  for (int i = 0; i < s_zeilen; i++) {
    nichts |= (strcmp(s_protokoll[i], "bh t=4711 nichts") == 0);
  }
  cl_assert(nichts);
  // main zuerst, die Daten vor dem Fenster, bild nach dem Schieben des Fensters.
  cl_assert_equal_i(strncmp(s_reihe, "tgaRBQAtgDtgwclllllpTEetg", 25), 0);
#else
  cl_assert_equal_i(s_zeilen, 0);
#endif
}

// Gegenpruefung A1: Beim Schliessen eines Details die Inhaltsebene vor ihrer ScrollLayer freigeben
// (die Attrappe schlaegt sonst fehl, wie Pebble freien Speicher laese). Auch mit offenem Detail
// beim Ende der App.
void test_bh_app__detail_frei_in_der_richtigen_reihenfolge(void) {
  s_druecke = "usx";
  prv_starten();
  cl_assert_equal_i(s_rollen_offen, 0);
  test_bh_app__initialize();
  s_druecke = "uus";
  prv_starten();
  cl_assert_equal_i(s_flaechen_offen, 0);
}

// Gegenpruefung A7: Puls live nur auf der Karte Heart - nicht in ihrem Detail, wieder zurueck auf
// der Karte.
void test_bh_app__puls_nur_auf_der_karte(void) {
  s_druecke = "uus";
  prv_starten();
  // an auf Heart, aus im Detail; das Ende setzt nichts mehr.
  cl_assert_equal_i(s_pulstakt_setzen, 2);
  cl_assert_equal_i(s_pulstakt, 0);
  test_bh_app__initialize();
  s_druecke = "uusxM";
  prv_starten();
  // an, aus im Detail, an zurueck auf Heart, aus beim Schliessen
  cl_assert_equal_i(s_pulstakt_setzen, 4);
  cl_assert_equal_i(s_pulstakt, 0);
}

// Gegenpruefung A6 (2.1): Mit Tracking aus zeigt keine Karte Werte - Select oeffnet kein Detail
// und Heart keinen Puls live; Precision bleibt bedienbar.
void test_bh_app__tracking_aus(void) {
  s_tracking = false;
  s_druecke = "us";
  prv_starten();
  cl_assert_equal_i(s_detail_gezeichnet, BhDetailKeins);
  cl_assert_equal_i(s_fenster_offen, 0);
  test_bh_app__initialize();
  s_tracking = false;
  s_druecke = "uu";
  prv_starten();
  cl_assert_equal_i(s_pulstakt_setzen, 0);
  test_bh_app__initialize();
  s_tracking = false;
  s_druecke = "uuuus";
  prv_starten();
  cl_assert_equal_i(s_detail_gezeichnet, BhDetailPrecision);
}

// Gegenpruefung A4, L3 an allen Tasten: Auch Select, Zurueck und die Tasten von Precision lesen
// nichts neu.
void test_bh_app__alle_tasten_ohne_lesen(void) {
  s_druecke = "usxusxusxusuus";
  prv_starten();
  const char *nach_dem_start = strchr(s_reihe, 'e');
  cl_assert(nach_dem_start != NULL);
  cl_assert(strpbrk(nach_dem_start, "RBQADNFP") == NULL);
  cl_assert(strchr(nach_dem_start, 'S') != NULL); // Precision hat die Stufe gesetzt
}

// Gegenpruefung A5: Kommt die Nacht erst mit Phase 2, waechst das offene Night-Detail mit; sonst
// schnitte die Ebene ab, was dazukam.
void test_bh_app__detail_waechst_mit_den_daten(void) {
  s_offen_beim_laden = BhQuelleNacht;
  s_nachfragen_bis_da = 1;
  s_druecke = "us";
  prv_starten();
  cl_assert_equal_i(s_rollen_inhalt.h, 60);
  test_bh_app__initialize();
  s_offen_beim_laden = BhQuelleNacht;
  s_nachfragen_bis_da = 1;
  s_druecke = "usn";
  prv_starten();
  cl_assert_equal_i(s_rollen_inhalt.h, 332);
}

// F40: Die Karten gleiten wie in Pebbles Health-App (card_view.c:234-274). Mit Unten gleitet die
// gezeigte Karte nach oben hinaus und die neue von unten herein, mit Oben umgekehrt; beide mit
// Pebbles Kurve (moook, ein Bild in der Mitte) und deren Dauer. Unterwegs sind beide zu sehen,
// danach nur die neue. Das Fenster traegt die Farbe, die beim Federn frei wird: unten den weissen
// Inhalt, oben den Kopf der neuen Karte.
void test_bh_app__gleiten_wie_pebble(void) {
  s_druecke = "u";
  prv_starten();
  cl_assert_equal_i(s_spawns_n, 1);
  const Spawn *g = &s_spawns[0];
  cl_assert(g->raus->ebene == &s_ebenen[BhKarteToday]);
  cl_assert(prv_rahmen(g->raus->von, 0, 0, 200, 228));
  cl_assert(prv_rahmen(g->raus->bis, 0, -228, 200, 228));
  cl_assert(g->rein->ebene == &s_ebenen[BhKarteNight]);
  cl_assert(prv_rahmen(g->rein->von, 0, 228, 200, 228));
  cl_assert(prv_rahmen(g->rein->bis, 0, 0, 200, 228));
  const Teil *teile[] = {g->raus, g->rein};
  for (int i = 0; i < 2; i++) {
    cl_assert_equal_i(teile[i]->dauer, MOOOK_DAUER(1));
    s_kurve_mittel = -1;
    teile[i]->kurve(ANIMATION_NORMALIZED_MAX / 3, 0, 228);
    cl_assert_equal_i(s_kurve_mittel, 1);
  }
  cl_assert_equal_i(s_dauer_mittel, 1);
  cl_assert(g->handler.started != NULL && g->handler.stopped != NULL);
  cl_assert_equal_i(g->karten_mitte, (1u << BhKarteToday) | (1u << BhKarteNight));
  cl_assert_equal_i(s_karten_im_bild, 1u << BhKarteNight);
  for (int k = 0; k < BhKartenAnzahl; k++) {
    cl_assert(s_ebene_versteckt[k] == (k != BhKarteNight));
  }
  cl_assert_equal_i(s_hintergrund.argb, GColorWhiteARGB8);

  // Oben, im Kreis aus dem Startmenue von Today auf Settings: von oben herein.
  test_bh_app__initialize();
  s_druecke = "o";
  prv_starten();
  cl_assert_equal_i(s_spawns_n, 1);
  g = &s_spawns[0];
  cl_assert(g->raus->ebene == &s_ebenen[BhKarteToday]);
  cl_assert(prv_rahmen(g->raus->bis, 0, 228, 200, 228));
  cl_assert(g->rein->ebene == &s_ebenen[BhKarteSettings]);
  cl_assert(prv_rahmen(g->rein->von, 0, -228, 200, 228));
  cl_assert(prv_rahmen(g->rein->bis, 0, 0, 200, 228));
  cl_assert(gcolor_equal(s_hintergrund, bh_karte_farbe(BhKarteSettings)));
  cl_assert_equal_s(s_titel, "Settings");

  // Die Richtung folgt der Taste, nicht der Reihenfolge der Karten: Unten von Settings auf Today
  // kommt Today von unten.
  test_bh_app__initialize();
  s_druecke = "ou";
  prv_starten();
  cl_assert_equal_i(s_spawns_n, 2);
  g = &s_spawns[1];
  cl_assert(g->rein->ebene == &s_ebenen[BhKarteToday]);
  cl_assert(prv_rahmen(g->rein->von, 0, 228, 200, 228));
  cl_assert_equal_i(s_hintergrund.argb, GColorWhiteARGB8);
}

// Jede Karte deckt ihre Ebene ganz: unter dem Kopfbalken weiss, sonst zeigte sie dort die Farbe
// des Fensters, das nach Oben die Farbe des Kopfes traegt.
void test_bh_app__karten_deckend(void) {
  s_druecke = "o";
  prv_starten();
  cl_assert(!gcolor_equal(s_hintergrund, GColorWhite));
  cl_assert(s_weiss_gefuellt > 0);
  cl_assert_equal_s(s_titel, "Settings");
}

// Eine Taste mitten im Gleiten bricht es ab wie bei Pebble: Die Karte, die gerade hinausglitt,
// wird versteckt, und die gezeigte gleitet aus der Ruhe weiter hinaus.
void test_bh_app__gleiten_abbrechen(void) {
  s_gleiten_stehen_lassen = true;
  s_druecke = "uu";
  prv_starten();
  cl_assert_equal_i(s_spawns_n, 2);
  const Spawn *zweites = &s_spawns[1];
  cl_assert(zweites->raus->ebene == &s_ebenen[BhKarteNight]);
  cl_assert(prv_rahmen(zweites->raus->von, 0, 0, 200, 228));
  cl_assert(zweites->rein->ebene == &s_ebenen[BhKarteHeart]);
  // Today blieb mitten im Fenster stehen, ist aber versteckt.
  cl_assert(s_ebene_versteckt[BhKarteToday]);
  cl_assert_equal_i(zweites->karten_mitte, (1u << BhKarteNight) | (1u << BhKarteHeart));
}

// Endet die App mitten im Gleiten, ist danach trotzdem alles frei. Das Gleiten laeuft nicht mehr:
// Pebble loescht am Ende des Prozesses nur seinen Timer (process_manager.c:548), wie bei Pebbles
// eigener Health-App.
void test_bh_app__schliessen_beim_gleiten(void) {
  s_gleiten_stehen_lassen = true;
  s_druecke = "u";
  prv_starten();
  cl_assert(s_spawns[0].geplant);
  cl_assert_equal_i(s_flaechen_offen, 0);
  cl_assert_equal_i(s_fenster_offen, 0);
  cl_assert_equal_i(s_offen, 0);
}

// L3 auch beim Gleiten: Die Karte, die hereingleitet, zeichnet aus dem Heap; die Taste liest
// nichts (auch nicht, wenn das Gleiten abbricht).
void test_bh_app__gleiten_ohne_lesen(void) {
  s_gleiten_stehen_lassen = true;
  s_druecke = "uuoouu";
  prv_starten();
  const char *nach_dem_start = strchr(s_reihe, 'e');
  cl_assert(strpbrk(nach_dem_start, "RBQADNFP") == NULL);
}

// --- Schlafmodus (F53) ---

// Select lange auf Night startet ihn ueber S8 (Pebbles Schalter zeigt den Dialog); auf den anderen
// Karten und mit Tracking aus geschieht nichts.
void test_bh_app__schlafmodus_starten(void) {
  s_druecke = "uh";
  prv_starten();
  cl_assert_equal_i(s_umgeschaltet, 1);
  cl_assert(!s_umschalten_laeuft);
  cl_assert_equal_i(s_detail_gezeichnet, BhDetailKeins);
  cl_assert_equal_i(s_offen, 0);

  static const char *const nichts[] = {"h", "uuh", "uuuh", "uuuuh"};
  for (size_t i = 0; i < sizeof(nichts) / sizeof(nichts[0]); i++) {
    test_bh_app__initialize();
    s_druecke = nichts[i];
    prv_starten();
    cl_assert_equal_i(s_umgeschaltet, 0);
  }
  test_bh_app__initialize();
  s_tracking = false;
  s_druecke = "uh";
  prv_starten();
  cl_assert_equal_i(s_umgeschaltet, 0);
}

// Solange er laeuft, oeffnet die App auf Night, und Select heisst "aufgewacht" statt Detail; Select
// lange beendet ihn ebenso.
void test_bh_app__schlafmodus_laeuft(void) {
  s_schlafmodus_seit = 1791748800;
  prv_starten();
  cl_assert_equal_i(s_inhalt_karte, BhKarteNight);

  test_bh_app__initialize();
  s_schlafmodus_seit = 1791748800;
  s_druecke = "s";
  prv_starten();
  cl_assert_equal_i(s_umgeschaltet, 1);
  cl_assert(s_umschalten_laeuft);
  cl_assert_equal_i(s_detail_gezeichnet, BhDetailKeins);

  test_bh_app__initialize();
  s_schlafmodus_seit = 1791748800;
  s_druecke = "h";
  prv_starten();
  cl_assert_equal_i(s_umgeschaltet, 1);
  cl_assert(s_umschalten_laeuft);

  // Auf den anderen Karten oeffnet Select das Detail wie sonst.
  test_bh_app__initialize();
  s_schlafmodus_seit = 1791748800;
  s_druecke = "us";
  prv_starten();
  cl_assert_equal_i(s_umgeschaltet, 0);
  cl_assert_equal_i(s_detail_gezeichnet, BhDetailHeart);
}

// Endete er eben von selbst (S8 schaltet nicht), liest die App die Lage neu und zeichnet.
void test_bh_app__schlafmodus_endete_eben(void) {
  s_schlafmodus_seit = 1791748800;
  s_umschalten_geht = false;
  s_druecke = "s";
  prv_starten();
  cl_assert_equal_i(s_umgeschaltet, 1);
  const char *taste = strchr(s_reihe, 'Z');
  cl_assert(taste != NULL);
  cl_assert(strchr(taste, 'F') != NULL);
  cl_assert(strchr(taste, 'd') != NULL);
}
