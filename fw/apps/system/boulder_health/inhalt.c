/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#ifdef CONFIG_BOULDER_HEALTH_APP

#include "inhalt.h"

#include "rechnung.h"
#include "texte.h"
#include "zeichnen.h"

#include "pbl/services/activity/boulder/messstufe.h"

#include <stdio.h>
#include <string.h>

#define BREITE         200
#define INNEN          (BREITE - 2 * BH_RAND)
#define TAGE_1970_2000 10957
#define S_JE_TAG       (24 * 60 * 60)

// --- Gemeinsam ---

bool bh_nacht_da(const BhDaten *d) {
  return d->nacht_lesen == BoulderLesenDa && d->nacht.anzahl > 0;
}

static int64_t prv_ort(const NachtBild *n, uint16_t minute) {
  return (int64_t)n->beginn_utc + (int64_t)minute * 60 + n->utc_to_local;
}

uint16_t bh_nacht_wachtag(const NachtBild *n) {
  return (uint16_t)(prv_ort(n, n->aufwachen) / S_JE_TAG - TAGE_1970_2000);
}

uint16_t bh_nacht_uhrzeit(const NachtBild *n, uint16_t minute) {
  return (uint16_t)((prv_ort(n, minute) % S_JE_TAG) / 60);
}

GColor bh_phasenfarbe(uint8_t phase) {
  switch (phase) {
    case 0:
      return GColorChromeYellow; // wach
    case 3:
      return GColorPictonBlue; // REM
    case 2:
      return GColorOxfordBlue; // tief
    default:
      return GColorBlueMoon; // leicht, Schlaf ohne Phase
  }
}

// Bahn von oben: Awake, REM, Light, Deep. Schlaf ohne Phase liegt in Light wie in Boulder.
static int prv_bahn(uint8_t phase) {
  switch (phase) {
    case 0:
      return 0;
    case 3:
      return 1;
    case 2:
      return 3;
    default:
      return 2;
  }
}

void bh_nacht_block(GContext *ctx, GRect r, const NachtBild *n) {
  const uint8_t anzahl = (n->anzahl <= NACHT_BILD_ABSCHNITTE) ? n->anzahl : NACHT_BILD_ABSCHNITTE;
  if (anzahl == 0) {
    return;
  }
  const int32_t von = n->abschnitte[0].von;
  const int32_t bis = n->abschnitte[anzahl - 1].bis;
  if (bis <= von) {
    return;
  }
  const int16_t bahn = r.size.h / 4;
  for (int b = 1; b < 4; b++) {
    const int16_t y = r.origin.y + b * bahn;
    bh_linie(ctx, GPoint(r.origin.x, y), GPoint(r.origin.x + r.size.w - 1, y), BH_HELLGRAU);
  }
  for (uint8_t i = 0; i < anzahl; i++) {
    const NachtDlsAbschnitt *a = &n->abschnitte[i];
    const int16_t x0 = r.origin.x + (int16_t)((a->von - von) * r.size.w / (bis - von));
    const int16_t x1 = r.origin.x + (int16_t)((a->bis - von) * r.size.w / (bis - von));
    const int16_t w = (x1 > x0) ? (x1 - x0) : 1;
    bh_rechteck(ctx, GRect(x0, r.origin.y + prv_bahn(a->phase) * bahn, w, bahn - 1),
                bh_phasenfarbe(a->phase));
  }
}

void bh_grenzen(const BhDaten *d, uint16_t grenzen[BH_ZONEN - 1]) {
  if (d->herz_da && d->herz.grenzen[0]) {
    for (int i = 0; i < BH_ZONEN - 1; i++) {
      grenzen[i] = d->herz.grenzen[i];
    }
    return;
  }
  bh_puls_grenzen(d->lage.ruhepuls_median, d->lage.maxpuls, grenzen);
}

void bh_pulsband(GContext *ctx, GRect r, const BhDaten *d, time_t von, time_t bis,
                 time_t schatten_von, time_t schatten_bis) {
  if (!d->herz_da || d->herz.neueste == 0 || bis <= von) {
    return;
  }
  const BoulderHerz *h = &d->herz;
  const uint32_t erste = (uint32_t)(von / HERZ_HALBSTUNDE_S);
  const uint32_t letzte = (uint32_t)((bis - 1) / HERZ_HALBSTUNDE_S);
  const uint32_t gueltig_ab =
      (h->neueste >= HERZ_HALBSTUNDEN - 1) ? h->neueste - (HERZ_HALBSTUNDEN - 1) : 0;
  // Skala aus den Halbstunden im Bereich, mit Rand.
  int unten = 255, oben = 0;
  for (uint32_t nr = erste; nr <= letzte; nr++) {
    if (nr < gueltig_ab || nr > h->neueste) {
      continue;
    }
    const uint8_t p = nr % HERZ_HALBSTUNDEN;
    if (h->n[p]) {
      unten = (h->min[p] < unten) ? h->min[p] : unten;
      oben = (h->max[p] > oben) ? h->max[p] : oben;
    }
  }
  if (oben == 0) {
    return;
  }
  unten = (unten > 5) ? unten - 5 : 0;
  oben += 5;
  const int32_t spanne_s = (int32_t)(bis - von);
  const int16_t saeule = (int16_t)(r.size.w * HERZ_HALBSTUNDE_S / spanne_s);
  if (schatten_von && schatten_bis > schatten_von) {
    const time_t a = (schatten_von > von) ? schatten_von : von;
    const time_t b = (schatten_bis < bis) ? schatten_bis : bis;
    if (b > a) {
      const int16_t x0 = r.origin.x + (int16_t)((a - von) * r.size.w / spanne_s);
      const int16_t x1 = r.origin.x + (int16_t)((b - von) * r.size.w / spanne_s);
      // Die Nacht als Streifen unter dem Band in der Farbe von Night: Eine Flaeche hinter den
      // grauen Saeulen nahm ihnen den Kontrast.
      bh_rechteck(ctx, GRect(x0, r.origin.y + r.size.h + 2, x1 - x0, 3), GColorIndigo);
    }
  }
  uint16_t grenzen[BH_ZONEN - 1];
  bh_grenzen(d, grenzen);
  GPoint vorher = GPoint(-1, -1);
#define PRV_Y(bpm) \
  ((int16_t)(r.origin.y + r.size.h - 1 - ((bpm) - unten) * (r.size.h - 1) / (oben - unten)))
  for (uint32_t nr = erste; nr <= letzte; nr++) {
    const uint8_t p = nr % HERZ_HALBSTUNDEN;
    if (nr < gueltig_ab || nr > h->neueste || h->n[p] == 0) {
      vorher = GPoint(-1, -1);
      continue;
    }
    const int16_t x =
        r.origin.x + (int16_t)(((time_t)nr * HERZ_HALBSTUNDE_S - von) * r.size.w / spanne_s);
    const int16_t y_max = PRV_Y(h->max[p]);
    const int16_t y_min = PRV_Y(h->min[p]);
    bh_rechteck(ctx, GRect(x, y_max, (saeule > 1) ? saeule - 1 : 1, y_min - y_max + 1),
                BH_HELLGRAU);
    const GPoint jetzt = GPoint(x + saeule / 2, PRV_Y(h->mittel[p]));
    const GColor farbe = bh_zonenfarbe(bh_puls_zone(h->mittel[p], grenzen));
    if (vorher.x >= 0) {
      bh_linie(ctx, vorher, jetzt, farbe);
    }
    bh_punkt(ctx, jetzt, 1, farbe);
    vorher = jetzt;
  }
#undef PRV_Y
}

// --- Bausteine der Karten ---

static GColor prv_urteilsfarbe(bool gut) {
  return gut ? BH_GUT : BH_AUFFAELLIG;
}

// Eine Zeile der Karte Today: Label oben, Wert gross links, Urteil rechts.
static void prv_zeile(GContext *ctx, int16_t y, BhText label, const char *zahl, const char *einheit,
                      const char *urteil, bool gut) {
  bh_schreiben(ctx, bh_text(label), BhSchriftG14, GRect(BH_RAND, y - 2, INNEN, 16),
               GTextAlignmentLeft, BH_GRAU);
  bh_zahl_mit_einheit(ctx, zahl, einheit, BhSchriftL26, GPoint(BH_RAND, y + 12), GColorBlack);
  if (urteil) {
    bh_schreiben(ctx, urteil, BhSchriftG18Fett, GRect(BH_RAND, y + 18, INNEN, 22),
                 GTextAlignmentRight, prv_urteilsfarbe(gut));
  }
}

static void prv_balken(GContext *ctx, GRect r, uint32_t wert, uint32_t ziel, GColor farbe) {
  bh_rechteck(ctx, r, BH_HELLGRAU);
  const uint16_t voll = bh_balken(wert, ziel, (uint16_t)r.size.w);
  if (voll) {
    bh_rechteck(ctx, GRect(r.origin.x, r.origin.y, voll, r.size.h), farbe);
  }
}

static const char *prv_schlafurteil(BhSchlafurteil u) {
  switch (u) {
    case BhSchlafAusgeruht:
      return bh_text(BhTWellRested);
    case BhSchlafKurz:
      return bh_text(BhTShortNight);
    default:
      return bh_text(BhTVeryShortNight);
  }
}

// Ruhepuls-Urteil als Text: "as usual", "+3" oder "−3"; NULL ohne Urteil.
static const char *prv_ruhe_text(BhVergleich v, uint8_t abstand, char *puffer, size_t platz) {
  switch (v) {
    case BhVergleichWieImmer:
      return bh_text(BhTAsUsual);
    case BhVergleichHoeher:
      bh_vorzeichen(abstand, puffer, platz);
      return puffer;
    case BhVergleichTiefer:
      bh_vorzeichen(-(int32_t)abstand, puffer, platz);
      return puffer;
    default:
      return NULL;
  }
}

static const char *prv_band_text(BhVergleich v) {
  switch (v) {
    case BhVergleichWieImmer:
      return bh_text(BhTInRange);
    case BhVergleichHoeher:
      return bh_text(BhTAbove);
    case BhVergleichTiefer:
      return bh_text(BhTBelow);
    default:
      return NULL;
  }
}

static void prv_tracking_aus(GContext *ctx, GRect flaeche) {
  bh_schreiben(ctx, bh_text(BhTTrackingOff), BhSchriftG18Fett,
               GRect(BH_RAND, flaeche.size.h / 2 - 12, INNEN, 24), GTextAlignmentCenter, BH_GRAU);
}

// --- Today ---

static void prv_today(GContext *ctx, const BhDaten *d) {
  char zahl[16], urteil[16];
  const char *fehlt = bh_fehlt(BhSchriftLeco);
  const bool heute = bh_nacht_da(d) && bh_nacht_wachtag(&d->nacht) == d->heute;
  BhNachtTag tage[NAECHTE_MAX];
  const uint8_t anzahl = bh_daten_nachttage(d, tage);

  // Sleep
  if (heute) {
    BhAngabe dauer;
    bh_dauer(d->nacht.schlaf_minuten, &dauer);
    const BhSchlafurteil u = bh_schlafurteil(d->nacht.schlaf_minuten, d->lage.ziel_schlaf_min);
    prv_zeile(ctx, 32, BhTSleep, dauer.zahl, dauer.einheit, prv_schlafurteil(u),
              u == BhSchlafAusgeruht);
  } else {
    prv_zeile(ctx, 32, BhTSleep, fehlt, NULL, NULL, false);
  }

  // Resting
  if (heute && d->nacht.ruhepuls) {
    snprintf(zahl, sizeof(zahl), "%u", d->nacht.ruhepuls);
    uint8_t abstand = 0;
    const BhVergleich v = bh_ruhepuls_urteil(d->nacht.ruhepuls, tage, anzahl, d->heute, &abstand);
    prv_zeile(ctx, 76, BhTResting, zahl, bh_text(BhTBpm),
              prv_ruhe_text(v, abstand, urteil, sizeof(urteil)), v == BhVergleichWieImmer);
  } else {
    prv_zeile(ctx, 76, BhTResting, fehlt, NULL, NULL, false);
  }

  // HRV: nur mit der Nacht von heute, wie Sleep und Resting. Der Wert des Wachtags kommt aus der
  // Liste (laengste Nacht mit HRV, wie Boulder), sonst aus der Nacht.
  BhBandtag woche[BH_WOCHE_TAGE];
  bh_hrv_woche(tage, anzahl, d->heute, woche);
  const uint16_t hrv =
      !heute ? 0 : (woche[BH_WOCHE_TAGE - 1].wert ? woche[BH_WOCHE_TAGE - 1].wert : d->nacht.hrv);
  if (hrv) {
    snprintf(zahl, sizeof(zahl), "%u", hrv);
    const char *u = NULL;
    BhVergleich v = BhVergleichKeiner;
    if (woche[BH_WOCHE_TAGE - 1].wert && bh_band_urteilt(&woche[BH_WOCHE_TAGE - 1])) {
      uint16_t abstand = 0;
      v = bh_band_urteil(&woche[BH_WOCHE_TAGE - 1], &abstand);
      u = prv_band_text(v);
    }
    prv_zeile(ctx, 120, BhTHrv, zahl, bh_text(BhTMs), u, v == BhVergleichWieImmer);
  } else {
    prv_zeile(ctx, 120, BhTHrv, fehlt, NULL, NULL, false);
  }

  // Steps
  bh_zahl(d->schritte, BhSchriftLeco, zahl, sizeof(zahl));
  prv_zeile(ctx, 164, BhTSteps, zahl, NULL, NULL, false);
  prv_balken(ctx, GRect(BH_RAND, 210, INNEN, 6), (uint32_t)d->schritte, d->lage.ziel_schritte,
             bh_karte_farbe(BhKarteActivity));
}

// --- Night ---

static void prv_night(GContext *ctx, GRect flaeche, const BhDaten *d) {
  if (!bh_nacht_da(d)) {
    bh_schreiben(
        ctx,
        (d->nacht_lesen == BoulderLesenSpaeter) ? bh_fehlt(BhSchriftGothic) : bh_text(BhTNoNight),
        BhSchriftG18Fett, GRect(BH_RAND, flaeche.size.h / 2 - 12, INNEN, 24), GTextAlignmentCenter,
        BH_GRAU);
    return;
  }
  const NachtBild *n = &d->nacht;
  BhAngabe dauer;
  bh_dauer(n->schlaf_minuten, &dauer);
  bh_zahl_mit_einheit(ctx, dauer.zahl, dauer.einheit, BhSchriftL38, GPoint(BH_RAND, 30),
                      GColorBlack);
  if (n->fertig) {
    const BhSchlafurteil u = bh_schlafurteil(n->schlaf_minuten, d->lage.ziel_schlaf_min);
    bh_schreiben(ctx, prv_schlafurteil(u), BhSchriftG18Fett, GRect(BH_RAND, 50, INNEN, 22),
                 GTextAlignmentRight, prv_urteilsfarbe(u == BhSchlafAusgeruht));
  } else {
    bh_schreiben(ctx, bh_text(BhTSoFar), BhSchriftG18Fett, GRect(BH_RAND, 50, INNEN, 22),
                 GTextAlignmentRight, BH_GRAU);
  }

  char von[12], bis[12], zeile[32];
  bh_uhrzeit(bh_nacht_uhrzeit(n, n->einschlafen), d->h24, von, sizeof(von));
  bh_uhrzeit(bh_nacht_uhrzeit(n, n->aufwachen), d->h24, bis, sizeof(bis));
  snprintf(zeile, sizeof(zeile), "%s \xe2\x80\x93 %s", von, bis);
  bh_schreiben(ctx, zeile, BhSchriftG18, GRect(BH_RAND, 78, INNEN, 22), GTextAlignmentLeft,
               GColorBlack);

  bh_nacht_block(ctx, GRect(BH_RAND, 106, INNEN, 60), n);

  // Vier Spalten Awake, REM, Light, Deep; Schlaf ohne Phase hat keine Spalte. Ohne REM, Light und
  // Deep entfaellt die Zeile (wie Boulders Nachtansicht).
  if (n->minuten[1] + n->minuten[2] + n->minuten[3] == 0) {
    return;
  }
  static const struct {
    BhText label;
    uint8_t phase;
  } spalten[] = {{BhTAwake, 0}, {BhTRem, 3}, {BhTLight, 1}, {BhTDeep, 2}};
  const int16_t breite = INNEN / 4;
  for (int i = 0; i < 4; i++) {
    const int16_t x = BH_RAND + i * breite;
    // Die Farbe als Strich ueber dem Label: neben "Awake" haette ein Quadrat keinen Platz.
    bh_rechteck(ctx, GRect(x, 174, breite - 6, 3), bh_phasenfarbe(spalten[i].phase));
    bh_schreiben(ctx, bh_text(spalten[i].label), BhSchriftG14, GRect(x, 176, breite, 16),
                 GTextAlignmentLeft, BH_GRAU);
    bh_stunden_minuten(n->minuten[spalten[i].phase], zeile, sizeof(zeile));
    bh_schreiben(ctx, zeile, BhSchriftG18Fett, GRect(x, 192, breite, 22), GTextAlignmentLeft,
                 GColorBlack);
  }
}

// --- Heart ---

// Die drei Spalten unten: Label klein, Wert fett.
static void prv_spalte(GContext *ctx, int i, BhText label, const char *wert) {
  const int16_t breite = INNEN / 3;
  const int16_t x = BH_RAND + i * breite;
  bh_schreiben(ctx, bh_text(label), BhSchriftG14, GRect(x, 160, breite, 16), GTextAlignmentLeft,
               BH_GRAU);
  bh_schreiben(ctx, wert, BhSchriftG24Fett, GRect(x, 176, breite, 28), GTextAlignmentLeft,
               GColorBlack);
}

static void prv_heart(GContext *ctx, const BhDaten *d) {
  char zahl[16], alter[24];
  if (d->puls > 0) {
    snprintf(zahl, sizeof(zahl), "%ld", (long)d->puls);
    bh_zahl_mit_einheit(ctx, zahl, bh_text(BhTBpm), BhSchriftL38, GPoint(BH_RAND, 30), GColorBlack);
    if (bh_puls_frisch(d->puls_zeit, d->jetzt)) {
      bh_schreiben(ctx, bh_text(BhTNow), BhSchriftG18Fett, GRect(BH_RAND, 50, INNEN, 22),
                   GTextAlignmentRight, BH_GUT);
    } else {
      BhAngabe a;
      const time_t s = (d->jetzt > d->puls_zeit) ? d->jetzt - d->puls_zeit : 0;
      bh_dauer((uint16_t)((s / 60 < 0xFFFF) ? s / 60 : 0xFFFF), &a);
      snprintf(alter, sizeof(alter), "%s %s", a.zahl, a.einheit ? a.einheit : "");
      bh_schreiben(ctx, alter, BhSchriftG18Fett, GRect(BH_RAND, 50, INNEN, 22), GTextAlignmentRight,
                   BH_GRAU);
    }
  } else {
    bh_schreiben(ctx, bh_fehlt(BhSchriftLeco), BhSchriftL38, GRect(BH_RAND, 30, INNEN, 50),
                 GTextAlignmentLeft, GColorBlack);
  }

  // Pulsband der letzten 24 h, die Nacht hinterlegt.
  time_t schatten_von = 0, schatten_bis = 0;
  if (bh_nacht_da(d)) {
    schatten_von = (time_t)d->nacht.beginn_utc + d->nacht.einschlafen * 60;
    schatten_bis = (time_t)d->nacht.beginn_utc + d->nacht.aufwachen * 60;
  }
  bh_pulsband(ctx, GRect(BH_RAND, 86, INNEN, 64), d, d->jetzt - S_JE_TAG, d->jetzt, schatten_von,
              schatten_bis);

  const char *fehlt = bh_fehlt(BhSchriftGothic);
  char ruhe[12], hrv[12], spo2[12];
  const bool nacht = bh_nacht_da(d);
  if (nacht && d->nacht.ruhepuls) {
    snprintf(ruhe, sizeof(ruhe), "%u", d->nacht.ruhepuls);
  }
  if (nacht && d->nacht.hrv) {
    snprintf(hrv, sizeof(hrv), "%u ms", d->nacht.hrv);
  }
  if (d->spo2_anzahl) {
    bh_prozent(d->spo2[d->spo2_anzahl - 1].prozent, spo2, sizeof(spo2));
  }
  prv_spalte(ctx, 0, BhTResting, (nacht && d->nacht.ruhepuls) ? ruhe : fehlt);
  prv_spalte(ctx, 1, BhTHrv, (nacht && d->nacht.hrv) ? hrv : fehlt);
  prv_spalte(ctx, 2, BhTSpo2,
             // "off" nur mit Lage: Vor der ersten Lage heisst 0 "unbekannt", nicht "aus".
             (d->lage_da && d->lage.spo2_takt_min == 0) ? bh_text(BhTOff)
                                                        : (d->spo2_anzahl ? spo2 : fehlt));
}

// --- Activity ---

static void prv_activity(GContext *ctx, const BhDaten *d) {
  char zahl[16], zeile[48];
  bh_zahl(d->schritte, BhSchriftLeco, zahl, sizeof(zahl));
  bh_zahl_mit_einheit(ctx, zahl, NULL, BhSchriftL38, GPoint(BH_RAND, 30), GColorBlack);
  prv_balken(ctx, GRect(BH_RAND, 80, INNEN, 6), (uint32_t)d->schritte, d->lage.ziel_schritte,
             bh_karte_farbe(BhKarteActivity));

  BhAngabe strecke;
  bh_strecke((uint32_t)(d->meter > 0 ? d->meter : 0), d->meilen, &strecke);
  snprintf(zeile, sizeof(zeile), "%s %s \xc2\xb7 %ld %s %s", strecke.zahl,
           strecke.einheit ? strecke.einheit : "", (long)(d->aktiv_s / 60), bh_text(BhTMin),
           bh_text(BhTActive));
  bh_schreiben(ctx, zeile, BhSchriftG18, GRect(BH_RAND, 92, INNEN, 22), GTextAlignmentLeft,
               GColorBlack);

  // Die Woche: sechs Vortage aus S7 und heute, heute rechts.
  uint32_t werte[7];
  uint32_t groesste = d->lage.ziel_schritte;
  for (int i = 0; i < 7; i++) {
    const uint16_t tag = (uint16_t)(d->heute - (6 - i));
    werte[i] = WOCHE_KEIN_WERT;
    if (i == 6) {
      werte[i] = (uint32_t)(d->schritte > 0 ? d->schritte : 0);
    } else if (d->woche_da && d->woche.tag && d->woche.tag >= tag &&
               d->woche.tag - tag < WOCHE_VORTAGE) {
      werte[i] = d->woche.schritte[d->woche.tag - tag];
    }
    if (werte[i] != WOCHE_KEIN_WERT && werte[i] > groesste) {
      groesste = werte[i];
    }
  }
  const GRect saeulen = GRect(BH_RAND, 124, INNEN, 76);
  const int16_t breite = INNEN / 7;
  for (int i = 0; i < 7; i++) {
    const int16_t x = BH_RAND + i * breite;
    if (werte[i] != WOCHE_KEIN_WERT && groesste) {
      const int16_t h = (int16_t)((uint64_t)werte[i] * saeulen.size.h / groesste);
      if (h > 0) {
        const GColor farbe =
            (i == 6) ? bh_karte_farbe(BhKarteActivity)
                     : (bh_ziel_erreicht(werte[i], d->lage.ziel_schritte) ? GColorMayGreen
                                                                          : BH_HELLGRAU);
        bh_rechteck(ctx, GRect(x + 3, saeulen.origin.y + saeulen.size.h - h, breite - 6, h), farbe);
      }
    }
    bh_schreiben(ctx, bh_wochentag_kurz(bh_wochentag((uint16_t)(d->heute - (6 - i)))),
                 (i == 6) ? BhSchriftG14Fett : BhSchriftG14,
                 GRect(x, saeulen.origin.y + saeulen.size.h + 2, breite, 16), GTextAlignmentCenter,
                 (i == 6) ? GColorBlack : BH_GRAU);
  }
  if (groesste) {
    const int16_t y = saeulen.origin.y + saeulen.size.h -
                      (int16_t)((uint64_t)d->lage.ziel_schritte * saeulen.size.h / groesste);
    bh_linie(ctx, GPoint(BH_RAND, y), GPoint(BH_RAND + INNEN - 1, y), GColorBlack);
  }
}

// --- Settings ---

static void prv_settings(GContext *ctx, const BhDaten *d) {
  bh_schreiben(ctx, bh_text(BhTPrecision), BhSchriftG18Fett, GRect(BH_RAND, 36, INNEN, 22),
               GTextAlignmentLeft, BH_GRAU);
  const uint8_t stufe = d->lage.messstufe;
  const bool gueltig = stufe >= 1 && stufe <= MESSSTUFE_MAX;
  bh_schreiben(ctx, gueltig ? BOULDER_MESSSTUFE_NAMEN[stufe - 1] : bh_fehlt(BhSchriftGothic),
               BhSchriftG24Fett, GRect(BH_RAND, 58, INNEN, 30), GTextAlignmentLeft, GColorBlack);
  for (int i = 0; i < MESSSTUFE_MAX; i++) {
    const GPoint mitte = GPoint(BH_RAND + 6 + i * 16, 104);
    if (gueltig && i < stufe) {
      bh_punkt(ctx, mitte, 5, bh_karte_farbe(BhKarteSettings));
    } else {
      bh_ring(ctx, mitte, 5, BH_GRAU);
    }
  }
}

void bh_inhalt_zeichnen(GContext *ctx, GRect flaeche, BhKarte karte, const BhDaten *d) {
  if (!d->lage.tracking && karte != BhKarteSettings) {
    prv_tracking_aus(ctx, flaeche);
    return;
  }
  switch (karte) {
    case BhKarteToday:
      prv_today(ctx, d);
      break;
    case BhKarteNight:
      prv_night(ctx, flaeche, d);
      break;
    case BhKarteHeart:
      prv_heart(ctx, d);
      break;
    case BhKarteActivity:
      prv_activity(ctx, d);
      break;
    case BhKarteSettings:
      prv_settings(ctx, d);
      break;
    default:
      break;
  }
}

#endif // CONFIG_BOULDER_HEALTH_APP
