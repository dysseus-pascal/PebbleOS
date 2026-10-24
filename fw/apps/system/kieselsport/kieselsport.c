/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// NUR MIT DER VARIANTE (Schritt 2, aus b25 src/fw/apps/system/kieselsport/). Der Ordner wird fuer
// jedes Board gebaut (GLOB_RECURSE in fw/apps/system/CMakeLists.txt); ohne den Schalter bleiben
// die Dateien leer, und die Registry (Eintrag KIESELSPORT, -103) fuehrt die App dann auch nicht.
#ifdef CONFIG_BOULDER_KIESELSPORT

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
// sie dieselben Nachrichten von Boulder, und sie sieht denselben Persist.
// Gemessen wird im Dienst (services/activity/boulder/sport), gefuehrt wird
// das Training von Pebbles Workout-Dienst; die App ist Anzeige und Tasten.
// Sie darf zugehen, das Training laeuft weiter.
//
// Was sie NICHT tut: Karten, Routen, Hoehenprofile. Die Uhr hat kein GPS;
// alles davon kaeme ohnehin vom Telefon.

const PebbleProcessMd *kieselsport_app_get_info(void);

static Window *s_menue;
static MenuLayer *s_liste;
// Das Untermenue einer Gruppe (Bike, Swimming), gebaut beim ersten Griff nach einer. Es baut seine
// Liste bei jedem Zeigen neu, darum genuegt eines fuer alle Gruppen.
static Window *s_unter;
static MenuLayer *s_unter_liste;
static int s_unter_gruppe;

static uint16_t prv_zeilen(MenuLayer *liste, uint16_t abschnitt, void *ctx) {
  return menue_zeilen();
}

static int16_t prv_zeilenhoehe(MenuLayer *liste, MenuIndex *index, void *ctx) {
  return PBL_IF_ROUND_ELSE(56, KS_BREIT ? 52 : 44);
}

// Eine Zeile beider Menues: Bild und Farbe von [art], darunter klein [gruppe].
static void prv_zeichne(GContext *ctx, const Layer *zelle, Sportart art, const char *name,
                        const char *gruppe) {
  // VOR JEDER ART IHR BILD. Das Bild liest man schneller als das Wort - und
  // es unterscheidet die Arten einer Gruppe, die alle denselben Obertitel tragen.
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
  graphics_context_set_text_color(ctx, farbe);
  const GFont schrift =
      fonts_get_system_font(KS_BREIT ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD);
  const int16_t nh = KS_BREIT ? 28 : 22;
  const int16_t ny = gruppe ? b.size.h / 2 - nh + 4 : (b.size.h - nh) / 2 - 3;
  graphics_draw_text(ctx, name, schrift, GRect(tx, ny, b.size.w - tx - 2, nh),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  if (gruppe) {
    graphics_draw_text(ctx, gruppe, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(tx, b.size.h / 2 + 2, b.size.w - tx - 2, 16),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
}

// KEINE LEISTE: Oben, Unten und Select erklaert die Liste selbst, und
// die Namen bekommen die volle Breite.
static MenuLayer *prv_liste(Window *fenster, MenuLayerCallbacks rueckrufe) {
  Layer *wurzel = window_get_root_layer(fenster);
  MenuLayer *liste = menu_layer_create(layer_get_bounds(wurzel));
  menu_layer_set_callbacks(liste, NULL, rueckrufe);
  menu_layer_set_normal_colors(liste, KS_FARBE_GRUND, KS_FARBE_TEXT);
  menu_layer_set_highlight_colors(liste, KS_FARBE_LEISTE, KS_FARBE_AUF_LEISTE);
  menu_layer_set_click_config_onto_window(liste, fenster);
  layer_add_child(wurzel, menu_layer_get_layer(liste));
  return liste;
}

// --- Das Untermenue einer Gruppe ---

static uint16_t prv_unter_zeilen(MenuLayer *liste, uint16_t abschnitt, void *ctx) {
  return menue_gruppe_zeilen(s_unter_gruppe);
}

static void prv_unter_zeichne(GContext *ctx, const Layer *zelle, MenuIndex *index, void *daten) {
  const Sportart art = menue_gruppe_art(s_unter_gruppe, index->row);
  prv_zeichne(ctx, zelle, art, art_name(art), art_gruppe(art));
}

static void prv_unter_gewaehlt(MenuLayer *liste, MenuIndex *index, void *daten) {
  lauf_window_zeige(menue_gruppe_art(s_unter_gruppe, index->row));
}

static void prv_unter_laden(Window *fenster) {
  s_unter_liste = prv_liste(fenster, (MenuLayerCallbacks) {
    .get_num_rows = prv_unter_zeilen,
    .get_cell_height = prv_zeilenhoehe,
    .draw_row = prv_unter_zeichne,
    .select_click = prv_unter_gewaehlt,
  });
}

// Pebble entlaedt ein Fenster, sobald es vom Stapel geht; das naechste Zeigen baut die Liste fuer
// die dann gewaehlte Gruppe.
static void prv_unter_entladen(Window *fenster) {
  menu_layer_destroy(s_unter_liste);
  s_unter_liste = NULL;
}

static void prv_unter_zeigen(int gruppe) {
  s_unter_gruppe = gruppe;
  if (!s_unter) {
    s_unter = window_create();
    window_set_background_color(s_unter, KS_FARBE_GRUND);
    window_set_window_handlers(s_unter, (WindowHandlers) {
      .load = prv_unter_laden,
      .unload = prv_unter_entladen,
    });
  }
  window_stack_push(s_unter, true);
}

// --- Das Hauptmenue ---

static void prv_zeichne_zeile(GContext *ctx, const Layer *zelle,
                              MenuIndex *index, void *daten) {
  // EINE GRUPPE IN EINER ZEILE: mit Bild und Farbe ihrer ersten Art, und ohne Obertitel steht
  // ihr Name in der Mitte wie jede andere Art.
  const int eintrag = menue_eintrag(index->row);
  if (menue_ist_gruppe(eintrag)) {
    prv_zeichne(ctx, zelle, menue_gruppe_art(eintrag, 0), menue_gruppe_name(eintrag), NULL);
  } else {
    prv_zeichne(ctx, zelle, (Sportart)eintrag, art_name((Sportart)eintrag), NULL);
  }
}

static void prv_gewaehlt(MenuLayer *liste, MenuIndex *index, void *daten) {
  const int eintrag = menue_eintrag(index->row);
  if (menue_ist_gruppe(eintrag)) {
    prv_unter_zeigen(eintrag);
  } else {
    lauf_window_zeige((Sportart)eintrag);
  }
}

static void prv_laden(Window *fenster) {
  s_liste = prv_liste(fenster, (MenuLayerCallbacks) {
    .get_num_rows = prv_zeilen,
    .get_cell_height = prv_zeilenhoehe,
    .draw_row = prv_zeichne_zeile,
    .select_click = prv_gewaehlt,
  });
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
  s_unter = NULL;
  s_unter_liste = NULL;

  // DER DIENST STARTET OHNE PEBBLE-HAKEN (Spezifikation 2.5, Regel 5): der erste Takt der
  // Variante oder diese App. Nach einem Boot am Ladekabel laeuft die Variante nicht, dann ist es
  // die App - ohne diesen Anstoss schickte niemand nach, was vor dem Neustart nicht ankam.
  kieselsport_init_anstossen();

  // Die Sprache der Uhr einmal lesen, bevor irgendein Text gezeichnet wird.
  strings_refresh();
  telefon_init();

  s_menue = window_create();
  window_set_background_color(s_menue, KS_FARBE_GRUND);
  window_set_window_handlers(s_menue, (WindowHandlers) {
    .load = prv_laden,
    .unload = prv_entladen,
  });
  window_stack_push(s_menue, true);

  // LAEUFT SCHON ETWAS, GLEICH HINEIN. Wer die App waehrend eines Trainings
  // oeffnet (Startmenue, Quick Launch), will den Lauf sehen und nicht das
  // Menue.
  if (kieselsport_laeuft()) {
    lauf_window_zeige_laufend();
  } else if (launch_reason() == APP_LAUNCH_WORKER && (telefon_wartet() || telefon_stopp_wartet())) {
    // DER DIENST HAT DIE APP GEHOLT, weil ein gespeichertes Training oder
    // der Stopp eines verworfenen nie beim Telefon ankam (boulder/sport/dienst.c,
    // Nachsenden).
    sende_window_zeige(!telefon_wartet());
  } else if (launch_reason() == APP_LAUNCH_TIMELINE_ACTION) {
    // AUS DEM PIN "TRAINING": die Aktion "Jetzt starten" traegt die Art als
    // Launch-Code (Art + 1, damit 0 "nichts" bleibt). Der Schirm steht dann
    // bereit; Select startet - wie beim Weg ueber das Menue.
    const int32_t code = (int32_t)launch_get_args();
    if (code >= 1 && code <= ArtAnzahl) lauf_window_zeige((Sportart)(code - 1));
  }
}

static void prv_ende(void) {
  if (s_unter) {
    window_destroy(s_unter);
  }
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
  return &s_info.common;
}

#endif  // CONFIG_BOULDER_KIESELSPORT
