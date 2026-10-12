/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "apps/system/boulder_health/daten.h"
#include "apps/system/boulder_health/details.h"
#include "apps/system/boulder_health/inhalt.h"
#include "apps/system/boulder_health/karten.h"

#include "applib/fonts/fonts.h"
#include "applib/graphics/graphics.h"
#include "applib/graphics/text.h"

#include <stdio.h>
#include <string.h>

// Der Grundsatz (Entscheid 08.10., Kriterium 3 von Schritt 3): Die Uhr zeigt nur, was sie selbst
// misst. Jede Quelle (S1-S7 und Pebbles Metriken) bekommt hier einen eigenen Erkennungswert, und
// der Test sucht ihn auf der Karte oder im Detail, wo er hingehoert. Dazu: Nichts auf irgendeinem
// Bild spricht von Koffein, Getraenken, Praeparaten oder Koerperwerten.
//
// Nachgebaut ist nur die Grafik: Sie merkt sich jeden gezeichneten Text. Karten, Details,
// Rechnungen und Texte sind echt.

static char s_texte[128][48];
static int s_anzahl;

// Ortszeit wie die Pruefdaten: Mitteleuropa im Sommer (daten.c rechnet sie mit sys_localtime_r).
int32_t bh_ortsversatz(time_t utc) {
  return 2 * 60 * 60;
}

void graphics_draw_text(GContext *ctx, const char *text, GFont const font, const GRect box,
                        const GTextOverflowMode overflow_mode, const GTextAlignment alignment,
                        GTextAttributes *text_attributes) {
  cl_assert(s_anzahl < 128);
  snprintf(s_texte[s_anzahl++], sizeof(s_texte[0]), "%s", text);
}
GSize app_graphics_text_layout_get_content_size(const char *text, GFont const font, const GRect box,
                                                const GTextOverflowMode mode,
                                                const GTextAlignment alignment) {
  return GSize((int16_t)(strlen(text) * 8), 20);
}
GFont fonts_get_system_font(const char *font_key) {
  return (GFont)font_key;
}
void graphics_context_set_fill_color(GContext *ctx, GColor color) {
}
void graphics_context_set_stroke_color(GContext *ctx, GColor color) {
}
void graphics_context_set_text_color(GContext *ctx, GColor color) {
}
void graphics_fill_round_rect_by_value(GContext *ctx, GRect rect, uint16_t r, GCornerMask m) {
}
void graphics_draw_line(GContext *ctx, GPoint p0, GPoint p1) {
}
void graphics_fill_circle(GContext *ctx, GPoint p, uint16_t radius) {
}
void graphics_draw_circle(GContext *ctx, GPoint p, uint16_t radius) {
}

#define HEUTE 9781
#define JETZT 1791806700

static BhDaten s_d;

// Jede Quelle mit einem Wert, der sonst nirgends vorkommt.
static void prv_erkennbar(BhDaten *d) {
  memset(d, 0, sizeof(*d));
  d->jetzt = JETZT;
  d->heute = HEUTE;
  d->h24 = true;
  d->lage_da = true;
  d->lage = (BoulderLage){
    .fassung = BOULDER_LAGE_FASSUNG,
    .tracking = true,
    .messstufe = 4,
    .ziel_schritte = 10000,
    .ziel_schlaf_min = 480,
    .maxpuls = 187,
    .ruhepuls_median = 50,
    .spo2_takt_min = 30
  };
  // S1: die Nacht, aufgewacht heute 07:00
  d->nacht_lesen = BoulderLesenDa;
  d->nacht = (NachtBild){
    .beginn_utc = 1791748800,
    .utc_to_local = 7200,
    .einschlafen = 17,
    .aufwachen = 540,
    .schlaf_minuten = 437,
    .hrv = 63,
    .ruhepuls = 49,
    .fertig = true,
    .anzahl = 2,
    .minuten = {13, 251, 71, 103, 0}
  };
  d->nacht.abschnitte[0] = (NachtDlsAbschnitt){.von = 17, .bis = 300, .phase = 1};
  d->nacht.abschnitte[1] = (NachtDlsAbschnitt){.von = 300, .bis = 540, .phase = 3};
  // S5: nur der heutige Wachtag
  d->naechte_da = true;
  d->naechte = (BoulderNaechte){.fassung = BOULDER_NAECHTE_FASSUNG, .anzahl = 1, .neu = 0};
  d->naechte.tage[0] = (BoulderNachtEintrag){.tag = HEUTE, .hrv = 63, .ruhepuls = 49};
  // S6: Zonen
  d->herz_da = true;
  d->herz.zonen_tag = HEUTE;
  d->herz.zonen_min[0] = 111;
  d->herz.zonen_min[1] = 222;
  d->herz.zonen_min[2] = 333;
  d->herz.zonen_min[3] = 444;
  // S3: SpO2, der letzte gilt
  d->spo2_anzahl = 2;
  d->spo2[0] = (Spo2Wert){.zeit = JETZT - 3600, .prozent = 97};
  d->spo2[1] = (Spo2Wert){.zeit = JETZT - 60, .prozent = 91};
  // Pebble
  d->schritte = 17239;
  d->meter = 7400;
  d->aktiv_s = 37 * 60;
  d->kcal = 2468;
  d->puls = 77;
  d->puls_zeit = JETZT - 30;
  d->sitzungen_anzahl = 1;
  d->sitzungen[0] = (ActivitySession){
    .start_utc = JETZT - 7200,
    .length_min = 26,
    .type = ActivitySessionType_Walk,
    .step_data = {.steps = 2345}
  };
}

static void prv_karte(BhKarte karte) {
  s_anzahl = 0;
  bh_inhalt_zeichnen(NULL, GRect(0, 0, 200, 228), karte, &s_d);
}

static void prv_detail(BhKarte karte) {
  s_anzahl = 0;
  bh_detail_zeichnen(NULL, 200, bh_detail_von(karte), &s_d, 0);
}

static bool prv_steht(const char *gesucht) {
  for (int i = 0; i < s_anzahl; i++) {
    if (strstr(s_texte[i], gesucht)) {
      return true;
    }
  }
  return false;
}

#define STEHT(text) cl_assert_(prv_steht(text), "fehlt: " text)

void test_bh_grundsatz__initialize(void) {
  prv_erkennbar(&s_d);
}

void test_bh_grundsatz__cleanup(void) {
}

void test_bh_grundsatz__jede_quelle_an_ihrem_ort(void) {
  prv_karte(BhKarteToday);
  STEHT("7:17");   // S1 Schlaf
  STEHT("49");     // S1 Ruhepuls
  STEHT("63");     // S5 HRV des Wachtags
  STEHT("17,239"); // Pebble Schritte

  prv_karte(BhKarteNight);
  STEHT("7:17");
  STEHT("22:17"); // S1 Einschlafen
  STEHT("07:00"); // S1 Aufwachen
  STEHT("4:11");  // S1 Minuten leicht
  STEHT("1:43");  // S1 Minuten REM

  prv_karte(BhKarteHeart);
  STEHT("77");  // Pebble Puls jetzt
  STEHT("91%"); // S3 juengster Wert

  prv_karte(BhKarteActivity);
  STEHT("17,239");
  STEHT("7.4"); // Pebble Strecke
  STEHT("37");  // Pebble aktive Minuten

  prv_karte(BhKarteSettings);
  STEHT("Detailed"); // S2 Messstufe

  prv_detail(BhKarteHeart);
  STEHT("111"); // S6 Zonen
  STEHT("222");
  STEHT("333");
  STEHT("444");

  prv_detail(BhKarteActivity);
  STEHT("2,345"); // Pebble Sitzung
  STEHT("2,468"); // Pebble kcal (V12)

  prv_detail(BhKarteNight);
  STEHT("49");
  STEHT("63");
}

// Nichts auf Karten und Details spricht von dem, was nur das Telefon kennt (F5, Grundsatz).
void test_bh_grundsatz__nichts_vom_telefon(void) {
  static const char *const verboten[] = {
    "mg",   "affein", "offee", "ater",  "rink",    "upplement",
    "tack", "kg",     "mmHg",  "eight", "ressure", "%/h",
  };
  for (int k = 0; k < BhKartenAnzahl; k++) {
    for (int detail = 0; detail < 2; detail++) {
      if (detail) {
        prv_detail((BhKarte)k);
      } else {
        prv_karte((BhKarte)k);
      }
      for (int i = 0; i < s_anzahl; i++) {
        for (size_t v = 0; v < sizeof(verboten) / sizeof(verboten[0]); v++) {
          if (strstr(s_texte[i], verboten[v])) {
            printf("Karte %d%s: \"%s\"\n", k, detail ? " (Detail)" : "", s_texte[i]);
            cl_fail("Wert vom Telefon auf der Uhr");
          }
        }
      }
    }
  }
}

// Ohne Daten der Variante steht keine Zahl der Variante da: Die Erkennungswerte verschwinden mit
// ihrer Quelle.
void test_bh_grundsatz__ohne_quelle_kein_wert(void) {
  s_d.nacht_lesen = BoulderLesenKeine;
  s_d.naechte_da = false;
  s_d.herz_da = false;
  s_d.spo2_anzahl = 0;
  for (int k = 0; k < BhKartenAnzahl; k++) {
    prv_karte((BhKarte)k);
    cl_assert(!prv_steht("7:17"));
    cl_assert(!prv_steht("49"));
    cl_assert(!prv_steht("63"));
    cl_assert(!prv_steht("91%"));
  }
  prv_detail(BhKarteHeart);
  cl_assert(!prv_steht("111"));
}

// --- Gegenpruefung Schritt 3 ---

// C3: Heart zeigt Ruhepuls und HRV des heutigen Wachtags wie Today. Wurde die Uhr letzte Nacht
// nicht getragen, ist S1 zwei Tage alt: dann steht weder auf Today noch auf Heart ein Wert davon.
void test_bh_grundsatz__heart_ohne_alte_nacht(void) {
  s_d.nacht.beginn_utc -= 2 * 24 * 60 * 60;
  s_d.nacht.ruhepuls = 71;
  s_d.nacht.hrv = 88;
  s_d.naechte.tage[0].tag = HEUTE - 2;
  prv_karte(BhKarteHeart);
  cl_assert(!prv_steht("71"));
  cl_assert(!prv_steht("88"));
  prv_karte(BhKarteToday);
  cl_assert(!prv_steht("71"));
  cl_assert(!prv_steht("88"));
}

// C3 und C2: Heart und Today zeigen fuer heute dieselben Werte, auch wenn S1 und die Liste
// verschieden sind (die Liste traegt die laengste Nacht).
void test_bh_grundsatz__heart_und_today_gleich(void) {
  s_d.naechte.min_ruhe = 500;
  s_d.naechte.min_hrv = 500;
  s_d.naechte.tage[0].ruhepuls = 48;
  s_d.naechte.tage[0].hrv = 66;
  prv_karte(BhKarteHeart);
  STEHT("48");
  STEHT("66 ms");
  cl_assert(!prv_steht("49"));
  prv_karte(BhKarteToday);
  STEHT("48");
  STEHT("66");
  STEHT("8:20"); // die laengere Nacht aus der Liste
}

// A8: Solange die Nacht nicht fertig ist, urteilt Today nicht ("so far" wie Night).
void test_bh_grundsatz__today_ohne_urteil_fuer_unfertige_nacht(void) {
  s_d.nacht.fertig = false;
  prv_karte(BhKarteToday);
  STEHT("so far");
  static const char *const urteile[] = {
    "Well rested", "Short night", "Very short night", "as usual", "in range", "above", "below",
  };
  for (size_t i = 0; i < sizeof(urteile) / sizeof(urteile[0]); i++) {
    cl_assert(!prv_steht(urteile[i]));
  }
}

// C8: Zonenminuten mit Tausendertrenner wie Boulder ("%,d").
void test_bh_grundsatz__zonen_mit_tausendern(void) {
  s_d.herz.zonen_min[0] = 1100;
  prv_detail(BhKarteHeart);
  STEHT("1,100 min");
}
