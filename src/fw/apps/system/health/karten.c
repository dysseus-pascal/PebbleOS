/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "karten.h"

// Gebaut wie Kieselsport: weisser Grund, oben ein Balken in der Farbe der
// Karte, eine grosse Zahl in LECO, darunter die kleinen. Pebbles Health-App
// zeigte vieles, was die Uhr nur schaetzte; hier steht, was sie misst.

// --- Texte: Deutsch, sonst Englisch ---

typedef enum {
  T_HEUTE, T_SCHLAF, T_HERZ, T_SCHRITTE, T_KM, T_KCAL, T_AKTIV_MIN,
  T_KEINE_NACHT, T_LAEUFT, T_WACH, T_REM, T_LEICHT, T_TIEF, T_BPM, T_JETZT, T_VOR_MIN,
  T_RUHE, T_HRV, T_SPO2, T_NACHT_TIEF, T_24H, T_AUS, T_ANZAHL,
} Text;

static const char *const s_texte[T_ANZAHL][2] = {
  [T_HEUTE] = { "Today", "Heute" },
  [T_SCHLAF] = { "Sleep", "Schlaf" },
  [T_HERZ] = { "Heart & SpO2", "Herz & SpO2" },
  [T_SCHRITTE] = { "steps", "Schritte" },
  [T_KM] = { "km", "km" },
  [T_KCAL] = { "kcal", "kcal" },
  [T_AKTIV_MIN] = { "active min", "aktive Min" },
  [T_KEINE_NACHT] = { "No night measured yet", "Noch keine Nacht gemessen" },
  [T_LAEUFT] = { "ongoing", "laeuft" },
  [T_WACH] = { "Awake", "Wach" },
  [T_REM] = { "REM", "REM" },
  [T_LEICHT] = { "Light", "Leicht" },
  [T_TIEF] = { "Deep", "Tief" },
  [T_BPM] = { "bpm", "bpm" },
  [T_JETZT] = { "now", "jetzt" },
  [T_VOR_MIN] = { "%d min ago", "vor %d min" },
  [T_RUHE] = { "resting", "Ruhe" },
  [T_HRV] = { "HRV ms", "HRV ms" },
  [T_SPO2] = { "SpO2 %", "SpO2 %" },
  [T_NACHT_TIEF] = { "SpO2 night %d %%", "SpO2 Nacht %d %%" },
  [T_24H] = { "last 24 h", "letzte 24 h" },
  [T_AUS] = { "off", "aus" },
};

static int s_de;

void karten_sprache(void) {
  const char *locale = app_get_system_locale();
  s_de = (locale && strncmp(locale, "de", 2) == 0) ? 1 : 0;
}

static const char *T(Text t) {
  return s_texte[t][s_de];
}

// --- Farben ---

static GColor prv_kartenfarbe(Karte k) {
  switch (k) {
    case KarteHeute: return GColorOrange;
    case KarteSchlaf: return GColorIndigo;
    default: return GColorFolly;
  }
}

// DIE PHASEN WIE BEI FITBIT UND IN BOULDER: wach warm, Schlaf in Blau, je
// tiefer desto dunkler.
static GColor prv_phasenfarbe(uint8_t phase) {
  switch (phase) {
    case SchlafPhaseWach: return GColorChromeYellow;
    case SchlafPhaseRem: return GColorPictonBlue;
    case SchlafPhaseLeicht: return GColorBlueMoon;
    case SchlafPhaseTief: return GColorOxfordBlue;
    default: return GColorLightGray;
  }
}

// --- Bausteine ---

#define BALKEN_H 30
#define RAND 8

static void prv_text(GContext *ctx, const char *text, const char *schrift, GRect wo,
                     GTextAlignment wie, GColor farbe) {
  graphics_context_set_text_color(ctx, farbe);
  graphics_draw_text(ctx, text, fonts_get_system_font(schrift), wo,
                     GTextOverflowModeTrailingEllipsis, wie, NULL);
}

static void prv_balken(GContext *ctx, GRect b, Karte k) {
  const GColor grund = prv_kartenfarbe(k);
  graphics_context_set_fill_color(ctx, grund);
  graphics_fill_rect(ctx, GRect(0, 0, b.size.w, BALKEN_H), 0, GCornerNone);
  const Text titel[] = { T_HEUTE, T_SCHLAF, T_HERZ };
  prv_text(ctx, T(titel[k]), FONT_KEY_GOTHIC_24_BOLD, GRect(0, 0, b.size.w, BALKEN_H),
           GTextAlignmentCenter, gcolor_legible_over(grund));
}

// Die Seiten als Striche unten, die aktuelle dunkel - wie in Kieselsport.
static void prv_striche(GContext *ctx, GRect b, Karte k) {
  const int16_t sb = 12, luecke = 4, sh = 3;
  const int16_t gesamt = KartenAnzahl * sb + (KartenAnzahl - 1) * luecke;
  int16_t x = (b.size.w - gesamt) / 2;
  for (int i = 0; i < KartenAnzahl; i++) {
    graphics_context_set_fill_color(ctx, i == (int)k ? GColorBlack : GColorLightGray);
    graphics_fill_rect(ctx, GRect(x, b.size.h - 8, sb, sh), 0, GCornerNone);
    x += sb + luecke;
  }
}

// Eine grosse Zahl mit ihrem Namen darunter.
static void prv_gross(GContext *ctx, GRect b, int16_t y, const char *zahl, const char *name) {
  prv_text(ctx, zahl, FONT_KEY_LECO_42_NUMBERS, GRect(0, y - 6, b.size.w, 50),
           GTextAlignmentCenter, GColorBlack);
  prv_text(ctx, name, FONT_KEY_GOTHIC_18, GRect(0, y + 40, b.size.w, 22),
           GTextAlignmentCenter, GColorDarkGray);
}

// Drei kleine Felder nebeneinander: Name oben, Zahl darunter.
static void prv_drei(GContext *ctx, GRect b, int16_t y, const char *namen[3],
                     const char *zahlen[3]) {
  const int16_t breite = (b.size.w - 2 * RAND) / 3;
  for (int i = 0; i < 3; i++) {
    const GRect feld = GRect(RAND + i * breite, y, breite, 50);
    prv_text(ctx, namen[i], FONT_KEY_GOTHIC_14, GRect(feld.origin.x, y, breite, 18),
             GTextAlignmentCenter, GColorDarkGray);
    prv_text(ctx, zahlen[i], FONT_KEY_GOTHIC_24_BOLD, GRect(feld.origin.x, y + 12, breite, 30),
             GTextAlignmentCenter, GColorBlack);
  }
}

static void prv_dauer(char *aus, size_t platz, int minuten) {
  snprintf(aus, platz, "%d:%02d", minuten / 60, minuten % 60);
}

// --- Heute ---

static void prv_heute(GContext *ctx, GRect b, const HealthDaten *d) {
  char zahl[16];
  snprintf(zahl, sizeof(zahl), "%d", (int)d->schritte);
  prv_gross(ctx, b, BALKEN_H + 8, zahl, T(T_SCHRITTE));

  char km[24], kcal[16], aktiv[16];
  snprintf(km, sizeof(km), "%d.%02d", (int)(d->meter / 1000), (int)((d->meter % 1000) / 10));
  snprintf(kcal, sizeof(kcal), "%d", (int)d->kcal);
  snprintf(aktiv, sizeof(aktiv), "%d", (int)d->aktiv_min);
  const char *namen[3] = { T(T_KM), T(T_KCAL), T(T_AKTIV_MIN) };
  const char *zahlen[3] = { km, kcal, aktiv };
  prv_drei(ctx, b, BALKEN_H + 72, namen, zahlen);

  // DER TAG IN STUNDEN: wann man sich bewegt hat. Die laufende Stunde traegt
  // die Farbe der Karte, die vergangenen sind dunkel.
  const int16_t oben = BALKEN_H + 128, hoehe = b.size.h - oben - 30;
  const int16_t breite = (b.size.w - 2 * RAND) / 24;
  const int16_t x0 = (b.size.w - 24 * breite) / 2;
  uint16_t hoechst = 1;
  for (int h = 0; h < 24; h++) {
    if (d->schritte_stunde[h] > hoechst) hoechst = d->schritte_stunde[h];
  }
  graphics_context_set_stroke_color(ctx, GColorLightGray);
  graphics_draw_line(ctx, GPoint(x0, oben + hoehe), GPoint(x0 + 24 * breite - 1, oben + hoehe));
  for (int h = 0; h < 24; h++) {
    const int16_t s = (int16_t)((int32_t)d->schritte_stunde[h] * hoehe / hoechst);
    if (s == 0) continue;
    graphics_context_set_fill_color(ctx, h == d->stunde_jetzt ? GColorOrange : GColorDarkGray);
    graphics_fill_rect(ctx, GRect(x0 + h * breite, oben + hoehe - s, breite - 1, s), 0,
                       GCornerNone);
  }
  static const char *const s_marken[] = { "0", "6", "12", "18" };
  for (int i = 0; i < 4; i++) {
    prv_text(ctx, s_marken[i], FONT_KEY_GOTHIC_14,
             GRect(x0 + i * 6 * breite - 10, oben + hoehe, 20, 16), GTextAlignmentCenter,
             GColorDarkGray);
  }
}

// --- Schlaf ---

static int prv_uhrzeit(const NachtBild *n, uint16_t minute) {
  return (int)((((int64_t)n->beginn_utc + n->utc_to_local) / 60 + minute) % (24 * 60));
}

static void prv_schlaf(GContext *ctx, GRect b, const HealthDaten *d) {
  if (!d->nacht_da) {
    prv_text(ctx, T(T_KEINE_NACHT), FONT_KEY_GOTHIC_24_BOLD,
             GRect(RAND, b.size.h / 2 - 30, b.size.w - 2 * RAND, 60), GTextAlignmentCenter,
             GColorDarkGray);
    return;
  }
  const NachtBild *n = &d->nacht;
  char zahl[16], spanne[48];
  prv_dauer(zahl, sizeof(zahl), n->schlaf_minuten);
  const int ein = prv_uhrzeit(n, n->einschlafen), auf = prv_uhrzeit(n, n->aufwachen);
  snprintf(spanne, sizeof(spanne), "%d:%02d - %d:%02d%s%s", ein / 60, ein % 60, auf / 60,
           auf % 60, n->fertig ? "" : "  ", n->fertig ? "" : T(T_LAEUFT));
  prv_gross(ctx, b, BALKEN_H + 8, zahl, spanne);

  // DAS HYPNOGRAMM: vier Bahnen, oben wach, unten tief - wie man es von
  // Fitbit und aus Boulder kennt.
  const int16_t oben = BALKEN_H + 76, bahn = 13;
  const int16_t x0 = RAND, breite = b.size.w - 2 * RAND;
  const int dauer = n->aufwachen > n->einschlafen ? n->aufwachen - n->einschlafen : 1;
  for (int k = 0; k < n->anzahl; k++) {
    const NachtDlsAbschnitt *a = &n->abschnitte[k];
    int reihe;
    switch (a->phase) {
      case SchlafPhaseWach: reihe = 0; break;
      case SchlafPhaseRem: reihe = 1; break;
      case SchlafPhaseTief: reihe = 3; break;
      default: reihe = 2; break;
    }
    const int16_t x = x0 + (int32_t)(a->von - n->einschlafen) * breite / dauer;
    const int16_t w = x0 + (int32_t)(a->bis - n->einschlafen) * breite / dauer - x;
    graphics_context_set_fill_color(ctx, prv_phasenfarbe(a->phase));
    graphics_fill_rect(ctx, GRect(x, oben + reihe * bahn, w > 0 ? w : 1, bahn - 1), 0,
                       GCornerNone);
  }

  // Die Minuten je Phase, zwei mal zwei.
  static const uint8_t s_phasen[] = { SchlafPhaseTief, SchlafPhaseRem, SchlafPhaseLeicht,
                                      SchlafPhaseWach };
  static const Text s_namen[] = { T_TIEF, T_REM, T_LEICHT, T_WACH };
  const int16_t y0 = oben + 4 * bahn + 4, spalte = (b.size.w - 2 * RAND) / 2;
  for (int i = 0; i < 4; i++) {
    const int16_t x = RAND + (i % 2) * spalte, y = y0 + (i / 2) * 20;
    graphics_context_set_fill_color(ctx, prv_phasenfarbe(s_phasen[i]));
    graphics_fill_rect(ctx, GRect(x, y + 5, 10, 10), 0, GCornerNone);
    char zeile[24], dauer_text[16];
    int minuten = n->minuten[s_phasen[i]];
    if (s_phasen[i] == SchlafPhaseLeicht) {
      minuten += n->minuten[SchlafPhaseSchlaf];   // Schlaf ohne Puls zaehlt als leicht
    }
    prv_dauer(dauer_text, sizeof(dauer_text), minuten);
    snprintf(zeile, sizeof(zeile), "%s %s", T(s_namen[i]), dauer_text);
    prv_text(ctx, zeile, FONT_KEY_GOTHIC_18, GRect(x + 14, y - 2, spalte - 14, 22),
             GTextAlignmentLeft, GColorBlack);
  }
}

// --- Herz ---

// Ein Herz aus zwei Kreisen und einem Dreieck, s ist die halbe Breite.
static void prv_herzform(GContext *ctx, GPoint m, int16_t s) {
  graphics_context_set_fill_color(ctx, GColorFolly);
  graphics_fill_circle(ctx, GPoint(m.x - s / 2, m.y - s / 4), s / 2 + 1);
  graphics_fill_circle(ctx, GPoint(m.x + s / 2, m.y - s / 4), s / 2 + 1);
  GPoint ecken[3] = {
    GPoint(m.x - s, m.y), GPoint(m.x + s, m.y), GPoint(m.x, m.y + s),
  };
  GPath dreieck = { .num_points = 3, .points = ecken };
  gpath_draw_filled(ctx, &dreieck);
}

static void prv_herz(GContext *ctx, GRect b, const HealthDaten *d, int herz) {
  char zahl[8], unter[32];
  if (d->puls) {
    snprintf(zahl, sizeof(zahl), "%d", d->puls);
    const int vor = (int)((time(NULL) - d->puls_zeit) / SECONDS_PER_MINUTE);
    if (vor < 2) {
      snprintf(unter, sizeof(unter), "%s  %s", T(T_BPM), T(T_JETZT));
    } else {
      char wann[20];
      snprintf(wann, sizeof(wann), T(T_VOR_MIN), vor);
      snprintf(unter, sizeof(unter), "%s  %s", T(T_BPM), wann);
    }
  } else {
    snprintf(zahl, sizeof(zahl), "--");
    snprintf(unter, sizeof(unter), "%s", T(T_BPM));
  }
  prv_gross(ctx, b, BALKEN_H + 8, zahl, unter);
  if (herz) {
    prv_herzform(ctx, GPoint(RAND + 34, BALKEN_H + 26), herz == 2 ? 12 : 9);
  }

  char ruhe[8] = "--", hrv[8] = "--", spo2[8] = "--";
  if (d->nacht_da && d->nacht.ruhepuls) snprintf(ruhe, sizeof(ruhe), "%d", d->nacht.ruhepuls);
  if (d->nacht_da && d->nacht.hrv) snprintf(hrv, sizeof(hrv), "%d", d->nacht.hrv);
  if (d->spo2) snprintf(spo2, sizeof(spo2), "%d", d->spo2);
  // "--" hiesse "noch keine Messung"; bei abgeschaltetem SpO2 kaeme nie eine.
  else if (d->spo2_aus) snprintf(spo2, sizeof(spo2), "%s", T(T_AUS));
  const char *namen[3] = { T(T_RUHE), T(T_HRV), T(T_SPO2) };
  const char *zahlen[3] = { ruhe, hrv, spo2 };
  prv_drei(ctx, b, BALKEN_H + 72, namen, zahlen);

  // DIE KURVE DER LETZTEN 24 STUNDEN: je halbe Stunde ein Strich vom tiefsten
  // zum hoechsten Puls. Wo keiner gemessen wurde, bleibt es leer.
  const int16_t oben = BALKEN_H + 128, hoehe = b.size.h - oben - 30;
  const int16_t breite = (b.size.w - 2 * RAND) / DATEN_PULS_SPALTEN;
  const int16_t x0 = (b.size.w - DATEN_PULS_SPALTEN * breite) / 2;
  int tief = 255, hoch = 0;
  for (int i = 0; i < DATEN_PULS_SPALTEN; i++) {
    if (d->puls_min[i] && d->puls_min[i] < tief) tief = d->puls_min[i];
    if (d->puls_max[i] > hoch) hoch = d->puls_max[i];
  }
  if (hoch > 0) {
    tief = tief / 10 * 10;
    hoch = (hoch + 9) / 10 * 10;
    if (hoch - tief < 30) hoch = tief + 30;
    graphics_context_set_fill_color(ctx, GColorFolly);
    for (int i = 0; i < DATEN_PULS_SPALTEN; i++) {
      if (!d->puls_max[i]) continue;
      const int16_t y1 = oben + hoehe - (d->puls_max[i] - tief) * hoehe / (hoch - tief);
      const int16_t y2 = oben + hoehe - (d->puls_min[i] - tief) * hoehe / (hoch - tief);
      graphics_fill_rect(ctx, GRect(x0 + i * breite, y1, breite > 1 ? breite - 1 : 1,
                                    y2 - y1 + 2), 0, GCornerNone);
    }
  }
  // DARUNTER DIE SPANNE DER 24 STUNDEN und der tiefste SpO2 der Nacht - die
  // zwei Zahlen, nach denen man in der Kurve sonst suchen muesste.
  int puls_tief = 255, puls_hoch = 0;
  for (int i = 0; i < DATEN_PULS_SPALTEN; i++) {
    if (d->puls_min[i] && d->puls_min[i] < puls_tief) puls_tief = d->puls_min[i];
    if (d->puls_max[i] > puls_hoch) puls_hoch = d->puls_max[i];
  }
  char spanne[20] = "", nacht[24] = "";
  if (puls_hoch > 0) {
    snprintf(spanne, sizeof(spanne), "%d-%d %s", puls_tief, puls_hoch, T(T_BPM));
  }
  if (d->spo2_nacht_tief) {
    snprintf(nacht, sizeof(nacht), T(T_NACHT_TIEF), d->spo2_nacht_tief);
  }
  char fuss[48];
  if (spanne[0] && nacht[0]) {
    snprintf(fuss, sizeof(fuss), "%s  %s", spanne, nacht);
  } else if (spanne[0] || nacht[0]) {
    snprintf(fuss, sizeof(fuss), "%s%s", spanne, nacht);
  } else {
    snprintf(fuss, sizeof(fuss), "%s", T(T_24H));
  }
  prv_text(ctx, fuss, FONT_KEY_GOTHIC_14, GRect(0, oben + hoehe, b.size.w, 16),
           GTextAlignmentCenter, GColorDarkGray);
}

void karten_zeichnen(GContext *ctx, GRect b, Karte karte, const HealthDaten *d, int herz) {
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, b, 0, GCornerNone);
  prv_balken(ctx, b, karte);
  switch (karte) {
    case KarteHeute: prv_heute(ctx, b, d); break;
    case KarteSchlaf: prv_schlaf(ctx, b, d); break;
    default: prv_herz(ctx, b, d, herz); break;
  }
  prv_striche(ctx, b, karte);
}
