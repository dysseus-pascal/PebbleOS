/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pebble_compat.h"
#include "art_text.h"
#include "lauf_window.h"
#include "sende_window.h"
#include "symbole.h"
#include "telefon.h"
#include "texte.h"
#include "thema.h"

#include "applib/ui/menu_layer.h"
#include "process_management/pebble_process_md.h"

// Kieselsport - Training auf der Uhr, Auswertung im eigenen Haus.
//
// WARUM ES DIESE APP GIBT, obwohl die Uhr eine Workout-App hat: die traegt
// ihre Daten in ihre eigene Welt. Diese schickt eine Zusammenfassung ans
// Telefon, wo sie im selben Wochenprofil steht wie Schlaf, Ruhepuls und
// Wasser.
//
// IN DER FIRMWARE, MIT DERSELBEN KENNUNG WIE DIE STORE-APP. So erreichen
// sie dieselben Nachrichten von Kiesel-Helper und der Konfigseite, und sie
// sieht denselben Persist. Gemessen wird im Dienst (services/kieselsport);
// die App ist Anzeige und Tasten. Sie darf zugehen, das Training laeuft
// weiter.
//
// Was sie NICHT tut: Karten, Routen, Hoehenprofile. Die Uhr hat kein GPS;
// alles davon kaeme ohnehin vom Telefon.

static Window *s_menue;
static MenuLayer *s_liste;

static uint16_t prv_zeilen(MenuLayer *liste, uint16_t abschnitt, void *ctx) {
  return ArtAnzahl;
}

static int16_t prv_zeilenhoehe(MenuLayer *liste, MenuIndex *index, void *ctx) {
  return PBL_IF_ROUND_ELSE(56, KS_BREIT ? 52 : 44);
}

static void prv_zeichne_zeile(GContext *ctx, const Layer *zelle,
                              MenuIndex *index, void *daten) {
  // VOR JEDER ART IHR BILD. Das Bild liest man schneller als das Wort - und
  // es unterscheidet Strasse/Gravel von MTB, die beide "Bike" heissen.
  //
  // DER OBERTITEL TRENNT, WAS ZUSAMMENGEHOERT: Strasse/Gravel und MTB
  // stehen im Menue nicht beieinander - die Zahl einer Art darf sich nie
  // verschieben, also kam MTB hinten dazu.
  const Sportart art = (Sportart)index->row;
  const GRect b = layer_get_bounds(zelle);
  const bool hell = menu_cell_layer_is_highlighted(zelle);
  // DIE GEWAEHLTE ZEILE TRAEGT DIE FARBE IHRER ART - dieselbe wie der Balken
  // im Training, also sieht man schon hier, wohin Select fuehrt.
  const GColor grund = PBL_IF_COLOR_ELSE(thema_sportfarbe(art), KS_FARBE_LEISTE);
  if (hell) {
    graphics_context_set_fill_color(ctx, grund);
    graphics_fill_rect(ctx, b, 0, GCornerNone);
  }
  const GColor farbe = hell ? gcolor_legible_over(grund) : KS_FARBE_TEXT;
  const int16_t g = KS_BREIT ? 32 : 26;
  const int16_t links = PBL_IF_ROUND_ELSE(30, 6);
  symbol_sport(ctx, art, GPoint(links + g / 2, b.size.h / 2), g, farbe);

  const int16_t tx = links + g + 8;
  const char *gruppe = art_gruppe(art);
  graphics_context_set_text_color(ctx, farbe);
  const GFont name = fonts_get_system_font(KS_BREIT ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD);
  const int16_t nh = KS_BREIT ? 28 : 22;
  const int16_t ny = gruppe ? b.size.h / 2 - nh + 4 : (b.size.h - nh) / 2 - 3;
  graphics_draw_text(ctx, art_name(art), name, GRect(tx, ny, b.size.w - tx - 2, nh),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  if (gruppe) {
    graphics_draw_text(ctx, gruppe, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(tx, b.size.h / 2 + 2, b.size.w - tx - 2, 16),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
}

static void prv_gewaehlt(MenuLayer *liste, MenuIndex *index, void *daten) {
  lauf_window_zeige((Sportart)index->row);
}

static void prv_laden(Window *fenster) {
  Layer *wurzel = window_get_root_layer(fenster);
  const GRect bounds = layer_get_bounds(wurzel);

  // KEINE LEISTE: Oben, Unten und Select erklaert die Liste selbst, und
  // die Namen bekommen die volle Breite.
  s_liste = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_liste, NULL, (MenuLayerCallbacks) {
    .get_num_rows = prv_zeilen,
    .get_cell_height = prv_zeilenhoehe,
    .draw_row = prv_zeichne_zeile,
    .select_click = prv_gewaehlt,
  });
  menu_layer_set_normal_colors(s_liste, KS_FARBE_GRUND, KS_FARBE_TEXT);
  menu_layer_set_highlight_colors(s_liste, KS_FARBE_LEISTE, KS_FARBE_AUF_LEISTE);
  menu_layer_set_click_config_onto_window(s_liste, fenster);
  layer_add_child(wurzel, menu_layer_get_layer(s_liste));
}

static void prv_entladen(Window *fenster) {
  menu_layer_destroy(s_liste);
}

static void prv_init(void) {
  // ALLES AUF ANFANG. Eine Systemapp behaelt ihre statischen Variablen ueber
  // ihr Ende hinaus, der App-Speicher nicht: das Dreieck des letzten Laufs
  // (thema.c) zeigte ins Leere, und die Pause-Ansicht verlangte beim
  // naechsten Oeffnen 1,9 MB - die App stuerzte ab, das Training lief weiter.
  thema_zuruecksetzen();
  telefon_zuruecksetzen();
  lauf_window_zuruecksetzen();
  sende_window_zuruecksetzen();

  // Die Sprache der Uhr einmal lesen, bevor irgendein Text gezeichnet wird.
  // Auch der Glance beim Beenden nimmt sie von hier.
  strings_refresh();
  telefon_init();

  s_menue = window_create();
  window_set_background_color(s_menue, KS_FARBE_GRUND);
  window_set_window_handlers(s_menue, (WindowHandlers) {
    .load = prv_laden,
    .unload = prv_entladen,
  });
  window_stack_push(s_menue, true);

  // LAEUFT SCHON ETWAS, GLEICH HINEIN. Wer die App aus dem Startmenue
  // oeffnet, weil dort "Laufen im Hintergrund" steht, will den Lauf sehen
  // und nicht das Menue.
  if (kieselsport_laeuft()) {
    lauf_window_zeige_laufend();
  } else if (launch_reason() == APP_LAUNCH_WORKER && telefon_wartet()) {
    // DER DIENST HAT DIE APP GEHOLT, weil ein gespeichertes Training nie
    // beim Telefon ankam (services/kieselsport, Nachsenden).
    sende_window_zeige();
  } else if (launch_reason() == APP_LAUNCH_TIMELINE_ACTION) {
    // AUS DEM PIN "TRAINING": die Aktion "Jetzt starten" traegt die Art als
    // Launch-Code (Art + 1, damit 0 "nichts" bleibt). Der Schirm steht dann
    // bereit; Select startet - wie beim Weg ueber das Menue.
    const int32_t code = (int32_t)launch_get_args();
    if (code >= 1 && code <= ArtAnzahl) lauf_window_zeige((Sportart)(code - 1));
  }
}

static void prv_ende(void) {
  // DER HINWEIS IM STARTMENUE kommt nicht mehr von hier: die Kachel dort
  // fragt den Dienst selbst (launcher/default/app_glance_kieselsport.c).
  window_destroy(s_menue);
}

static void prv_main(void) {
  prv_init();
  app_event_loop();
  prv_ende();
}

const PebbleProcessMd *kieselsport_app_get_info(void) {
  static const PebbleProcessMdSystem s_info = {
    .common = {
      .main_func = prv_main,
      .uuid = KIESELSPORT_UUID_INIT,
    },
    .name = "Kieselsport",
    .icon_resource_id = RESOURCE_ID_ACTIVITY_TINY,
  };
  return (const PebbleProcessMd *)&s_info;
}
