/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/app_timer.h"
#include "applib/tick_timer_service.h"
#include "applib/ui/layer.h"
#include "applib/ui/window.h"
#include "apps/system/boulder_health/daten.h"
#include "apps/system/boulder_tagesband/bild.h"
#include "clar.h"
#include "pbl/util/uuid.h"
#include "process_management/pebble_process_md.h"
#include "resource/resource_ids.auto.h"

#include <stdlib.h>
#include <string.h>

// Das Zifferblatt Tagesband beim Start, im Takt und am Ende: dass es ein Zifferblatt ohne
// Systemrechte ist, vor dem ersten Bild liest, nach dem Boot nachlaedt, im Minutentakt auffrischt,
// den Wechsel 12/24 h sofort zeigt, unter der Timeline-Vorschau die untere Zeile weglaesst und
// alles wieder freigibt. Nachgebaut sind Pebbles Fenster, Ebene, Timer, Takt und Heap, dazu die
// Datenschicht der Health-App und das Bild; echt ist tagesband.c.

const PebbleProcessMd *boulder_tagesband_get_app_info(void);

// --- Was das Zifferblatt ruft, mitgezaehlt ---

static void *s_zustand;
static int s_offen; //< Bloecke im Heap
static Window s_fenster;
static bool s_fenster_da;
static Layer s_wurzel;
static Layer s_ebene;
static bool s_ebene_da;
static LayerUpdateProc s_zeichnen;
static GRect s_frei; //< die unverdeckte Flaeche
static int s_dirty;
static bool s_geschoben;
static TickHandler s_minute;
static struct {
  bool an;
  uint32_t ms;
  AppTimerCallback rueckruf;
} s_timer;
static bool s_h24;

// Die Datenschicht: wie oft gelesen, und was Phase 2 noch holt.
static int s_laden, s_nachladen, s_auffrischen;
static uint8_t s_offen_nach_laden;
static int s_nachfragen_bis_fertig; //< so oft sagt bh_daten_nachladen "weiter"
static int s_bilder;
static bool s_verdeckt;
static BhDaten *s_gezeichnet;
static void (*s_im_takt)(void); //< laeuft in app_event_loop

void *app_state_get_user_data(void) {
  return s_zustand;
}

void app_state_set_user_data(void *daten) {
  s_zustand = daten;
}

void *app_zalloc_check(size_t groesse) {
  s_offen++;
  return calloc(1, groesse);
}

void task_free(void *zeiger) {
  s_offen--;
  free(zeiger);
}

Window *window_create(void) {
  cl_assert(!s_fenster_da);
  s_fenster_da = true;
  return &s_fenster;
}

void window_destroy(Window *fenster) {
  cl_assert(fenster == &s_fenster);
  s_fenster_da = false;
}

void window_set_background_color(Window *fenster, GColor farbe) {
}

Layer *window_get_root_layer(const Window *fenster) {
  return &s_wurzel;
}

GRect layer_get_bounds_by_value(const Layer *ebene) {
  return GRect(0, 0, 200, 228);
}

GRect layer_get_unobstructed_bounds_by_value(const Layer *ebene) {
  return s_frei;
}

Layer *layer_create(GRect rahmen) {
  cl_assert(!s_ebene_da);
  s_ebene_da = true;
  return &s_ebene;
}

void layer_destroy(Layer *ebene) {
  cl_assert(ebene == &s_ebene);
  s_ebene_da = false;
}

void layer_set_update_proc(Layer *ebene, LayerUpdateProc proc) {
  s_zeichnen = proc;
}

void layer_add_child(Layer *mutter, Layer *kind) {
}

void layer_mark_dirty(Layer *ebene) {
  s_dirty++;
}

void app_window_stack_push(Window *fenster, bool animiert) {
  s_geschoben = true;
}

void tick_timer_service_subscribe(TimeUnits einheiten, TickHandler handler) {
  cl_assert_equal_i(einheiten, MINUTE_UNIT);
  s_minute = handler;
}

void tick_timer_service_unsubscribe(void) {
  s_minute = NULL;
}

AppTimer *app_timer_register(uint32_t ms, AppTimerCallback rueckruf, void *daten) {
  cl_assert(!s_timer.an);
  s_timer.an = true;
  s_timer.ms = ms;
  s_timer.rueckruf = rueckruf;
  return (AppTimer *)&s_timer;
}

void app_timer_cancel(AppTimer *timer) {
  cl_assert(timer == (AppTimer *)&s_timer);
  s_timer.an = false;
}

bool clock_is_24h_style(void) {
  return s_h24;
}

void app_event_loop(void) {
  if (s_im_takt) {
    s_im_takt();
  }
}

void bh_daten_laden(BhDaten *d) {
  s_laden++;
  memset(d, 0, sizeof(*d));
  d->h24 = true;
  d->offen = s_offen_nach_laden;
}

bool bh_daten_nachladen(BhDaten *d) {
  s_nachladen++;
  return --s_nachfragen_bis_fertig > 0;
}

void bh_daten_auffrischen(BhDaten *d) {
  s_auffrischen++;
}

void bt_zeichnen(GContext *ctx, GRect flaeche, bool verdeckt, const BhDaten *d) {
  s_bilder++;
  s_verdeckt = verdeckt;
  s_gezeichnet = (BhDaten *)d;
}

// --- Ablauf ---

void test_tagesband_app__initialize(void) {
  s_zustand = NULL;
  s_offen = 0;
  s_fenster_da = s_ebene_da = s_geschoben = false;
  s_zeichnen = NULL;
  s_frei = GRect(0, 0, 200, 228);
  s_dirty = 0;
  s_minute = NULL;
  memset(&s_timer, 0, sizeof(s_timer));
  s_h24 = true;
  s_laden = s_nachladen = s_auffrischen = 0;
  s_offen_nach_laden = 0;
  s_nachfragen_bis_fertig = 0;
  s_bilder = 0;
  s_verdeckt = false;
  s_gezeichnet = NULL;
  s_im_takt = NULL;
}

static void prv_lauf(void (*im_takt)(void)) {
  s_im_takt = im_takt;
  boulder_tagesband_get_app_info()->main_func();
}

void test_tagesband_app__ein_zifferblatt_ohne_systemrechte(void) {
  const PebbleProcessMdSystem *md = (const PebbleProcessMdSystem *)boulder_tagesband_get_app_info();
  cl_assert_equal_i(md->common.process_type, ProcessTypeWatchface);
  cl_assert(md->common.is_unprivileged);
  cl_assert_equal_s(md->name, "Tagesband");
  cl_assert_equal_i(md->icon_resource_id, RESOURCE_ID_INVALID);
  const Uuid uuid = {0x33, 0xf4, 0xef, 0x6a, 0x4e, 0x17, 0x4c, 0xe6,
                     0x96, 0xa6, 0x5d, 0xe9, 0x48, 0x53, 0xfb, 0xb3};
  cl_assert(uuid_equal(&md->common.uuid, &uuid));
}

static void prv_start_pruefen(void) {
  // Vor dem ersten Bild gelesen, und das Fenster liegt auf dem Stapel.
  cl_assert_equal_i(s_laden, 1);
  cl_assert(s_geschoben);
  cl_assert(s_minute != NULL);
  cl_assert(!s_timer.an);
}

void test_tagesband_app__start_und_ende(void) {
  prv_lauf(prv_start_pruefen);
  // Am Ende: kein Takt, kein Timer, Fenster und Ebene frei, der Zustand im Heap zurueck.
  cl_assert(s_minute == NULL);
  cl_assert(!s_timer.an);
  cl_assert(!s_fenster_da);
  cl_assert(!s_ebene_da);
  cl_assert_equal_i(s_offen, 0);
}

static void prv_minute(void) {
  const int vorher = s_dirty;
  s_minute(NULL, MINUTE_UNIT);
  cl_assert_equal_i(s_auffrischen, 1);
  cl_assert_equal_i(s_dirty, vorher + 1);
}

void test_tagesband_app__minutentakt_frischt_auf(void) {
  prv_lauf(prv_minute);
}

static void prv_wechsel_12h(void) {
  // Pebble ruft den Handler beim Wechsel 12/24 h mit 0: nur die Einstellung neu, sofort sichtbar.
  s_h24 = false;
  s_minute(NULL, 0);
  cl_assert_equal_i(s_auffrischen, 0);
  s_zeichnen(&s_ebene, NULL);
  cl_assert(!s_gezeichnet->h24);
  cl_assert(s_dirty > 0);
}

void test_tagesband_app__wechsel_12h_sofort(void) {
  prv_lauf(prv_wechsel_12h);
}

static void prv_nachladen(void) {
  // Nach dem Boot fehlt noch etwas: im Sekundenabstand nachfragen, bis alles da ist.
  cl_assert(s_timer.an);
  cl_assert_equal_i(s_timer.ms, BH_NACHLADEN_MS);
  for (int i = 0; i < 2; i++) {
    s_timer.an = false;
    s_timer.rueckruf(NULL);
    cl_assert(s_timer.an);
  }
  s_timer.an = false;
  s_timer.rueckruf(NULL);
  cl_assert(!s_timer.an);
  cl_assert_equal_i(s_nachladen, 3);
  cl_assert(s_dirty >= 3);
}

void test_tagesband_app__nachladen_nach_dem_boot(void) {
  s_offen_nach_laden = BhQuelleNacht | BhQuelleHerz;
  s_nachfragen_bis_fertig = 3;
  prv_lauf(prv_nachladen);
}

static void prv_timer_offen_am_ende(void) {
  cl_assert(s_timer.an);
}

void test_tagesband_app__offener_timer_endet_mit(void) {
  s_offen_nach_laden = BhQuelleNacht;
  s_nachfragen_bis_fertig = 5;
  prv_lauf(prv_timer_offen_am_ende);
  cl_assert(!s_timer.an);
}

static void prv_verdeckt(void) {
  s_zeichnen(&s_ebene, NULL);
  cl_assert(!s_verdeckt);
  // Die Timeline-Vorschau nimmt unten 51 px.
  s_frei = GRect(0, 0, 200, 177);
  s_zeichnen(&s_ebene, NULL);
  cl_assert(s_verdeckt);
  cl_assert_equal_i(s_bilder, 2);
}

void test_tagesband_app__vorschau_verdeckt_die_zeile(void) {
  prv_lauf(prv_verdeckt);
}
