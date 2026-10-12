/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "apps/system/boulder_health/daten.h"
#include "apps/system/boulder_health/details.h"
#include "apps/system/boulder_health/inhalt.h"
#include "apps/system/boulder_health/karten.h"
#include "apps/system/boulder_health/rechnung.h"
#include "apps/system/boulder_health/zeichnen.h"

#include "applib/graphics/framebuffer.h"
#include "applib/graphics/graphics.h"
#include "pbl/services/activity/boulder/messstufe.h"
#include "pbl/util/size.h"

#include "fake_spi_flash.h"
#include "fixtures/load_test_resources.h"

#include "stubs_analytics.h"
#include "stubs_app_state.h"
#include "stubs_bootbits.h"
#include "stubs_compiled_with_legacy2_sdk.h"
#include "stubs_logging.h"
#include "stubs_memory_layout.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_tasks.h"
#include "stubs_process_manager.h"
#include "stubs_serial.h"
#include "stubs_sleep.h"
#include "stubs_syscalls.h"
#include "stubs_task_wdt.h"

#include "fw/graphics/util.h"

#include <stdio.h>
#include <string.h>

// Karten und Details der Health-App, gezeichnet mit Pebbles echter Grafik und Schrift und mit
// Bildern verglichen (Spezifikation Schritt 3, 2.1-2.3, AP6): jede Karte voll, leer und mit
// Tracking aus, jedes Detail oben und gerollt, 12 h und Meilen. Dazu der Kontrast (V10, F14).
//
// DIE BILDER SIND GESEHEN, NICHT ERRECHNET: Ein neues Bild schreibt der Test als "-actual" in den
// Ausgabeordner; erst nach dem Ansehen kommt es nach tests/test_images/.

static GContext s_ctx;
static FrameBuffer s_fb;
static BhDaten s_d;

// Ortszeit wie die Pruefdaten: Mitteleuropa im Sommer (daten.c rechnet sie mit sys_localtime_r).
int32_t bh_ortsversatz(time_t utc) {
  return 2 * 60 * 60;
}

GContext *graphics_context_get_current_context(void) {
  return &s_ctx;
}

// --- Pruefdaten: 12.10.2026, 14:05 Ortszeit (MESZ), nach der Nacht 22:17-07:00 ---

#define JETZT        1791806700 // 12.10.2026 12:05 UTC
#define NACHT_BEGINN 1791748800 // 11.10.2026 20:00 UTC = 22:00 MESZ
#define HEUTE        9781

// Eine Nacht mit vier Zyklen: leicht, tief, leicht, REM, dazu kurze Wachphasen.
static void prv_nacht(NachtBild *n) {
  memset(n, 0, sizeof(*n));
  n->beginn_utc = NACHT_BEGINN;
  n->utc_to_local = 2 * 60 * 60;
  n->einschlafen = 17;
  n->aufwachen = 540;
  n->schlaf_minuten = 472;
  n->minuten[0] = 51;  // wach
  n->minuten[1] = 287; // leicht
  n->minuten[2] = 74;  // tief
  n->minuten[3] = 111; // REM
  n->hrv = 56;
  n->ruhepuls = 52;
  n->fertig = true;
  n->version = NACHT_BILD_VERSION;
  static const struct {
    uint16_t dauer;
    uint8_t phase;
  } folge[] = {
    {12, 0}, {35, 1}, {30, 2}, {40, 1}, {18, 3}, {5, 0}, {45, 1}, {24, 2}, {30, 1}, {25, 3},
    {6, 0},  {50, 1}, {20, 2}, {40, 1}, {30, 3}, {8, 0}, {47, 1}, {38, 3}, {32, 0},
  };
  uint16_t m = 17 - 12;
  for (size_t i = 0; i < sizeof(folge) / sizeof(folge[0]); i++) {
    n->abschnitte[i] =
        (NachtDlsAbschnitt){.von = m, .bis = m + folge[i].dauer, .phase = folge[i].phase};
    m += folge[i].dauer;
  }
  n->anzahl = sizeof(folge) / sizeof(folge[0]);
}

static void prv_voll(BhDaten *d) {
  memset(d, 0, sizeof(*d));
  d->jetzt = JETZT;
  d->heute = HEUTE;
  d->minute = 14 * 60 + 5;
  d->wochentag = bh_wochentag(HEUTE);
  d->h24 = true;
  d->lage_da = true;
  d->lage = (BoulderLage){
    .fassung = BOULDER_LAGE_FASSUNG,
    .tracking = true,
    .messstufe = 3,
    .nacht_stufe = 3,
    .stand_utc = JETZT,
    .ziel_schritte = 10000,
    .ziel_schlaf_min = 480,
    .maxpuls = 187,
    .ruhepuls_median = 52,
    .spo2_takt_min = 30
  };
  d->nacht_lesen = BoulderLesenDa;
  prv_nacht(&d->nacht);

  // 37 Wachtage: Ruhepuls um 53, HRV um 52; heute 52 und 56.
  d->naechte_da = true;
  d->naechte.fassung = BOULDER_NAECHTE_FASSUNG;
  d->naechte.anzahl = NAECHTE_MAX;
  d->naechte.neu = NAECHTE_MAX - 1;
  for (int i = 0; i < NAECHTE_MAX; i++) {
    const int alter = NAECHTE_MAX - 1 - i;
    d->naechte.tage[i] = (BoulderNachtEintrag){
      .tag = (uint16_t)(HEUTE - alter),
      .hrv = (uint16_t)(alter == 0 ? 56 : 46 + (alter * 7) % 13),
      .ruhepuls = (uint8_t)(alter == 0 ? 52 : 51 + (alter * 3) % 5)
    };
  }

  // Herzband: nachts um 55, tags um 75, am Morgen ein Lauf bis 158.
  d->herz_da = true;
  d->herz.neueste = JETZT / HERZ_HALBSTUNDE_S;
  for (int k = 0; k < HERZ_HALBSTUNDEN; k++) {
    const uint32_t nr = d->herz.neueste - (HERZ_HALBSTUNDEN - 1) + k;
    const int stunde_ort = (int)(((nr * HERZ_HALBSTUNDE_S + 7200) % 86400) / 3600);
    int mittel = (stunde_ort >= 22 || stunde_ort < 7) ? 55 + k % 3 : 72 + (k * 5) % 11;
    if (stunde_ort == 8) {
      mittel = 128 + k % 2 * 18;
    }
    const uint8_t p = nr % HERZ_HALBSTUNDEN;
    d->herz.mittel[p] = (uint8_t)mittel;
    d->herz.min[p] = (uint8_t)(mittel - 6);
    d->herz.max[p] = (uint8_t)(mittel + 9 + (stunde_ort == 8 ? 6 : 0));
    d->herz.n[p] = 25;
  }
  d->herz.zonen_tag = HEUTE;
  d->herz.zonen_min[0] = 64;
  d->herz.zonen_min[1] = 21;
  d->herz.zonen_min[2] = 9;
  d->herz.zonen_min[3] = 2;
  d->herz.grenzen[0] = 106;
  d->herz.grenzen[1] = 133;
  d->herz.grenzen[2] = 160;

  d->woche_da = true;
  d->woche.tag = HEUTE - 1;
  const uint32_t woche[WOCHE_VORTAGE] = {8432, 12011, 5320, 10250, 7800, WOCHE_KEIN_WERT};
  memcpy(d->woche.schritte, woche, sizeof(woche));

  // SpO2 alle 30 min in den letzten 24 h, nachts etwas tiefer.
  for (int i = 0; i < 46; i++) {
    const time_t t = JETZT - 23 * 3600 + i * 1800;
    const int stunde_ort = (int)(((t + 7200) % 86400) / 3600);
    d->spo2[d->spo2_anzahl++] = (Spo2Wert){
      .zeit = t,
      .prozent = (uint8_t)((stunde_ort >= 22 || stunde_ort < 7) ? 93 + i % 4 : 97 + i % 2)
    };
  }

  d->schritte = 6230;
  d->meter = 4800;
  d->aktiv_s = 42 * 60;
  d->kcal = 1890;
  d->puls = 64;
  d->puls_zeit = JETZT - 120;

  // Sitzungen von heute (Ortszeit): Spaziergang 08:10, Velo 11:00, Nickerchen 13:00, dazu die
  // Nacht, die nicht in der Liste steht.
  d->sitzungen_anzahl = 4;
  d->sitzungen[0] = (ActivitySession){
    .start_utc = NACHT_BEGINN + 17 * 60,
    .length_min = 523,
    .type = ActivitySessionType_Sleep
  };
  d->sitzungen[1] = (ActivitySession){
    .start_utc = JETZT - (6 * 60 - 5) * 60 + 5 * 60,
    .length_min = 32,
    .type = ActivitySessionType_Walk,
    .step_data = {.steps = 3420, .active_kcalories = 120}
  };
  d->sitzungen[2] = (ActivitySession){
    .start_utc = JETZT - (3 * 60 + 5) * 60,
    .length_min = 41,
    .type = ActivitySessionType_Open,
    .step_data = {.active_kcalories = 312}
  };
  d->sitzungen[3] = (ActivitySession){
    .start_utc = JETZT - 65 * 60,
    .length_min = 20,
    .type = ActivitySessionType_Nap
  };
}

// Leer: frisch nach dem Boot ohne jede Messung, wie die Syscalls es dann liefern.
static void prv_leer(BhDaten *d) {
  memset(d, 0, sizeof(*d));
  d->jetzt = JETZT;
  d->heute = HEUTE;
  d->h24 = true;
  d->lage = (BoulderLage){
    .fassung = BOULDER_LAGE_FASSUNG,
    .tracking = true,
    .ziel_schritte = 10000,
    .ziel_schlaf_min = 480
  };
  d->nacht_lesen = BoulderLesenKeine;
}

// --- Zeichnen ---

void test_bh_karten__initialize(void) {
  framebuffer_init(&s_fb, &(GSize){DISP_COLS, DISP_ROWS});
  framebuffer_clear(&s_fb);
  graphics_context_init(&s_ctx, &s_fb, GContextInitializationMode_App);
  s_app_state_get_graphics_context = &s_ctx;
  fake_spi_flash_init(0 /* offset */, 0x1000000 /* length */);
  pfs_init(false /* run filesystem check */);
  pfs_format(true /* write erase headers */);
  load_resource_fixture_in_flash(RESOURCES_FIXTURE_PATH, SYSTEM_RESOURCES_FIXTURE_NAME,
                                 false /* is_next */);
  resource_init();
  prv_voll(&s_d);
}

void test_bh_karten__cleanup(void) {
}

static void prv_grund(void) {
  graphics_context_set_fill_color(&s_ctx, GColorWhite);
  graphics_fill_rect(&s_ctx, &GRect(0, 0, DISP_COLS, DISP_ROWS));
}

static void prv_karte(BhKarte karte) {
  prv_grund();
  const GRect r = GRect(0, 0, DISP_COLS, DISP_ROWS);
  bh_kopf_zeichnen(&s_ctx, r, karte);
  bh_inhalt_zeichnen(&s_ctx, r, karte, &s_d);
}

// Ein Detail wie in der App: Kopfbalken fest, darunter der Inhalt, um `gerollt` px nach oben
// verschoben; `ganz_unten` rollt bis zum Ende.
static void prv_detail(BhKarte karte, uint8_t auswahl, bool ganz_unten) {
  prv_grund();
  const BhDetail detail = bh_detail_von(karte);
  const int16_t hoehe = bh_detail_hoehe(detail, &s_d);
  const int16_t sichtbar = DISP_ROWS - BH_KOPF_H;
  const int16_t gerollt = (ganz_unten && hoehe > sichtbar) ? hoehe - sichtbar : 0;
  const GDrawState vorher = s_ctx.draw_state;
  s_ctx.draw_state.clip_box = GRect(0, BH_KOPF_H, DISP_COLS, sichtbar);
  s_ctx.draw_state.drawing_box = GRect(0, BH_KOPF_H - gerollt, DISP_COLS, hoehe);
  bh_detail_zeichnen(&s_ctx, DISP_COLS, detail, &s_d, auswahl);
  s_ctx.draw_state = vorher;
  bh_kopf_detail(&s_ctx, DISP_COLS, bh_detail_titel(detail), bh_karte_farbe(karte));
}

#define PRUEFE_BILD() cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE))

// --- Karten, voll ---

void test_bh_karten__today(void) {
  prv_karte(BhKarteToday);
  PRUEFE_BILD();
}
void test_bh_karten__night(void) {
  prv_karte(BhKarteNight);
  PRUEFE_BILD();
}
void test_bh_karten__heart(void) {
  prv_karte(BhKarteHeart);
  PRUEFE_BILD();
}
void test_bh_karten__activity(void) {
  prv_karte(BhKarteActivity);
  PRUEFE_BILD();
}
void test_bh_karten__settings(void) {
  prv_karte(BhKarteSettings);
  PRUEFE_BILD();
}

// 12 h und Meilen (V7): Uhrzeiten mit AM/PM, Strecke in mi.
void test_bh_karten__night_12h(void) {
  s_d.h24 = false;
  prv_karte(BhKarteNight);
  PRUEFE_BILD();
}
void test_bh_karten__activity_meilen(void) {
  s_d.meilen = true;
  prv_karte(BhKarteActivity);
  PRUEFE_BILD();
}

// Die Nacht laeuft noch: "so far" statt Urteil; Today zeigt sie als Nacht von heute.
void test_bh_karten__night_so_far(void) {
  s_d.nacht.fertig = false;
  prv_karte(BhKarteNight);
  PRUEFE_BILD();
}

// Ein Puls, der aelter als eine Viertelstunde ist, zeigt sein Alter statt "now".
void test_bh_karten__heart_alter_puls(void) {
  s_d.puls_zeit = JETZT - 42 * 60;
  s_d.lage.spo2_takt_min = 0; // Stufe 1: SpO2 "off"
  prv_karte(BhKarteHeart);
  PRUEFE_BILD();
}

// --- Karten, leer und Tracking aus ---

void test_bh_karten__today_leer(void) {
  prv_leer(&s_d);
  prv_karte(BhKarteToday);
  PRUEFE_BILD();
}
void test_bh_karten__night_leer(void) {
  prv_leer(&s_d);
  prv_karte(BhKarteNight);
  PRUEFE_BILD();
}
void test_bh_karten__heart_leer(void) {
  prv_leer(&s_d);
  prv_karte(BhKarteHeart);
  PRUEFE_BILD();
}
void test_bh_karten__activity_leer(void) {
  prv_leer(&s_d);
  prv_karte(BhKarteActivity);
  PRUEFE_BILD();
}
void test_bh_karten__settings_leer(void) {
  prv_leer(&s_d);
  prv_karte(BhKarteSettings);
  PRUEFE_BILD();
}
void test_bh_karten__today_tracking_aus(void) {
  s_d.lage.tracking = false;
  prv_karte(BhKarteToday);
  PRUEFE_BILD();
}
// Settings bleibt mit Tracking aus bedienbar.
void test_bh_karten__settings_tracking_aus(void) {
  s_d.lage.tracking = false;
  prv_karte(BhKarteSettings);
  PRUEFE_BILD();
}

// --- Details ---

void test_bh_karten__detail_night(void) {
  prv_detail(BhKarteNight, 0, false);
  PRUEFE_BILD();
}
void test_bh_karten__detail_night_unten(void) {
  prv_detail(BhKarteNight, 0, true);
  PRUEFE_BILD();
}
void test_bh_karten__detail_heart(void) {
  prv_detail(BhKarteHeart, 0, false);
  PRUEFE_BILD();
}
void test_bh_karten__detail_heart_unten(void) {
  prv_detail(BhKarteHeart, 0, true);
  PRUEFE_BILD();
}
void test_bh_karten__detail_sessions(void) {
  s_d.h24 = false;
  prv_detail(BhKarteActivity, 0, false);
  PRUEFE_BILD();
}
void test_bh_karten__detail_sessions_leer(void) {
  prv_leer(&s_d);
  prv_detail(BhKarteActivity, 0, false);
  PRUEFE_BILD();
}
void test_bh_karten__detail_precision(void) {
  prv_detail(BhKarteSettings, 4, false);
  PRUEFE_BILD();
}
void test_bh_karten__detail_night_leer(void) {
  prv_leer(&s_d);
  prv_detail(BhKarteNight, 0, false);
  PRUEFE_BILD();
}

// --- Urteile neben der Zahl (Spezifikation 2.5, Gegenpruefung C1) ---

// Die laengsten Zahlen mit den laengsten Urteilen, die zusammen vorkommen: Jedes Urteil beginnt
// rechts der Einheit, jedes Wort hat in seiner Zeile Platz, und es braucht hoechstens zwei Zeilen.
// "Very short night" gibt es bis 10:59 h (Ziel hoechstens 12 h), "Short night" bis 11:59 h, eine
// Nacht hat hoechstens 15 h; der Puls hoechstens 255 bpm, sein Alter bis 1092:15 h.
void test_bh_karten__urteile_passen_neben_die_zahl(void) {
  char minus[8];
  bh_vorzeichen(-99, minus, sizeof(minus));
  const struct {
    const char *zahl, *einheit, *urteil;
  } paare[] = {
    {"10:59", "h", "Very short night"},
    {"11:59", "h", "Short night"},
    {"15:00", "h", "Well rested"},
    {"15:00", "h", "so far"},
    {"255", "bpm", "now"},
    {"255", "bpm", "1092:15 h"},
    {"255", "bpm", "as usual"},
    {"255", "bpm", "+99"},
    {"255", "bpm", minus},
    {"999", "ms", "in range"},
    {"999", "ms", "above"},
    {"999", "ms", "below"},
  };
  const BhSchriftart schriften[] = {BhSchriftL26, BhSchriftL38};
  for (size_t s_nr = 0; s_nr < ARRAY_LENGTH(schriften); s_nr++) {
    for (size_t i = 0; i < ARRAY_LENGTH(paare); i++) {
      const int16_t ende = bh_zahl_mit_einheit(&s_ctx, paare[i].zahl, paare[i].einheit,
                                               schriften[s_nr], GPoint(BH_RAND, 30), GColorBlack);
      BhSchriftart art;
      const GRect r = bh_urteil_rahmen(paare[i].urteil, ende, DISP_COLS - BH_RAND, 50, &art);
      const GSize g = app_graphics_text_layout_get_content_size(
          paare[i].urteil, bh_schrift(art), GRect(0, 0, r.size.w, 100), GTextOverflowModeWordWrap,
          GTextAlignmentRight);
      printf("\n%s %s neben \"%s\": ab x=%d, %d x %d px, Text %d px hoch%s", paare[i].zahl,
             paare[i].einheit, paare[i].urteil, r.origin.x, r.size.w, r.size.h, g.h,
             (art == BhSchriftG14Fett) ? " (GOTHIC 14)" : "");
      cl_assert(r.origin.x > ende);
      char wort[32];
      for (const char *w = paare[i].urteil; *w;) {
        const size_t n = strcspn(w, " ");
        snprintf(wort, sizeof(wort), "%.*s", (int)n, w);
        cl_assert(bh_textbreite(wort, art) <= r.size.w);
        w += n + (w[n] == ' ');
      }
      cl_assert(g.h <= r.size.h);
      // Nicht in den Kopfbalken.
      cl_assert(r.origin.y >= BH_KOPF_H);
    }
  }
}

// Die haeufigste Nacht unter dem Ziel und die laengste mit "Very short night": Das Urteil steht
// rechts der Einheit, notfalls in zwei Zeilen.
void test_bh_karten__night_sehr_kurz(void) {
  s_d.nacht.schlaf_minuten = 6 * 60 + 30;
  prv_karte(BhKarteNight);
  PRUEFE_BILD();
}
void test_bh_karten__night_ziel_hoch(void) {
  s_d.lage.ziel_schlaf_min = 720;
  s_d.nacht.schlaf_minuten = 10 * 60 + 59;
  prv_karte(BhKarteNight);
  PRUEFE_BILD();
}
void test_bh_karten__today_ziel_hoch(void) {
  s_d.lage.ziel_schlaf_min = 720;
  s_d.nacht.schlaf_minuten = 10 * 60 + 59;
  prv_karte(BhKarteToday);
  PRUEFE_BILD();
}

// --- Kontrast (V10, F14) ---

static double prv_linear(uint8_t stufe) {
  // Pebbles 64 Farben haben je Kanal die Stufen 0, 1, 2, 3 = 0x00, 0x55, 0xAA, 0xFF.
  static const double werte[4] = {0.0, 0.0908417, 0.4019778, 1.0};
  return werte[stufe & 3];
}
static double prv_leuchtdichte(GColor c) {
  return 0.2126 * prv_linear(c.r) + 0.7152 * prv_linear(c.g) + 0.0722 * prv_linear(c.b);
}
static double prv_kontrast(GColor a, GColor b) {
  const double la = prv_leuchtdichte(a), lb = prv_leuchtdichte(b);
  return ((la > lb ? la : lb) + 0.05) / ((la > lb ? lb : la) + 0.05);
}

void test_bh_karten__kontrast(void) {
  // Weisser Titel in GOTHIC 18 fett auf dem Kopfbalken: mindestens 3:1.
  for (int k = 0; k < BhKartenAnzahl; k++) {
    cl_assert(prv_kontrast(GColorWhite, bh_karte_farbe((BhKarte)k)) >= 3.0);
  }
  // Text auf Weiss (Urteile, Labels): mindestens 4,5:1.
  cl_assert(prv_kontrast(BH_GUT, GColorWhite) >= 4.5);
  cl_assert(prv_kontrast(BH_AUFFAELLIG, GColorWhite) >= 4.5);
  cl_assert(prv_kontrast(BH_GRAU, GColorWhite) >= 4.5);
  // Gut und auffaellig unterscheiden sich auch in der Helligkeit, nicht nur im Ton.
  cl_assert(prv_kontrast(BH_GUT, BH_AUFFAELLIG) >= 1.5);
}
