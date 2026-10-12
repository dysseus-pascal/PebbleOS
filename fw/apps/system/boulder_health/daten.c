/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#ifdef CONFIG_BOULDER_HEALTH_APP

#include "daten.h"

#include "pbl/services/clock.h"
#include "shell/prefs_syscalls.h"
#include "syscall/syscall.h"

#include <string.h>

// SpO2 der letzten 24 h: Heart zeigt den letzten Wert, das Detail die Punkte.
#define SPO2_FENSTER_S (24 * 60 * 60)

static int32_t prv_metrik(ActivityMetric metrik) {
  int32_t wert = 0;
  return sys_activity_get_metric(metrik, 1, &wert) ? wert : 0;
}

static void prv_uhr(BhDaten *d) {
  d->jetzt = sys_get_time();
  struct tm ort = {0};
  sys_localtime_r(&d->jetzt, &ort);
  d->utc_zu_ort = ort.tm_gmtoff;
  d->heute = bh_tag(ort.tm_year, ort.tm_yday);
  d->minute = (uint16_t)(ort.tm_hour * 60 + ort.tm_min);
  d->wochentag = bh_wochentag(d->heute);
  d->h24 = clock_is_24h_style();
  d->meilen = (sys_shell_prefs_get_units_distance() == UnitsDistance_Miles);
}

// Die Merker in `offen` stehen fuer genau die Syscalls, die nach dem Boot "noch nicht" sagen.
static void prv_merke(BhDaten *d, BhQuelle quelle, bool fehlt) {
  if (fehlt) {
    d->offen |= quelle;
  } else {
    d->offen &= (uint8_t)~quelle;
  }
}

static void prv_lage(BhDaten *d) {
  d->lage_da = sys_boulder_lage(&d->lage);
  prv_merke(d, BhQuelleLage, !d->lage_da);
}

static void prv_nacht(BhDaten *d) {
  d->nacht_lesen = sys_boulder_nacht(&d->nacht);
  prv_merke(d, BhQuelleNacht, d->nacht_lesen == BoulderLesenSpaeter);
}

static void prv_naechte(BhDaten *d) {
  d->naechte_da = sys_boulder_naechte(&d->naechte);
  prv_merke(d, BhQuelleNaechte, !d->naechte_da);
}

static void prv_herz(BhDaten *d) {
  d->herz_da = sys_boulder_herz(&d->herz);
  prv_merke(d, BhQuelleHerz, !d->herz_da);
}

static void prv_woche(BhDaten *d) {
  d->woche_da = sys_boulder_woche(&d->woche);
  prv_merke(d, BhQuelleWoche, !d->woche_da);
}

static void prv_spo2(BhDaten *d) {
  d->spo2_anzahl =
      sys_boulder_spo2(d->jetzt - SPO2_FENSTER_S, d->jetzt + 1, d->spo2, SPO2_WERTE_MAX);
}

static void prv_pebble(BhDaten *d) {
  d->schritte = prv_metrik(ActivityMetricStepCount);
  d->meter = prv_metrik(ActivityMetricDistanceMeters);
  d->aktiv_s = prv_metrik(ActivityMetricActiveSeconds);
  d->kcal = prv_metrik(ActivityMetricRestingKCalories) + prv_metrik(ActivityMetricActiveKCalories);
  d->sitzungen_anzahl = ACTIVITY_MAX_ACTIVITY_SESSIONS_COUNT;
  if (!sys_activity_get_sessions(&d->sitzungen_anzahl, d->sitzungen)) {
    d->sitzungen_anzahl = 0;
  }
  bh_daten_puls(d);
}

void bh_daten_puls(BhDaten *d) {
  d->puls = prv_metrik(ActivityMetricHeartRateRawBPM);
  d->puls_zeit = (time_t)prv_metrik(ActivityMetricHeartRateRawUpdatedTimeUTC);
}

void bh_daten_laden(BhDaten *d) {
  memset(d, 0, sizeof(*d));
  prv_uhr(d);
  prv_lage(d);
  prv_nacht(d);
  prv_naechte(d);
  prv_herz(d);
  prv_woche(d);
  prv_spo2(d);
  prv_pebble(d);
}

bool bh_daten_nachladen(BhDaten *d) {
  if (!d->offen || d->nachfragen >= BH_NACHFRAGEN) {
    return false;
  }
  d->nachfragen++;
  if (d->offen & BhQuelleLage) {
    prv_lage(d);
  }
  if (d->offen & BhQuelleNacht) {
    prv_nacht(d);
  }
  if (d->offen & BhQuelleNaechte) {
    prv_naechte(d);
  }
  if (d->offen & BhQuelleHerz) {
    prv_herz(d);
  }
  if (d->offen & BhQuelleWoche) {
    prv_woche(d);
  }
  return d->offen && d->nachfragen < BH_NACHFRAGEN;
}

void bh_daten_auffrischen(BhDaten *d) {
  // WARUM S1 UND S5 NUR BEI NEUER NACHT: Sie aendern sich nur, wenn eine Nacht beginnt oder
  // endet, und das meldet die Lage (nacht_stufe, fenster_utc). So bleibt der Minutentakt bei den
  // kleinen Kopien.
  const uint8_t stufe = d->lage.nacht_stufe;
  const uint32_t fenster = d->lage.fenster_utc;
  const bool war_da = d->lage_da;
  prv_uhr(d);
  prv_lage(d);
  if (!war_da || d->lage.nacht_stufe != stufe || d->lage.fenster_utc != fenster ||
      (d->offen & (BhQuelleNacht | BhQuelleNaechte))) {
    prv_nacht(d);
    prv_naechte(d);
  }
  prv_herz(d);
  prv_woche(d);
  prv_spo2(d);
  prv_pebble(d);
}

#endif // CONFIG_BOULDER_HEALTH_APP
