/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "wetter_quelle.h"

#include "kernel/pbl_malloc.h"
#include "pbl/services/blob_db/watch_app_prefs_db.h"
#include "pbl/services/blob_db/weather_db.h"
#include "pbl/services/weather/weather_service.h"
#include "pbl/services/weather/weather_service_private.h"

#include <string.h>

// Was nur der v4-Satz kennt: Ort (fuer Sonne oder Mond), Wetter des ganzen
// Tages und UV. Derselbe Satz wie weather_service_create_default_forecast
// (Ort 0 der Reihenfolge); fehlt er oder ist er noch v3, bleibt das Feld leer.
static void prv_v4_dazu(WetterStand *aus) {
  SerializedWeatherAppPrefs *prefs = watch_app_prefs_get_weather();
  if (!prefs) {
    return;
  }
  if (prefs->num_locations == 0) {
    watch_app_prefs_destroy_weather(prefs);
    return;
  }
  WeatherDBKey schluessel = prefs->locations[0];
  watch_app_prefs_destroy_weather(prefs);

  const int laenge = weather_db_get_len((uint8_t *)&schluessel, sizeof(schluessel));
  // Ein kaputter Satz kann v4 sagen und nur v3 lang sein (siehe
  // weather_data_source.c): erst die Laenge, dann die Felder.
  if (laenge < (int)WEATHER_DB_V4_0_FIXED_SIZE) {
    return;
  }
  WeatherDBEntry *satz = task_zalloc_check(laenge);
  const status_t rv =
      weather_db_read((uint8_t *)&schluessel, sizeof(schluessel), (uint8_t *)satz, laenge);
  if (rv == S_SUCCESS && satz->version == WEATHER_DB_CURRENT_VERSION) {
    aus->breite_e2 = satz->latitude_e2;
    aus->laenge_e2 = satz->longitude_e2;
    if (satz->num_daily > 0) {
      aus->art_heute = satz->daily[0].weather_type;
    }
    if (satz->today_uv_index_x10 >= 0) {
      aus->uv = satz->today_uv_index_x10 / 10;
    }
  }
  task_free(satz);
}

bool wetter_quelle_lesen(WetterStand *aus) {
  memset(aus, 0, sizeof(*aus));
  WeatherLocationForecast *vorhersage = weather_service_create_default_forecast();
  if (!vorhersage) {
    return false;
  }
  aus->temperatur = vorhersage->current_temp;
  aus->hoch = vorhersage->today_high;
  aus->tief = vorhersage->today_low;
  aus->art = (uint8_t)vorhersage->current_weather_type;
  aus->art_heute = aus->art;
  aus->uv = -1;
  aus->breite_e2 = INT16_MIN;
  aus->laenge_e2 = INT16_MIN;
  aus->stand_utc = vorhersage->time_updated_utc;
  weather_service_destroy_default_forecast(vorhersage);

  prv_v4_dazu(aus);
  return true;
}
