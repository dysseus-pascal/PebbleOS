/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "wetter_symbol.h"

#include "pbl/services/weather/weather_types.h"
#include "pbl/util/trig.h"

WeatherCondition wetter_symbol(uint8_t wetterart, bool nacht) {
  switch ((WeatherType)wetterart) {
    case WeatherType_Sun:
      return nacht ? CLEAR_NIGHT : CLEAR_DAY;
    case WeatherType_PartlyCloudy:
      return nacht ? PARTLY_CLOUDY_NIGHT : PARTLY_CLOUDY;
    case WeatherType_CloudyDay:
      return CLOUDY_DAY;
    case WeatherType_LightRain:
      return LIGHT_RAIN;
    case WeatherType_HeavyRain:
      return HEAVY_RAIN;
    case WeatherType_LightSnow:
      return LIGHT_SNOW;
    case WeatherType_HeavySnow:
      return HEAVY_SNOW;
    case WeatherType_RainAndSnow:
      return RAINING_AND_SNOWING;
    case WeatherType_Generic:
    case WeatherType_Unknown:
    default:
      return WEATHER_GENERIC;
  }
}

// --- Sonnenstand ---
// Dieselbe Naeherung wie die Sonnenuntergangszeit der Wetter-App
// (apps/system/weather/expanded_view.c): Deklination und Zeitgleichung aus dem
// Tag im Jahr, der halbe Tagbogen aus dem Stundenwinkel. Auf wenige Minuten
// genau - fuer Sonne oder Mond im Symbol reicht das.

// Sinus von 0,833 Grad (Brechung und Sonnenradius) in TRIG_MAX_RATIO.
#define SIN_HORIZONT 953

// Hundertstelgrad in den Winkel der Ganzzahl-Trigonometrie.
static int32_t prv_winkel(int32_t hundertstel_grad) {
  int32_t w = (int32_t)(((int64_t)hundertstel_grad * TRIG_MAX_ANGLE) / 36000);
  w %= TRIG_MAX_ANGLE;
  return (w < 0) ? w + TRIG_MAX_ANGLE : w;
}

bool wetter_ist_nacht(int16_t breite_e2, int16_t laenge_e2, int tag_im_jahr, int minute_utc) {
  if (breite_e2 == INT16_MIN || laenge_e2 == INT16_MIN) {
    return false;
  }
  const int32_t r = TRIG_MAX_RATIO;
  const int32_t b = (36000 * (tag_im_jahr - 81)) / 365;
  const int32_t sin_b = sin_lookup(prv_winkel(b));
  const int32_t cos_b = cos_lookup(prv_winkel(b));
  const int32_t sin_2b = sin_lookup(prv_winkel(2 * b));

  const int32_t dekl = (int32_t)(((int64_t)2345 * sin_b) / r);
  const int32_t sin_dekl = sin_lookup(prv_winkel(dekl));
  const int32_t cos_dekl = cos_lookup(prv_winkel(dekl));
  const int32_t sin_br = sin_lookup(prv_winkel(breite_e2));
  const int32_t cos_br = cos_lookup(prv_winkel(breite_e2));

  // cos(Stundenwinkel bei Auf- und Untergang), in Millionstel.
  const int64_t nenner = (int64_t)cos_br * cos_dekl;
  const int64_t zaehler = -(int64_t)SIN_HORIZONT * r - (int64_t)sin_br * sin_dekl;
  if (nenner <= 0) {
    // Am Pol: Tag, solange die Sonne auf dieser Seite des Aequators steht.
    return zaehler >= 0;
  }
  const int64_t cos_h = zaehler * 1000000 / nenner;
  if (cos_h >= 1000000) {
    return true; // Polarnacht
  }
  if (cos_h <= -1000000) {
    return false; // Mitternachtssonne
  }

  // Stundenwinkel = acos(cos_h), durch Halbieren; cos faellt auf [0, 180].
  int32_t unten = 0;
  int32_t oben = TRIG_MAX_ANGLE / 2;
  while (oben - unten > 1) {
    const int32_t mitte = (unten + oben) / 2;
    if ((int64_t)cos_lookup(mitte) * 1000000 / r > cos_h) {
      unten = mitte;
    } else {
      oben = mitte;
    }
  }
  // Halber Tagbogen in Tausendstelminuten: 360 Grad sind 1440 Minuten.
  const int64_t halbtag = (int64_t)unten * 1440000 / TRIG_MAX_ANGLE;

  // Wahrer Mittag in UTC: 12 Uhr minus 4 Minuten je Grad Ost, minus
  // Zeitgleichung (Tausendstelminuten).
  const int64_t zeitgleichung =
      ((int64_t)9870 * sin_2b - (int64_t)7530 * cos_b - (int64_t)1500 * sin_b) / r;
  const int64_t mittag = 720000 - (int64_t)40 * laenge_e2 - zeitgleichung;

  // Abstand zum Mittag, auf einen Tag um den Mittag gefaltet.
  int64_t abstand = ((int64_t)minute_utc * 1000 - mittag) % 1440000;
  if (abstand < -720000) {
    abstand += 1440000;
  } else if (abstand >= 720000) {
    abstand -= 1440000;
  }
  if (abstand < 0) {
    abstand = -abstand;
  }
  return abstand >= halbtag;
}

bool wetter_frisch(time_t stand_utc, time_t jetzt_utc) {
  if (stand_utc <= 0) {
    return false;
  }
  const int64_t alter = (int64_t)jetzt_utc - (int64_t)stand_utc;
  return alter <= WETTER_HOECHSTALTER_S && alter >= -(60 * 60);
}

// Tage seit 1970 in Ortszeit, abgerundet (auch vor 1970 richtig).
static int64_t prv_tag(int64_t lokal_s) {
  return (lokal_s >= 0) ? lokal_s / 86400 : -((-lokal_s + 86399) / 86400);
}

int wetter_uv_jetzt(const uint8_t *stunden_x10, int tag_x10, int ort_versatz_min,
                    int32_t uhr_versatz_s, time_t stand_utc, time_t jetzt_utc) {
  const int64_t versatz_s =
      (ort_versatz_min != INT16_MIN) ? (int64_t)ort_versatz_min * 60 : (int64_t)uhr_versatz_s;
  if (stunden_x10) {
    const int64_t jetzt = (int64_t)jetzt_utc + versatz_s;
    if (prv_tag(jetzt) != prv_tag((int64_t)stand_utc + versatz_s)) {
      return -1;
    }
    const int64_t sekunde_des_tages = jetzt - prv_tag(jetzt) * 86400;
    int stunde = (int)((sekunde_des_tages + 1800) / 3600);
    if (stunde > 23) {
      stunde = 23;
    }
    if (stunden_x10[stunde] != 255) {
      return (stunden_x10[stunde] + 5) / 10;
    }
  }
  return (tag_x10 >= 0) ? tag_x10 / 10 : -1;
}
