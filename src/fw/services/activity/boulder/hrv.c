/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/activity/boulder/hrv.h"

#include "pbl/services/activity/boulder/messstufe.h"
#include "util/time/time.h"
#include <pbl/logging/logging.h>

#ifdef CONFIG_HRM
#include "pbl/services/hrm/hrm_manager.h"
#include "pbl/services/hrm/hrm_manager_private.h"
#endif

#include <string.h>

// Unter so vielen Intervallen ist es Rauschen, keine Messung.
#define MIN_INTERVALLE 30

typedef struct {
  time_t start;
  uint16_t rmssd;
} Fenster;

static Fenster s_fenster[HRV_FENSTER_MAX];
static int s_fenster_naechstes;   //< Ringpuffer: hierhin kommt das naechste

static bool s_misst;
static time_t s_start;
static uint16_t s_vorher;
static uint32_t s_summe;          //< Summe der quadrierten Differenzen
static uint16_t s_anzahl;
#ifdef CONFIG_HRM
static HRMSessionRef s_sitzung = HRM_INVALID_SESSION_REF;
#endif

// RMSSD laufend: Summe und Anzahl, die Wurzel erst am Ende. EIN INTERVALL, DAS
// UM MEHR ALS EIN DRITTEL VOM VORIGEN ABWEICHT, ist ein verpasster Schlag, kein
// Herz - sonst zoege ein einziger 1800er den Wert ins Absurde.
void boulder_hrv_intervall(uint16_t ppi) {
  if (ppi < 300 || ppi > 2000) {
    return;
  }
  if (s_vorher != 0) {
    const int d = (int)ppi - (int)s_vorher;
    const int grenze = (int)s_vorher / 3;
    if (d > -grenze && d < grenze && s_summe < 0xF0000000u) {
      s_summe += (uint32_t)(d * d);
      if (s_anzahl < 0xFFFF) {
        s_anzahl++;
      }
    }
  }
  s_vorher = ppi;
}

static uint32_t prv_wurzel(uint32_t x) {
  uint32_t r = 0;
  uint32_t bit = 1u << 30;
  while (bit > x) {
    bit >>= 2;
  }
  while (bit) {
    if (x >= r + bit) {
      x -= r + bit;
      r = (r >> 1) + bit;
    } else {
      r >>= 1;
    }
    bit >>= 2;
  }
  return r;
}

uint16_t boulder_hrv_rmssd(void) {
  return (s_anzahl < MIN_INTERVALLE) ? 0 : (uint16_t)prv_wurzel(s_summe / s_anzahl);
}

#ifdef CONFIG_HRM
static void prv_hrm_ereignis(PebbleHRMEvent *e, void *context) {
  if (s_misst && e->event_type == HRMEvent_HRV && e->hrv.ppi_ms) {
    boulder_hrv_intervall(e->hrv.ppi_ms);
  }
}
#endif

static void prv_beginne(time_t utc) {
  s_misst = true;
  s_start = utc;
  s_vorher = 0;
  s_summe = 0;
  s_anzahl = 0;
#ifdef CONFIG_HRM
  // Jede Sekunde, mit HRV. Ablauf nach etwas mehr als dem Fenster: geht das
  // Ende verloren (Neustart), gibt der Sensor von selbst auf.
  s_sitzung = hrm_manager_subscribe_with_callback(
      INSTALL_ID_INVALID, 1, (HRV_FENSTER_MIN + 1) * SECONDS_PER_MINUTE,
      HRMFeature_BPM | HRMFeature_HRV, false /*low_latency*/, prv_hrm_ereignis, NULL);
#endif
}

static void prv_beende(void) {
#ifdef CONFIG_HRM
  if (s_sitzung != HRM_INVALID_SESSION_REF) {
    sys_hrm_manager_unsubscribe(s_sitzung);
    s_sitzung = HRM_INVALID_SESSION_REF;
  }
#endif
  s_misst = false;
  const uint16_t rmssd = boulder_hrv_rmssd();
  if (rmssd == 0) {
    return;
  }
  s_fenster[s_fenster_naechstes] = (Fenster){ .start = s_start, .rmssd = rmssd };
  s_fenster_naechstes = (s_fenster_naechstes + 1) % HRV_FENSTER_MAX;
  PBL_LOG_INFO("HRV-Fenster: RMSSD %u ms aus %u Intervallen", rmssd, s_anzahl);
}

void boulder_hrv_minute(time_t utc, uint16_t letzte_bewegung) {
  if (s_misst) {
    if (utc - s_start >= HRV_FENSTER_MIN * SECONDS_PER_MINUTE) {
      prv_beende();
    }
    return;
  }
  const uint16_t takt = boulder_hrv_takt_min();
  if (takt == 0 || (utc / SECONDS_PER_MINUTE) % takt != 0) {
    return;
  }
  // Nur in Ruhe: wer sich bewegt, liefert Abstaende voller Stoerungen.
  if (!boulder_nachtruhe(utc, letzte_bewegung)) {
    return;
  }
  prv_beginne(utc);
}

int boulder_hrv_fenster(time_t von, time_t bis, SchlafHrv *aus, int max) {
  int n = 0;
  for (int k = 0; k < HRV_FENSTER_MAX && n < max; k++) {
    // Aelteste zuerst, wie die Nacht sie erlebt hat.
    const Fenster *f = &s_fenster[(s_fenster_naechstes + k) % HRV_FENSTER_MAX];
    const time_t mitte = f->start + (HRV_FENSTER_MIN / 2) * SECONDS_PER_MINUTE;
    // Ein Fenster, das vor `von` beginnt, gehoert zur Nacht davor.
    if (f->rmssd == 0 || f->start < von || mitte >= bis) {
      continue;
    }
    aus[n++] = (SchlafHrv){
      .minute = (uint16_t)((f->start - von) / SECONDS_PER_MINUTE),
      .rmssd = f->rmssd,
    };
  }
  return n;
}
