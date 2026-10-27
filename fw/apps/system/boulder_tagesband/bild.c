/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#ifdef CONFIG_BOULDER_TAGESBAND

#include "bild.h"

#include "applib/fonts/fonts.h"
#include "applib/graphics/graphics.h"
#include "applib/graphics/text.h"
#include "apps/system/boulder_health/rechnung.h"
#include "apps/system/boulder_health/texte.h"
#include "apps/system/boulder_health/zeichnen.h"
#include "band.h"
#include "font_resource_keys.auto.h"

#include <stdio.h>

// Die Lage wie im Entwurf vom 27.10.2026: Zeit gross wie Kickstart, darunter Datum, Puls, Band,
// Stunden und eine Zeile mit drei Werten.
#define RAND     8
#define ZEIT_Y   2
#define ZEIT_H   96
#define DATUM_Y  96
#define DATUM_H  22
#define PULS_Y   122
#define PULS_H   22
#define BAND_Y   146
#define BAND_H   14
#define LABEL_Y  184
#define WERT_Y   196
#define STREIFEN 5 //!< Abstand der Schraffur des Schlafmodus

#define S_JE_TAG (24 * 60 * 60)

static void prv_text(GContext *ctx, const char *text, const char *schluessel, GRect r,
                     GTextAlignment ausrichtung, GColor farbe) {
  graphics_context_set_text_color(ctx, farbe);
  graphics_draw_text(ctx, text, fonts_get_system_font(schluessel), r,
                     GTextOverflowModeTrailingEllipsis, ausrichtung, NULL);
}

static int16_t prv_breite(const char *text, const char *schluessel) {
  return app_graphics_text_layout_get_content_size(text, fonts_get_system_font(schluessel),
                                                   GRect(0, 0, 400, ZEIT_H),
                                                   GTextOverflowModeWordWrap, GTextAlignmentLeft)
      .w;
}

// Im Schlafmodus tritt die Zeit zurueck: Sie bleibt lesbar, leuchtet aber nicht in der Nacht.
static void prv_kopf(GContext *ctx, int16_t breite, const BhDaten *d, bool schlaf) {
  char zeit[8];
  const char *ampm;
  bt_zeit(d->minute, d->h24, zeit, sizeof(zeit), &ampm);
  const GColor farbe = schlaf ? GColorLightGray : GColorWhite;
  if (!ampm) {
    prv_text(ctx, zeit, FONT_KEY_AGENCY_FB_88_NUMBERS_AM_PM, GRect(0, ZEIT_Y, breite, ZEIT_H),
             GTextAlignmentCenter, farbe);
  } else {
    // Wie Kickstart: AM/PM in der duennen Schrift gleicher Groesse direkt dahinter, beides mittig.
    const int16_t w_zeit = prv_breite(zeit, FONT_KEY_AGENCY_FB_88_NUMBERS_AM_PM);
    const int16_t w_ampm = prv_breite(ampm, FONT_KEY_AGENCY_FB_88_THIN_NUMBERS_AM_PM);
    const int16_t x = (breite - w_zeit - w_ampm) / 2;
    prv_text(ctx, zeit, FONT_KEY_AGENCY_FB_88_NUMBERS_AM_PM, GRect(x, ZEIT_Y, breite, ZEIT_H),
             GTextAlignmentLeft, farbe);
    prv_text(ctx, ampm, FONT_KEY_AGENCY_FB_88_THIN_NUMBERS_AM_PM,
             GRect(x + w_zeit, ZEIT_Y, breite, ZEIT_H), GTextAlignmentLeft, farbe);
  }
  char datum[16];
  bt_datum(d->heute, datum, sizeof(datum));
  prv_text(ctx, datum, FONT_KEY_GOTHIC_18_BOLD, GRect(0, DATUM_Y, breite, DATUM_H),
           GTextAlignmentCenter, schlaf ? GColorDarkGray : GColorLightGray);
}

static void prv_streifen(GContext *ctx, const BtBand *b, time_t von, time_t bis, GColor farbe) {
  int16_t l, r;
  if (bt_strecke(b, von, bis, &l, &r)) {
    bh_rechteck(ctx, GRect(l, BAND_Y, r - l, BAND_H), farbe);
  }
}

// Der Schlafmodus, offen bis jetzt: violett mit schraegen Streifen, damit er nicht wie eine
// gerechnete Nacht aussieht.
static void prv_schlafmodus(GContext *ctx, const BtBand *b, time_t seit) {
  int16_t l, r;
  if (!bt_strecke(b, seit, b->jetzt, &l, &r)) {
    return;
  }
  bh_rechteck(ctx, GRect(l, BAND_Y, r - l, BAND_H), GColorVividViolet);
  for (int16_t x = l - BAND_H; x < r; x += STREIFEN) {
    for (int16_t k = 0; k < BAND_H; k++) {
      if (x + k >= l && x + k < r) {
        bh_rechteck(ctx, GRect(x + k, BAND_Y + BAND_H - 1 - k, 1, 1), GColorImperialPurple);
      }
    }
  }
}

static void prv_puls(GContext *ctx, const BtBand *b, const BhDaten *d) {
  BtPulspunkt p[HERZ_HALBSTUNDEN];
  uint8_t unten, oben;
  const uint8_t anzahl = bt_pulspunkte(b, d, p, &unten, &oben);
#define PRV_Y(bpm) \
  ((int16_t)(PULS_Y + PULS_H - 1 - ((bpm) - unten) * (PULS_H - 1) / (oben - unten)))
  for (uint8_t i = 0; i < anzahl; i++) {
    const GPoint hier = GPoint(p[i].x, PRV_Y(p[i].bpm));
    if (!p[i].neu) {
      bh_linie(ctx, GPoint(p[i - 1].x, PRV_Y(p[i - 1].bpm)), hier, GColorFolly);
    } else if (i + 1 == anzahl || p[i + 1].neu) {
      // Eine Halbstunde allein zwischen Luecken: ohne Nachbarn keine Linie, also ein Punkt.
      bh_rechteck(ctx, GRect(hier.x, hier.y, 2, 2), GColorFolly);
    }
  }
#undef PRV_Y
}

static void prv_marken(GContext *ctx, const BtBand *b, bool h24) {
  BtMarke marken[BT_MARKEN_MAX];
  const uint8_t anzahl = bt_marken(b, marken);
  for (uint8_t i = 0; i < anzahl; i++) {
    const int16_t x = marken[i].x;
    bh_rechteck(ctx, GRect(x, BAND_Y + BAND_H, 1, 3), GColorLightGray);
    char text[4];
    bt_marke_text(marken[i].stunde, h24, text, sizeof(text));
    prv_text(ctx, text, FONT_KEY_GOTHIC_14, GRect(x - 12, BAND_Y + BAND_H + 1, 24, 16),
             GTextAlignmentCenter, GColorLightGray);
  }
}

static void prv_band(GContext *ctx, const BtBand *b, const BhDaten *d) {
  const int16_t jetzt_x = bt_x(b, b->jetzt);
  // Vergangenheit grau, die Zukunft offen mit gepunkteter Mitte.
  bh_rechteck(ctx, GRect(b->x0, BAND_Y, jetzt_x - b->x0, BAND_H), GColorDarkGray);
  for (int16_t x = jetzt_x + 2; x < b->x0 + b->breite; x += 4) {
    bh_rechteck(ctx, GRect(x, BAND_Y + BAND_H / 2, 2, 1), GColorDarkGray);
  }
  if (bh_nacht_da(d)) {
    const NachtBild *n = &d->nacht;
    for (uint8_t i = 0; i < n->anzahl && i < NACHT_BILD_ABSCHNITTE; i++) {
      const NachtDlsAbschnitt *a = &n->abschnitte[i];
      prv_streifen(ctx, b, (time_t)n->beginn_utc + a->von * 60, (time_t)n->beginn_utc + a->bis * 60,
                   bt_phasenfarbe(a->phase));
    }
  }
  for (uint32_t i = 0; i < d->sitzungen_anzahl && i < ACTIVITY_MAX_ACTIVITY_SESSIONS_COUNT; i++) {
    const ActivitySession *s = &d->sitzungen[i];
    const GColor farbe = bt_sitzungsfarbe(s);
    if (!gcolor_equal(farbe, GColorClear)) {
      prv_streifen(ctx, b, s->start_utc, (time_t)s->start_utc + s->length_min * 60, farbe);
    }
  }
  if (d->lage.schlafmodus_seit) {
    prv_schlafmodus(ctx, b, d->lage.schlafmodus_seit);
  }
  prv_puls(ctx, b, d);
  prv_marken(ctx, b, d->h24);
  bh_rechteck(ctx, GRect(jetzt_x, PULS_Y - 2, 2, BAND_Y + BAND_H - PULS_Y + 4), GColorWhite);
}

static void prv_spalte(GContext *ctx, int16_t breite, int i, BhText label, const char *wert,
                       GColor farbe) {
  const int16_t w = (breite - 2 * RAND) / 3;
  const int16_t x = RAND + i * w;
  prv_text(ctx, bh_text(label), FONT_KEY_GOTHIC_14, GRect(x, LABEL_Y, w, 16), GTextAlignmentCenter,
           GColorLightGray);
  prv_text(ctx, wert, FONT_KEY_GOTHIC_24_BOLD, GRect(x, WERT_Y, w, 28), GTextAlignmentCenter,
           farbe);
}

static void prv_werte(GContext *ctx, int16_t breite, const BhDaten *d) {
  char schritte[16], ruhe[8], hrv[8];
  bh_zahl(d->schritte, BhSchriftGothic, schritte, sizeof(schritte));
  uint8_t ruhepuls;
  uint16_t ms;
  bt_heute(d, &ruhepuls, &ms);
  snprintf(ruhe, sizeof(ruhe), "%u", ruhepuls);
  snprintf(hrv, sizeof(hrv), "%u", ms);
  // Ein fehlender Wert traegt nicht die Farbe seiner Groesse: Der Strich ist keine Messung.
  const char *fehlt = bh_fehlt(BhSchriftGothic);
  prv_spalte(ctx, breite, 0, BhTSteps, schritte, GColorJaegerGreen);
  prv_spalte(ctx, breite, 1, BhTResting, ruhepuls ? ruhe : fehlt,
             ruhepuls ? GColorFolly : GColorLightGray);
  prv_spalte(ctx, breite, 2, BhTHrv, ms ? hrv : fehlt, ms ? GColorPictonBlue : GColorLightGray);
}

static void prv_schlafzeile(GContext *ctx, int16_t breite, const BhDaten *d) {
  // Der Mond: ein Kreis, aus dem ein zweiter in der Farbe des Grunds schneidet.
  bh_punkt(ctx, GPoint(30, 203), 9, GColorVividViolet);
  bh_punkt(ctx, GPoint(34, 200), 9, GColorBlack);
  prv_text(ctx, bh_text(BhTSleepingSince), FONT_KEY_GOTHIC_14, GRect(48, LABEL_Y, breite - 48, 16),
           GTextAlignmentLeft, GColorLightGray);
  const time_t seit = d->lage.schlafmodus_seit;
  const int64_t ort = (int64_t)seit + bh_ortsversatz(seit);
  char zeit[12];
  bh_uhrzeit((uint16_t)((ort % S_JE_TAG) / 60), d->h24, zeit, sizeof(zeit));
  prv_text(ctx, zeit, FONT_KEY_GOTHIC_24_BOLD, GRect(48, WERT_Y, breite - 48, 28),
           GTextAlignmentLeft, GColorVividViolet);
}

void bt_zeichnen(GContext *ctx, GRect flaeche, bool verdeckt, const BhDaten *d) {
  const int16_t breite = flaeche.size.w;
  const bool schlaf = d->lage.schlafmodus_seit != 0;
  bh_rechteck(ctx, flaeche, GColorBlack);
  prv_kopf(ctx, breite, d, schlaf);
  const BtBand b = bt_band(d->jetzt, RAND, breite - 2 * RAND);
  prv_band(ctx, &b, d);
  if (verdeckt) {
    return;
  }
  if (schlaf) {
    prv_schlafzeile(ctx, breite, d);
  } else {
    prv_werte(ctx, breite, d);
  }
}

#endif // CONFIG_BOULDER_TAGESBAND
