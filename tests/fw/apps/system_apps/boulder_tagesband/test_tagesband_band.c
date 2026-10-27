/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "apps/system/boulder_tagesband/band.h"
#include "clar.h"

#include <string.h>

// Die Rechnung des Zifferblatts Tagesband: wo im Band was liegt, und die Texte.

#define JETZT  1791806700 // 12.10.2026 12:05 UTC = 14:05 MESZ, ein Montag
#define HEUTE  9781
#define STUNDE 3600

// Ortszeit: MESZ, ab `s_umstellung` MEZ (daten.c rechnet sie mit sys_localtime_r).
static time_t s_umstellung;

int32_t bh_ortsversatz(time_t utc) {
  return (s_umstellung && utc >= s_umstellung) ? STUNDE : 2 * STUNDE;
}

static BhDaten s_d;

void test_tagesband_band__initialize(void) {
  s_umstellung = 0;
  memset(&s_d, 0, sizeof(s_d));
  s_d.jetzt = JETZT;
  s_d.heute = HEUTE;
}

// --- Band und Strecken ---

void test_tagesband_band__drei_viertel_vergangenheit(void) {
  const BtBand b = bt_band(JETZT, 8, 184);
  cl_assert_equal_i(b.von, JETZT - 18 * STUNDE);
  cl_assert_equal_i(bt_x(&b, b.von), 8);
  cl_assert_equal_i(bt_x(&b, JETZT), 8 + 138);
  cl_assert_equal_i(bt_x(&b, b.von + 24 * STUNDE), 8 + 184);
}

void test_tagesband_band__strecke_nur_in_der_vergangenheit(void) {
  const BtBand b = bt_band(JETZT, 8, 184);
  int16_t l = -1, r = -1;
  // Vor dem Band, in der Zukunft, leer: nichts.
  cl_assert(!bt_strecke(&b, b.von - 2 * STUNDE, b.von - STUNDE, &l, &r));
  cl_assert(!bt_strecke(&b, JETZT + STUNDE, JETZT + 2 * STUNDE, &l, &r));
  cl_assert(!bt_strecke(&b, JETZT - STUNDE, JETZT - STUNDE, &l, &r));
  // Ueber den linken Rand: ab dem Rand.
  cl_assert(bt_strecke(&b, b.von - STUNDE, b.von + STUNDE, &l, &r));
  cl_assert_equal_i(l, 8);
  cl_assert_equal_i(r, 8 + 7);
  // Ueber das Jetzt: bis zum Jetzt.
  cl_assert(bt_strecke(&b, JETZT - STUNDE, JETZT + STUNDE, &l, &r));
  cl_assert_equal_i(l, bt_x(&b, JETZT - STUNDE));
  cl_assert_equal_i(r, 8 + 138);
}

void test_tagesband_band__kurzes_bleibt_sichtbar(void) {
  const BtBand b = bt_band(JETZT, 8, 184);
  int16_t l, r;
  // Zwei Minuten sind weniger als ein Pixel.
  cl_assert(bt_strecke(&b, JETZT - 3 * STUNDE, JETZT - 3 * STUNDE + 120, &l, &r));
  cl_assert_equal_i(r, l + 1);
}

// --- Farben ---

void test_tagesband_band__phasen(void) {
  cl_assert(gcolor_equal(bt_phasenfarbe(0), GColorChromeYellow));
  cl_assert(gcolor_equal(bt_phasenfarbe(1), GColorBlueMoon));
  cl_assert(gcolor_equal(bt_phasenfarbe(2), GColorBlue));
  cl_assert(gcolor_equal(bt_phasenfarbe(3), GColorPictonBlue));
  cl_assert(gcolor_equal(bt_phasenfarbe(4), GColorBlueMoon));
}

void test_tagesband_band__sitzungen(void) {
  ActivitySession s = {0};
  const struct {
    ActivitySessionType art;
    GColor farbe;
  } erwartet[] = {
    {ActivitySessionType_Walk, GColorJaegerGreen},
    {ActivitySessionType_Run, GColorJaegerGreen},
    {ActivitySessionType_Open, GColorJaegerGreen},
    {ActivitySessionType_Nap, GColorBlueMoon},
    // Pebbles Schlaf zeigt das Band nicht: Die Nacht kommt mit Phasen aus S1.
    {ActivitySessionType_Sleep, GColorClear},
    {ActivitySessionType_RestfulSleep, GColorClear},
    {ActivitySessionType_RestfulNap, GColorClear},
    {ActivitySessionType_None, GColorClear},
  };
  for (size_t i = 0; i < sizeof(erwartet) / sizeof(erwartet[0]); i++) {
    s.type = erwartet[i].art;
    cl_assert(gcolor_equal(bt_sitzungsfarbe(&s), erwartet[i].farbe));
  }
}

// --- Puls ---

static void prv_halbstunde(uint32_t nr, uint8_t bpm) {
  const uint8_t p = nr % HERZ_HALBSTUNDEN;
  s_d.herz.mittel[p] = bpm;
  s_d.herz.min[p] = bpm - 4;
  s_d.herz.max[p] = bpm + 4;
  s_d.herz.n[p] = 20;
}

void test_tagesband_band__puls_ohne_herz(void) {
  const BtBand b = bt_band(JETZT, 8, 184);
  BtPulspunkt p[HERZ_HALBSTUNDEN];
  uint8_t unten = 1, oben = 1;
  cl_assert_equal_i(bt_pulspunkte(&b, &s_d, p, &unten, &oben), 0);
  cl_assert_equal_i(unten, 0);
  cl_assert_equal_i(oben, 0);
}

void test_tagesband_band__puls_mit_luecke(void) {
  const BtBand b = bt_band(JETZT, 8, 184);
  const uint32_t jetzt = JETZT / HERZ_HALBSTUNDE_S;
  s_d.herz_da = true;
  s_d.herz.neueste = jetzt;
  // Drei Halbstunden, eine Luecke, dann die laufende.
  prv_halbstunde(jetzt - 4, 60);
  prv_halbstunde(jetzt - 3, 70);
  prv_halbstunde(jetzt - 2, 90);
  prv_halbstunde(jetzt, 80);
  BtPulspunkt p[HERZ_HALBSTUNDEN];
  uint8_t unten, oben;
  cl_assert_equal_i(bt_pulspunkte(&b, &s_d, p, &unten, &oben), 4);
  cl_assert(p[0].neu);
  cl_assert(!p[1].neu);
  cl_assert(!p[2].neu);
  cl_assert(p[3].neu);
  cl_assert_equal_i(p[0].bpm, 60);
  cl_assert_equal_i(p[3].bpm, 80);
  cl_assert_equal_i(unten, 55);
  cl_assert_equal_i(oben, 95);
  // In der Mitte der Halbstunde; die laufende endet beim Jetzt.
  cl_assert_equal_i(p[0].x, bt_x(&b, (time_t)(jetzt - 4) * HERZ_HALBSTUNDE_S + 900));
  cl_assert_equal_i(p[3].x, bt_x(&b, JETZT));
}

void test_tagesband_band__puls_nach_der_neuesten_ist_alt(void) {
  // Seit drei Stunden ohne Minute (Uhr am Kabel): Die Plaetze danach tragen noch Werte von gestern.
  const BtBand b = bt_band(JETZT, 8, 184);
  const uint32_t jetzt = JETZT / HERZ_HALBSTUNDE_S;
  s_d.herz_da = true;
  s_d.herz.neueste = jetzt - 6;
  prv_halbstunde(jetzt - 7, 64);
  prv_halbstunde(jetzt - 6, 66);
  prv_halbstunde(jetzt - 3, 120);
  prv_halbstunde(jetzt, 130);
  BtPulspunkt p[HERZ_HALBSTUNDEN];
  uint8_t unten, oben;
  cl_assert_equal_i(bt_pulspunkte(&b, &s_d, p, &unten, &oben), 2);
  cl_assert_equal_i(p[1].bpm, 66);
  cl_assert_equal_i(oben, 71);
}

void test_tagesband_band__puls_nur_im_band(void) {
  const BtBand b = bt_band(JETZT, 8, 184);
  const uint32_t jetzt = JETZT / HERZ_HALBSTUNDE_S;
  s_d.herz_da = true;
  s_d.herz.neueste = jetzt;
  // 20 Stunden zurueck liegt vor dem Band, 18 Stunden zurueck (die Mitte) knapp darin.
  prv_halbstunde(jetzt - 40, 50);
  prv_halbstunde(jetzt - 35, 52);
  BtPulspunkt p[HERZ_HALBSTUNDEN];
  uint8_t unten, oben;
  cl_assert_equal_i(bt_pulspunkte(&b, &s_d, p, &unten, &oben), 1);
  cl_assert_equal_i(p[0].bpm, 52);
}

// --- Stundenmarken ---

void test_tagesband_band__marken_ortszeit(void) {
  // 14:05 MESZ: das Band von 20:05 gestern bis 20:05 heute, Ortszeit.
  const BtBand b = bt_band(JETZT, 8, 184);
  BtMarke m[BT_MARKEN_MAX];
  cl_assert_equal_i(bt_marken(&b, m), 4);
  const time_t mitternacht = 1791756000; // 12.10.2026 0:00 MESZ
  for (int i = 0; i < 4; i++) {
    cl_assert_equal_i(m[i].stunde, i * 6);
    cl_assert_equal_i(m[i].x, bt_x(&b, mitternacht + i * 6 * STUNDE));
  }
}

void test_tagesband_band__marke_am_rand(void) {
  // Der linke Rand genau auf 18:00 MESZ (11.10. 16:00 UTC): fuenf Marken, die letzte am rechten
  // Rand.
  const BtBand b = bt_band(1791734400 + 18 * STUNDE, 8, 184);
  BtMarke m[BT_MARKEN_MAX];
  cl_assert_equal_i(bt_marken(&b, m), 5);
  cl_assert_equal_i(m[0].stunde, 18);
  cl_assert_equal_i(m[0].x, 8);
  cl_assert_equal_i(m[4].stunde, 18);
  cl_assert_equal_i(m[4].x, 8 + 184);
}

void test_tagesband_band__marken_ueber_die_zeitumstellung(void) {
  // 25.10.2026: um 01:00 UTC (03:00 MESZ) zurueck auf 02:00 MEZ. Jetzt 11:00 MEZ, der linke Rand
  // 18:00 MESZ am Vortag. Die Marken nach der Umstellung liegen eine Stunde spaeter in UTC.
  s_umstellung = 1792890000;
  const BtBand b = bt_band(1792922400, 8, 184);
  BtMarke m[BT_MARKEN_MAX];
  cl_assert_equal_i(bt_marken(&b, m), 4);
  const time_t utc[] = {1792857600, 1792879200, 1792904400, 1792926000};
  const uint8_t stunde[] = {18, 0, 6, 12};
  for (int i = 0; i < 4; i++) {
    cl_assert_equal_i(m[i].stunde, stunde[i]);
    cl_assert_equal_i(m[i].x, bt_x(&b, utc[i]));
  }
}

void test_tagesband_band__marken_text(void) {
  char t[4];
  const struct {
    uint8_t stunde;
    bool h24;
    const char *text;
  } faelle[] = {
    {0, true, "0"},    {6, true, "6"},   {12, true, "12"},   {18, true, "18"},
    {0, false, "12a"}, {6, false, "6a"}, {12, false, "12p"}, {18, false, "6p"},
  };
  for (size_t i = 0; i < sizeof(faelle) / sizeof(faelle[0]); i++) {
    bt_marke_text(faelle[i].stunde, faelle[i].h24, t, sizeof(t));
    cl_assert_equal_s(t, faelle[i].text);
  }
}

// --- Datum und Zeit ---

void test_tagesband_band__datum(void) {
  char t[16];
  bt_datum(HEUTE, t, sizeof(t));
  cl_assert_equal_s(t, "MON 12 OCT");
  bt_datum(9796, t, sizeof(t));
  cl_assert_equal_s(t, "TUE 27 OCT");
  bt_datum(0, t, sizeof(t));
  cl_assert_equal_s(t, "SAT 1 JAN");
  bt_datum(59, t, sizeof(t));
  cl_assert_equal_s(t, "TUE 29 FEB");
  bt_datum(60, t, sizeof(t));
  cl_assert_equal_s(t, "WED 1 MAR");
  bt_datum(8825, t, sizeof(t));
  cl_assert_equal_s(t, "THU 29 FEB");
  bt_datum(36524, t, sizeof(t));
  cl_assert_equal_s(t, "THU 31 DEC");
}

void test_tagesband_band__zeit(void) {
  char t[8];
  const char *ampm;
  bt_zeit(14 * 60 + 5, true, t, sizeof(t), &ampm);
  cl_assert_equal_s(t, "14:05");
  cl_assert(ampm == NULL);
  bt_zeit(9 * 60 + 5, true, t, sizeof(t), &ampm);
  cl_assert_equal_s(t, "09:05");
  bt_zeit(0, false, t, sizeof(t), &ampm);
  cl_assert_equal_s(t, "12:00");
  cl_assert_equal_s(ampm, "AM");
  bt_zeit(12 * 60, false, t, sizeof(t), &ampm);
  cl_assert_equal_s(t, "12:00");
  cl_assert_equal_s(ampm, "PM");
  bt_zeit(23 * 60 + 59, false, t, sizeof(t), &ampm);
  cl_assert_equal_s(t, "11:59");
  cl_assert_equal_s(ampm, "PM");
}

// --- Werte von heute ---

static void prv_liste(uint16_t juengster, uint8_t ruhepuls, uint16_t hrv) {
  s_d.naechte_da = true;
  s_d.naechte.fassung = BOULDER_NAECHTE_FASSUNG;
  s_d.naechte.anzahl = 2;
  s_d.naechte.neu = 1;
  s_d.naechte.tage[0] = (BoulderNachtEintrag){.tag = juengster - 1, .hrv = 40, .ruhepuls = 60};
  s_d.naechte.tage[1] = (BoulderNachtEintrag){.tag = juengster, .hrv = hrv, .ruhepuls = ruhepuls};
}

void test_tagesband_band__heute_aus_der_liste(void) {
  prv_liste(HEUTE, 52, 48);
  uint8_t ruhe;
  uint16_t hrv;
  bt_heute(&s_d, &ruhe, &hrv);
  cl_assert_equal_i(ruhe, 52);
  cl_assert_equal_i(hrv, 48);
}

void test_tagesband_band__gestern_zeigt_nichts(void) {
  // Die Uhr war heute Nacht nicht am Arm: Die Werte von gestern gelten nicht fuer heute.
  prv_liste(HEUTE - 1, 52, 48);
  uint8_t ruhe = 1;
  uint16_t hrv = 1;
  bt_heute(&s_d, &ruhe, &hrv);
  cl_assert_equal_i(ruhe, 0);
  cl_assert_equal_i(hrv, 0);
}

void test_tagesband_band__heute_aus_der_letzten_nacht(void) {
  // Die Nacht von heute ist gerechnet (S1), aber noch nicht in der Liste (S5).
  prv_liste(HEUTE - 1, 52, 48);
  s_d.nacht_lesen = BoulderLesenDa;
  s_d.nacht.beginn_utc = 1791748800; // 11.10. 22:00 MESZ
  s_d.nacht.aufwachen = 540;         // 12.10. 07:00 MESZ
  s_d.nacht.schlaf_minuten = 470;
  s_d.nacht.ruhepuls = 51;
  s_d.nacht.hrv = 55;
  s_d.nacht.anzahl = 1;
  s_d.nacht.abschnitte[0] = (NachtDlsAbschnitt){.von = 0, .bis = 540, .phase = 1};
  uint8_t ruhe;
  uint16_t hrv;
  bt_heute(&s_d, &ruhe, &hrv);
  cl_assert_equal_i(ruhe, 51);
  cl_assert_equal_i(hrv, 55);
}
