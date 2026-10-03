/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder: aus den Wetterdaten der Firmware das Symbol von TimeStyle. Rein
// rechnend (keine Firmware-Aufrufe ausser der Ganzzahl-Trigonometrie), damit
// es ein Host-Test pruefen kann.
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

// TimeStyles Wettersymbole. Stand vorher in weather.h; Namen und Werte
// unveraendert (die Store-App bekam sie so vom Telefon).
typedef enum {
  CLEAR_DAY = 0,
  CLEAR_NIGHT = 1,
  CLOUDY_DAY = 2,
  HEAVY_RAIN = 3,
  HEAVY_SNOW = 4,
  LIGHT_RAIN = 5,
  LIGHT_SNOW = 6,
  PARTLY_CLOUDY_NIGHT = 7,
  PARTLY_CLOUDY = 8,
  RAINING_AND_SNOWING = 9,
  THUNDERSTORM = 10,
  WEATHER_GENERIC = 11
} WeatherCondition;

// Wie alt die Wetterdaten hoechstens sein duerfen. Boulder holt stuendlich;
// nach drei verpassten Abrufen zeigt das Zifferblatt lieber nichts als ein
// Wetter von gestern.
#define WETTER_HOECHSTALTER_S (3 * 60 * 60)

// Die Wetterart der Firmware (WeatherType, 0..8 oder 255) als TimeStyle-
// Symbol. Sonne und "teils bewoelkt" haben ein Nachtsymbol wie in TimeStyle;
// ein Gewitter kennt die Firmware nicht. Unbekanntes: das allgemeine Symbol.
WeatherCondition wetter_symbol(uint8_t wetterart, bool nacht);

// Ob die Sonne fuer diesen Ort gerade unter dem Horizont steht (mit der
// ueblichen Brechung von 0,833 Grad). Ort in Hundertstelgrad, Nord und Ost
// positiv; INT16_MIN heisst unbekannt - dann gilt Tag, wie ohne Ort.
// tag_im_jahr 1..366 und minute_utc 0..1439, beides in UTC: so braucht es
// keine Zeitzone. Polarnacht und Mitternachtssonne ergeben sich von selbst.
bool wetter_ist_nacht(int16_t breite_e2, int16_t laenge_e2, int tag_im_jahr, int minute_utc);

// Ob Daten mit diesem Stand (UTC) jetzt noch gelten: vorhanden, hoechstens
// WETTER_HOECHSTALTER_S alt und nicht mehr als eine Stunde in der Zukunft
// (eine falsch gestellte Uhr soll altes Wetter nicht ewig frisch halten).
bool wetter_frisch(time_t stand_utc, time_t jetzt_utc);
