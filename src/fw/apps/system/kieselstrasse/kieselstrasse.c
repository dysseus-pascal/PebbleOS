/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pebble_compat.h"
#include "nav_window.h"
#include "pfeil.h"
#include "schluessel.h"
#include "texte.h"
#include "weg.h"

#include "process_management/pebble_process_md.h"

// Kieselstrasse - der naechste Schritt einer OsmAnd-Navigation am Handgelenk.
//
// IN DER FIRMWARE, MIT DERSELBEN KENNUNG WIE DIE STORE-APP
// (a83bf269-e798-45bc-9bf0-a0f68362a79a). So erreichen sie dieselben
// Nachrichten von Boulder, mit denselben festen Schluesseln (schluessel.h),
// und sie liest den Persist, den die Store-App zuletzt schrieb. Steht die
// Store-App noch auf der Uhr, blendet das Startmenue sie aus
// (app_install_manager.c, prv_uuid_gehoert_system_app).
//
// AUF ABRUF: Boulder verbindet sich nur mit OsmAnd, solange diese App auf der
// Uhr offen ist, und holt sie nicht mehr von selbst nach vorn. Wer den
// naechsten Schritt sehen will, oeffnet sie.
//
// Was sie NICHT tut: Karten, Routen, Rechnen. Die Uhr hat kein GPS; jede Zahl
// kommt fertig vom Telefon.

// Wie oft hoechstens gesummt wird. Ein Netz unter dem Kern-Vergleich: sollte
// die Gegenseite je die Abbiegeart flattern lassen, klopft die Uhr trotzdem
// nicht Sturm. Der Preis ist ehrlich zu nennen - folgt ein echter zweiter
// Schritt binnen zwanzig Sekunden ("rechts, dann sofort links"), bleibt sein
// Summen aus. Auf dem Schirm steht er trotzdem.
#define KST_SUMM_PAUSE_S 20

// 16 Byte der Kennung, in der Reihenfolge der Schreibweise oben.
#define KIESELSTRASSE_UUID_INIT                                                           \
  {0xa8, 0x3b, 0xf2, 0x69, 0xe7, 0x98, 0x45, 0xbc, 0x9b, 0xf0, 0xa0, 0xf6, 0x83, 0x62, \
   0xa7, 0x9a}

static time_t s_zuletzt_gesummt;

// DER EINGANG FASST AUCH LANGE NAMEN. Sechs Zahlen brauchen 67 Byte, der
// Rest bleibt dem Strassennamen. Boulder kuerzt ihn nicht, und eine
// Ausfahrt mit Wegweisern ("Exit 23 · A1 Bern, Zuerich, Luzern") wird auf
// Kyrillisch schnell laenger als die 245 Byte, die 320 erlaubten - dann
// verwarf die Uhr die ganze Nachricht, und das alte Manoever blieb stehen.
// 640 Byte fassen gut 560 Byte Namen; die Uhr behaelt ohnehin nur 47.
#define KST_EINGANG_BYTE 640
// Keinen Ausgang: die App schickt nie etwas (nur das System quittiert).
#define KST_AUSGANG_BYTE 0

// Eine Nachricht, die nicht ankam - zu gross, oder der Eingang war besetzt.
// Die Uhr zeigt dann weiter den alten Schritt; wenigstens im Log soll stehen,
// warum.
static void prv_verworfen(AppMessageResult grund, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "Nachricht verworfen: %d", (int)grund);
}

static void prv_empfangen(DictionaryIterator *iter, void *context) {
  const bool neuer_schritt = weg_uebernimm(iter);
  nav_window_refresh();

  if (!neuer_schritt) return;
  const time_t jetzt = time(NULL);
  if (jetzt - s_zuletzt_gesummt < KST_SUMM_PAUSE_S) return;
  s_zuletzt_gesummt = jetzt;
  // In der Ruhezeit schweigt die Uhr - so haelt sie es auch mit Mitteilungen.
  if (!quiet_time_is_active()) vibes_short_pulse();
}

#ifdef KST_DEMO
// Nur im Pruefbau (-DKST_DEMO): eine Schleife ueber alle Zeichen, damit sich
// die Anzeige im Emulator ansehen laesst - ohne Telefon kommt sonst nie etwas
// an. Ein Tippen auf den Schirm (pbl touch) schaltet weiter; so lassen sich
// die Bilder der Reihe nach aufnehmen, ohne auf eine Uhr zu warten.
typedef struct {
  int art, meter;
  const char *strasse;
  int ausfahrt, winkel;  // winkel 999: keiner
} DemoStand;

static const DemoStand s_demo[] = {
  { 1, 800, "Bahnhofstrasse", 0, 999 },
  { 2, 250, "Hauptstrasse", 0, 999 },
  { 3, 120, "Seestrasse", 0, 999 },
  { 4, 60, "Rue du Lac", 0, 999 },
  { 5, 15, "Bahnhofstrasse", 0, 999 },
  { 6, 1200, "Avenue de la Gare", 0, 999 },
  { 7, 90, "Chemin des Pins", 0, 999 },
  { 8, 2400, "A1", 0, 999 },
  { 9, 450, "Bern", 0, 999 },
  { 10, 30, "Hauptstrasse", 0, 999 },
  { 11, 200, "Hauptstrasse", 0, 999 },
  { 12, 350, "", 0, 999 },
  { 13, 180, "Exit 2 · Rue de la Côte", 2, 0 },
  { 14, 180, "Exit 1 · Station Road", 1, -90 },
  { 5, 280, "Route de la Gare de Neuchatel", 0, 999 },
  { 6, 150, "", 0, 999 },
  { 13, 120, "Exit 3 · Rue du Seyon", 3, -90 },
  { 0, -1, "", 0, 999 },
  // Wenden im Kreisel: die lange Seite, knapp an der Einfahrt vorbei.
  { 13, 90, "Exit 4 · Rue de l'Ecluse", 4, 180 },
  { 14, 90, "Exit 3", 3, 180 },
};
static int s_demo_nr;

static void prv_demo(void) {
  const DemoStand *d = &s_demo[s_demo_nr];
  uint8_t puffer[160];
  Tuplet werte[] = {
    TupletInteger(MESSAGE_KEY_ABBIEGEART, (int32_t)d->art),
    TupletInteger(MESSAGE_KEY_ENTFERNUNG, (int32_t)d->meter),
    TupletCString(MESSAGE_KEY_STRASSE, d->strasse),
    TupletInteger(MESSAGE_KEY_REST, (int32_t)(d->art ? 12400 : -1)),
    TupletInteger(MESSAGE_KEY_ANKUNFT, (int32_t)(time(NULL) + 1500)),
    TupletInteger(MESSAGE_KEY_AUSFAHRT, (int32_t)d->ausfahrt),
    TupletInteger(MESSAGE_KEY_WINKEL, (int32_t)d->winkel),
  };
  // Ohne Winkel fehlt der Schluessel ganz - wie bei einem Telefon, das ihn
  // nicht kennt.
  const uint8_t anzahl = (uint8_t)(ARRAY_LENGTH(werte) - (d->winkel == 999 ? 1 : 0));
  // DIE BENUTZTE GROESSE ZURUECKLESEN, nicht die des Puffers weiterreichen.
  // Wer zum Lesen den ganzen Puffer reicht, laesst den Leser hinter dem Ende
  // weiterlaufen; er findet dort Muell, meldet das Woerterbuch als kaputt und
  // dict_find liefert fuer alles null - ohne ein Wort der Warnung.
  uint32_t benutzt = sizeof(puffer);
  dict_serialize_tuplets_to_buffer(werte, anzahl, puffer, &benutzt);
  DictionaryIterator lesen;
  dict_read_begin_from_buffer(&lesen, puffer, (uint16_t)benutzt);
  weg_uebernimm(&lesen);
  APP_LOG(APP_LOG_LEVEL_INFO, "Demo %d", s_demo_nr);
}

// Ein Tippen im Emulator kommt mitunter doppelt an; danach ist kurz Ruhe.
static AppTimer *s_demo_sperre;

static void prv_demo_frei(void *kontext) { s_demo_sperre = NULL; }

static void prv_demo_weiter(void) {
  s_demo_nr = (s_demo_nr + 1) % (int)ARRAY_LENGTH(s_demo);
  prv_demo();
  nav_window_refresh();
}

static void prv_demo_tippen(const TouchEvent *ereignis, void *kontext) {
  if (ereignis->type != TouchEvent_Touchdown || s_demo_sperre) return;
  s_demo_sperre = app_timer_register(700, prv_demo_frei, NULL);
  prv_demo_weiter();
}

// Schwarzweiss-Uhren haben keinen Beruehrungsschirm: dort schaltet "unten".
static void prv_demo_knopf(ClickRecognizerRef erkenner, void *kontext) { prv_demo_weiter(); }

static void prv_demo_knoepfe(void *kontext) {
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_demo_knopf);
}
#endif

static void prv_init(void) {
  // ALLES AUF ANFANG. Eine System-App behaelt ihre statischen Variablen ueber
  // ihr Ende hinaus, der App-Speicher nicht (Lehre aus Kieselsport,
  // 580ba98f8): der Bauplatz des Pfeils und die Fensterzeiger zeigten sonst
  // in den Speicher des vorigen Laufs, und das Summen bliebe nach einem Neustart der
  // App bis zu zwanzig Sekunden stumm.
  s_zuletzt_gesummt = 0;
  pfeil_zuruecksetzen();
  nav_window_zuruecksetzen();

  // Die Sprache zuerst: der erste Aufbau des Schirms braucht sie schon.
  strasse_texte_lesen();
  weg_init();
#ifdef KST_DEMO
  s_demo_nr = 0;
  s_demo_sperre = NULL;
  prv_demo();
  touch_service_subscribe(prv_demo_tippen, NULL);
#endif
  nav_window_push();
#ifdef KST_DEMO
  window_set_click_config_provider(app_window_stack_get_top_window(), prv_demo_knoepfe);
#endif

  app_message_register_inbox_received(prv_empfangen);
  app_message_register_inbox_dropped(prv_verworfen);
  const AppMessageResult offen = app_message_open(KST_EINGANG_BYTE, KST_AUSGANG_BYTE);
  // Scheitert das, kommt still nie etwas an - wenigstens im Log soll es stehen.
  if (offen != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "app_message_open: %d", (int)offen);
  }
}

static void prv_ende(void) {
  weg_sichern();
}

static void prv_main(void) {
  prv_init();
  app_event_loop();
  prv_ende();
}

const PebbleProcessMd *kieselstrasse_app_get_info(void) {
  static const PebbleProcessMdSystem s_info = {
    .common = {
      .main_func = prv_main,
      .uuid = KIESELSTRASSE_UUID_INIT,
    },
    .name = "Kieselstrasse",
    .icon_resource_id = RESOURCE_ID_LOCATION_TINY,
  };
  return (const PebbleProcessMd *)&s_info;
}
