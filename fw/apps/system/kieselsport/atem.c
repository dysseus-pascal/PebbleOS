/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Nur mit der Variante: fw/apps/system/ baut jedes Board (siehe kieselsport.c).
#ifdef CONFIG_BOULDER_KIESELSPORT

#include "atem.h"

#include "applib/ui/animation_timing.h"

// GUT 15 BILDER JE SEKUNDE: der Kreis gleitet ohne Ruck, und das Zeichnen kostet halb so viel wie
// mit Pebbles 30.
#define BILD_MS 66

#define LAENGE(a) (sizeof(a) / sizeof((a)[0]))

static uint32_t prv_phase_ms(const Atemrhythmus *r, AtemWert phase) {
  return (uint32_t)r->wert[phase] * 1000;
}

uint32_t atem_runde_ms(const Atemrhythmus *r) {
  uint32_t ms = 0;
  for (AtemWert p = AtemEin; p <= AtemLeer; p++) {
    ms += prv_phase_ms(r, p);
  }
  return ms;
}

uint32_t atem_gesamt_ms(const Atemrhythmus *r) {
  return atem_runde_ms(r) * r->wert[AtemRunden];
}

Atemstand atem_stand(const Atemrhythmus *r, uint32_t ms) {
  const uint32_t runde_ms = atem_runde_ms(r);
  const uint8_t runden = r->wert[AtemRunden];
  if (runde_ms == 0 || runden == 0 || ms >= runde_ms * runden) {
    AtemWert letzte = AtemLeer;
    while (letzte > AtemEin && r->wert[letzte] == 0) {
      letzte--;
    }
    return (Atemstand){
      .phase = letzte,
      .runde = runden,
      .dauer_ms = prv_phase_ms(r, letzte),
      .fertig = true,
    };
  }
  uint32_t in_runde = ms % runde_ms;
  AtemWert p = AtemEin;
  // Endet, weil in_runde kleiner ist als die Summe der vier Phasen.
  while (in_runde >= prv_phase_ms(r, p)) {
    in_runde -= prv_phase_ms(r, p);
    p++;
  }
  return (Atemstand){
    .phase = p,
    .runde = (uint8_t)(ms / runde_ms + 1),
    .dauer_ms = prv_phase_ms(r, p),
    .rest_ms = prv_phase_ms(r, p) - in_runde,
  };
}

uint32_t atem_rest_s(const Atemstand *s) {
  return (s->rest_ms + 999) / 1000;
}

int16_t atem_radius(const Atemstand *s, int16_t r_min, int16_t r_max) {
  if (s->fertig || s->phase == AtemLeer) {
    return r_min;
  }
  if (s->phase == AtemHalten) {
    return r_max;
  }
  const AnimationProgress t = (AnimationProgress)((uint64_t)(s->dauer_ms - s->rest_ms) *
                                                  ANIMATION_NORMALIZED_MAX / s->dauer_ms);
  const int32_t kurve = animation_timing_curve(t, AnimationCurveEaseInOut);
  const int32_t weg =
      ((int32_t)(r_max - r_min) * kurve + ANIMATION_NORMALIZED_MAX / 2) / ANIMATION_NORMALIZED_MAX;
  return (int16_t)(s->phase == AtemEin ? r_min + weg : r_max - weg);
}

uint32_t atem_naechstes_bild_ms(const Atemstand *s) {
  if (s->fertig) {
    return 0;
  }
  // Die Zahl springt, wenn der Rest eine ganze Sekunde erreicht (atem_rest_s).
  uint32_t ms = s->rest_ms % 1000;
  if (ms == 0) {
    ms = 1000;
  }
  if ((s->phase == AtemEin || s->phase == AtemAus) && ms > BILD_MS) {
    ms = BILD_MS;
  }
  return ms;
}

static const uint32_t s_ein[] = {100};
static const uint32_t s_halten[] = {50, 100, 50};
static const uint32_t s_aus[] = {300};
// Zwei kurze und ein langes: das hat keine Phase.
static const uint32_t s_ende[] = {150, 100, 150, 100, 500};

VibePattern atem_muster(AtemWert phase) {
  switch (phase) {
    case AtemEin:
      return (VibePattern){.durations = s_ein, .num_segments = LAENGE(s_ein)};
    case AtemAus:
      return (VibePattern){.durations = s_aus, .num_segments = LAENGE(s_aus)};
    default:
      return (VibePattern){.durations = s_halten, .num_segments = LAENGE(s_halten)};
  }
}

VibePattern atem_muster_ende(void) {
  return (VibePattern){.durations = s_ende, .num_segments = LAENGE(s_ende)};
}

#endif  // CONFIG_BOULDER_KIESELSPORT
