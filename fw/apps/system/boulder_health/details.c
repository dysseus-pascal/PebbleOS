/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#ifdef CONFIG_BOULDER_HEALTH_APP

#include "details.h"

#include "inhalt.h"
#include "rechnung.h"
#include "texte.h"
#include "zeichnen.h"

#include "pbl/services/activity/boulder/messstufe.h"

#include <stdio.h>

#define ZEILE_SITZUNG 46
#define ZEILE_STUFE   38
#define SPO2_UNTEN    85
#define SPO2_OBEN     100

static void prv_abschnitt(GContext *ctx, int16_t breite, int16_t y, BhText titel) {
  bh_schreiben(ctx, bh_text(titel), BhSchriftG14Fett, GRect(BH_RAND, y, breite - 2 * BH_RAND, 16),
               GTextAlignmentLeft, BH_GRAU);
}

// SpO2-Punkte der Zeit [von, bis) in `r`, 85-100 %.
static void prv_spo2_punkte(GContext *ctx, GRect r, const BhDaten *d, time_t von, time_t bis) {
  bh_linie(ctx, GPoint(r.origin.x, r.origin.y + r.size.h - 1),
           GPoint(r.origin.x + r.size.w - 1, r.origin.y + r.size.h - 1), BH_HELLGRAU);
  if (bis <= von) {
    return;
  }
  for (uint32_t i = 0; i < d->spo2_anzahl; i++) {
    const Spo2Wert *w = &d->spo2[i];
    if (w->zeit < von || w->zeit >= bis) {
      continue;
    }
    const int p = (w->prozent < SPO2_UNTEN) ? SPO2_UNTEN : w->prozent;
    const int16_t x = r.origin.x + (int16_t)((w->zeit - von) * (r.size.w - 1) / (bis - von));
    const int16_t y = r.origin.y + r.size.h - 1 -
                      (int16_t)((p - SPO2_UNTEN) * (r.size.h - 1) / (SPO2_OBEN - SPO2_UNTEN));
    bh_punkt(ctx, GPoint(x, y), 2, GColorCobaltBlue);
  }
}

// Eine Zeile "Label  Wert  zusatz" im Body-Abschnitt.
static void prv_wertzeile(GContext *ctx, int16_t breite, int16_t y, BhText label, const char *wert,
                          const char *zusatz) {
  const int16_t innen = breite - 2 * BH_RAND;
  bh_schreiben(ctx, bh_text(label), BhSchriftG14, GRect(BH_RAND, y, innen, 16), GTextAlignmentLeft,
               BH_GRAU);
  bh_schreiben(ctx, wert, BhSchriftG24Fett, GRect(BH_RAND, y + 12, innen, 28), GTextAlignmentLeft,
               GColorBlack);
  if (zusatz) {
    bh_schreiben(ctx, zusatz, BhSchriftG14, GRect(BH_RAND, y + 20, innen, 16), GTextAlignmentRight,
                 BH_GRAU);
  }
}

// --- Night ---

static void prv_night(GContext *ctx, int16_t breite, const BhDaten *d) {
  const int16_t innen = breite - 2 * BH_RAND;
  if (!bh_nacht_da(d)) {
    bh_schreiben(ctx, bh_text(BhTNoNight), BhSchriftG18Fett, GRect(BH_RAND, 20, innen, 24),
                 GTextAlignmentCenter, BH_GRAU);
    return;
  }
  const NachtBild *n = &d->nacht;
  const uint8_t anzahl = (n->anzahl <= NACHT_BILD_ABSCHNITTE) ? n->anzahl : NACHT_BILD_ABSCHNITTE;
  const time_t von = (time_t)n->beginn_utc + n->abschnitte[0].von * 60;
  const time_t bis = (time_t)n->beginn_utc + n->abschnitte[anzahl - 1].bis * 60;

  prv_abschnitt(ctx, breite, 4, BhTDurchDieNacht);
  bh_nacht_block(ctx, GRect(BH_RAND, 22, innen, 48), n);
  bh_pulsband(ctx, GRect(BH_RAND, 76, innen, 40), d, von, bis, 0, 0);
  prv_spo2_punkte(ctx, GRect(BH_RAND, 120, innen, 28), d, von, bis);
  char zeit[12];
  bh_uhrzeit(bh_nacht_uhrzeit(n, n->abschnitte[0].von), d->h24, zeit, sizeof(zeit));
  bh_schreiben(ctx, zeit, BhSchriftG14, GRect(BH_RAND, 150, innen, 16), GTextAlignmentLeft,
               BH_GRAU);
  bh_uhrzeit(bh_nacht_uhrzeit(n, n->abschnitte[anzahl - 1].bis), d->h24, zeit, sizeof(zeit));
  bh_schreiben(ctx, zeit, BhSchriftG14, GRect(BH_RAND, 150, innen, 16), GTextAlignmentRight,
               BH_GRAU);

  prv_abschnitt(ctx, breite, 176, BhTKoerperImSchlaf);
  BhNachtTag tage[NAECHTE_MAX];
  const uint8_t tage_anzahl = bh_daten_nachttage(d, tage);
  const uint16_t wachtag = bh_nacht_wachtag(n);
  const char *fehlt = bh_fehlt(BhSchriftGothic);
  char wert[16], zusatz[24];
  uint16_t schnitt = 0;

  // "avg" ist der Schnitt der frueheren Naechte, wie in Boulders Nachtansicht (ab drei).
  if (n->ruhepuls) {
    snprintf(wert, sizeof(wert), "%u %s", n->ruhepuls, bh_text(BhTBpm));
  }
  const bool ruhe_avg =
      bh_naechte_schnitt(tage, tage_anzahl, wachtag, BhWertRuhepuls, &schnitt) >= 3;
  if (ruhe_avg) {
    snprintf(zusatz, sizeof(zusatz), "%s %u", bh_text(BhTAvg), schnitt);
  }
  prv_wertzeile(ctx, breite, 196, BhTResting, n->ruhepuls ? wert : fehlt, ruhe_avg ? zusatz : NULL);

  if (n->hrv) {
    snprintf(wert, sizeof(wert), "%u %s", n->hrv, bh_text(BhTMs));
  }
  const bool hrv_avg = bh_naechte_schnitt(tage, tage_anzahl, wachtag, BhWertHrv, &schnitt) >= 3;
  if (hrv_avg) {
    snprintf(zusatz, sizeof(zusatz), "%s %u", bh_text(BhTAvg), schnitt);
  }
  prv_wertzeile(ctx, breite, 236, BhTHrv, n->hrv ? wert : fehlt, hrv_avg ? zusatz : NULL);

  // SpO2 der Nacht: Mittel und tiefster Wert der Messungen zwischen Beginn und Ende.
  uint32_t summe = 0, zahl = 0;
  uint8_t tief = 0xFF;
  for (uint32_t i = 0; i < d->spo2_anzahl; i++) {
    if (d->spo2[i].zeit >= von && d->spo2[i].zeit < bis) {
      summe += d->spo2[i].prozent;
      zahl++;
      tief = (d->spo2[i].prozent < tief) ? d->spo2[i].prozent : tief;
    }
  }
  if (zahl) {
    bh_prozent((uint8_t)((summe + zahl / 2) / zahl), wert, sizeof(wert));
    char tief_text[8];
    bh_prozent(tief, tief_text, sizeof(tief_text));
    snprintf(zusatz, sizeof(zusatz), "%s %s", bh_text(BhTLowest), tief_text);
  }
  prv_wertzeile(ctx, breite, 276, BhTSpo2, zahl ? wert : fehlt, zahl ? zusatz : NULL);
}

// --- Heart ---

static void prv_heart(GContext *ctx, int16_t breite, const BhDaten *d) {
  const int16_t innen = breite - 2 * BH_RAND;
  prv_abschnitt(ctx, breite, 4, BhTZonenHeute);
  static const BhText zonen[BH_ZONEN] = {
    BhTZoneLight, BhTZoneModerate, BhTZoneVigorous, BhTZonePeak
  };
  const bool heute = d->herz_da && d->herz.zonen_tag == d->heute;
  char wert[16];
  for (int i = 0; i < BH_ZONEN; i++) {
    const int16_t y = 22 + i * 22;
    bh_rechteck(ctx, GRect(BH_RAND, y + 5, 10, 10), bh_zonenfarbe((uint8_t)i));
    bh_schreiben(ctx, bh_text(zonen[i]), BhSchriftG18, GRect(BH_RAND + 16, y, innen - 16, 22),
                 GTextAlignmentLeft, GColorBlack);
    snprintf(wert, sizeof(wert), "%u %s", heute ? d->herz.zonen_min[i] : 0, bh_text(BhTMin));
    bh_schreiben(ctx, wert, BhSchriftG18Fett, GRect(BH_RAND, y, innen, 22), GTextAlignmentRight,
                 GColorBlack);
  }

  prv_abschnitt(ctx, breite, 116, BhTSpo224h);
  prv_spo2_punkte(ctx, GRect(BH_RAND, 136, innen, 36), d, d->jetzt - 24 * 60 * 60, d->jetzt);

  // HRV der letzten sieben Wachtage mit dem persoenlichen Band (Boulders hrvWoche): Band als
  // Flaeche, Punkt gruen im Band, bernstein daneben, neutral, solange das Band nicht urteilt.
  prv_abschnitt(ctx, breite, 186, BhTHrv7);
  BhNachtTag tage[NAECHTE_MAX];
  const uint8_t anzahl = bh_daten_nachttage(d, tage);
  BhBandtag woche[BH_WOCHE_TAGE];
  bh_hrv_woche(tage, anzahl, d->heute, woche);
  uint16_t unten = 0xFFFF, oben = 0;
  for (int i = 0; i < BH_WOCHE_TAGE; i++) {
    const BhBandtag *t = &woche[i];
    if (t->wert) {
      unten = (t->wert < unten) ? t->wert : unten;
      oben = (t->wert > oben) ? t->wert : oben;
    }
    if (t->band) {
      unten = (t->unten < unten) ? t->unten : unten;
      oben = (t->oben > oben) ? t->oben : oben;
    }
  }
  const GRect r = GRect(BH_RAND, 206, innen, 56);
  const int16_t spalte = innen / BH_WOCHE_TAGE;
  if (oben > 0) {
    unten = (unten > 5) ? unten - 5 : 0;
    oben += 5;
#define PRV_Y(ms) \
  ((int16_t)(r.origin.y + r.size.h - 1 - ((ms) - unten) * (r.size.h - 1) / (oben - unten)))
    GPoint vorher = GPoint(-1, -1);
    for (int i = 0; i < BH_WOCHE_TAGE; i++) {
      const BhBandtag *t = &woche[i];
      const int16_t x = r.origin.x + i * spalte;
      if (t->band) {
        bh_rechteck(ctx, GRect(x, PRV_Y(t->oben), spalte, PRV_Y(t->unten) - PRV_Y(t->oben) + 1),
                    GColorMintGreen);
      }
    }
    for (int i = 0; i < BH_WOCHE_TAGE; i++) {
      const BhBandtag *t = &woche[i];
      if (!t->wert) {
        vorher = GPoint(-1, -1);
        continue;
      }
      const GPoint p = GPoint(r.origin.x + i * spalte + spalte / 2, PRV_Y(t->wert));
      if (vorher.x >= 0) {
        bh_linie(ctx, vorher, p, BH_GRAU);
      }
      vorher = p;
    }
    for (int i = 0; i < BH_WOCHE_TAGE; i++) {
      const BhBandtag *t = &woche[i];
      if (!t->wert) {
        continue;
      }
      const GPoint p = GPoint(r.origin.x + i * spalte + spalte / 2, PRV_Y(t->wert));
      GColor farbe = BH_GRAU;
      if (bh_band_urteilt(t)) {
        uint16_t abstand = 0;
        farbe = (bh_band_urteil(t, &abstand) == BhVergleichWieImmer) ? BH_GUT : BH_AUFFAELLIG;
      }
      bh_punkt(ctx, p, 3, farbe);
      if (i == BH_WOCHE_TAGE - 1) {
        bh_ring(ctx, p, 5, farbe);
      }
    }
#undef PRV_Y
  } else {
    bh_schreiben(ctx, bh_fehlt(BhSchriftGothic), BhSchriftG18Fett, GRect(BH_RAND, 220, innen, 24),
                 GTextAlignmentCenter, BH_GRAU);
  }
  for (int i = 0; i < BH_WOCHE_TAGE; i++) {
    bh_schreiben(ctx, bh_wochentag_kurz(woche[i].tag ? bh_wochentag(woche[i].tag) : 0),
                 (i == BH_WOCHE_TAGE - 1) ? BhSchriftG14Fett : BhSchriftG14,
                 GRect(r.origin.x + i * spalte, r.origin.y + r.size.h + 2, spalte, 16),
                 GTextAlignmentCenter, (i == BH_WOCHE_TAGE - 1) ? GColorBlack : BH_GRAU);
  }
}

// --- Sessions ---

// Art einer Sitzung fuer die Liste; Sleep und die Restful-Abschnitte stehen nicht darin.
static bool prv_sitzung_art(const ActivitySession *s, BhText *art) {
  switch (s->type) {
    case ActivitySessionType_Walk:
      *art = s->manual ? BhTWorkout : BhTWalk;
      return true;
    case ActivitySessionType_Run:
      *art = s->manual ? BhTWorkout : BhTRun;
      return true;
    // Ein offenes Training ohne "manual" ist die bestaetigte Velofahrt der Variante (kontext.c).
    case ActivitySessionType_Open:
      *art = s->manual ? BhTWorkout : BhTRide;
      return true;
    case ActivitySessionType_Nap:
      *art = BhTNap;
      return true;
    default:
      return false;
  }
}

static uint32_t prv_sitzungen_sichtbar(const BhDaten *d) {
  uint32_t n = 0;
  BhText art;
  for (uint32_t i = 0; i < d->sitzungen_anzahl; i++) {
    n += prv_sitzung_art(&d->sitzungen[i], &art) ? 1 : 0;
  }
  return n;
}

static void prv_sessions(GContext *ctx, int16_t breite, const BhDaten *d) {
  const int16_t innen = breite - 2 * BH_RAND;
  int16_t y = 4;
  char zeit[16], zeile[48], zahl[16];
  if (prv_sitzungen_sichtbar(d) == 0) {
    bh_schreiben(ctx, bh_text(BhTNoSessions), BhSchriftG18Fett, GRect(BH_RAND, y, innen, 24),
                 GTextAlignmentCenter, BH_GRAU);
    y += 32;
  }
  for (uint32_t i = 0; i < d->sitzungen_anzahl; i++) {
    const ActivitySession *s = &d->sitzungen[i];
    BhText art;
    if (!prv_sitzung_art(s, &art)) {
      continue;
    }
    bh_schreiben(ctx, bh_text(art), BhSchriftG18Fett, GRect(BH_RAND, y, innen, 22),
                 GTextAlignmentLeft, GColorBlack);
    const int64_t ort = (int64_t)s->start_utc + d->utc_zu_ort;
    bh_uhrzeit((uint16_t)((ort % (24 * 60 * 60)) / 60), d->h24, zeit, sizeof(zeit));
    bh_schreiben(ctx, zeit, BhSchriftG18, GRect(BH_RAND, y, innen, 22), GTextAlignmentRight,
                 GColorBlack);
    BhAngabe dauer;
    bh_dauer(s->length_min, &dauer);
    if (art == BhTWalk || art == BhTRun) {
      bh_zahl(s->step_data.steps, BhSchriftGothic, zahl, sizeof(zahl));
      snprintf(zeile, sizeof(zeile), "%s %s \xc2\xb7 %s %s", dauer.zahl,
               dauer.einheit ? dauer.einheit : "", zahl, bh_text(BhTSchritteWort));
    } else if (art == BhTNap) {
      snprintf(zeile, sizeof(zeile), "%s %s", dauer.zahl, dauer.einheit ? dauer.einheit : "");
    } else {
      bh_zahl(s->step_data.active_kcalories, BhSchriftGothic, zahl, sizeof(zahl));
      snprintf(zeile, sizeof(zeile), "%s %s \xc2\xb7 %s %s", dauer.zahl,
               dauer.einheit ? dauer.einheit : "", zahl, bh_text(BhTKcal));
    }
    bh_schreiben(ctx, zeile, BhSchriftG14, GRect(BH_RAND, y + 22, innen, 16), GTextAlignmentLeft,
                 BH_GRAU);
    y += ZEILE_SITZUNG;
  }

  // Der Tag: Pebbles Summen (V12).
  bh_linie(ctx, GPoint(BH_RAND, y), GPoint(BH_RAND + innen - 1, y), BH_HELLGRAU);
  BhAngabe strecke;
  bh_strecke((uint32_t)(d->meter > 0 ? d->meter : 0), d->meilen, &strecke);
  snprintf(zeile, sizeof(zeile), "%s %s \xc2\xb7 %ld %s %s", strecke.zahl,
           strecke.einheit ? strecke.einheit : "", (long)(d->aktiv_s / 60), bh_text(BhTMin),
           bh_text(BhTActive));
  bh_schreiben(ctx, zeile, BhSchriftG18, GRect(BH_RAND, y + 4, innen, 22), GTextAlignmentLeft,
               GColorBlack);
  bh_zahl(d->kcal, BhSchriftGothic, zahl, sizeof(zahl));
  snprintf(zeile, sizeof(zeile), "%s %s", zahl, bh_text(BhTKcal));
  bh_schreiben(ctx, zeile, BhSchriftG18, GRect(BH_RAND, y + 26, innen, 22), GTextAlignmentLeft,
               GColorBlack);
}

// --- Precision ---

static void prv_precision(GContext *ctx, int16_t breite, const BhDaten *d, uint8_t auswahl) {
  const GColor farbe = bh_karte_farbe(BhKarteSettings);
  for (int i = 0; i < MESSSTUFE_MAX; i++) {
    const int16_t y = 4 + i * ZEILE_STUFE;
    const bool markiert = (i == auswahl);
    if (markiert) {
      bh_rechteck(ctx, GRect(0, y, breite, ZEILE_STUFE - 2), farbe);
    }
    const GColor schrift = markiert ? GColorWhite : GColorBlack;
    bh_schreiben(ctx, BOULDER_MESSSTUFE_NAMEN[i], BhSchriftG18Fett,
                 GRect(BH_RAND, y + 7, breite - 2 * BH_RAND - 20, 22), GTextAlignmentLeft, schrift);
    if (d->lage.messstufe == i + 1) {
      // Das Haekchen gezeichnet: GOTHIC hat keins.
      const int16_t x = breite - BH_RAND - 14;
      const int16_t m = y + ZEILE_STUFE / 2;
      bh_linie(ctx, GPoint(x, m), GPoint(x + 4, m + 4), schrift);
      bh_linie(ctx, GPoint(x + 4, m + 4), GPoint(x + 12, m - 5), schrift);
      bh_linie(ctx, GPoint(x, m + 1), GPoint(x + 4, m + 5), schrift);
      bh_linie(ctx, GPoint(x + 4, m + 5), GPoint(x + 12, m - 4), schrift);
    }
  }
}

int16_t bh_detail_hoehe(BhDetail detail, const BhDaten *d) {
  switch (detail) {
    case BhDetailNight:
      return bh_nacht_da(d) ? 332 : 60;
    case BhDetailHeart:
      return 290;
    case BhDetailSessions: {
      const uint32_t n = prv_sitzungen_sichtbar(d);
      return (int16_t)(4 + (n ? n * ZEILE_SITZUNG : 32) + 56);
    }
    case BhDetailPrecision:
      return 4 + MESSSTUFE_MAX * ZEILE_STUFE;
    default:
      return 0;
  }
}

void bh_detail_zeichnen(GContext *ctx, int16_t breite, BhDetail detail, const BhDaten *d,
                        uint8_t auswahl) {
  switch (detail) {
    case BhDetailNight:
      prv_night(ctx, breite, d);
      break;
    case BhDetailHeart:
      prv_heart(ctx, breite, d);
      break;
    case BhDetailSessions:
      prv_sessions(ctx, breite, d);
      break;
    case BhDetailPrecision:
      prv_precision(ctx, breite, d, auswahl);
      break;
    default:
      break;
  }
}

#endif // CONFIG_BOULDER_HEALTH_APP
