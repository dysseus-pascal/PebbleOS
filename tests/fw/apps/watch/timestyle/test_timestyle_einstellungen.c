/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "apps/watch/timestyle/languages.h"
#include "apps/watch/timestyle/schriftwahl.h"
#include "apps/watch/timestyle/ts_format.h"
#include "apps/watch/timestyle/zweitzeit.h"

#include <stdio.h>
#include <string.h>

// Die Einstellungen von TimeStyle Bedrock aus Boulder: Leseregeln mit den
// gemeinsamen Testvektoren (dieselben stehen in Boulders TsBedrockFormatTest),
// die erlaubten Sprachen, die Schrift je Zeile (schriftwahl.c) und die Stunde
// des Widgets "Time zone".

void test_timestyle_einstellungen__initialize(void) {
}
void test_timestyle_einstellungen__cleanup(void) {
}

// --- Hilfen ---

// Hex aus der Spezifikation in Bytes; gibt die Laenge zurueck.
static size_t prv_hex(const char *hex, uint8_t *aus, size_t max) {
  const size_t n = strlen(hex) / 2;
  cl_assert(n <= max);
  for (size_t i = 0; i < n; i++) {
    unsigned int byte;
    cl_assert(sscanf(&hex[2 * i], "%2x", &byte) == 1);
    aus[i] = (uint8_t)byte;
  }
  return n;
}

static Settings prv_dekodiert(const char *hex) {
  uint8_t b[64];
  const size_t n = prv_hex(hex, b, sizeof(b));
  Settings s;
  // Muell vorher, damit ein vergessenes Feld auffaellt.
  memset(&s, 0xa5, sizeof(s));
  ts_format_dekodieren(b, n, &s);
  return s;
}

// Was ein Testvektor ergeben soll, in der Reihenfolge der Spezifikation.
typedef struct {
  uint8_t farben[4];
  uint8_t sprache;
  uint8_t schrift;
  uint8_t stundenvibration;
  uint8_t widgets[3];
  int versatz;
  char trenner;
  bool fuehrende_null, bt_vibe, links, gross, trennungssymbol, akku_prozent, kein_autoakku;
  bool distanz, tiefschlaf;
  const char *name;
} Erwartet;

static void prv_pruefe(const Settings *s, const Erwartet *e) {
  cl_assert_equal_i(s->timeColor.argb, e->farben[0]);
  cl_assert_equal_i(s->timeBgColor.argb, e->farben[1]);
  cl_assert_equal_i(s->sidebarColor.argb, e->farben[2]);
  cl_assert_equal_i(s->sidebarTextColor.argb, e->farben[3]);
  cl_assert_equal_i(s->languageId, e->sprache);
  cl_assert_equal_i(s->clockFontId, e->schrift);
  cl_assert_equal_i(s->hourlyVibe, e->stundenvibration);
  for (int i = 0; i < 3; i++) {
    cl_assert_equal_i(s->widgets[i], e->widgets[i]);
  }
  cl_assert_equal_i(s->altclockOffset, e->versatz);
  cl_assert_equal_i(s->decimalSeparator, e->trenner);
  cl_assert_equal_b(s->showLeadingZero, e->fuehrende_null);
  cl_assert_equal_b(s->btVibe, e->bt_vibe);
  cl_assert_equal_b(s->sidebarOnLeft, e->links);
  cl_assert_equal_b(s->useLargeFonts, e->gross);
  cl_assert_equal_b(s->activateDisconnectIcon, e->trennungssymbol);
  cl_assert_equal_b(s->showBatteryPct, e->akku_prozent);
  cl_assert_equal_b(s->disableAutobattery, e->kein_autoakku);
  cl_assert_equal_b(s->healthUseDistance, e->distanz);
  cl_assert_equal_b(s->healthUseRestfulSleep, e->tiefschlaf);
  cl_assert_equal_s(s->altclockName, e->name);
  // Immer mit Null am Ende, auch wenn Byte 23 etwas anderes sagt.
  cl_assert_equal_i(s->altclockName[7], 0);
  // Reserviert (frueher useMetric): bleibt die Vorgabe.
  cl_assert_equal_b(s->useMetric, true);
}

// Die gemeinsamen Testvektoren (Spezifikation 5.1), woertlich.
#define V0 "01ffc0ffc0000100070a04002e500000414c5400000000000000000000000000"
#define V1 "01f4c1dfff02010203090dfb2c8f03004e594300000000000000000000000000"
#define V2 "0130003f0c090007010eff0f7820fc0041414141414141410000000000000000"
#define V3 "0000000000000000000000000000000000000000000000000000000000000000"
#define V4 "02ffc0ffc0000100070a04002e5000ff414c540000000000ffffffffffffffff"
#define V5 "01c0ffc0ff01010100020c0e2e0000005ac3bc00000000000000000000000000"
// Ab b24: Sprache 37 und Schrift 5 gibt es nicht (Vorgabe), Schrift 3 und
// Sprache 36 (Hebraeisch) schon.
#define V7 "01ffc0ffc0250500070a04002e500000414c5400000000000000000000000000"
#define V8 "01ffc0ffc0240300070a04002e500000414c5400000000000000000000000000"

static const Erwartet s_vorgabe = {
  .farben = {0xff, 0xc0, 0xff, 0xc0},
  .sprache = 0,
  .schrift = 1,
  .stundenvibration = 0,
  .widgets = {7, 10, 4},
  .versatz = 0,
  .trenner = '.',
  .trennungssymbol = true,
  .akku_prozent = true,
  .name = "ALT",
};

// --- Die Testvektoren ---

void test_timestyle_einstellungen__v0_vorgabe(void) {
  const Settings s = prv_dekodiert(V0);
  prv_pruefe(&s, &s_vorgabe);
}

void test_timestyle_einstellungen__v1_alles_anders(void) {
  const Settings s = prv_dekodiert(V1);
  const Erwartet e = {
    .farben = {0xf4, 0xc1, 0xdf, 0xff},
    .sprache = 2,
    .schrift = 1,
    .stundenvibration = 2,
    .widgets = {3, 9, 13},
    .versatz = -5,
    .trenner = ',',
    .fuehrende_null = true,
    .bt_vibe = true,
    .links = true,
    .gross = true,
    .trennungssymbol = false,
    .akku_prozent = false,
    .kein_autoakku = true,
    .distanz = true,
    .tiefschlaf = true,
    .name = "NYC",
  };
  prv_pruefe(&s, &e);
  // Die Farben sind die erwarteten Pebble-Farben.
  cl_assert_equal_i(s.timeColor.argb, GColorOrangeARGB8);
  cl_assert_equal_i(s.timeBgColor.argb, GColorOxfordBlueARGB8);
  cl_assert_equal_i(s.sidebarColor.argb, GColorElectricBlueARGB8);
  cl_assert_equal_i(s.sidebarTextColor.argb, GColorWhiteARGB8);
}

void test_timestyle_einstellungen__v2_ungueltige_felder(void) {
  const Settings s = prv_dekodiert(V2);
  const Erwartet e = {
    .farben = {0xf0, 0xc0, 0xff, 0xcc}, // Alpha erzwungen
    .sprache = 9,                       // Griechisch: ab b24 erlaubt (Sprachpaket)
    .schrift = 0,                       // ab b24 gelesen: "Regular"
    .stundenvibration = 0,              // 7 -> aus
    .widgets = {7, 10, 4},              // 1, 14, 255 -> Vorgabe je Platz
    .versatz = 0,                       // 15 -> 0
    .trenner = '.',                     // 'x' -> '.'
    .name = "AAAAAAA",                  // Byte 23 = 'A' zaehlt nicht
  };
  prv_pruefe(&s, &e);
}

void test_timestyle_einstellungen__v3_fassung_0_ist_vorgabe(void) {
  const Settings s = prv_dekodiert(V3);
  prv_pruefe(&s, &s_vorgabe);
}

void test_timestyle_einstellungen__v4_fassung_2_wird_wie_1_gelesen(void) {
  const Settings s = prv_dekodiert(V4);
  prv_pruefe(&s, &s_vorgabe);
}

void test_timestyle_einstellungen__v5_schwarz_auf_weiss_utf8(void) {
  const Settings s = prv_dekodiert(V5);
  const Erwartet e = {
    .farben = {0xc0, 0xff, 0xc0, 0xff},
    .sprache = 1,
    .schrift = 1,
    .stundenvibration = 1,
    .widgets = {0, 2, 12},
    .versatz = 14,
    .trenner = '.',
    .name = "Z\xc3\xbc", // "Zü"
  };
  prv_pruefe(&s, &e);
}

void test_timestyle_einstellungen__v7_sprache_und_schrift_ausserhalb(void) {
  const Settings s = prv_dekodiert(V7);
  prv_pruefe(&s, &s_vorgabe);
}

void test_timestyle_einstellungen__v8_hebraeisch_fette_stunde(void) {
  const Settings s = prv_dekodiert(V8);
  Erwartet e = s_vorgabe;
  e.sprache = 36;
  e.schrift = 3;
  prv_pruefe(&s, &e);
}

// --- Schalter-Bits: je genau eines gesetzt (Walking One) ---
// Jedes Bit hat sein eigenes, ausgeschriebenes Ergebnis - so faellt auch ein
// Vertauschen zweier Bits auf (frueher waren in allen Vektoren mehrere Bits
// gleich gesetzt).

// V0 mit Byte 13 = [a] und Byte 14 = [b].
static Settings prv_mit_flags(uint8_t a, uint8_t b) {
  uint8_t w[32];
  prv_hex(V0, w, sizeof(w));
  w[13] = a;
  w[14] = b;
  Settings s;
  memset(&s, 0xa5, sizeof(s));
  ts_format_dekodieren(w, sizeof(w), &s);
  return s;
}

static Erwartet prv_ohne_flags(void) {
  Erwartet e = s_vorgabe;
  e.trennungssymbol = false;
  e.akku_prozent = false;
  return e;
}

void test_timestyle_einstellungen__flags_a_einzeln(void) {
  Erwartet e = prv_ohne_flags();
  Settings s = prv_mit_flags(0x00, 0x00);
  prv_pruefe(&s, &e);

  e = prv_ohne_flags(); e.fuehrende_null = true;
  s = prv_mit_flags(0x01, 0x00);
  prv_pruefe(&s, &e);
  e = prv_ohne_flags(); e.bt_vibe = true;
  s = prv_mit_flags(0x02, 0x00);
  prv_pruefe(&s, &e);
  e = prv_ohne_flags(); e.links = true;
  s = prv_mit_flags(0x04, 0x00);
  prv_pruefe(&s, &e);
  e = prv_ohne_flags(); e.gross = true;
  s = prv_mit_flags(0x08, 0x00);
  prv_pruefe(&s, &e);
  e = prv_ohne_flags(); e.trennungssymbol = true;
  s = prv_mit_flags(0x10, 0x00);
  prv_pruefe(&s, &e);
  // Bit 5 ist frei (frueher useMetric) und aendert nichts.
  e = prv_ohne_flags();
  s = prv_mit_flags(0x20, 0x00);
  prv_pruefe(&s, &e);
  e = prv_ohne_flags(); e.akku_prozent = true;
  s = prv_mit_flags(0x40, 0x00);
  prv_pruefe(&s, &e);
  e = prv_ohne_flags(); e.kein_autoakku = true;
  s = prv_mit_flags(0x80, 0x00);
  prv_pruefe(&s, &e);
}

void test_timestyle_einstellungen__flags_b_einzeln(void) {
  Erwartet e = prv_ohne_flags(); e.distanz = true;
  Settings s = prv_mit_flags(0x00, 0x01);
  prv_pruefe(&s, &e);
  e = prv_ohne_flags(); e.tiefschlaf = true;
  s = prv_mit_flags(0x00, 0x02);
  prv_pruefe(&s, &e);
  // Bits 2-7 sind frei.
  e = prv_ohne_flags();
  for (int bit = 2; bit < 8; bit++) {
    s = prv_mit_flags(0x00, (uint8_t)(1 << bit));
    prv_pruefe(&s, &e);
  }
}

// --- Schrift (Byte 6) ---

void test_timestyle_einstellungen__schriften_wie_das_original(void) {
  // Die Nummern von FONT_SETTING_* sind die des Originals (index.js) und von
  // Boulder - sie duerfen sich nicht verschieben.
  cl_assert_equal_i(FONT_SETTING_DEFAULT, 0);
  cl_assert_equal_i(FONT_SETTING_LECO, 1);
  cl_assert_equal_i(FONT_SETTING_BOLD, 2);
  cl_assert_equal_i(FONT_SETTING_BOLD_H, 3);
  cl_assert_equal_i(FONT_SETTING_BOLD_M, 4);
  uint8_t b[32];
  prv_hex(V0, b, sizeof(b));
  for (int id = 0; id <= 255; id++) {
    b[6] = (uint8_t)id;
    Settings s;
    ts_format_dekodieren(b, sizeof(b), &s);
    cl_assert_equal_i(s.clockFontId, (id <= 4) ? id : 1);
  }
}

// Vom Byte bis zur Schrift je Zeile (schriftwahl.c, wie update_fonts im
// Original): Standard beide normal, fett beide fett, "Bold Hour" nur die
// Stunde, "Bold Minute" nur die Minute, LECO beide LECO.
void test_timestyle_einstellungen__byte_6_waehlt_die_schrift_je_zeile(void) {
  static const struct {
    uint8_t byte;
    TsSchrift stunden;
    TsSchrift minuten;
  } erwartet[] = {
      {0, TS_SCHRIFT_SANS, TS_SCHRIFT_SANS},
      {1, TS_SCHRIFT_LECO, TS_SCHRIFT_LECO},
      {2, TS_SCHRIFT_SANS_FETT, TS_SCHRIFT_SANS_FETT},
      {3, TS_SCHRIFT_SANS_FETT, TS_SCHRIFT_SANS},
      {4, TS_SCHRIFT_SANS, TS_SCHRIFT_SANS_FETT},
      // Ausserhalb: die Leseregel laesst die Vorgabe LECO stehen.
      {5, TS_SCHRIFT_LECO, TS_SCHRIFT_LECO},
      {255, TS_SCHRIFT_LECO, TS_SCHRIFT_LECO},
  };
  uint8_t b[32];
  prv_hex(V0, b, sizeof(b));
  for (size_t i = 0; i < sizeof(erwartet) / sizeof(erwartet[0]); i++) {
    b[6] = erwartet[i].byte;
    Settings s;
    ts_format_dekodieren(b, sizeof(b), &s);
    TsSchrift stunden = (TsSchrift)99;
    TsSchrift minuten = (TsSchrift)99;
    ts_schriftwahl(s.clockFontId, &stunden, &minuten);
    cl_assert_equal_i(stunden, erwartet[i].stunden);
    cl_assert_equal_i(minuten, erwartet[i].minuten);
  }
}

void test_timestyle_einstellungen__unbekannte_schriftnummer_ist_leco(void) {
  // Direkt, ohne die Leseregel davor: eine Nummer, die es nicht gibt, darf
  // keine Schrift unbelegt lassen.
  TsSchrift stunden = (TsSchrift)99;
  TsSchrift minuten = (TsSchrift)99;
  ts_schriftwahl(7, &stunden, &minuten);
  cl_assert_equal_i(stunden, TS_SCHRIFT_LECO);
  cl_assert_equal_i(minuten, TS_SCHRIFT_LECO);
}

// --- Laenge und Vorgaben ---

void test_timestyle_einstellungen__falsche_laenge_ergibt_vorgaben(void) {
  uint8_t b[33];
  prv_hex(V1, b, sizeof(b));
  b[32] = 0;
  Settings s;

  // V6: V1 ohne das letzte Byte, V1 mit einem Byte mehr.
  ts_format_dekodieren(b, 31, &s);
  prv_pruefe(&s, &s_vorgabe);
  ts_format_dekodieren(b, 33, &s);
  prv_pruefe(&s, &s_vorgabe);

  ts_format_dekodieren(b, 0, &s);
  prv_pruefe(&s, &s_vorgabe);
  ts_format_dekodieren(NULL, 32, &s);
  prv_pruefe(&s, &s_vorgabe);
}

void test_timestyle_einstellungen__vorgaben_sind_v0(void) {
  Settings vorgabe;
  memset(&vorgabe, 0xa5, sizeof(vorgabe));
  ts_format_vorgaben(&vorgabe);
  prv_pruefe(&vorgabe, &s_vorgabe);

  // Byte fuer Byte gleich, also auch kein Feld, das nur eine Seite setzt.
  uint8_t b[32];
  prv_hex(V0, b, sizeof(b));
  Settings v0;
  memset(&v0, 0x5a, sizeof(v0));
  ts_format_dekodieren(b, sizeof(b), &v0);
  cl_assert_equal_i(memcmp(&vorgabe, &v0, sizeof(vorgabe)), 0);
}

// Ein Feld ungueltig, die anderen bleiben, wie sie ankamen.
void test_timestyle_einstellungen__ungueltiges_feld_laesst_andere_stehen(void) {
  uint8_t b[32];
  prv_hex(V1, b, sizeof(b));
  b[9] = 1; // Mitte: das Trennungssymbol ist nicht waehlbar
  Settings s;
  ts_format_dekodieren(b, sizeof(b), &s);
  cl_assert_equal_i(s.widgets[0], 3);
  cl_assert_equal_i(s.widgets[1], 10);
  cl_assert_equal_i(s.widgets[2], 13);
  cl_assert_equal_i(s.languageId, 2);
  cl_assert_equal_i(s.altclockOffset, -5);

  // Die Grenzen des Versatzes.
  b[11] = (uint8_t)-12;
  ts_format_dekodieren(b, sizeof(b), &s);
  cl_assert_equal_i(s.altclockOffset, -12);
  b[11] = (uint8_t)-13;
  ts_format_dekodieren(b, sizeof(b), &s);
  cl_assert_equal_i(s.altclockOffset, 0);
  b[11] = 14;
  ts_format_dekodieren(b, sizeof(b), &s);
  cl_assert_equal_i(s.altclockOffset, 14);
}

// --- Sprachen ---

void test_timestyle_einstellungen__sprachen_wie_spezifiziert(void) {
  // Ab b24 alle 37 Sprachen des Originals (0-36), auch die acht mit
  // Sprachpaket (GK 9, VN 13, RU 17, CN 29, UK 31, JP 34, KR 35, IW 36).
  for (int id = 0; id <= 255; id++) {
    cl_assert_equal_b(ts_format_sprache_erlaubt((uint8_t)id), id <= 36);
  }
  cl_assert_equal_i(LANGUAGE_IW, 36);
  cl_assert(ts_format_sprache_erlaubt(9));
  cl_assert(ts_format_sprache_erlaubt(36));
  cl_assert(!ts_format_sprache_erlaubt(37));
}

void test_timestyle_einstellungen__erlaubte_sprache_kommt_an(void) {
  uint8_t b[32];
  prv_hex(V0, b, sizeof(b));
  for (int id = 0; id <= 255; id++) {
    b[5] = (uint8_t)id;
    Settings s;
    ts_format_dekodieren(b, sizeof(b), &s);
    cl_assert_equal_i(s.languageId, ts_format_sprache_erlaubt((uint8_t)id) ? id : LANGUAGE_EN);
  }
}

// --- Widget "Time zone" ---

static const char *prv_zweitzeit(int stunde, int minute, long gmtoff, int versatz, bool h24,
                                 bool null) {
  static char puffer[8];
  memset(puffer, 'x', sizeof(puffer));
  zweitzeit_text(puffer, sizeof(puffer), stunde, minute, gmtoff, versatz, h24, null);
  return puffer;
}

// Zuerich im Sommer: 14:00 Ortszeit ist 12:00 UTC, UTC-4 also 8 Uhr.
// TimeStyle zog die Sommerzeit doppelt ab und zeigte 7.
void test_timestyle_einstellungen__zeitzone_im_sommer(void) {
  cl_assert_equal_s(prv_zweitzeit(14, 0, 7200, -4, false, false), "8a");
  cl_assert_equal_s(prv_zweitzeit(14, 0, 7200, -4, true, false), "8");
}

void test_timestyle_einstellungen__zeitzone_im_winter(void) {
  cl_assert_equal_s(prv_zweitzeit(14, 0, 3600, -5, false, false), "8a");
  cl_assert_equal_s(prv_zweitzeit(14, 0, 3600, -5, true, false), "8");
}

void test_timestyle_einstellungen__zeitzone_format(void) {
  // Fuehrende Null wie die Uhrzeit.
  cl_assert_equal_s(prv_zweitzeit(14, 0, 3600, -5, false, true), "08a");
  cl_assert_equal_s(prv_zweitzeit(14, 0, 3600, -5, true, true), "08");
  // Mittag und Mitternacht in 12 h.
  cl_assert_equal_s(prv_zweitzeit(13, 0, 3600, 0, false, false), "12p");
  cl_assert_equal_s(prv_zweitzeit(1, 0, 3600, 0, false, false), "12a");
  // Ueber den Tageswechsel, in beide Richtungen.
  cl_assert_equal_s(prv_zweitzeit(14, 0, 7200, 14, true, false), "2");
  cl_assert_equal_s(prv_zweitzeit(1, 0, 7200, -12, true, false), "11");
  cl_assert_equal_s(prv_zweitzeit(1, 0, 7200, -12, false, false), "11a");
  // Ein Ort mit halber Stunde Abstand (Indien, 14:00 = 08:30 UTC).
  cl_assert_equal_s(prv_zweitzeit(14, 0, 19800, 0, true, false), "8");
  cl_assert_equal_s(prv_zweitzeit(14, 0, 19800, -5, true, false), "3");
}
