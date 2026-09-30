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
// Nur im Pruefbau (-DKST_DEMO): ein Stand, damit sich die Anzeige im Emulator
// ansehen laesst. Ohne Telefon kommt sonst nie etwas an.
static void prv_demo(void) {
  uint8_t puffer[128];
  Tuplet werte[] = {
    TupletInteger(MESSAGE_KEY_ABBIEGEART, (int32_t)5),      // TR, rechts
    TupletInteger(MESSAGE_KEY_ENTFERNUNG, (int32_t)250),
    TupletCString(MESSAGE_KEY_STRASSE, "Bahnhofstrasse"),
    TupletInteger(MESSAGE_KEY_REST, (int32_t)12400),
    TupletInteger(MESSAGE_KEY_ANKUNFT, (int32_t)(time(NULL) + 1500)),
  };
  // DIE BENUTZTE GROESSE ZURUECKLESEN, nicht die des Puffers weiterreichen.
  // Wer zum Lesen die ganzen 128 Byte reicht, laesst den Leser hinter dem Ende
  // weiterlaufen; er findet dort Muell, meldet das Woerterbuch als kaputt und
  // dict_find liefert fuer alles null - ohne ein Wort der Warnung.
  uint32_t benutzt = sizeof(puffer);
  dict_serialize_tuplets_to_buffer(werte, ARRAY_LENGTH(werte), puffer, &benutzt);
  DictionaryIterator lesen;
  dict_read_begin_from_buffer(&lesen, puffer, (uint16_t)benutzt);
  weg_uebernimm(&lesen);
}
#endif

static void prv_init(void) {
  // ALLES AUF ANFANG. Eine System-App behaelt ihre statischen Variablen ueber
  // ihr Ende hinaus, der App-Speicher nicht (Lehre aus Kieselsport,
  // 580ba98f8): der Pfeil (GPath) und die Fensterzeiger zeigten sonst in den
  // Speicher des vorigen Laufs, und das Summen bliebe nach einem Neustart der
  // App bis zu zwanzig Sekunden stumm.
  s_zuletzt_gesummt = 0;
  pfeil_zuruecksetzen();
  nav_window_zuruecksetzen();

  // Die Sprache zuerst: der erste Aufbau des Schirms braucht sie schon.
  strasse_texte_lesen();
  weg_init();
#ifdef KST_DEMO
  prv_demo();
#endif
  nav_window_push();

  app_message_register_inbox_received(prv_empfangen);
  // Klein genug fuer fuenf Felder, gross genug fuer einen langen
  // Strassennamen. Ein zu knapper Eingang verwirft die Nachricht still.
  const AppMessageResult offen = app_message_open(256, 64);
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
