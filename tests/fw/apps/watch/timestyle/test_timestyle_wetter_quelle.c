/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "apps/watch/timestyle/wetter_quelle.h"
#include "pbl/services/blob_db/watch_app_prefs_db.h"
#include "pbl/services/blob_db/weather_db.h"
#include "pbl/services/weather/weather_service.h"
#include "pbl/services/weather/weather_service_private.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "fake_pbl_malloc.h"

// Der Lesepfad des Wetters fuer TimeStyle (wetter_quelle.c): welcher Satz
// gelesen wird (Ort 0 der Reihenfolge), welche Teile ein Satz je nach Fassung
// und Laenge hergibt (Ort, Wetter des Tages, Zeitzone des Ortes ab v4.1, UV je
// Stunde ab v4.4) und welche Werte daraus an wetter_uv_jetzt gehen.
// WARUM mit nachgebauter Datenbank: wetter_quelle.c liest ueber vier
// Firmware-Aufrufe (Reihenfolge, Laenge, Satz, Standardvorhersage); hier
// liefern sie einen Satz, wie Boulder ihn schickt - Byte fuer Byte im
// WeatherDBEntry, mit der Laenge der jeweiligen Fassung.

// --- Nachgebaute Datenbank ---

static const Uuid ORT0 = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
                          0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f};
static const Uuid ORT1 = {0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
                          0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f};

static int s_orte;           // Zahl der Orte in der Reihenfolge (0: keine)
static bool s_ohne_prefs;    // watch_app_prefs_get_weather liefert NULL
static uint8_t s_satz0[sizeof(WeatherDBEntry) + 64];
static int s_laenge0;        // Laenge des Satzes von ORT0 (0: keiner)
static uint8_t s_satz1[sizeof(WeatherDBEntry) + 64];
static int s_laenge1;
static bool s_ohne_vorhersage;
static int s_gelesen_ort;    // 0 oder 1: welcher Satz zuletzt gelesen wurde, -1 keiner

SerializedWeatherAppPrefs *watch_app_prefs_get_weather(void) {
  if (s_ohne_prefs) {
    return NULL;
  }
  SerializedWeatherAppPrefs *p = malloc(sizeof(SerializedWeatherAppPrefs) + 2 * sizeof(Uuid));
  p->num_locations = s_orte;
  p->locations[0] = ORT0;
  p->locations[1] = ORT1;
  return p;
}

void watch_app_prefs_destroy_weather(SerializedWeatherAppPrefs *prefs) {
  free(prefs);
}

static bool prv_ist(const uint8_t *key, int key_len, const Uuid *ort) {
  return key_len == (int)sizeof(Uuid) && memcmp(key, ort, sizeof(Uuid)) == 0;
}

int weather_db_get_len(const uint8_t *key, int key_len) {
  if (prv_ist(key, key_len, &ORT0)) {
    return s_laenge0;
  }
  if (prv_ist(key, key_len, &ORT1)) {
    return s_laenge1;
  }
  return 0;
}

status_t weather_db_read(const uint8_t *key, int key_len, uint8_t *val_out, int val_len) {
  if (prv_ist(key, key_len, &ORT0) && val_len <= s_laenge0) {
    memcpy(val_out, s_satz0, val_len);
    s_gelesen_ort = 0;
    return S_SUCCESS;
  }
  if (prv_ist(key, key_len, &ORT1) && val_len <= s_laenge1) {
    memcpy(val_out, s_satz1, val_len);
    s_gelesen_ort = 1;
    return S_SUCCESS;
  }
  return E_DOES_NOT_EXIST;
}

static WeatherLocationForecast s_vorhersage;

WeatherLocationForecast *weather_service_create_default_forecast(void) {
  if (s_ohne_vorhersage) {
    return NULL;
  }
  WeatherLocationForecast *v = malloc(sizeof(*v));
  *v = s_vorhersage;
  return v;
}

void weather_service_destroy_default_forecast(WeatherLocationForecast *forecast) {
  free(forecast);
}

// Ein Satz v4.<minor> fuer [satz] in der Laenge seiner Fassung; die Werte
// unterscheiden sich je Feld, damit eine Verwechslung auffaellt.
static int prv_satz(uint8_t *satz, uint8_t minor, int16_t breite, int16_t laenge) {
  memset(satz, 0, sizeof(WeatherDBEntry) + 64);
  WeatherDBEntry *e = (WeatherDBEntry *)satz;
  e->version = WEATHER_DB_CURRENT_VERSION;
  e->minor_version = minor;
  e->current_temp = 22;
  e->current_weather_type = WeatherType_CloudyDay;
  e->today_uv_index_x10 = 47;
  e->latitude_e2 = breite;
  e->longitude_e2 = laenge;
  e->num_daily = 2;
  e->daily[0].weather_type = WeatherType_Sun;
  e->daily[1].weather_type = WeatherType_HeavySnow;
  e->location_utc_offset_min = 120;
  for (int h = 0; h < 24; h++) {
    e->today_hourly_uv_x10[h] = (uint8_t)(h * 3 + 1); // 1, 4, 7 ... 70
  }
  if (minor >= 5) {
    return (int)sizeof(WeatherDBEntry) + 8; // dazu zwei leere Zeichenketten
  }
  return (int)weather_db_entry_strings_offset(WEATHER_DB_CURRENT_VERSION, minor) + 8;
}

void test_timestyle_wetter_quelle__initialize(void) {
  s_orte = 1;
  s_ohne_prefs = false;
  s_ohne_vorhersage = false;
  s_laenge0 = prv_satz(s_satz0, 5, 4737, 854);
  s_laenge1 = prv_satz(s_satz1, 5, -3386, 15121);
  ((WeatherDBEntry *)s_satz1)->today_uv_index_x10 = 99;
  ((WeatherDBEntry *)s_satz1)->location_utc_offset_min = 600;
  s_gelesen_ort = -1;
  s_vorhersage = (WeatherLocationForecast){
      .current_temp = 22,
      .today_high = 24,
      .today_low = 12,
      .current_weather_type = WeatherType_CloudyDay,
      .time_updated_utc = 1790985600,
  };
}

void test_timestyle_wetter_quelle__cleanup(void) {
  fake_pbl_malloc_check_net_allocs();
}

// --- Was gelesen wird ---

void test_timestyle_wetter_quelle__ohne_vorhersage_kein_wetter(void) {
  s_ohne_vorhersage = true;
  WetterStand w;
  cl_assert(!wetter_quelle_lesen(&w));
}

void test_timestyle_wetter_quelle__die_vorhersage_gibt_temperaturen_und_stand(void) {
  WetterStand w;
  cl_assert(wetter_quelle_lesen(&w));
  cl_assert_equal_i(w.temperatur, 22);
  cl_assert_equal_i(w.hoch, 24);
  cl_assert_equal_i(w.tief, 12);
  cl_assert_equal_i(w.art, WeatherType_CloudyDay);
  cl_assert_equal_i((int)w.stand_utc, 1790985600);
}

void test_timestyle_wetter_quelle__satz_v4_5_gibt_alles(void) {
  WetterStand w;
  cl_assert(wetter_quelle_lesen(&w));
  cl_assert_equal_i(s_gelesen_ort, 0);
  cl_assert_equal_i(w.breite_e2, 4737);
  cl_assert_equal_i(w.laenge_e2, 854);
  cl_assert_equal_i(w.art_heute, WeatherType_Sun); // daily[0], nicht daily[1]
  cl_assert_equal_i(w.uv_tag_x10, 47);
  cl_assert_equal_i(w.ort_versatz_min, 120);
  cl_assert(w.uv_stunden_da);
  cl_assert_equal_i(w.uv_stunden_x10[0], 1);
  cl_assert_equal_i(w.uv_stunden_x10[13], 40);
  cl_assert_equal_i(w.uv_stunden_x10[23], 70);
}

void test_timestyle_wetter_quelle__es_gilt_ort_0_der_reihenfolge(void) {
  // Wie weather_service_create_default_forecast: der erste Ort, nicht der zweite.
  s_orte = 2;
  WetterStand w;
  cl_assert(wetter_quelle_lesen(&w));
  cl_assert_equal_i(s_gelesen_ort, 0);
  cl_assert_equal_i(w.uv_tag_x10, 47);
  cl_assert_equal_i(w.ort_versatz_min, 120);
}

void test_timestyle_wetter_quelle__uv_je_stunde_erst_ab_v4_4(void) {
  WetterStand w;
  // v4.4 in seiner eigenen Laenge: da.
  s_laenge0 = prv_satz(s_satz0, 4, 4737, 854);
  cl_assert(wetter_quelle_lesen(&w));
  cl_assert(w.uv_stunden_da);
  cl_assert_equal_i(w.uv_stunden_x10[23], 70);
  // v4.3, selbst wenn der Satz lang genug waere: nicht da (das Feld ist dort etwas anderes).
  prv_satz(s_satz0, 3, 4737, 854);
  s_laenge0 = (int)sizeof(WeatherDBEntry);
  cl_assert(wetter_quelle_lesen(&w));
  cl_assert(!w.uv_stunden_da);
  cl_assert_equal_i(w.uv_tag_x10, 47); // der Tageswert bleibt
}

void test_timestyle_wetter_quelle__zu_kurzer_satz_hat_keine_stundenwerte(void) {
  // Sagt v4.4, ist aber nur so lang wie v4.3: die Stundenwerte fehlen.
  prv_satz(s_satz0, 4, 4737, 854);
  s_laenge0 = (int)WEATHER_DB_V4_4_FIXED_SIZE - 1;
  WetterStand w;
  cl_assert(wetter_quelle_lesen(&w));
  cl_assert(!w.uv_stunden_da);
  cl_assert_equal_i(w.ort_versatz_min, 120); // v4.1 passt noch
  // Genau die Laenge von v4.4: da.
  s_laenge0 = (int)WEATHER_DB_V4_4_FIXED_SIZE;
  cl_assert(wetter_quelle_lesen(&w));
  cl_assert(w.uv_stunden_da);
}

void test_timestyle_wetter_quelle__zeitzone_des_ortes_erst_ab_v4_1(void) {
  WetterStand w;
  s_laenge0 = prv_satz(s_satz0, 0, 4737, 854);
  cl_assert(wetter_quelle_lesen(&w));
  cl_assert_equal_i(w.ort_versatz_min, INT16_MIN);
  cl_assert(!w.uv_stunden_da);
  // v4.0, selbst wenn der Satz lang genug waere: unbekannt (dort steht kein Versatz).
  s_laenge0 = (int)sizeof(WeatherDBEntry);
  cl_assert(wetter_quelle_lesen(&w));
  cl_assert_equal_i(w.ort_versatz_min, INT16_MIN);
  // v4.1, aber nur so lang wie v4.0: ebenfalls unbekannt.
  prv_satz(s_satz0, 1, 4737, 854);
  s_laenge0 = (int)WEATHER_DB_V4_1_FIXED_SIZE - 1;
  cl_assert(wetter_quelle_lesen(&w));
  cl_assert_equal_i(w.ort_versatz_min, INT16_MIN);
  s_laenge0 = (int)WEATHER_DB_V4_1_FIXED_SIZE;
  cl_assert(wetter_quelle_lesen(&w));
  cl_assert_equal_i(w.ort_versatz_min, 120);
}

void test_timestyle_wetter_quelle__satz_v3_oder_zu_kurz_gibt_nur_die_vorhersage(void) {
  WetterStand w;
  ((WeatherDBEntry *)s_satz0)->version = 3;
  cl_assert(wetter_quelle_lesen(&w));
  cl_assert_equal_i(w.uv_tag_x10, -1);
  cl_assert_equal_i(w.breite_e2, INT16_MIN);
  cl_assert_equal_i(w.art_heute, WeatherType_CloudyDay); // = art
  cl_assert(!w.uv_stunden_da);
  // v4, aber kuerzer als das Feste von v4.0 (kaputter Satz): ebenso.
  s_laenge0 = prv_satz(s_satz0, 5, 4737, 854);
  s_laenge0 = (int)WEATHER_DB_V4_0_FIXED_SIZE - 1;
  cl_assert(wetter_quelle_lesen(&w));
  cl_assert_equal_i(w.uv_tag_x10, -1);
  cl_assert_equal_i(w.ort_versatz_min, INT16_MIN);
}

void test_timestyle_wetter_quelle__ohne_orte_nur_die_vorhersage(void) {
  WetterStand w;
  s_orte = 0;
  cl_assert(wetter_quelle_lesen(&w));
  cl_assert_equal_i(s_gelesen_ort, -1);
  cl_assert_equal_i(w.uv_tag_x10, -1);
  s_ohne_prefs = true;
  cl_assert(wetter_quelle_lesen(&w));
  cl_assert_equal_i(w.uv_tag_x10, -1);
}

void test_timestyle_wetter_quelle__uv_tag_unbekannt_bleibt_unbekannt(void) {
  ((WeatherDBEntry *)s_satz0)->today_uv_index_x10 = -1;
  WetterStand w;
  cl_assert(wetter_quelle_lesen(&w));
  cl_assert_equal_i(w.uv_tag_x10, -1);
}

// --- Was an wetter_uv_jetzt geht (wetter_stand_uv) ---

// Sa 3.10.2026 00:00 UTC.
#define MITTERNACHT_UTC 1790985600

void test_timestyle_wetter_quelle__uv_jetzt_aus_der_stunde_des_ortes(void) {
  WetterStand w;
  cl_assert(wetter_quelle_lesen(&w));
  w.stand_utc = MITTERNACHT_UTC + 9 * 3600;
  // 11:10 UTC = 13:10 am Ort (+120 min): Stunde 13 -> 40 -> UV 4. Die Uhr steht
  // auf UTC (0) - es zaehlt die Zeit des Ortes.
  cl_assert_equal_i(wetter_stand_uv(&w, 0, MITTERNACHT_UTC + 11 * 3600 + 600), 4);
}

void test_timestyle_wetter_quelle__ohne_zeitzone_des_ortes_die_der_uhr(void) {
  WetterStand w;
  s_laenge0 = prv_satz(s_satz0, 0, 4737, 854); // v4.0: weder Zone noch Stunden
  cl_assert(wetter_quelle_lesen(&w));
  w.uv_stunden_da = true; // Stundenwerte von Hand, Zone bleibt unbekannt
  for (int h = 0; h < 24; h++) {
    w.uv_stunden_x10[h] = (uint8_t)(h * 3 + 1);
  }
  w.stand_utc = MITTERNACHT_UTC + 9 * 3600;
  // 11:10 UTC mit Uhr auf +3 h = 14:10: Stunde 14 -> 43 -> 4; mit +5 h: 16 -> 49 -> 5.
  cl_assert_equal_i(wetter_stand_uv(&w, 3 * 3600, MITTERNACHT_UTC + 11 * 3600 + 600), 4);
  cl_assert_equal_i(wetter_stand_uv(&w, 5 * 3600, MITTERNACHT_UTC + 11 * 3600 + 600), 5);
}

void test_timestyle_wetter_quelle__ohne_stundenwerte_der_tageswert(void) {
  WetterStand w;
  cl_assert(wetter_quelle_lesen(&w));
  w.stand_utc = MITTERNACHT_UTC + 9 * 3600;
  // Die Werte stehen im Feld, gelten aber nicht (aelterer Satz): Tageswert 47 -> 4.
  w.uv_stunden_da = false;
  w.uv_stunden_x10[13] = 110;
  cl_assert_equal_i(wetter_stand_uv(&w, 0, MITTERNACHT_UTC + 11 * 3600 + 600), 4);
  w.uv_stunden_da = true;
  cl_assert_equal_i(wetter_stand_uv(&w, 0, MITTERNACHT_UTC + 11 * 3600 + 600), 11);
}
