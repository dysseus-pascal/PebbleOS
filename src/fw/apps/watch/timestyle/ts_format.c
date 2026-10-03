/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "ts_format.h"

#include "languages.h"

#include <string.h>

// Die Vorgabe je Widget-Platz, auch fuer ein ungueltiges Widget dort.
static const SidebarWidgetType s_widget_vorgabe[3] = {WEATHER_CURRENT, STEP_COUNTER, DATE};

void ts_format_vorgaben(Settings *s) {
  // Zuerst alles auf null: der Satz ist eine statische Variable, und eine
  // System-App behaelt sie ueber ihr Ende hinaus.
  memset(s, 0, sizeof(*s));

  // Das Foto: schwarzer Grund, weisse LECO-Ziffern, weisse Leiste rechts mit
  // schwarzer Schrift; oben Wetter, Mitte Schritte, unten das Datum.
  s->timeColor = GColorWhite;
  s->timeBgColor = GColorBlack;
  s->sidebarColor = GColorWhite;
  s->sidebarTextColor = GColorBlack;

  s->languageId = LANGUAGE_EN;
  s->showLeadingZero = false;
  s->clockFontId = FONT_SETTING_LECO;

  s->btVibe = false;
  s->hourlyVibe = NO_VIBE;

  for (int i = 0; i < 3; i++) {
    s->widgets[i] = s_widget_vorgabe[i];
  }
  s->sidebarOnLeft = false;
  s->useLargeFonts = false;
  s->activateDisconnectIcon = true;

  // Nicht gelesen: die Temperaturen kommen schon in Boulders Einheit.
  s->useMetric = true;

  s->showBatteryPct = true;
  s->disableAutobattery = false;

  strncpy(s->altclockName, "ALT", sizeof(s->altclockName));
  s->altclockOffset = 0;

  s->healthUseDistance = false;
  s->healthUseRestfulSleep = false;
  s->decimalSeparator = '.';
}

bool ts_format_sprache_erlaubt(uint8_t id) {
  // Ohne GK, VN, RU, CN, UK, JP, KR, IW: dafuer fehlen den eingebauten
  // Gothic-Schriften die Zeichen, es kaemen nur Ersatzkaestchen.
  switch (id) {
    case LANGUAGE_GK:
    case LANGUAGE_VN:
    case LANGUAGE_RU:
    case LANGUAGE_CN:
    case LANGUAGE_UK:
    case LANGUAGE_JP:
    case LANGUAGE_KR:
    case LANGUAGE_IW:
      return false;
    default:
      return id <= LANGUAGE_IW;
  }
}

static GColor prv_farbe(uint8_t wert) {
  // Deckend erzwingen: jede Zahl ist dann eine der 64 Farben.
  return (GColor8){.argb = (uint8_t)(wert | 0xC0)};
}

static bool prv_bit(uint8_t flags, int nummer) {
  return (flags >> nummer) & 1;
}

void ts_format_dekodieren(const uint8_t *b, size_t len, Settings *s) {
  ts_format_vorgaben(s);
  if (b == NULL || len != TS_FORMAT_LAENGE || b[0] == 0) {
    return;
  }

  // Ab hier Fassung 1. Freie Bits und Bytes werden nicht angesehen, so
  // bleibt eine spaetere Fassung mit mehr Feldern lesbar.
  s->timeColor = prv_farbe(b[1]);
  s->timeBgColor = prv_farbe(b[2]);
  s->sidebarColor = prv_farbe(b[3]);
  s->sidebarTextColor = prv_farbe(b[4]);

  if (ts_format_sprache_erlaubt(b[5])) {
    s->languageId = b[5];
  }
  // Byte 6 (Schrift) bleibt ungelesen: eingebaut ist nur LECO.
  if (b[7] <= VIBE_EVERY_HALF_HOUR) {
    s->hourlyVibe = (VibeIntervalType)b[7];
  }

  for (int i = 0; i < 3; i++) {
    const uint8_t w = b[8 + i];
    // Das Trennungssymbol setzt nur die Leiste selbst ein, waehlbar ist es nicht.
    if (w != BLUETOOTH_DISCONNECT && w <= WEATHER_UV_INDEX) {
      s->widgets[i] = (SidebarWidgetType)w;
    }
  }

  const int versatz = (int8_t)b[11];
  if (versatz >= -12 && versatz <= 14) {
    s->altclockOffset = versatz;
  }
  if (b[12] == '.' || b[12] == ',') {
    s->decimalSeparator = (char)b[12];
  }

  const uint8_t a = b[13];
  s->showLeadingZero = prv_bit(a, 0);
  s->btVibe = prv_bit(a, 1);
  s->sidebarOnLeft = prv_bit(a, 2);
  s->useLargeFonts = prv_bit(a, 3);
  s->activateDisconnectIcon = prv_bit(a, 4);
  s->showBatteryPct = prv_bit(a, 6);
  s->disableAutobattery = prv_bit(a, 7);

  const uint8_t flags_b = b[14];
  s->healthUseDistance = prv_bit(flags_b, 0);
  s->healthUseRestfulSleep = prv_bit(flags_b, 1);

  // Hoechstens 7 Byte und immer mit Null am Ende, egal was in Byte 23 steht.
  // Ungueltiges UTF-8 bleibt, wie es ist: die Schrift zeigt dann nichts.
  _Static_assert(sizeof(s->altclockName) == 8, "Name: 7 Byte und die Null");
  memcpy(s->altclockName, &b[16], sizeof(s->altclockName) - 1);
  s->altclockName[sizeof(s->altclockName) - 1] = '\0';
}
