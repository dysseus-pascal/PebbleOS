/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#ifdef CONFIG_BOULDER_TAGESBAND

#include "band.h"

#include "pbl/util/math.h"

#include <stdio.h>

#define S_JE_STUNDE (60 * 60)
#define MARKE_S     (6 * S_JE_STUNDE)

// Vom 1.3.0000, ab dem die Datumsrechnung zaehlt, bis zum 1.1.2000, dem Tag 0 von bh_tag.
#define TAGE_BIS_2000 730425

BtBand bt_band(time_t jetzt, int16_t x0, int16_t breite) {
  return (BtBand){.von = jetzt - BT_VERGANGEN_S, .jetzt = jetzt, .x0 = x0, .breite = breite};
}

int16_t bt_x(const BtBand *b, time_t t) {
  return (int16_t)(b->x0 + (int64_t)(t - b->von) * b->breite / BT_SPANNE_S);
}

bool bt_strecke(const BtBand *b, time_t von, time_t bis, int16_t *links, int16_t *rechts) {
  const time_t a = MAX(von, b->von);
  const time_t e = MIN(bis, b->jetzt);
  if (e <= a) {
    return false;
  }
  *links = bt_x(b, a);
  *rechts = bt_x(b, e);
  // Ein Pixel sind fast acht Minuten: Eine kurze Wachphase bliebe sonst unsichtbar.
  if (*rechts <= *links) {
    *rechts = *links + 1;
  }
  return true;
}

GColor bt_phasenfarbe(uint8_t phase) {
  switch (phase) {
    case 0:
      return GColorChromeYellow; // wach
    case 2:
      return GColorBlue; // tief
    case 3:
      return GColorPictonBlue; // REM
    default:
      return GColorBlueMoon; // leicht, Schlaf ohne Phase
  }
}

GColor bt_sitzungsfarbe(const ActivitySession *s) {
  switch (s->type) {
    case ActivitySessionType_Walk:
    case ActivitySessionType_Run:
    case ActivitySessionType_Open:
      return GColorJaegerGreen;
    case ActivitySessionType_Nap:
      return GColorBlueMoon;
    default:
      return GColorClear;
  }
}

uint8_t bt_pulspunkte(const BtBand *b, const BhDaten *d, BtPulspunkt aus[HERZ_HALBSTUNDEN],
                      uint8_t *unten, uint8_t *oben) {
  *unten = 0;
  *oben = 0;
  if (!d->herz_da || d->herz.neueste == 0) {
    return 0;
  }
  const BoulderHerz *h = &d->herz;
  const uint32_t gueltig_ab =
      (h->neueste >= HERZ_HALBSTUNDEN - 1) ? h->neueste - (HERZ_HALBSTUNDEN - 1) : 0;
  const uint32_t erste = (uint32_t)(b->von / HERZ_HALBSTUNDE_S);
  const uint32_t letzte = (uint32_t)(b->jetzt / HERZ_HALBSTUNDE_S);
  uint8_t anzahl = 0, tief = UINT8_MAX, hoch = 0;
  bool luecke = true;
  for (uint32_t nr = erste; nr <= letzte && anzahl < HERZ_HALBSTUNDEN; nr++) {
    const uint8_t p = nr % HERZ_HALBSTUNDEN;
    const time_t mitte = (time_t)nr * HERZ_HALBSTUNDE_S + HERZ_HALBSTUNDE_S / 2;
    if (nr < gueltig_ab || nr > h->neueste || h->n[p] == 0 || mitte < b->von) {
      luecke = true;
      continue;
    }
    const uint8_t bpm = h->mittel[p];
    aus[anzahl++] = (BtPulspunkt){.x = bt_x(b, MIN(mitte, b->jetzt)), .bpm = bpm, .neu = luecke};
    luecke = false;
    tief = MIN(tief, bpm);
    hoch = MAX(hoch, bpm);
  }
  if (anzahl) {
    *unten = (tief > 5) ? tief - 5 : 0;
    *oben = (hoch < UINT8_MAX - 5) ? hoch + 5 : UINT8_MAX;
  }
  return anzahl;
}

uint8_t bt_marken(const BtBand *b, BtMarke aus[BT_MARKEN_MAX]) {
  const time_t bis = b->von + BT_SPANNE_S;
  const int32_t versatz = bh_ortsversatz(b->von);
  const int64_t ort = (int64_t)b->von + versatz;
  uint8_t anzahl = 0;
  for (int64_t marke = ((ort + MARKE_S - 1) / MARKE_S) * MARKE_S; anzahl < BT_MARKEN_MAX;
       marke += MARKE_S) {
    // Der Versatz in der Marke selbst: Liegt die Zeitumstellung im Band, rueckt die Marke dahinter
    // um die Stunde, wie die Uhr.
    const time_t utc = (time_t)(marke - bh_ortsversatz((time_t)(marke - versatz)));
    if (utc > bis) {
      break;
    }
    if (utc >= b->von) {
      aus[anzahl++] = (BtMarke){.x = bt_x(b, utc), .stunde = (uint8_t)((marke / S_JE_STUNDE) % 24)};
    }
  }
  return anzahl;
}

void bt_marke_text(uint8_t stunde, bool h24, char *aus, size_t platz) {
  if (h24) {
    snprintf(aus, platz, "%u", stunde);
    return;
  }
  const unsigned h = (stunde % 12 == 0) ? 12 : stunde % 12;
  snprintf(aus, platz, "%u%c", h, (stunde < 12) ? 'a' : 'p');
}

void bt_datum(uint16_t heute, char *aus, size_t platz) {
  static const char *const s_tage[7] = {"MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN"};
  static const char *const s_monate[12] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                           "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
  // Tag und Monat aus der Tageszahl (Hinnant, civil_from_days): Das Jahr beginnt am 1. Maerz,
  // so steht der Schalttag am Ende.
  const int32_t z = (int32_t)heute + TAGE_BIS_2000;
  const int32_t doe = z % 146097;
  const int32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const int32_t mp = (5 * doy + 2) / 153;
  const int32_t tag = doy - (153 * mp + 2) / 5 + 1;
  const int32_t monat = (mp < 10) ? mp + 3 : mp - 9;
  snprintf(aus, platz, "%s %d %s", s_tage[bh_wochentag(heute)], (int)tag, s_monate[monat - 1]);
}

void bt_zeit(uint16_t minute, bool h24, char *aus, size_t platz, const char **ampm) {
  const int stunde = (minute / 60) % 24, min = minute % 60;
  if (h24) {
    snprintf(aus, platz, "%02d:%02d", stunde, min);
    *ampm = NULL;
    return;
  }
  snprintf(aus, platz, "%d:%02d", (stunde % 12 == 0) ? 12 : stunde % 12, min);
  *ampm = (stunde < 12) ? "AM" : "PM";
}

void bt_heute(const BhDaten *d, uint8_t *ruhepuls, uint16_t *hrv) {
  BhNachtTag tage[NAECHTE_MAX];
  const uint8_t anzahl = bh_daten_nachttage(d, tage);
  const bool heute = (anzahl > 0 && tage[0].tag == d->heute);
  *ruhepuls = heute ? tage[0].ruhepuls : 0;
  *hrv = heute ? tage[0].hrv : 0;
}

#endif // CONFIG_BOULDER_TAGESBAND
