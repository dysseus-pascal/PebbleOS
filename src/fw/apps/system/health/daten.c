/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "daten.h"

#include "applib/health_service.h"
#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/boulder/spo2.h"

// Die Minutenhistorie wird in Stuecken gelesen: ein Tag sind 1440 Saetze zu
// je 16 Byte, und so viel Stapel hat die App nicht.
#define STUECK 60

static int32_t prv_metrik(ActivityMetric m) {
  int32_t wert = 0;
  activity_get_metric(m, 1, &wert);
  return wert > 0 ? wert : 0;
}

// Die letzten 24 Stunden in einem Lauf: Schritte je Stunde fuer heute, Puls
// je halbe Stunde fuer die Kurve.
static void prv_minuten(HealthDaten *d, time_t jetzt, time_t mitternacht) {
  HealthMinuteData *puffer = app_malloc_check(STUECK * sizeof(HealthMinuteData));
  const time_t kurve_beginn = jetzt - 24 * SECONDS_PER_HOUR;
  time_t ab = kurve_beginn;
  while (ab < jetzt) {
    time_t von = ab;
    time_t bis = jetzt;
    const uint32_t n = health_service_get_minute_history(puffer, STUECK, &von, &bis);
    if (n == 0 || bis <= ab) {
      break;
    }
    for (uint32_t i = 0; i < n; i++) {
      const time_t t = von + (time_t)i * SECONDS_PER_MINUTE;
      const HealthMinuteData *m = &puffer[i];
      if (m->is_invalid) {
        continue;
      }
      if (t >= mitternacht) {
        const int stunde = (t - mitternacht) / SECONDS_PER_HOUR;
        if (stunde >= 0 && stunde < 24) {
          d->schritte_stunde[stunde] += m->steps;
        }
      }
      const int spalte = (t - kurve_beginn) / (30 * SECONDS_PER_MINUTE);
      const uint8_t bpm = m->heart_rate_bpm;
      if (bpm > 0 && spalte >= 0 && spalte < DATEN_PULS_SPALTEN) {
        if (d->puls_min[spalte] == 0 || bpm < d->puls_min[spalte]) {
          d->puls_min[spalte] = bpm;
        }
        if (bpm > d->puls_max[spalte]) {
          d->puls_max[spalte] = bpm;
        }
      }
    }
    ab = bis;
  }
  app_free(puffer);
}

static void prv_spo2(HealthDaten *d, time_t jetzt) {
  Spo2Wert *werte = app_malloc_check(SPO2_WERTE_MAX * sizeof(Spo2Wert));
  const int n = boulder_spo2_werte(jetzt - 24 * SECONDS_PER_HOUR, jetzt + 1, werte, SPO2_WERTE_MAX);
  if (n > 0) {
    d->spo2 = werte[n - 1].prozent;
    d->spo2_zeit = werte[n - 1].zeit;
  }
  // DER TIEFSTE WERT DER NACHT sagt mehr als der Schnitt: ein Aussetzer der
  // Atmung zeigt sich als Einbruch, nicht als verschobenes Mittel.
  if (d->nacht_da) {
    const time_t von = d->nacht.beginn_utc + d->nacht.einschlafen * SECONDS_PER_MINUTE;
    const time_t bis = d->nacht.beginn_utc + d->nacht.aufwachen * SECONDS_PER_MINUTE;
    for (int i = 0; i < n; i++) {
      if (werte[i].zeit >= von && werte[i].zeit < bis &&
          (d->spo2_nacht_tief == 0 || werte[i].prozent < d->spo2_nacht_tief)) {
        d->spo2_nacht_tief = werte[i].prozent;
      }
    }
  }
  app_free(werte);
}

void daten_puls(HealthDaten *d) {
  const HealthValue bpm = health_service_peek_current_value(HealthMetricHeartRateBPM);
  const uint8_t neu = (bpm > 0 && bpm < 256) ? (uint8_t)bpm : 0;
  if (neu && neu != d->puls) {
    d->puls_zeit = time(NULL);
  }
  if (neu) {
    d->puls = neu;
  }
}

void daten_sammeln(HealthDaten *d) {
  memset(d, 0, sizeof(*d));
  const time_t jetzt = time(NULL);
  const time_t mitternacht = time_start_of_today();
  d->stunde_jetzt = (jetzt - mitternacht) / SECONDS_PER_HOUR;
  d->wochentag_heute = localtime(&jetzt)->tm_wday;

  d->schritte = prv_metrik(ActivityMetricStepCount);
  d->meter = prv_metrik(ActivityMetricDistanceMeters);
  d->kcal = prv_metrik(ActivityMetricActiveKCalories);
  d->aktiv_min = prv_metrik(ActivityMetricActiveSeconds) / SECONDS_PER_MINUTE;

  activity_get_metric(ActivityMetricStepCount, DATEN_TAGE, d->woche_schritte);
  int32_t schlaf_s[DATEN_TAGE] = { 0 };
  activity_get_metric(ActivityMetricSleepTotalSeconds, DATEN_TAGE, schlaf_s);
  for (int i = 0; i < DATEN_TAGE; i++) {
    d->woche_schlaf_min[i] = schlaf_s[i] > 0 ? schlaf_s[i] / SECONDS_PER_MINUTE : 0;
    if (d->woche_schritte[i] < 0) {
      d->woche_schritte[i] = 0;
    }
  }

  d->nacht_da = boulder_nacht_letzte(&d->nacht);

  // Der Puls: der gefilterte Wert der Uhr, bis der Sensor einen frischen
  // liefert (daten_puls).
  d->puls = (uint8_t)prv_metrik(ActivityMetricHeartRateFilteredBPM);
  d->puls_zeit = prv_metrik(ActivityMetricHeartRateFilteredUpdatedTimeUTC);

  prv_minuten(d, jetzt, mitternacht);
  prv_spo2(d, jetzt);
}
