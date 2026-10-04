/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "apps/watch/timestyle/distanz_text.h"
#include "apps/watch/timestyle/schritte_text.h"
#include "apps/watch/timestyle/wetter_symbol.h"
#include "pbl/services/weather/weather_types.h"

#include <stdlib.h>
#include <string.h>

// Die reinen Teile des System-TimeStyle: welches Symbol zur Wetterart der
// Firmware gehoert, ob Sonne oder Mond, wann Wetterdaten zu alt sind und wie
// die Schrittzahl in die Leiste kommt.

void test_timestyle__initialize(void) {
}
void test_timestyle__cleanup(void) {
}

// --- Wetterart -> Symbol ---

void test_timestyle__jede_wetterart_hat_ihr_symbol(void) {
  cl_assert_equal_i(wetter_symbol(WeatherType_Sun, false), CLEAR_DAY);
  cl_assert_equal_i(wetter_symbol(WeatherType_PartlyCloudy, false), PARTLY_CLOUDY);
  cl_assert_equal_i(wetter_symbol(WeatherType_CloudyDay, false), CLOUDY_DAY);
  cl_assert_equal_i(wetter_symbol(WeatherType_LightRain, false), LIGHT_RAIN);
  cl_assert_equal_i(wetter_symbol(WeatherType_HeavyRain, false), HEAVY_RAIN);
  cl_assert_equal_i(wetter_symbol(WeatherType_LightSnow, false), LIGHT_SNOW);
  cl_assert_equal_i(wetter_symbol(WeatherType_HeavySnow, false), HEAVY_SNOW);
  cl_assert_equal_i(wetter_symbol(WeatherType_RainAndSnow, false), RAINING_AND_SNOWING);
  cl_assert_equal_i(wetter_symbol(WeatherType_Generic, false), WEATHER_GENERIC);
}

void test_timestyle__nachts_mond_nur_bei_sonne_und_teils_bewoelkt(void) {
  cl_assert_equal_i(wetter_symbol(WeatherType_Sun, true), CLEAR_NIGHT);
  cl_assert_equal_i(wetter_symbol(WeatherType_PartlyCloudy, true), PARTLY_CLOUDY_NIGHT);
  // Wolken, Regen und Schnee sehen nachts aus wie am Tag - wie in TimeStyle.
  cl_assert_equal_i(wetter_symbol(WeatherType_CloudyDay, true), CLOUDY_DAY);
  cl_assert_equal_i(wetter_symbol(WeatherType_LightRain, true), LIGHT_RAIN);
  cl_assert_equal_i(wetter_symbol(WeatherType_HeavySnow, true), HEAVY_SNOW);
}

void test_timestyle__unbekannte_wetterart_ergibt_das_allgemeine_symbol(void) {
  cl_assert_equal_i(wetter_symbol(WeatherType_Unknown, false), WEATHER_GENERIC);
  cl_assert_equal_i(wetter_symbol(WeatherType_Unknown, true), WEATHER_GENERIC);
  cl_assert_equal_i(wetter_symbol(9, false), WEATHER_GENERIC);
  cl_assert_equal_i(wetter_symbol(200, false), WEATHER_GENERIC);
}

void test_timestyle__die_symbolnummern_bleiben_die_von_timestyle(void) {
  // Die Store-App bekam diese Zahlen vom Telefon; die Liste darf sich nicht
  // verschieben, falls spaeter Einstellungen von Boulder kommen.
  cl_assert_equal_i(CLEAR_DAY, 0);
  cl_assert_equal_i(CLEAR_NIGHT, 1);
  cl_assert_equal_i(CLOUDY_DAY, 2);
  cl_assert_equal_i(PARTLY_CLOUDY, 8);
  cl_assert_equal_i(THUNDERSTORM, 10);
  cl_assert_equal_i(WEATHER_GENERIC, 11);
}

// --- Tag oder Nacht ---

// Die erste Minute (UTC) in [von, bis), in der der Zustand wechselt; -1 ohne.
static int prv_wechsel(int16_t breite, int16_t laenge, int tag, int von, int bis) {
  bool vorher = wetter_ist_nacht(breite, laenge, tag, von % 1440);
  for (int m = von + 1; m < bis; m++) {
    const bool jetzt = wetter_ist_nacht(breite, laenge, tag, m % 1440);
    if (jetzt != vorher) {
      return m % 1440;
    }
  }
  return -1;
}

// Sonnenauf- und -untergang auf wenige Minuten wie die NOAA-Rechnung (mit
// 0,833 Grad Brechung). Die Sollwerte: UTC-Minuten des Tages.
static void prv_pruefe_sonne(int16_t breite, int16_t laenge, int tag, int aufgang, int untergang) {
  const int auf = prv_wechsel(breite, laenge, tag, aufgang - 60, aufgang + 60);
  const int unter = prv_wechsel(breite, laenge, tag, untergang - 60, untergang + 60);
  cl_assert(auf >= 0);
  cl_assert(unter >= 0);
  cl_assert(abs(auf - aufgang) <= 6);
  cl_assert(abs(unter - untergang) <= 6);
  // Dazwischen Tag, davor und danach Nacht.
  cl_assert(!wetter_ist_nacht(breite, laenge, tag, (aufgang + 30) % 1440));
  cl_assert(!wetter_ist_nacht(breite, laenge, tag, (untergang + 1440 - 30) % 1440));
  cl_assert(wetter_ist_nacht(breite, laenge, tag, (aufgang + 1440 - 30) % 1440));
  cl_assert(wetter_ist_nacht(breite, laenge, tag, (untergang + 30) % 1440));
}

void test_timestyle__zuerich_am_tag_des_fotos(void) {
  // 3. Oktober (Tag 276): Aufgang 05:26 UTC (07:26 Sommerzeit), Untergang
  // 17:02 UTC. Das Foto um 15:45 Ortszeit (13:45 UTC) zeigt die Tagwolke.
  prv_pruefe_sonne(4737, 854, 276, 5 * 60 + 26, 17 * 60 + 2);
  cl_assert(!wetter_ist_nacht(4737, 854, 276, 13 * 60 + 45));
  cl_assert(wetter_ist_nacht(4737, 854, 276, 21 * 60));
}

void test_timestyle__zuerich_sommer_und_winter(void) {
  prv_pruefe_sonne(4737, 854, 172, 3 * 60 + 28, 19 * 60 + 25);
  prv_pruefe_sonne(4737, 854, 355, 7 * 60 + 10, 15 * 60 + 37);
}

void test_timestyle__westlich_und_suedlich(void) {
  // New York, 10. Maerz: Aufgang 11:16, Untergang 22:56 UTC.
  prv_pruefe_sonne(4071, -7401, 69, 11 * 60 + 16, 22 * 60 + 56);
  // Sydney, 1. Januar: der Tag liegt in UTC ueber Mitternacht (Aufgang
  // 18:47, Untergang 09:09 UTC).
  prv_pruefe_sonne(-3387, 15121, 1, 18 * 60 + 47, 9 * 60 + 9);
}

void test_timestyle__polarnacht_und_mitternachtssonne(void) {
  // Tromsoe: am 21. Dezember ist es den ganzen Tag Nacht, am 21. Juni Tag.
  for (int m = 0; m < 1440; m += 10) {
    cl_assert(wetter_ist_nacht(6965, 1896, 355, m));
    cl_assert(!wetter_ist_nacht(6965, 1896, 172, m));
  }
}

void test_timestyle__ohne_ort_immer_tag(void) {
  // Ein Satz vor v4 kennt den Ort nicht: dann die Tagsymbole wie bisher.
  for (int m = 0; m < 1440; m += 60) {
    cl_assert(!wetter_ist_nacht(INT16_MIN, INT16_MIN, 276, m));
    cl_assert(!wetter_ist_nacht(4737, INT16_MIN, 276, m));
  }
}

// Die Wechsel AUF DIE MINUTE, wie diese Rechnung sie heute ergibt. Boulders
// Vorschau rechnet dasselbe nach (TsWetterJetztTest, dieselben Zahlen) - sonst
// zeigte sie um die Daemmerung ein anderes Symbol als die Uhr. Aendert sich die
// Rechnung hier, muss Boulder bewusst nachziehen.
static void prv_genau(int16_t breite, int16_t laenge, int tag, int tag_ab, int nacht_ab,
                      int nachtminuten) {
  int nacht = 0;
  for (int m = 0; m < 1440; m++) {
    nacht += wetter_ist_nacht(breite, laenge, tag, m) ? 1 : 0;
  }
  cl_assert_equal_i(nacht, nachtminuten);
  if (tag_ab >= 0) {
    cl_assert(wetter_ist_nacht(breite, laenge, tag, (tag_ab + 1439) % 1440));
    cl_assert(!wetter_ist_nacht(breite, laenge, tag, tag_ab));
  }
  if (nacht_ab >= 0) {
    cl_assert(!wetter_ist_nacht(breite, laenge, tag, (nacht_ab + 1439) % 1440));
    cl_assert(wetter_ist_nacht(breite, laenge, tag, nacht_ab));
  }
}

void test_timestyle__wechsel_auf_die_minute(void) {
  prv_genau(4737, 854, 276, 331, 1018, 753);   // Zuerich, 3. Oktober
  prv_genau(4737, 854, 172, 209, 1166, 483);   // Zuerich, 21. Juni
  prv_genau(4737, 854, 355, 431, 939, 932);    // Zuerich, 21. Dezember
  prv_genau(4071, -7401, 69, 680, 1375, 745);  // New York, 10. Maerz
  prv_genau(-3387, 15121, 1, 1129, 550, 579);  // Sydney, 1. Januar
  prv_genau(-22, -7851, 100, 673, 1399, 714);  // Quito
  prv_genau(3568, 13976, 200, 1179, 595, 584); // Tokio
  prv_genau(6965, 1896, 355, -1, -1, 1440);    // Tromsoe, Polarnacht
  prv_genau(6965, 1896, 172, -1, -1, 0);       // Tromsoe, Mitternachtssonne
}

// Dazu ein Fingerabdruck ueber ein Gitter (Breite -80..80, Laenge -170..170,
// jeder 25. Tag, jede Minute): schon eine um eins andere Konstante verschiebt
// irgendwo eine Minute. Dieselben Zahlen in Boulders TsWetterJetztTest.
void test_timestyle__nacht_fingerabdruck(void) {
  uint32_t h = 0;
  int nacht = 0;
  for (int b = -8000; b <= 8000; b += 1000) {
    for (int l = -17000; l <= 17000; l += 8500) {
      for (int tag = 1; tag <= 366; tag += 25) {
        for (int m = 0; m < 1440; m++) {
          const bool n = wetter_ist_nacht(b, l, tag, m);
          nacht += n ? 1 : 0;
          h = h * 31u + (n ? 1u : 0u) + (uint32_t)m;
        }
      }
    }
  }
  cl_assert_equal_i(nacht, 903070);
  cl_assert(h == 3572735266u);
}

// --- Alter der Wetterdaten ---

void test_timestyle__wetter_hoechstens_drei_stunden_alt(void) {
  const time_t jetzt = 1790000000;
  cl_assert(!wetter_frisch(0, jetzt));
  cl_assert(wetter_frisch(jetzt, jetzt));
  cl_assert(wetter_frisch(jetzt - 60 * 60, jetzt));
  cl_assert(wetter_frisch(jetzt - WETTER_HOECHSTALTER_S, jetzt));
  cl_assert(!wetter_frisch(jetzt - WETTER_HOECHSTALTER_S - 1, jetzt));
  cl_assert(!wetter_frisch(jetzt - 24 * 60 * 60, jetzt));
}

void test_timestyle__wetter_aus_der_zukunft_nur_wenig(void) {
  // Geht die Uhr etwas nach, ist der Stand des Telefons spaeter als "jetzt".
  const time_t jetzt = 1790000000;
  cl_assert(wetter_frisch(jetzt + 30 * 60, jetzt));
  cl_assert(wetter_frisch(jetzt + 60 * 60, jetzt));
  cl_assert(!wetter_frisch(jetzt + 60 * 60 + 1, jetzt));
}

// --- Schritte ---

static const char *prv_schritte(int32_t schritte, char trenner) {
  static char puffer[16];
  schritte_text(puffer, sizeof(puffer), schritte, trenner);
  return puffer;
}

void test_timestyle__schritte_wie_auf_dem_foto(void) {
  cl_assert_equal_s(prv_schritte(5523, '.'), "5.5k");
  // Abgeschnitten, nicht gerundet - wie das Original.
  cl_assert_equal_s(prv_schritte(5599, '.'), "5.5k");
}

void test_timestyle__schritte_alle_bereiche(void) {
  cl_assert_equal_s(prv_schritte(0, '.'), "0");
  cl_assert_equal_s(prv_schritte(7, '.'), "7");
  cl_assert_equal_s(prv_schritte(999, '.'), "999");
  cl_assert_equal_s(prv_schritte(1000, '.'), "1.0k");
  cl_assert_equal_s(prv_schritte(1099, '.'), "1.0k");
  cl_assert_equal_s(prv_schritte(9999, '.'), "9.9k");
  cl_assert_equal_s(prv_schritte(10000, '.'), "10k");
  cl_assert_equal_s(prv_schritte(12345, '.'), "12k");
  cl_assert_equal_s(prv_schritte(123456, '.'), "123k");
}

void test_timestyle__schritte_mit_komma_und_ohne_daten(void) {
  cl_assert_equal_s(prv_schritte(5523, ','), "5,5k");
  cl_assert_equal_s(prv_schritte(-1, '.'), "0");
}

void test_timestyle__schritte_kuerzen_im_kleinen_puffer(void) {
  char puffer[4];
  schritte_text(puffer, sizeof(puffer), 5523, '.');
  cl_assert_equal_s(puffer, "5.5");
}

// --- Distanz (Widget "Steps" mit "Distance") ---
// Dieselben Vektoren prueft Boulders TsTexteTest gegen die Vorschau.

static const char *prv_distanz(int32_t meter, bool metrisch, char trenner, bool *klein) {
  static char puffer[16];
  memset(puffer, 'x', sizeof(puffer));
  *klein = distanz_text(puffer, sizeof(puffer), meter, metrisch, trenner);
  return puffer;
}

void test_timestyle__distanz_metrisch_an_den_grenzen(void) {
  bool klein;
  cl_assert_equal_s(prv_distanz(0, true, '.', &klein), "0m");
  cl_assert_equal_s(prv_distanz(99, true, '.', &klein), "99m");
  cl_assert_equal_s(prv_distanz(100, true, '.', &klein), ".1km");
  cl_assert_equal_s(prv_distanz(999, true, '.', &klein), ".9km");
  cl_assert_equal_s(prv_distanz(999, true, ',', &klein), ",9km");
  cl_assert_equal_s(prv_distanz(1000, true, '.', &klein), "1km");
  // Ganze km, abgeschnitten: 4,23 km ist "4km", nie "4.2km".
  cl_assert_equal_s(prv_distanz(4230, true, '.', &klein), "4km");
  cl_assert(!klein);
  cl_assert_equal_s(prv_distanz(9999, true, '.', &klein), "9km");
  cl_assert(!klein);
  // Ab 10 km die kleine Schrift, wie im Original.
  cl_assert_equal_s(prv_distanz(10000, true, '.', &klein), "10km");
  cl_assert(klein);
}

void test_timestyle__distanz_meilen_an_den_grenzen(void) {
  bool klein;
  // Unter einer halben Meile (804 m) die Zehntel, sonst gerundete Meilen.
  cl_assert_equal_s(prv_distanz(0, false, '.', &klein), ".0mi");
  cl_assert_equal_s(prv_distanz(804, false, '.', &klein), ".4mi");
  cl_assert_equal_s(prv_distanz(804, false, ',', &klein), ",4mi");
  cl_assert_equal_s(prv_distanz(805, false, '.', &klein), "1mi");
  cl_assert_equal_s(prv_distanz(4230, false, '.', &klein), "3mi");
  cl_assert_equal_s(prv_distanz(2413, false, '.', &klein), "1mi");
  cl_assert_equal_s(prv_distanz(2414, false, '.', &klein), "2mi");
  cl_assert_equal_s(prv_distanz(20000, false, '.', &klein), "12mi");
  // Meilen nie in der kleinen Schrift (das Original kennt das nur fuer km).
  cl_assert(!klein);
}

// --- UV der Stunde ---

// Eine Ortszeit in UTC-Sekunden: 3.10.2026 00:00 UTC ist 1790985600.
#define TAG_0310_UTC ((time_t)1790985600)

static uint8_t s_uv_stunden[24];

static void prv_uv_stunden(void) {
  for (int h = 0; h < 24; h++) {
    s_uv_stunden[h] = (uint8_t)(h * 10);  // Stunde h hat UV h,0
  }
}

void test_timestyle__uv_die_naechste_stunde(void) {
  prv_uv_stunden();
  const time_t stand = TAG_0310_UTC + 6 * 3600;
  // Ort in UTC (Versatz 0): 12:29 ist noch 12 Uhr, ab 12:30 die 13.
  cl_assert_equal_i(wetter_uv_jetzt(s_uv_stunden, 70, 0, 0, stand, TAG_0310_UTC + 12 * 3600 + 29 * 60 + 59), 12);
  cl_assert_equal_i(wetter_uv_jetzt(s_uv_stunden, 70, 0, 0, stand, TAG_0310_UTC + 12 * 3600 + 30 * 60), 13);
  // Nach 23:30 bleibt es die 23 (es gibt keine Stunde 24).
  cl_assert_equal_i(wetter_uv_jetzt(s_uv_stunden, 70, 0, 0, stand, TAG_0310_UTC + 23 * 3600 + 45 * 60), 23);
  cl_assert_equal_i(wetter_uv_jetzt(s_uv_stunden, 70, 0, 0, stand, TAG_0310_UTC), 0);
}

void test_timestyle__uv_in_der_zeit_des_wetterorts(void) {
  prv_uv_stunden();
  const time_t stand = TAG_0310_UTC + 6 * 3600;
  const time_t jetzt = TAG_0310_UTC + 10 * 3600;  // 10:00 UTC
  // Zuerich (UTC+2 im Oktober): 12 Uhr am Ort.
  cl_assert_equal_i(wetter_uv_jetzt(s_uv_stunden, 70, 120, 0, stand, jetzt), 12);
  // Ohne Zeitzone des Ortes gilt die der Uhr (hier UTC+5:30 -> 15:30 -> 16).
  cl_assert_equal_i(wetter_uv_jetzt(s_uv_stunden, 70, INT16_MIN, 19800, stand, jetzt), 16);
  // Der Ort zaehlt vor der Uhr.
  cl_assert_equal_i(wetter_uv_jetzt(s_uv_stunden, 70, 120, 19800, stand, jetzt), 12);
}

void test_timestyle__uv_gerundet(void) {
  prv_uv_stunden();
  const time_t stand = TAG_0310_UTC;
  s_uv_stunden[12] = 24;  // 2,4 -> 2
  cl_assert_equal_i(wetter_uv_jetzt(s_uv_stunden, 70, 0, 0, stand, TAG_0310_UTC + 12 * 3600), 2);
  s_uv_stunden[12] = 25;  // 2,5 -> 3
  cl_assert_equal_i(wetter_uv_jetzt(s_uv_stunden, 70, 0, 0, stand, TAG_0310_UTC + 12 * 3600), 3);
}

void test_timestyle__uv_am_naechsten_tag_unbekannt(void) {
  prv_uv_stunden();
  // Daten von gestern Abend: die Stundenwerte gehoeren zum Vortag.
  const time_t stand = TAG_0310_UTC - 2 * 3600;
  cl_assert_equal_i(wetter_uv_jetzt(s_uv_stunden, 70, 0, 0, stand, TAG_0310_UTC + 9 * 3600), -1);
  // Am Ort ist es noch derselbe Tag (UTC-5: 2.10. 21:00 und 3.10. 04:00 UTC = 23:00).
  cl_assert_equal_i(wetter_uv_jetzt(s_uv_stunden, 70, -300, 0, TAG_0310_UTC - 3 * 3600, TAG_0310_UTC + 4 * 3600), 23);
}

void test_timestyle__uv_ohne_stundenwert_der_tageswert(void) {
  prv_uv_stunden();
  const time_t stand = TAG_0310_UTC;
  s_uv_stunden[12] = 255;  // unbekannt
  cl_assert_equal_i(wetter_uv_jetzt(s_uv_stunden, 70, 0, 0, stand, TAG_0310_UTC + 12 * 3600), 7);
  // Alter Boulder ohne Stundenwerte: der Tageswert (abgeschnitten wie bisher).
  cl_assert_equal_i(wetter_uv_jetzt(NULL, 79, 0, 0, stand, TAG_0310_UTC + 12 * 3600), 7);
  cl_assert_equal_i(wetter_uv_jetzt(NULL, -1, 0, 0, stand, TAG_0310_UTC + 12 * 3600), -1);
}
