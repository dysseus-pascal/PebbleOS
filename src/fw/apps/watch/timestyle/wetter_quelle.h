/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder: das Wetter fuer TimeStyle aus dem Wetterspeicher der Firmware.
// Ersetzt messaging.c und PebbleKit JS der Store-App: Boulder fuellt den
// Speicher ohnehin (stuendlicher Abruf fuer die Wetter-App), also fragt das
// Zifferblatt nie selbst beim Telefon.
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

typedef struct {
  // Temperaturen in der Einheit, die das Telefon eingestellt hat.
  int temperatur;
  int hoch;          // heute, WETTER_UNBEKANNT wenn unbekannt
  int tief;          // heute, WETTER_UNBEKANNT wenn unbekannt
  int uv_tag_x10;    // UV-Index des Tages * 10, -1 wenn unbekannt
  bool uv_stunden_da; // ob uv_stunden_x10 gilt (Satz v4.4 oder neuer)
  uint8_t uv_stunden_x10[24]; // UV * 10 je Stunde 0-23 am Wetterort, 255 unbekannt
  int16_t ort_versatz_min;    // Zeitzone des Wetterorts in Minuten, INT16_MIN unbekannt
  uint8_t art;       // WeatherType jetzt
  uint8_t art_heute; // WeatherType fuer den ganzen Tag (v4), sonst = art
  int16_t breite_e2; // Ort, INT16_MIN wenn unbekannt (Satz vor v4)
  int16_t laenge_e2;
  time_t stand_utc;
} WetterStand;

#define WETTER_UNBEKANNT (INT16_MAX)

// Liest den Standardort. false, wenn es keinen gueltigen Eintrag gibt.
// Ob er frisch genug ist, entscheidet der Aufrufer (wetter_frisch).
bool wetter_quelle_lesen(WetterStand *aus);

// Der UV-Index JETZT aus einem gelesenen Stand (wetter_uv_jetzt): die
// Stundenwerte nur, wenn der Satz sie hatte (uv_stunden_da, ab v4.4), sonst der
// Tageswert. [uhr_versatz_s] ist tm_gmtoff der Uhr - es gilt nur, wenn der Satz
// die Zeitzone des Ortes nicht kennt. -1: unbekannt.
// WARUM HIER und nicht in weather.c: so prueft der Host-Test
// (test_timestyle_wetter_quelle) auch, WELCHE Werte an wetter_uv_jetzt gehen.
int wetter_stand_uv(const WetterStand *stand, int32_t uhr_versatz_s, time_t jetzt_utc);
