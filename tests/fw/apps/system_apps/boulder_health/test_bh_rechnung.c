/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "apps/system/boulder_health/rechnung.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// T4.2 (Spezifikation Schritt 3, AP5): rechnung.c gegen Boulders eigene Rechnung. Die Sollwerte
// stammen nicht aus dieser Datei, sondern aus dem Orakel orakel_rechnung.h: Boulders Kotlin aus
// 34fc13cb (Tagesurteile.kt, Gesundheitsrechnung.kt, Formate.kt, Fenster.kt, Statistik.kt;
// Zeilen unveraendert ausgeschnitten), mit kotlinc auf Faellen ausgefuehrt, die jede Schwelle
// knapp darunter und darueber treffen, mit den Vorgabe-Zielen und mit 6:30 h und 7 500, dazu
// 120 Zufallsverlaeufe. Erzeuger: boulder-v2/belege-schritt3/ap5-orakel/.
//
// Jede Abweichung wird gezaehlt und mit ihrem Fall gemeldet; verlangt sind 0.

// --- Was das Orakel liefert ---

typedef struct {
  uint16_t minuten, ziel;
  BhSchlafurteil urteil;
} OrakelSchlaf;

typedef struct {
  uint32_t schritte, ziel;
  bool erreicht;
} OrakelZiel;

typedef struct {
  uint8_t median;
  uint16_t max;
  uint16_t grenzen[3];
  struct {
    uint16_t bpm;
    uint8_t zone;
  } proben[16];
  int proben_zahl;
} OrakelZonen;

typedef struct {
  time_t gemessen, jetzt;
  bool frisch;
} OrakelFrisch;

typedef struct {
  uint16_t tag, wert, unten, oben;
  uint8_t frueher;
  bool urteilt, im_band;
} OrakelBandtag;

typedef struct {
  const char *name;
  uint16_t heute;
  uint16_t erster, anzahl; // Wachtage in ORAKEL_TAGE
  uint8_t letzte;          // Ruhepuls der Nacht von heute, 0 = keine
  uint8_t ruhe_naechte;
  uint16_t ruhe_schnitt;
  BhVergleich ruhe_urteil;
  uint16_t ruhe_abstand;
  uint16_t avg_ruhe, avg_hrv, avg_hrv_boulder; // "avg" im Night-Detail, 0 = keiner
  BhVergleich hrv_urteil;
  uint16_t hrv_abstand;
  OrakelBandtag woche[BH_WOCHE_TAGE];
} OrakelNaechte;

typedef struct {
  uint16_t minuten;
  const char *zahl, *einheit, *stunden_minuten;
} OrakelDauer;

typedef struct {
  int32_t zahl;
  const char *text;
} OrakelZahl;

typedef struct {
  uint32_t meter;
  bool meilen;
  const char *zahl, *einheit;
} OrakelStrecke;

typedef struct {
  uint16_t minute;
  const char *h24, *h12;
} OrakelUhrzeit;

typedef struct {
  int tm_year, tm_yday;
  uint16_t tag;
  uint8_t wochentag;
  const char *kurz;
} OrakelDatum;

#include "orakel_rechnung.h"

#define ANZAHL(a) ((int)(sizeof(a) / sizeof((a)[0])))

static int s_abweichungen;

static void prv_abweichung(const char *fall, const char *was, long soll, long ist) {
  if (s_abweichungen++ < 20) {
    printf("  ABWEICHUNG %s: %s soll %ld, ist %ld\n", fall, was, soll, ist);
  }
}

static void prv_gleich(const char *fall, const char *was, long soll, long ist) {
  if (soll != ist) {
    prv_abweichung(fall, was, soll, ist);
  }
}

static void prv_text(const char *fall, const char *soll, const char *ist) {
  if ((soll == NULL) != (ist == NULL) || (soll && strcmp(soll, ist) != 0)) {
    if (s_abweichungen++ < 20) {
      printf("  ABWEICHUNG %s: soll \"%s\", ist \"%s\"\n", fall, soll ? soll : "(NULL)",
             ist ? ist : "(NULL)");
    }
  }
}

void test_bh_rechnung__initialize(void) {
  s_abweichungen = 0;
}

// --- Urteile ---

void test_bh_rechnung__schlafurteil(void) {
  for (int i = 0; i < ANZAHL(ORAKEL_SCHLAF); i++) {
    const OrakelSchlaf *o = &ORAKEL_SCHLAF[i];
    char fall[32];
    snprintf(fall, sizeof(fall), "%u min, Ziel %u", o->minuten, o->ziel);
    prv_gleich(fall, "Urteil", o->urteil, bh_schlafurteil(o->minuten, o->ziel));
  }
  printf("Schlafurteil: %d Faelle (Ziele 8:00, 6:30, 4:00, 12:00 h), %d Abweichungen\n",
         ANZAHL(ORAKEL_SCHLAF), s_abweichungen);
  cl_assert_equal_i(s_abweichungen, 0);
}

void test_bh_rechnung__ruhepuls(void) {
  for (int i = 0; i < ANZAHL(ORAKEL_NAECHTE); i++) {
    const OrakelNaechte *o = &ORAKEL_NAECHTE[i];
    const BhNachtTag *tage = &ORAKEL_TAGE[o->erster];
    uint16_t schnitt;
    const uint8_t naechte = bh_naechte_schnitt(tage, o->anzahl, o->heute, BhWertRuhepuls, &schnitt);
    prv_gleich(o->name, "fruehere Naechte", o->ruhe_naechte, naechte);
    prv_gleich(o->name, "Schnitt", o->ruhe_schnitt, schnitt);
    uint8_t abstand = 99;
    const BhVergleich urteil = bh_ruhepuls_urteil(o->letzte, tage, o->anzahl, o->heute, &abstand);
    prv_gleich(o->name, "Ruhepuls-Urteil", o->ruhe_urteil, urteil);
    prv_gleich(o->name, "Ruhepuls-Abstand", o->ruhe_abstand, abstand);
  }
  printf("Ruhepuls: %d Faelle, %d Abweichungen\n", ANZAHL(ORAKEL_NAECHTE), s_abweichungen);
  cl_assert_equal_i(s_abweichungen, 0);
}

// "avg" unter Resting und HRV im Night-Detail: der Schnitt erst ab drei Naechten
// (NachtAnsicht.kt:171). Die HRV kommt aus der Liste der Uhr (die HRV der laengsten Nacht MIT
// HRV); Boulders Nachtansicht nimmt die der laengsten Nacht, auch ohne HRV. Das Orakel rechnet
// beides; wie oft sie sich unterscheiden, steht im Bericht.
void test_bh_rechnung__avg(void) {
  int anders = 0;
  for (int i = 0; i < ANZAHL(ORAKEL_NAECHTE); i++) {
    const OrakelNaechte *o = &ORAKEL_NAECHTE[i];
    const BhNachtTag *tage = &ORAKEL_TAGE[o->erster];
    uint16_t schnitt;
    uint8_t n = bh_naechte_schnitt(tage, o->anzahl, o->heute, BhWertRuhepuls, &schnitt);
    prv_gleich(o->name, "avg Ruhepuls", o->avg_ruhe, n >= 3 ? schnitt : 0);
    n = bh_naechte_schnitt(tage, o->anzahl, o->heute, BhWertHrv, &schnitt);
    prv_gleich(o->name, "avg HRV", o->avg_hrv, n >= 3 ? schnitt : 0);
    if (o->avg_hrv != o->avg_hrv_boulder) {
      anders++;
      printf("  avg HRV %s: Uhr %u, Boulders Nachtansicht %u\n", o->name, o->avg_hrv,
             o->avg_hrv_boulder);
    }
  }
  printf("avg: %d Faelle, %d Abweichungen; HRV anders als Boulders Nachtansicht: %d\n",
         ANZAHL(ORAKEL_NAECHTE), s_abweichungen, anders);
  cl_assert_equal_i(s_abweichungen, 0);
}

void test_bh_rechnung__hrv_woche(void) {
  int urteile = 0;
  for (int i = 0; i < ANZAHL(ORAKEL_NAECHTE); i++) {
    const OrakelNaechte *o = &ORAKEL_NAECHTE[i];
    BhBandtag woche[BH_WOCHE_TAGE];
    memset(woche, 0xa5, sizeof(woche));
    bh_hrv_woche(&ORAKEL_TAGE[o->erster], o->anzahl, o->heute, woche);
    for (int t = 0; t < BH_WOCHE_TAGE; t++) {
      const OrakelBandtag *s = &o->woche[t];
      const BhBandtag *b = &woche[t];
      char fall[64];
      snprintf(fall, sizeof(fall), "%s Tag %d", o->name, t);
      prv_gleich(fall, "Tag", s->tag, b->tag);
      prv_gleich(fall, "Wert", s->wert, b->wert);
      prv_gleich(fall, "Band", s->unten != 0, b->band);
      prv_gleich(fall, "unten", s->unten, b->band ? b->unten : 0);
      prv_gleich(fall, "oben", s->oben, b->band ? b->oben : 0);
      prv_gleich(fall, "fruehere Werte", s->frueher, b->frueher);
      prv_gleich(fall, "urteilt", s->urteilt, bh_band_urteilt(b));
      uint16_t abstand;
      const BhVergleich u = bh_band_urteil(b, &abstand);
      prv_gleich(fall, "im Band", s->im_band, u == BhVergleichWieImmer);
      if (t == BH_WOCHE_TAGE - 1) {
        prv_gleich(o->name, "HRV-Urteil", o->hrv_urteil, u);
        prv_gleich(o->name, "HRV-Abstand", o->hrv_abstand, abstand);
        urteile += (u != BhVergleichKeiner);
      }
    }
  }
  printf("HRV-Woche: %d Faelle zu 7 Tagen, %d mit Urteil, %d Abweichungen\n",
         ANZAHL(ORAKEL_NAECHTE), urteile, s_abweichungen);
  cl_assert_equal_i(s_abweichungen, 0);
}

void test_bh_rechnung__puls_frisch(void) {
  for (int i = 0; i < ANZAHL(ORAKEL_FRISCH); i++) {
    const OrakelFrisch *o = &ORAKEL_FRISCH[i];
    prv_gleich("Puls frisch", "Alter", o->frisch, bh_puls_frisch(o->gemessen, o->jetzt));
  }
  cl_assert_equal_i(s_abweichungen, 0);
}

void test_bh_rechnung__schrittziel(void) {
  for (int i = 0; i < ANZAHL(ORAKEL_ZIEL); i++) {
    const OrakelZiel *o = &ORAKEL_ZIEL[i];
    char fall[48];
    snprintf(fall, sizeof(fall), "%lu Schritte, Ziel %lu", (unsigned long)o->schritte,
             (unsigned long)o->ziel);
    prv_gleich(fall, "erreicht", o->erreicht, bh_ziel_erreicht(o->schritte, o->ziel));
  }
  printf("Schrittziel: %d Faelle (Ziele 10 000, 7 500, 1 000, 50 000), %d Abweichungen\n",
         ANZAHL(ORAKEL_ZIEL), s_abweichungen);
  cl_assert_equal_i(s_abweichungen, 0);
}

// Der Balken bis zum Ziel. Boulder rechnet den Anteil als Float und laesst Compose runden; auf
// der Uhr sind es ganze Pixel, abgerundet. Darum hier ohne Orakel, mit festen Werten.
void test_bh_rechnung__balken(void) {
  cl_assert_equal_i(bh_balken(0, 7500, 184), 0);
  cl_assert_equal_i(bh_balken(3750, 7500, 184), 92);
  cl_assert_equal_i(bh_balken(7499, 7500, 184), 183);
  cl_assert_equal_i(bh_balken(7500, 7500, 184), 184);
  cl_assert_equal_i(bh_balken(20000, 7500, 184), 184);
  cl_assert_equal_i(bh_balken(9999, 10000, 184), 183);
  cl_assert_equal_i(bh_balken(5000, 10000, 184), 92);
  // 7 500 statt 10 000: dieselben 6 230 Schritte fuellen mehr
  cl_assert_equal_i(bh_balken(6230, 10000, 184), 114);
  cl_assert_equal_i(bh_balken(6230, 7500, 184), 152);
  // Ohne Ziel leer statt geteilt durch 0
  cl_assert_equal_i(bh_balken(6230, 0, 184), 0);
  // Viele Schritte laufen nicht ueber
  cl_assert_equal_i(bh_balken(UINT32_MAX - 1, UINT32_MAX, 60000), 59999);
}

// --- Pulszonen ---

void test_bh_rechnung__zonen(void) {
  int proben = 0;
  for (int i = 0; i < ANZAHL(ORAKEL_ZONEN); i++) {
    const OrakelZonen *o = &ORAKEL_ZONEN[i];
    char fall[48];
    snprintf(fall, sizeof(fall), "Ruhe %u, Max %u", o->median, o->max);
    uint16_t g[BH_ZONEN - 1];
    bh_puls_grenzen(o->median, o->max, g);
    for (int z = 0; z < BH_ZONEN - 1; z++) {
      prv_gleich(fall, "Grenze", o->grenzen[z], g[z]);
    }
    for (int p = 0; p < o->proben_zahl; p++, proben++) {
      prv_gleich(fall, "Zone", o->proben[p].zone, bh_puls_zone(o->proben[p].bpm, g));
    }
  }
  printf("Zonen: %d Grenzen, %d Pulse an den Grenzen, %d Abweichungen\n", ANZAHL(ORAKEL_ZONEN),
         proben, s_abweichungen);
  cl_assert_equal_i(s_abweichungen, 0);
}

// Relative Leuchtdichte nach WCAG aus den 2 Bit je Kanal der 64 Farben.
static double prv_leuchtdichte(GColor c) {
  const int kanal[3] = {c.r, c.g, c.b};
  const double gewicht[3] = {0.2126, 0.7152, 0.0722};
  double l = 0;
  for (int i = 0; i < 3; i++) {
    const double s = kanal[i] * 85 / 255.0;
    l += gewicht[i] * (s <= 0.03928 ? s / 12.92 : pow((s + 0.055) / 1.055, 2.4));
  }
  return l;
}

// V10: Jede Zonenfarbe haelt auf Weiss mindestens 3:1, keine ist Grau (das Band dahinter), und
// die vier sind verschieden. Die Farben selbst sind festgelegt, nicht aus Boulder gerechnet.
void test_bh_rechnung__zonenfarben(void) {
  const GColor soll[BH_ZONEN] = {GColorCobaltBlue, GColorJaegerGreen, GColorWindsorTan, GColorRed};
  for (int z = 0; z < BH_ZONEN; z++) {
    const GColor f = bh_zonenfarbe(z);
    cl_assert_equal_i(f.argb, soll[z].argb);
    const double kontrast = 1.05 / (prv_leuchtdichte(f) + 0.05);
    printf("Zone %d: #%02X%02X%02X, Kontrast auf Weiss %.2f:1\n", z, f.r * 85, f.g * 85, f.b * 85,
           kontrast);
    cl_assert(kontrast >= 3.0);
    cl_assert(!(f.r == f.g && f.g == f.b));
    for (int y = 0; y < z; y++) {
      cl_assert(f.argb != bh_zonenfarbe(y).argb);
    }
  }
  cl_assert_equal_i(bh_zonenfarbe(BH_ZONEN).argb, GColorLightGray.argb);
}

// --- Formate ---

// Das Orakel kennt nur GOTHIC; in LECO steht "-" statt U+2212 (Spezifikation 2.5).
static void prv_leco(const char *gothic, char *aus, size_t platz) {
  const char *minus = "\xe2\x88\x92";
  const char *wo = strstr(gothic, minus);
  if (wo) {
    snprintf(aus, platz, "%.*s-%s", (int)(wo - gothic), gothic, wo + strlen(minus));
  } else {
    snprintf(aus, platz, "%s", gothic);
  }
}

void test_bh_rechnung__formate(void) {
  char text[32];
  int faelle = 0;
  for (int i = 0; i < ANZAHL(ORAKEL_DAUER); i++, faelle++) {
    const OrakelDauer *o = &ORAKEL_DAUER[i];
    BhAngabe a;
    bh_dauer(o->minuten, &a);
    prv_text("Dauer", o->zahl, a.zahl);
    prv_text("Dauer Einheit", o->einheit, a.einheit);
    bh_stunden_minuten(o->minuten, text, sizeof(text));
    prv_text("Stunden:Minuten", o->stunden_minuten, text);
  }
  for (int i = 0; i < ANZAHL(ORAKEL_ZAHL); i++, faelle++) {
    bh_zahl(ORAKEL_ZAHL[i].zahl, BhSchriftGothic, text, sizeof(text));
    prv_text("Zahl GOTHIC", ORAKEL_ZAHL[i].text, text);
    char leco[32];
    prv_leco(ORAKEL_ZAHL[i].text, leco, sizeof(leco));
    bh_zahl(ORAKEL_ZAHL[i].zahl, BhSchriftLeco, text, sizeof(text));
    prv_text("Zahl LECO", leco, text);
  }
  for (int i = 0; i < ANZAHL(ORAKEL_VORZEICHEN); i++, faelle++) {
    bh_vorzeichen(ORAKEL_VORZEICHEN[i].zahl, text, sizeof(text));
    prv_text("Vorzeichen", ORAKEL_VORZEICHEN[i].text, text);
  }
  for (int i = 0; i < ANZAHL(ORAKEL_STRECKE); i++, faelle++) {
    const OrakelStrecke *o = &ORAKEL_STRECKE[i];
    BhAngabe a;
    bh_strecke(o->meter, o->meilen, &a);
    snprintf(text, sizeof(text), "Strecke %lu m", (unsigned long)o->meter);
    prv_text(text, o->zahl, a.zahl);
    prv_text(text, o->einheit, a.einheit);
  }
  for (int i = 0; i < ANZAHL(ORAKEL_UHRZEIT); i++, faelle++) {
    const OrakelUhrzeit *o = &ORAKEL_UHRZEIT[i];
    bh_uhrzeit(o->minute, true, text, sizeof(text));
    prv_text("Uhrzeit 24 h", o->h24, text);
    bh_uhrzeit(o->minute, false, text, sizeof(text));
    prv_text("Uhrzeit 12 h", o->h12, text);
  }
  for (int i = 0; i < ANZAHL(ORAKEL_PROZENT); i++, faelle++) {
    bh_prozent(ORAKEL_PROZENT[i].zahl, text, sizeof(text));
    prv_text("Prozent", ORAKEL_PROZENT[i].text, text);
  }
  for (int i = 0; i < ANZAHL(ORAKEL_DATUM); i++, faelle++) {
    const OrakelDatum *o = &ORAKEL_DATUM[i];
    const uint16_t tag = bh_tag(o->tm_year, o->tm_yday);
    prv_gleich(o->kurz, "Tag seit 2000", o->tag, tag);
    prv_gleich(o->kurz, "Wochentag", o->wochentag, bh_wochentag(tag));
    prv_text("Wochentag kurz", o->kurz, bh_wochentag_kurz(bh_wochentag(tag)));
  }
  for (int w = 0; w < 7; w++, faelle++) {
    prv_text("Wochentag", ORAKEL_WOCHENTAG[w], bh_wochentag_kurz(w));
  }
  prv_text("fehlt GOTHIC", ORAKEL_FEHLT, bh_fehlt(BhSchriftGothic));
  prv_text("fehlt LECO", "-", bh_fehlt(BhSchriftLeco));
  printf("Formate: %d Faelle, %d Abweichungen\n", faelle + 2, s_abweichungen);
  cl_assert_equal_i(s_abweichungen, 0);
}

// Ein zu kleiner Puffer wird abgeschnitten und endet mit 0; dahinter bleibt alles stehen.
void test_bh_rechnung__kleiner_puffer(void) {
  char puffer[8];
  memset(puffer, 'X', sizeof(puffer));
  bh_zahl(-1234567, BhSchriftGothic, puffer, 4);
  cl_assert_equal_i(strlen(puffer), 3);
  cl_assert_equal_i(puffer[4], 'X');
  memset(puffer, 'X', sizeof(puffer));
  bh_uhrzeit(1334, false, puffer, 5);
  cl_assert_equal_s(puffer, "10:1");
  cl_assert_equal_i(puffer[5], 'X');
  memset(puffer, 'X', sizeof(puffer));
  bh_vorzeichen(-3, puffer, 3);
  cl_assert_equal_i(strlen(puffer), 2);
  cl_assert_equal_i(puffer[3], 'X');
}

// Die Tage vor 2000 und nach 2179 (uint16_t) laufen nicht um.
void test_bh_rechnung__tag_am_rand(void) {
  cl_assert_equal_i(bh_tag(99, 364), 0);
  cl_assert_equal_i(bh_tag(100, 0), 0);
  cl_assert_equal_i(bh_tag(279, 157), UINT16_MAX);
  cl_assert_equal_i(bh_tag(300, 0), UINT16_MAX);
}
