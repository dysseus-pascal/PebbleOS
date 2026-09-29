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

void daten_minuten_beginnen(MinutenLesen *m) {
  memset(m, 0, sizeof(*m));
  m->jetzt = time(NULL);
  m->mitternacht = time_start_of_today();
  m->kurve_beginn = m->jetzt - 24 * SECONDS_PER_HOUR;
  m->ab = m->kurve_beginn;
}

// Ein Stueck der letzten 24 Stunden: Schritte je Stunde fuer heute, Puls je
// halbe Stunde fuer die Kurve.
bool daten_minuten_schritt(MinutenLesen *m, HealthDaten *d) {
  if (m->ab < m->jetzt) {
    HealthMinuteData *puffer = app_malloc_check(STUECK * sizeof(HealthMinuteData));
    time_t von = m->ab;
    time_t bis = m->jetzt;
    const uint32_t n = health_service_get_minute_history(puffer, STUECK, &von, &bis);
    for (uint32_t i = 0; i < n; i++) {
      const time_t t = von + (time_t)i * SECONDS_PER_MINUTE;
      const HealthMinuteData *e = &puffer[i];
      if (e->is_invalid) {
        continue;
      }
      if (t >= m->mitternacht) {
        const int stunde = (t - m->mitternacht) / SECONDS_PER_HOUR;
        if (stunde >= 0 && stunde < 24) {
          m->schritte_stunde[stunde] += e->steps;
        }
      }
      const int spalte = (t - m->kurve_beginn) / (30 * SECONDS_PER_MINUTE);
      const uint8_t bpm = e->heart_rate_bpm;
      if (bpm > 0 && spalte >= 0 && spalte < DATEN_PULS_SPALTEN) {
        if (m->puls_min[spalte] == 0 || bpm < m->puls_min[spalte]) {
          m->puls_min[spalte] = bpm;
        }
        if (bpm > m->puls_max[spalte]) {
          m->puls_max[spalte] = bpm;
        }
      }
    }
    app_free(puffer);
    // Nichts mehr da, oder die Historie kommt nicht weiter: fertig.
    m->ab = (n == 0 || bis <= m->ab) ? m->jetzt : bis;
    if (m->ab < m->jetzt) {
      return false;
    }
  }
  memcpy(d->schritte_stunde, m->schritte_stunde, sizeof(d->schritte_stunde));
  memcpy(d->puls_min, m->puls_min, sizeof(d->puls_min));
  memcpy(d->puls_max, m->puls_max, sizeof(d->puls_max));
  return true;
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

// Der rohe Wert des Sensors mit seiner Zeit: nur so laesst sich sagen, ob er
// gerade misst - ein gleichbleibender Puls aendert den Wert nicht.
void daten_puls(HealthDaten *d) {
  int32_t bpm = 0, zeit = 0;
  activity_get_metric(ActivityMetricHeartRateRawBPM, 1, &bpm);
  activity_get_metric(ActivityMetricHeartRateRawUpdatedTimeUTC, 1, &zeit);
  d->puls_live = bpm > 0 && bpm < 256 && time(NULL) - zeit <= 5;
  if (d->puls_live) {
    d->puls = (uint8_t)bpm;
    d->puls_zeit = zeit;
  }
}

void daten_sammeln(HealthDaten *d) {
  // Die Kurven aus der Minutenhistorie ueberleben das Neusammeln; sie liest
  // daten_minuten_schritt.
  uint16_t schritte_stunde[24];
  uint8_t puls_min[DATEN_PULS_SPALTEN];
  uint8_t puls_max[DATEN_PULS_SPALTEN];
  memcpy(schritte_stunde, d->schritte_stunde, sizeof(schritte_stunde));
  memcpy(puls_min, d->puls_min, sizeof(puls_min));
  memcpy(puls_max, d->puls_max, sizeof(puls_max));
  memset(d, 0, sizeof(*d));
  memcpy(d->schritte_stunde, schritte_stunde, sizeof(schritte_stunde));
  memcpy(d->puls_min, puls_min, sizeof(puls_min));
  memcpy(d->puls_max, puls_max, sizeof(puls_max));
  const time_t jetzt = time(NULL);
  const time_t mitternacht = time_start_of_today();
  d->stunde_jetzt = (jetzt - mitternacht) / SECONDS_PER_HOUR;

  d->schritte = prv_metrik(ActivityMetricStepCount);
  d->meter = prv_metrik(ActivityMetricDistanceMeters);
  d->kcal = prv_metrik(ActivityMetricActiveKCalories);
  d->aktiv_min = prv_metrik(ActivityMetricActiveSeconds) / SECONDS_PER_MINUTE;

  d->nacht_da = boulder_nacht_letzte(&d->nacht);

  // Der Puls: der gefilterte Wert der Uhr, bis der Sensor einen frischen
  // liefert (daten_puls).
  d->puls = (uint8_t)prv_metrik(ActivityMetricHeartRateFilteredBPM);
  d->puls_zeit = prv_metrik(ActivityMetricHeartRateFilteredUpdatedTimeUTC);

  prv_spo2(d, jetzt);
}
