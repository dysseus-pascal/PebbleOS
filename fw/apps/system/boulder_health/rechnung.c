/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Nur auf emery mit der Variante (siehe boulder_health.c).
#ifdef CONFIG_BOULDER_HEALTH_APP

#include "rechnung.h"

#include <stdio.h>

// Was "frueher" heisst und wann geurteilt wird, wie in Boulder (Fenster.kt:19-49).
#define VERGLEICHSNAECHTE 7  // hoechstens so viele fruehere Naechte ...
#define VERGLEICHSTAGE    8  // ... aus so vielen Tagen davor (Tagesurteile.kt:225)
#define LERNNAECHTE       3  // ab so vielen gibt es Schnitt und Ruhepuls-Urteil
#define RUHEPULS_SPANNE   2  // so weit daneben ist der Ruhepuls noch "as usual"
#define HRV_TAGE          30 // das Band eines Tags kommt aus so vielen Tagen davor
#define BAND_WERTE        3  // ab so vielen Werten steht das Band im Bild
#define BAND_URTEILT      7  // ab so vielen urteilt es
#define BAND_HALB_MIN     3  // das Band reicht mindestens so weit nach jeder Seite
#define PULS_FRISCH_S     (15 * 60)
#define RUHEPULS_ERSATZ   60 // Trainingsanalyse.kt:139

// Die Texte der Formate. Bytes statt Zeichen, weil die Quelle ASCII bleibt.
#define STRICH    "\xe2\x80\x93" // U+2013, fehlender Wert in GOTHIC
#define MINUS     "\xe2\x88\x92" // U+2212, das typografische Minus
#define PLUSMINUS "\xc2\xb1"     // U+00B1, ein Unterschied ohne Richtung

// --- Ganzzahlen ---

// Abrunden auch fuer negative Zaehler. Der Nenner ist positiv.
static int64_t prv_abrunden(int64_t zaehler, int64_t nenner) {
  const int64_t q = zaehler / nenner;
  return (zaehler % nenner != 0 && zaehler < 0) ? q - 1 : q;
}

// Math.round(zaehler / nenner) = floor(zaehler / nenner + 1/2), wie Kotlins roundToInt.
static int64_t prv_runden(int64_t zaehler, int64_t nenner) {
  return prv_abrunden(2 * zaehler + nenner, 2 * nenner);
}

// Die abgerundete Wurzel.
static uint64_t prv_wurzel(uint64_t x) {
  uint64_t r = 0;
  for (uint64_t bit = (uint64_t)1 << 62; bit != 0; bit >>= 2) {
    if (x >= r + bit) {
      x -= r + bit;
      r = (r >> 1) + bit;
    } else {
      r >>= 1;
    }
  }
  return r;
}

// --- Tage ---

// Tage vom 1.1.2000 bis zum 1.1. des Jahres: 365 je Jahr und die Schalttage der Jahre davor.
static int32_t prv_tage_vor_jahr(int32_t jahr) {
  const int32_t j = jahr - 1;
  const int32_t schalttage = (j / 4 - j / 100 + j / 400) - (1999 / 4 - 1999 / 100 + 1999 / 400);
  return 365 * (jahr - 2000) + schalttage;
}

uint16_t bh_tag(int tm_year, int tm_yday) {
  const int32_t jahr = 1900 + tm_year;
  // Vor 2000 kennt die Zaehlung keinen Tag; die Uhr steht dann ohnehin falsch.
  if (jahr < 2000) {
    return 0;
  }
  const int32_t tag = prv_tage_vor_jahr(jahr) + tm_yday;
  return tag > UINT16_MAX ? UINT16_MAX : (uint16_t)tag;
}

uint8_t bh_wochentag(uint16_t tag) {
  // Der 1.1.2000 war ein Samstag.
  return (uint8_t)((tag + 5) % 7);
}

// --- Urteile ---

BhSchlafurteil bh_schlafurteil(uint16_t minuten, uint16_t ziel_min) {
  if (minuten >= ziel_min) {
    return BhSchlafAusgeruht;
  }
  return ((int32_t)minuten >= (int32_t)ziel_min - 60) ? BhSchlafKurz : BhSchlafSehrKurz;
}

static uint16_t prv_wert(const BhNachtTag *t, BhWert wert) {
  return (wert == BhWertHrv) ? t->hrv : t->ruhepuls;
}

uint8_t bh_naechte_schnitt(const BhNachtTag *tage, uint8_t anzahl, uint16_t tag, BhWert wert,
                           uint16_t *schnitt) {
  // Boulder nimmt eine Nacht je Tag, ordnet nach der Zeit und schneidet die sieben juengsten
  // der acht Tage ab - ERST DANACH fallen die Naechte ohne Wert weg (naechteTeilen, dann
  // naechteSchnitt). Eine Nacht ohne Ruhepuls schiebt also die achte nicht nach.
  const BhNachtTag *fenster[VERGLEICHSTAGE] = {0};
  for (uint8_t i = 0; i < anzahl; i++) {
    const int32_t davor = (int32_t)tag - tage[i].tag;
    if (davor >= 1 && davor <= VERGLEICHSTAGE) {
      fenster[davor - 1] = &tage[i];
    }
  }
  uint8_t genommen = 0;
  uint8_t mit_wert = 0;
  uint32_t summe = 0;
  for (int d = 0; d < VERGLEICHSTAGE && genommen < VERGLEICHSNAECHTE; d++) {
    if (!fenster[d]) {
      continue;
    }
    genommen++;
    const uint16_t w = prv_wert(fenster[d], wert);
    if (w > 0) {
      mit_wert++;
      summe += w;
    }
  }
  *schnitt = mit_wert ? (uint16_t)prv_runden(summe, mit_wert) : 0;
  return mit_wert;
}

BhVergleich bh_ruhepuls_urteil(uint8_t letzte, const BhNachtTag *tage, uint8_t anzahl,
                               uint16_t heute, uint8_t *abstand) {
  uint16_t schnitt;
  const uint8_t naechte = bh_naechte_schnitt(tage, anzahl, heute, BhWertRuhepuls, &schnitt);
  *abstand = 0;
  if (letzte == 0 || naechte < LERNNAECHTE) {
    return BhVergleichKeiner;
  }
  // Am GERUNDETEN Schnitt, denn der steht als "avg" da (Tagesurteile.kt:119-124).
  const int32_t daneben = (int32_t)letzte - schnitt;
  *abstand = (uint8_t)(daneben < 0 ? -daneben : daneben);
  if (*abstand <= RUHEPULS_SPANNE) {
    return BhVergleichWieImmer;
  }
  return daneben > 0 ? BhVergleichHoeher : BhVergleichTiefer;
}

// Das Band aus n Werten mit Summe s und Quadratsumme q: Mittel m = s/n plus und minus
// h = max(Streuung der Stichprobe, 3), beide Raender gerundet wie Math.round.
//
// EXAKT IN GANZEN ZAHLEN statt mit sqrt: Mit D = n*q - s*s ist die Varianz V = D / (n(n-1)).
// Ein Rand k = floor(x + 1/2) wird ueber Quadrate geprueft. Boulder rechnet in double; die beiden
// treffen sich, weil eine Grenze m +- sqrt(V) + 1/2 nie naeher als etwa 1e-12 an einer ganzen Zahl
// liegt, ausser sie trifft sie genau - und dann rechnet auch double genau.
static void prv_band(uint32_t n, uint64_t s, uint64_t q, BhBandtag *aus) {
  const int64_t N = n, S = (int64_t)s;
  const int64_t D = N * (int64_t)q - S * S;
  int64_t unten, oben;
  if (D <= (int64_t)BAND_HALB_MIN * BAND_HALB_MIN * N * (N - 1)) {
    // Streuung hoechstens 3: m - 3 + 1/2 = (2s - 5n) / 2n, m + 3 + 1/2 = (2s + 7n) / 2n
    unten = prv_abrunden(2 * S - (2 * BAND_HALB_MIN - 1) * N, 2 * N);
    oben = prv_abrunden(2 * S + (2 * BAND_HALB_MIN + 1) * N, 2 * N);
  } else {
    // r <= sqrt(V) < r + 1, r >= 3. Der obere Rand ist floor(m + r + 1/2) oder eins mehr: k
    // gilt, wenn k - 1/2 - m <= sqrt(V), also mit A = 2kn - n - 2s: A^2 (n-1) <= 4nD.
    const int64_t r = (int64_t)prv_wurzel((uint64_t)(D / (N * (N - 1))));
    oben = prv_abrunden(2 * S + (2 * r + 1) * N, 2 * N) + 1;
    const int64_t a = 2 * oben * N - N - 2 * S;
    if (a * a * (N - 1) > 4 * N * D) {
      oben--;
    }
    // Der untere Rand ist floor(m - r + 1/2) oder eins weniger: k gilt, wenn
    // m + 1/2 - k >= sqrt(V), also mit B = 2s + n - 2kn: B^2 (n-1) >= 4nD.
    unten = prv_abrunden(2 * S - (2 * r - 1) * N, 2 * N);
    const int64_t b = 2 * S + N - 2 * unten * N;
    if (b * b * (N - 1) < 4 * N * D) {
      unten--;
    }
  }
  aus->unten = (uint16_t)(unten < 1 ? 1 : unten);
  aus->oben = (uint16_t)(oben > UINT16_MAX ? UINT16_MAX : oben);
  aus->band = true;
}

void bh_hrv_woche(const BhNachtTag *tage, uint8_t anzahl, uint16_t heute,
                  BhBandtag woche[BH_WOCHE_TAGE]) {
  for (int i = 0; i < BH_WOCHE_TAGE; i++) {
    BhBandtag *t = &woche[i];
    *t = (BhBandtag){.tag = (uint16_t)(heute - (BH_WOCHE_TAGE - 1 - i))};
    uint32_t n = 0;
    uint64_t summe = 0, quadrate = 0;
    for (uint8_t j = 0; j < anzahl; j++) {
      const uint16_t hrv = tage[j].hrv;
      if (hrv == 0) {
        continue;
      }
      const int32_t davor = (int32_t)t->tag - tage[j].tag;
      if (davor == 0) {
        t->wert = hrv;
      } else if (davor >= 1 && davor <= HRV_TAGE) {
        n++;
        summe += hrv;
        quadrate += (uint64_t)hrv * hrv;
      }
    }
    t->frueher = (uint8_t)n;
    if (n >= BAND_WERTE) {
      prv_band(n, summe, quadrate, t);
    }
  }
}

bool bh_band_urteilt(const BhBandtag *tag) {
  return tag->band && tag->frueher >= BAND_URTEILT;
}

BhVergleich bh_band_urteil(const BhBandtag *tag, uint16_t *abstand) {
  *abstand = 0;
  if (tag->wert == 0 || !bh_band_urteilt(tag)) {
    return BhVergleichKeiner;
  }
  // Bis zum Rand, nicht bis zur Mitte (Tagesurteile.kt:156-158).
  if (tag->wert < tag->unten) {
    *abstand = tag->unten - tag->wert;
    return BhVergleichTiefer;
  }
  if (tag->wert > tag->oben) {
    *abstand = tag->wert - tag->oben;
    return BhVergleichHoeher;
  }
  return BhVergleichWieImmer;
}

bool bh_puls_frisch(time_t gemessen, time_t jetzt) {
  return jetzt - gemessen <= PULS_FRISCH_S;
}

bool bh_ziel_erreicht(uint32_t schritte, uint32_t ziel) {
  return schritte >= ziel;
}

uint16_t bh_balken(uint32_t wert, uint32_t ziel, uint16_t laenge) {
  if (ziel == 0) {
    return 0;
  }
  if (wert >= ziel) {
    return laenge;
  }
  return (uint16_t)((uint64_t)wert * laenge / ziel);
}

// --- Pulszonen ---

void bh_puls_grenzen(uint8_t ruhepuls_median, uint16_t maxpuls, uint16_t grenzen[BH_ZONEN - 1]) {
  const int32_t ruhe = ruhepuls_median ? ruhepuls_median : RUHEPULS_ERSATZ;
  int32_t reserve = (int32_t)maxpuls - ruhe;
  if (reserve < 20) {
    reserve = 20;
  }
  // 40, 60 und 80 % der Reserve, gerundet; ein Zehntel davon ist nie genau x,5.
  static const uint8_t s_zehntel[BH_ZONEN - 1] = {4, 6, 8};
  for (int i = 0; i < BH_ZONEN - 1; i++) {
    grenzen[i] = (uint16_t)(ruhe + prv_runden(s_zehntel[i] * reserve, 10));
  }
}

uint8_t bh_puls_zone(uint16_t bpm, const uint16_t grenzen[BH_ZONEN - 1]) {
  uint8_t zone = 0;
  for (int i = 0; i < BH_ZONEN - 1; i++) {
    if (bpm >= grenzen[i]) {
      zone++;
    }
  }
  return zone;
}

// Boulders Zonenfarben fuer helle Karten (Farben.kt:164-169: blau, gruen, bernstein, rot) in
// der Palette der Uhr. Jede haelt auf Weiss mindestens 3:1 (V10); die naechsten Toene der Palette
// (MayGreen, ChromeYellow) taten es nicht. Grau ist es keine: Grau ist das Band dahinter.
GColor bh_zonenfarbe(uint8_t zone) {
  switch (zone) {
    case 0:
      return GColorCobaltBlue;
    case 1:
      return GColorJaegerGreen;
    case 2:
      return GColorWindsorTan;
    case 3:
      return GColorRed;
    default:
      return GColorLightGray;
  }
}

// --- Formate ---

const char *bh_fehlt(BhSchrift schrift) {
  return (schrift == BhSchriftLeco) ? "-" : STRICH;
}

void bh_stunden_minuten(uint16_t minuten, char *aus, size_t platz) {
  snprintf(aus, platz, "%d:%02d", minuten / 60, minuten % 60);
}

void bh_dauer(uint16_t minuten, BhAngabe *aus) {
  if (minuten >= 60) {
    bh_stunden_minuten(minuten, aus->zahl, sizeof(aus->zahl));
    aus->einheit = "h";
  } else {
    snprintf(aus->zahl, sizeof(aus->zahl), "%d", minuten);
    aus->einheit = "min";
  }
}

// Der Betrag mit Tausendertrennern, wie "%,d" auf Englisch: "6,230". Hoechstens 13 Zeichen.
static void prv_gruppiert(uint32_t betrag, char aus[16]) {
  char rueckwaerts[16];
  size_t n = 0;
  int stellen = 0;
  do {
    if (stellen > 0 && stellen % 3 == 0) {
      rueckwaerts[n++] = ',';
    }
    rueckwaerts[n++] = (char)('0' + betrag % 10);
    betrag /= 10;
    stellen++;
  } while (betrag > 0);
  size_t t = 0;
  while (n > 0) {
    aus[t++] = rueckwaerts[--n];
  }
  aus[t] = '\0';
}

static uint32_t prv_betrag(int32_t zahl) {
  return (zahl < 0) ? (uint32_t)(-(int64_t)zahl) : (uint32_t)zahl;
}

void bh_zahl(int32_t zahl, BhSchrift schrift, char *aus, size_t platz) {
  char betrag[16];
  prv_gruppiert(prv_betrag(zahl), betrag);
  const char *minus = (schrift == BhSchriftLeco) ? "-" : MINUS;
  snprintf(aus, platz, "%s%s", zahl < 0 ? minus : "", betrag);
}

void bh_vorzeichen(int32_t zahl, char *aus, size_t platz) {
  char betrag[16];
  prv_gruppiert(prv_betrag(zahl), betrag);
  const char *zeichen = (zahl > 0) ? "+" : (zahl < 0) ? MINUS : PLUSMINUS;
  snprintf(aus, platz, "%s%s", zeichen, betrag);
}

void bh_strecke(uint32_t meter, bool meilen, BhAngabe *aus) {
  // Zehntel, halbe aufgerundet wie Javas "%.1f": km = m / 1000, mi = m * 1000 / 1 609 344.
  const uint64_t zehntel =
      meilen ? ((uint64_t)meter * 20000 + 1609344) / (2 * 1609344) : ((uint64_t)meter + 50) / 100;
  snprintf(aus->zahl, sizeof(aus->zahl), "%lu.%lu", (unsigned long)(zehntel / 10),
           (unsigned long)(zehntel % 10));
  aus->einheit = meilen ? "mi" : "km";
}

void bh_uhrzeit(uint16_t minute, bool h24, char *aus, size_t platz) {
  const int stunde = (minute / 60) % 24, min = minute % 60;
  if (h24) {
    snprintf(aus, platz, "%02d:%02d", stunde, min);
  } else {
    // "h:mm a": 0 und 12 Uhr heissen 12
    const int h = (stunde % 12 == 0) ? 12 : stunde % 12;
    snprintf(aus, platz, "%d:%02d %s", h, min, stunde < 12 ? "AM" : "PM");
  }
}

void bh_prozent(uint8_t prozent, char *aus, size_t platz) {
  snprintf(aus, platz, "%d%%", prozent);
}

const char *bh_wochentag_kurz(uint8_t wochentag) {
  static const char *const s_namen[7] = {"Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"};
  return (wochentag < 7) ? s_namen[wochentag] : "";
}

#endif // CONFIG_BOULDER_HEALTH_APP
