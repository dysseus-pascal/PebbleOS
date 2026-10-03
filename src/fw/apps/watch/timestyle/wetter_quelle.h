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
  int uv;            // UV-Index heute, -1 wenn unbekannt
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
