/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "stubs_logging.h"
#include "stubs_pbl_malloc.h"

#include "apps/system/kieselsport/atem.h"
#include "apps/system/kieselsport/lauf_window.h"
#include "apps/system/kieselsport/symbole.h"
#include "apps/system/kieselsport/telefon.h"
#include "apps/system/kieselsport/thema.h"
#undef malloc
#undef calloc
#undef free
#undef time

// Der laufende Schirm (lauf_window.c): wann er neu gezeichnet wird. Der
// Dienst liefert einen Stand, den der Test stellt; Uhr, Wecker, Fenster und
// Tasten sind nachgebaut. Gezeichnet wird hier nichts - gezaehlt wird, wie oft
// der Schirm darum bittet (layer_mark_dirty).
//
// Aus b25 (main a7d8b16e1); neu ist der Endschirm vor Pebbles Meldung (V4).

// --- Uhr und Wecker ---

static time_t s_jetzt;
time_t rtc_get_time(void) { return s_jetzt; }

typedef struct {
  uint32_t id;
  uint32_t faellig_ms;
  AppTimerCallback cb;
  void *daten;
} Wecker;
#define WECKER_MAX 16
static Wecker s_wecker[WECKER_MAX];
static uint32_t s_wecker_id;
static uint32_t s_ms;
static int s_wecker_gestellt;   //< jedes Stellen weckt die App einmal

AppTimer *app_timer_register(uint32_t timeout_ms, AppTimerCallback cb, void *daten) {
  s_wecker_gestellt++;
  for (int i = 0; i < WECKER_MAX; i++) {
    if (!s_wecker[i].id) {
      s_wecker[i] = (Wecker){ .id = ++s_wecker_id, .faellig_ms = s_ms + timeout_ms,
                              .cb = cb, .daten = daten };
      return (AppTimer *)(uintptr_t)s_wecker[i].id;
    }
  }
  cl_fail("zu viele Wecker");
  return NULL;
}

void app_timer_cancel(AppTimer *timer) {
  for (int i = 0; i < WECKER_MAX; i++) {
    if (s_wecker[i].id == (uint32_t)(uintptr_t)timer) s_wecker[i].id = 0;
  }
}

// Die Uhr der App in Ticks, aufgerundet: zurueck in ms ergibt sie genau s_ms.
RtcTicks sys_get_ticks(void) { return ((RtcTicks)s_ms * RTC_TICKS_HZ + 999) / 1000; }

// Laesst [ms] vergehen; die Uhr in Sekunden laeuft mit.
static void prv_warte(uint32_t ms) {
  const uint32_t bis = s_ms + ms;
  for (;;) {
    int n = -1;
    for (int i = 0; i < WECKER_MAX; i++) {
      if (s_wecker[i].id && s_wecker[i].faellig_ms <= bis &&
          (n < 0 || s_wecker[i].faellig_ms < s_wecker[n].faellig_ms)) {
        n = i;
      }
    }
    if (n < 0) break;
    const Wecker w = s_wecker[n];
    s_wecker[n].id = 0;
    if (w.faellig_ms > s_ms) {
      s_jetzt += (w.faellig_ms / 1000) - (s_ms / 1000);
      s_ms = w.faellig_ms;
    }
    w.cb(w.daten);
  }
  s_jetzt += (bis / 1000) - (s_ms / 1000);
  s_ms = bis;
}

// --- Der Dienst ---

static KsStand s_stand;
static int s_pause_befehle;
static int s_speichern;
static Atemrhythmus s_rhythmus;
void kieselsport_stand(KsStand *aus) { *aus = s_stand; }
void kieselsport_atem(Atemrhythmus *aus) { *aus = s_rhythmus; }
const ArtInfo *kieselsport_art_info(Sportart art) {
  static const ArtInfo s_info = { .schritte = true, .distanz = true };
  return &s_info;
}
int kieselsport_zone(uint16_t bpm) { return bpm >= 130 ? 3 : 2; }
void kieselsport_starte(Sportart art, uint32_t beginn) {}
void kieselsport_pause_umschalten(void) { s_pause_befehle++; }
void kieselsport_speichern(void) { s_speichern++; }
void kieselsport_verwerfen(void) {}
// Die gemerkte Kurve des Trainings: mit ihr zeigt der Endschirm die Zonen (15 s).
static bool s_zonen;
bool kieselsport_kurve_zeiten(uint32_t sekunden[KS_ZONEN + 1]) {
  if (!s_zonen) return false;
  for (int z = 0; z <= KS_ZONEN; z++) sekunden[z] = 60 * (uint32_t)z;
  return true;
}
uint32_t kieselsport_kurve_beginn(void) { return s_zonen ? s_stand.t.beginn : 0; }

// --- Telefon ---

// Die Zustandsmeldungen ans Telefon, der Reihe nach.
static Trainingsmeldung s_meldung[16];
static int s_meldungen;
void telefon_melde_zustand(Trainingsmeldung was, uint8_t art, uint32_t beginn) {
  cl_assert(s_meldungen < 16);
  s_meldung[s_meldungen++] = was;
}
void telefon_nachsenden(void) {}
static bool s_telefon_wartet;
bool telefon_wartet(void) { return s_telefon_wartet; }
void telefon_stopp_nachsenden(void) {}
bool telefon_stopp_wartet(void) { return false; }

// --- Fenster, Schicht, Tasten ---

static Window s_fenster;
static Layer s_wurzel;
static Layer s_flaeche;
static WindowHandlers s_handler;
static ClickConfigProvider s_tasten;
static ClickHandler s_klick[NUM_BUTTONS];
static int s_neu_zeichnen;

Window *window_create(void) { return &s_fenster; }
void window_destroy(Window *window) {}
void window_set_background_color(Window *window, GColor color) {}
void window_set_click_config_provider(Window *window, ClickConfigProvider provider) {
  s_tasten = provider;
}
void window_set_window_handlers_by_value(Window *window, WindowHandlers handlers) {
  s_handler = handlers;
}
Layer *window_get_root_layer(const Window *window) { return &s_wurzel; }
void app_window_stack_push(Window *window, bool animated) {
  s_handler.load(window);
  if (s_tasten) s_tasten(NULL);
}
// Wann der Schirm zuging (Uhr des Tests), 0 = noch offen.
static time_t s_zu_um;
bool app_window_stack_remove(Window *window, bool animated) {
  if (!s_zu_um) s_zu_um = s_jetzt;
  return true;
}
Window *app_window_stack_pop(bool animated) { return NULL; }
void app_window_stack_pop_all(const bool animated) {}
void window_single_click_subscribe(ButtonId button_id, ClickHandler handler) {
  s_klick[button_id] = handler;
}

Layer *layer_create(GRect frame) { return &s_flaeche; }
void layer_destroy(Layer *layer) {}
void layer_set_update_proc(Layer *layer, LayerUpdateProc update_proc) {}
void layer_add_child(Layer *parent, Layer *child) {}
GRect layer_get_bounds_by_value(const Layer *layer) { return GRect(0, 0, 200, 228); }
GRect layer_get_unobstructed_bounds_by_value(const Layer *layer) { return GRect(0, 0, 200, 228); }
void layer_mark_dirty(Layer *layer) {
  cl_assert(layer == &s_flaeche);
  s_neu_zeichnen++;
}
void app_unobstructed_area_service_subscribe(UnobstructedAreaHandlers handlers, void *context) {}
void app_unobstructed_area_service_unsubscribe(void) {}

// --- Was nur beim Zeichnen gebraucht wuerde: hier nie gerufen ---

void graphics_context_set_text_color(GContext *ctx, GColor color) { cl_fail("gezeichnet"); }
void graphics_context_set_fill_color(GContext *ctx, GColor color) { cl_fail("gezeichnet"); }
void graphics_context_set_stroke_color(GContext *ctx, GColor color) { cl_fail("gezeichnet"); }
void graphics_context_set_stroke_width(GContext *ctx, uint8_t width) { cl_fail("gezeichnet"); }
void graphics_fill_round_rect_by_value(GContext *ctx, GRect rect, uint16_t radius,
                                       GCornerMask corner_mask) { cl_fail("gezeichnet"); }
void graphics_draw_rect_by_value(GContext *ctx, GRect rect) { cl_fail("gezeichnet"); }
void graphics_draw_line(GContext *ctx, GPoint p0, GPoint p1) { cl_fail("gezeichnet"); }
void graphics_fill_circle(GContext *ctx, GPoint p, uint16_t radius) { cl_fail("gezeichnet"); }
void graphics_draw_circle(GContext *ctx, GPoint p, uint16_t radius) { cl_fail("gezeichnet"); }
void graphics_draw_text(GContext *ctx, const char *text, GFont const font, const GRect box,
                        const GTextOverflowMode overflow_mode, const GTextAlignment alignment,
                        GTextAttributes *text_attributes) { cl_fail("gezeichnet"); }
GSize app_graphics_text_layout_get_content_size(const char *text, GFont const font,
                                                const GRect box,
                                                const GTextOverflowMode overflow_mode,
                                                const GTextAlignment alignment) {
  cl_fail("gezeichnet");
  return GSizeZero;
}
GFont fonts_get_system_font(const char *font_key) { return NULL; }
void gpath_init(GPath *path, const GPathInfo *init) { cl_fail("gezeichnet"); }
void gpath_move_to(GPath *path, GPoint point) { cl_fail("gezeichnet"); }
void gpath_draw_filled(GContext *ctx, GPath *path) { cl_fail("gezeichnet"); }
void gpath_draw_outline(GContext *ctx, GPath *path) { cl_fail("gezeichnet"); }
GColor gcolor_legible_over(GColor background_color) { return GColorBlack; }
void thema_leiste(GContext *ctx, GRect b, Symbol oben, Symbol mitte, Symbol unten) {
  cl_fail("gezeichnet");
}
GColor thema_zonenfarbe(int zone) { return GColorBlack; }
GColor thema_sportfarbe(Sportart art) { return GColorBlack; }
void symbol_sport(GContext *ctx, Sportart art, GPoint mitte, int16_t groesse, GColor farbe) {
  cl_fail("gezeichnet");
}
void clock_copy_time_string(char *buffer, uint8_t size) { cl_fail("gezeichnet"); }
bool clock_is_24h_style(void) { return true; }
struct pbl_tm *pbl_override_localtime(const time_t *timep) {
  cl_fail("gezeichnet");
  return NULL;
}
bool connection_service_peek_pebble_app_connection(void) { return true; }
const char *app_get_system_locale(void) { return "en_US"; }

// --- Brummen: was, wann (Uhr des Tests) ---
//
// K und L: kurz und lang im Countdown. E, H, A: einatmen, halten, ausatmen; X: das Ende.
typedef struct {
  char was;
  uint32_t ms;
} Brummen;
static Brummen s_brummen[64];
static int s_brummen_n;
static void prv_brumm(char was) {
  cl_assert(s_brummen_n < 64);
  s_brummen[s_brummen_n++] = (Brummen){ .was = was, .ms = s_ms };
}
void vibes_short_pulse(void) { prv_brumm('K'); }
void vibes_long_pulse(void) { prv_brumm('L'); }
void vibes_enqueue_custom_pattern(VibePattern muster) {
  if (muster.durations == atem_muster(AtemEin).durations) prv_brumm('E');
  else if (muster.durations == atem_muster(AtemHalten).durations) prv_brumm('H');
  else if (muster.durations == atem_muster(AtemAus).durations) prv_brumm('A');
  else if (muster.durations == atem_muster_ende().durations) prv_brumm('X');
  else cl_fail("unbekanntes Brummen");
}

// --- Hilfen ---

static void prv_laufend(uint32_t dauer, uint16_t puls) {
  s_stand.zustand = LaufLaeuft;
  s_stand.t.dauer_s = dauer;
  s_stand.t.beginn = 1790000000;
  s_stand.puls = puls;
  s_stand.frisch = true;
  s_stand.minute[dauer % KS_MINUTE_S] = (uint8_t)puls;
}

void test_kieselsport_lauf__initialize(void) {
  memset(s_wecker, 0, sizeof(s_wecker));
  s_wecker_id = 0;
  s_ms = 0;
  s_jetzt = 1790000000 - 1790000000 % 60 + 5;
  memset(&s_stand, 0, sizeof(s_stand));
  memset(s_klick, 0, sizeof(s_klick));
  s_tasten = NULL;
  s_neu_zeichnen = 0;
  s_pause_befehle = 0;
  s_speichern = 0;
  s_meldungen = 0;
  s_brummen_n = 0;
  s_rhythmus = (Atemrhythmus){ .wert = { 4, 4, 4, 4, 10 } };
  s_zonen = false;
  s_telefon_wartet = false;
  s_zu_um = 0;
  lauf_window_zuruecksetzen();
}

void test_kieselsport_lauf__cleanup(void) {}

void test_kieselsport_lauf__gleicher_stand_zeichnet_nicht_neu(void) {
  prv_laufend(100, 120);
  lauf_window_zeige_laufend();
  prv_warte(100);
  const int erstes = s_neu_zeichnen;
  cl_assert(erstes >= 1);
  // Zwei Sekunden, viermal gefragt - derselbe Stand, kein neues Bild.
  prv_warte(2000);
  cl_assert_equal_i(s_neu_zeichnen, erstes);
  // Eine Sekunde weiter: genau ein neues Bild.
  prv_laufend(101, 120);
  prv_warte(500);
  cl_assert_equal_i(s_neu_zeichnen, erstes + 1);
  // Der Puls aendert sich: auch.
  s_stand.puls = 125;
  prv_warte(500);
  cl_assert_equal_i(s_neu_zeichnen, erstes + 2);
}

void test_kieselsport_lauf__pause_steht_still(void) {
  prv_laufend(100, 120);
  s_stand.zustand = LaufPause;
  lauf_window_zeige_laufend();
  prv_warte(100);
  const int erstes = s_neu_zeichnen;
  // Zwanzig Sekunden Pause: die Zeit steht, der Schirm auch.
  prv_warte(20000);
  cl_assert(s_neu_zeichnen - erstes <= 1);
}

void test_kieselsport_lauf__uhrzeit_und_hinweis_kommen_trotzdem(void) {
  prv_laufend(100, 120);
  s_stand.zustand = LaufPause;
  lauf_window_zeige_laufend();
  prv_warte(100);
  // Eine neue Minute: die Uhrzeit auf dem Schirm geht weiter.
  int vorher = s_neu_zeichnen;
  prv_warte(60000);
  cl_assert(s_neu_zeichnen > vorher);
  // Der Hinweis "nochmal druecken" verschwindet nach drei Sekunden wieder.
  prv_warte(5000);
  cl_assert(s_klick[BUTTON_ID_DOWN]);
  s_klick[BUTTON_ID_DOWN](NULL, NULL);
  vorher = s_neu_zeichnen;
  prv_warte(4000);
  cl_assert(s_neu_zeichnen > vorher);
}

// --- V4: Pebbles Meldung kommt 30 s nach dem Speichern ---

// Der Dienst hat gespeichert (dienst.c): der Stand meldet ein neues Ende. Rueckgabe: wann.
static time_t prv_gespeichert(void) {
  s_stand.zustand = LaufAus;
  s_stand.ende = KsEndeGespeichert;
  s_stand.ende_nr++;
  return s_jetzt;
}

// Bestaetigt das Telefon spaet, stuenden die Zonen sonst noch 15 s - bis in Pebbles Meldung
// hinein, die sich darueber legt.
void test_kieselsport_lauf__endschirm_zu_bevor_pebble_meldet(void) {
  prv_laufend(600, 140);
  lauf_window_zeige_laufend();
  prv_warte(100);
  s_zonen = true;
  s_telefon_wartet = true;
  const time_t gespeichert = prv_gespeichert();
  prv_warte(18000);
  cl_assert_equal_i(s_zu_um, 0);
  s_telefon_wartet = false;
  prv_warte(20000);
  cl_assert(s_zu_um != 0);
  cl_assert(s_zu_um - gespeichert < 30);
}

// Bestaetigt es gleich, bleiben die Zonen 15 s zum Lesen stehen - wie in b25.
void test_kieselsport_lauf__endschirm_zeigt_die_zonen(void) {
  prv_laufend(600, 140);
  lauf_window_zeige_laufend();
  prv_warte(100);
  s_zonen = true;
  s_telefon_wartet = true;
  const time_t gespeichert = prv_gespeichert();
  prv_warte(2000);
  s_telefon_wartet = false;
  prv_warte(30000);
  cl_assert(s_zu_um - gespeichert >= 17);
  cl_assert(s_zu_um - gespeichert < 30);
}

// --- Die Atemuebung: der Takt nach der Uhr der App ---

// Die aktive Zeit, wie der Dienst sie zaehlt: in ganzen Sekunden und [s_nachlauf_ms] hinter der
// Uhr der App (sein Takt laeuft auf KernelBG).
static uint32_t s_aktiv_ms;
static uint32_t s_nachlauf_ms;

// Laesst [ms] vergehen, in Schritten von 50 ms, und zaehlt dabei die Zeit des Dienstes.
static void prv_atmen(uint32_t ms) {
  for (uint32_t t = 0; t < ms; t += 50) {
    prv_warte(50);
    if (s_stand.zustand == LaufLaeuft) s_aktiv_ms += 50;
    s_stand.t.dauer_s = s_aktiv_ms > s_nachlauf_ms ? (s_aktiv_ms - s_nachlauf_ms) / 1000 : 0;
  }
}

// Atmen gewaehlt, Select, der Countdown: Rueckgabe die Uhr beim ersten Einatmen. Der Dienst hat
// dann gestartet.
static uint32_t prv_atem_starten(bool countdown) {
  s_aktiv_ms = 0;
  lauf_window_zeige(ArtAtem);
  s_klick[BUTTON_ID_SELECT](NULL, NULL);
  if (countdown) {
    prv_warte(3000);
  } else {
    prv_warte(500);
    s_klick[BUTTON_ID_SELECT](NULL, NULL);
  }
  s_stand.zustand = LaufLaeuft;
  s_stand.art = ArtAtem;
  s_stand.t.beginn = (uint32_t)s_jetzt;
  return s_ms;
}

static void prv_brummen_ist(const char *soll) {
  char ist[65];
  for (int i = 0; i < s_brummen_n; i++) ist[i] = s_brummen[i].was;
  ist[s_brummen_n] = 0;
  cl_assert_equal_s(ist, soll);
}

// Jede Phase brummt zu ihrem Beginn, auf die Millisekunde der Uhr der App. Das erste Einatmen
// sagt das "Los" des Countdowns, kein Brummen dazu.
void test_kieselsport_lauf__atem_brummt_jede_phase(void) {
  const uint32_t t0 = prv_atem_starten(true);
  prv_brummen_ist("KKKL");
  cl_assert_equal_i(s_brummen[3].ms, t0);
  s_brummen_n = 0;
  prv_atmen(32000);
  prv_brummen_ist("HAHEHAHE");
  for (int i = 0; i < 8; i++) {
    cl_assert_equal_i(s_brummen[i].ms, t0 + 4000 * (uint32_t)(i + 1));
  }
}

// Wer den Countdown mit einem zweiten Select ueberspringt, hoert kein "Los": dann brummt das
// erste Einatmen selbst.
void test_kieselsport_lauf__atem_ohne_countdown_brummt_das_einatmen(void) {
  const uint32_t t0 = prv_atem_starten(false);
  prv_brummen_ist("KE");
  cl_assert_equal_i(s_brummen[1].ms, t0);
}

// Halten von 0 s gibt es nicht: weder als Phase noch als Brummen. Nach der letzten Runde das Ende.
void test_kieselsport_lauf__atem_ohne_halten(void) {
  s_rhythmus = (Atemrhythmus){ .wert = { 2, 0, 3, 0, 3 } };
  const uint32_t t0 = prv_atem_starten(true);
  s_brummen_n = 0;
  prv_atmen(16000);
  prv_brummen_ist("AEAEAX");
  const uint32_t soll[] = { 2000, 5000, 7000, 10000, 12000, 15000 };
  for (int i = 0; i < 6; i++) {
    cl_assert_equal_i(s_brummen[i].ms, t0 + soll[i]);
  }
  cl_assert_equal_i(s_speichern, 1);
}

// Das Ende wie Pause und Speichern von Hand: erst, wenn der Dienst die ganze Zeit hat - seine
// Sekunden hinken nach, und das Telefon rechnet die Runden aus der gespeicherten Dauer.
void test_kieselsport_lauf__atem_ende_wie_pause_und_speichern(void) {
  s_rhythmus = (Atemrhythmus){ .wert = { 4, 4, 4, 4, 1 } };
  s_nachlauf_ms = 1200;
  const uint32_t t0 = prv_atem_starten(true);
  prv_atmen(16000);
  cl_assert_equal_i(s_brummen[s_brummen_n - 1].was, 'X');
  cl_assert_equal_i(s_brummen[s_brummen_n - 1].ms, t0 + 16000);
  // Der Dienst steht bei 14 s: noch nichts.
  cl_assert_equal_i(s_pause_befehle, 0);
  cl_assert_equal_i(s_speichern, 0);
  prv_atmen(1500);
  // Mit 16 s beim Dienst: Pause, ans Telefon, Speichern - genau einmal.
  cl_assert_equal_i(s_pause_befehle, 1);
  cl_assert_equal_i(s_speichern, 1);
  cl_assert_equal_i(s_meldungen, 2);
  cl_assert_equal_i(s_meldung[0], ZustandStart);
  cl_assert_equal_i(s_meldung[1], ZustandPause);
  s_stand.zustand = LaufPause;
  prv_atmen(3000);
  cl_assert_equal_i(s_pause_befehle, 1);
  cl_assert_equal_i(s_speichern, 1);
  // Der Dienst hat gespeichert: der Endschirm wie sonst, er geht vor Pebbles Meldung zu.
  const time_t gespeichert = prv_gespeichert();
  prv_warte(30000);
  cl_assert(s_zu_um != 0);
  cl_assert(s_zu_um - gespeichert < 30);
  s_nachlauf_ms = 0;
}

// Zaehlt der Dienst die Zeit nicht zu Ende, wartet das Ende hoechstens fuenf Sekunden.
void test_kieselsport_lauf__atem_ende_wartet_nicht_ewig(void) {
  s_rhythmus = (Atemrhythmus){ .wert = { 4, 4, 4, 4, 1 } };
  s_nachlauf_ms = 60000;
  const uint32_t t0 = prv_atem_starten(true);
  prv_atmen(16000 + 4500);
  cl_assert_equal_i(s_speichern, 0);
  prv_atmen(1000);
  cl_assert_equal_i(s_speichern, 1);
  cl_assert_equal_i(s_pause_befehle, 1);
  cl_assert(s_ms - t0 >= 21000);
  s_nachlauf_ms = 0;
}

// Die Pause haelt die Uebung an, ohne Brummen; das Weiter setzt sie dort fort, wo sie stand.
void test_kieselsport_lauf__atem_pause_haelt_den_takt_an(void) {
  const uint32_t t0 = prv_atem_starten(true);
  s_brummen_n = 0;
  prv_atmen(2000);
  s_klick[BUTTON_ID_SELECT](NULL, NULL);
  s_stand.zustand = LaufPause;
  prv_atmen(10000);
  cl_assert_equal_i(s_brummen_n, 0);
  s_klick[BUTTON_ID_SELECT](NULL, NULL);
  s_stand.zustand = LaufLaeuft;
  const uint32_t weiter = s_ms;
  prv_atmen(4000);
  prv_brummen_ist("H");
  cl_assert_equal_i(s_brummen[0].ms, weiter + 2000);
  cl_assert_equal_i(s_ms - t0, 16000);
  cl_assert_equal_i(s_meldung[1], ZustandPause);
  cl_assert_equal_i(s_meldung[2], ZustandWeiter);
}

// Ein Stand des Dienstes von vor der Taste laesst die Uebung nicht weiterlaufen: Pause 200 ms vor
// dem Halten, und der Dienst meldet noch eine halbe Sekunde "laeuft".
void test_kieselsport_lauf__atem_alter_stand_nach_der_taste(void) {
  prv_atem_starten(true);
  s_brummen_n = 0;
  prv_atmen(3800);
  s_klick[BUTTON_ID_SELECT](NULL, NULL);
  prv_warte(600);
  s_stand.zustand = LaufPause;
  prv_atmen(5000);
  cl_assert_equal_i(s_brummen_n, 0);
}

// Neu geoeffnet, waehrend das Atmen laeuft: die Uebung steht bei der aktiven Zeit des Dienstes,
// ohne Brummen fuer die Phase, die schon laeuft.
void test_kieselsport_lauf__atem_neu_geoeffnet(void) {
  s_stand.zustand = LaufLaeuft;
  s_stand.art = ArtAtem;
  s_stand.t.dauer_s = 10;
  s_aktiv_ms = 10000;
  lauf_window_zeige_laufend();
  prv_warte(50);
  const uint32_t verankert = s_ms;
  prv_atmen(6500);
  // Bei 10 s steht das Ausatmen der ersten Runde, 2 s vor dem leeren Halten.
  prv_brummen_ist("HE");
  cl_assert_equal_i(s_brummen[0].ms, verankert + 2000);
  cl_assert_equal_i(s_brummen[1].ms, verankert + 6000);
}

// Sind die Runden beim Oeffnen schon um, endet die Uebung gleich - Pause und Speichern, ohne das
// Brummen des Endes, das keiner mehr erwartet.
void test_kieselsport_lauf__atem_beim_oeffnen_schon_um(void) {
  s_stand.zustand = LaufLaeuft;
  s_stand.art = ArtAtem;
  s_stand.t.dauer_s = 170;
  lauf_window_zeige_laufend();
  prv_warte(100);
  cl_assert_equal_i(s_brummen_n, 0);
  cl_assert_equal_i(s_pause_befehle, 1);
  cl_assert_equal_i(s_speichern, 1);
  cl_assert_equal_i(s_meldung[0], ZustandPause);
}

// Gut 15 Bilder je Sekunde, solange der Kreis waechst oder schrumpft; beim Halten eines je
// Sekunde fuer die Zahl.
void test_kieselsport_lauf__atem_bilder(void) {
  const uint32_t t0 = prv_atem_starten(true);
  prv_warte(1000);
  int vorher = s_neu_zeichnen;
  prv_warte(1000);
  const int atmen = s_neu_zeichnen - vorher;
  cl_assert(atmen >= 15 && atmen <= 16);
  prv_warte(t0 + 4100 - s_ms);
  vorher = s_neu_zeichnen;
  prv_warte(2000);
  cl_assert_equal_i(s_neu_zeichnen - vorher, 2);
}

// Auf der Pulsseite zeichnet der Kreis nicht und weckt die App nur zum Wechsel der Phase; gebrummt
// wird weiter.
void test_kieselsport_lauf__atem_auf_anderer_seite(void) {
  const uint32_t t0 = prv_atem_starten(true);
  s_brummen_n = 0;
  prv_warte(500);
  s_klick[BUTTON_ID_DOWN](NULL, NULL);
  const int vorher = s_neu_zeichnen;
  const int gestellt = s_wecker_gestellt;
  prv_warte(t0 + 3900 - s_ms);
  // Die Abfrage des Dienstes zweimal je Sekunde, der Takt der Uebung gar nicht.
  cl_assert(s_wecker_gestellt - gestellt <= 7);
  prv_warte(t0 + 4100 - s_ms);
  cl_assert(s_neu_zeichnen - vorher <= 1);
  prv_brummen_ist("H");
  cl_assert_equal_i(s_brummen[0].ms, t0 + 4000);
  // Zurueck auf der Atemseite mitten im Einatmen der zweiten Runde: die Bilder kommen gleich.
  prv_warte(t0 + 16500 - s_ms);
  s_klick[BUTTON_ID_DOWN](NULL, NULL);
  s_klick[BUTTON_ID_DOWN](NULL, NULL);
  const int zurueck = s_neu_zeichnen;
  prv_warte(300);
  cl_assert(s_neu_zeichnen - zurueck >= 4);
}
