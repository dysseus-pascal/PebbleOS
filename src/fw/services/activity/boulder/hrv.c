/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/activity/boulder/hrv.h"

#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/boulder/messstufe.h"
#include "pbl/services/settings/settings_file.h"
#include "util/time/time.h"
#include <pbl/drivers/rtc.h>
#include <pbl/logging/logging.h>

#ifdef CONFIG_HRM
#include "pbl/services/hrm/hrm_manager.h"
#include "pbl/services/hrm/hrm_manager_private.h"
#endif

#include <string.h>

// Unter so vielen Intervallen ist es Rauschen, keine Messung.
#define MIN_INTERVALLE 30
// SO LANGE OHNE ABSTAND, UND DAZWISCHEN FEHLEN SCHLAEGE: der Sensor liefert
// bis zu vier Abstaende auf einmal, sechs Sekunden Pause sind also noch
// normal. Laenger hat ihn etwas unterbrochen (SpO2 braucht denselben Lichtweg,
// die Uhr lag ab) - die Differenz ueber die Luecke waere kein Schlag-zu-Schlag.
#define LUECKE_MS 6000

// DIE FENSTER UEBERSTEHEN EINEN NEUSTART: sonst rechnete die Uhr eine Nacht,
// die am Morgen neu gerechnet wird, ohne HRV - mit anderen Phasen, und ohne
// den Satz auf Tag 89. Je Fenster ein kleiner Satz unter seinem Platz im Ring.
#define FENSTER_DATEI "boulder_hrv"
#define FENSTER_DATEI_GROESSE 4096

typedef struct {
  time_t start;
  uint16_t rmssd;
  uint8_t puls;   //< bpm aus den Abstaenden, 0 = unbekannt
} Fenster;

static Fenster s_fenster[HRV_FENSTER_MAX];
static int s_fenster_naechstes;   //< Ringpuffer: hierhin kommt das naechste
static bool s_fenster_geladen;

static bool s_misst;
static time_t s_start;
static uint16_t s_vorher;
static RtcTicks s_vorher_um;      //< wann der vorige Abstand kam
static uint32_t s_summe;          //< Summe der quadrierten Differenzen
static uint16_t s_anzahl;
static uint32_t s_ppi_summe;      //< Summe der gezaehlten Abstaende, fuer den Puls
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
    if (d > -grenze && d < grenze && s_summe < 0xF0000000u && s_anzahl < 0xFFFF) {
      s_summe += (uint32_t)(d * d);
      s_ppi_summe += ppi;
      s_anzahl++;
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

// DER PULS DES FENSTERS aus denselben Abstaenden wie der RMSSD, gerundet. Er
// geht mit dem Fenster ans Telefon: eine HRV sagt erst neben ihrem Puls etwas.
uint8_t boulder_hrv_puls(void) {
  if (s_anzahl < MIN_INTERVALLE || s_ppi_summe == 0) {
    return 0;
  }
  const uint32_t bpm = (60000u * s_anzahl + s_ppi_summe / 2) / s_ppi_summe;
  return (bpm > 0xFF) ? 0xFF : (uint8_t)bpm;
}

#ifdef CONFIG_HRM
// NUR SICHERE ABSTAENDE, UND KEINER UEBER EINE LUECKE. Der Treiber sagt
// "Worst" unter 70 % Konfidenz; ein solcher Abstand zaehlt nicht, und auch der
// danach nicht gegen den davor - zwischen beiden fehlt die Gewissheit.
static void prv_hrm_ereignis(PebbleHRMEvent *e, void *context) {
  if (!s_misst || e->event_type != HRMEvent_HRV) {
    return;
  }
  const RtcTicks jetzt = rtc_get_ticks();
  const bool luecke = s_vorher != 0 &&
      (jetzt - s_vorher_um) * MS_PER_SECOND / RTC_TICKS_HZ > LUECKE_MS;
  if (e->hrv.quality < HRMQuality_Acceptable || e->hrv.ppi_ms == 0 || luecke) {
    s_vorher = 0;
  }
  s_vorher_um = jetzt;
  if (e->hrv.quality >= HRMQuality_Acceptable && e->hrv.ppi_ms) {
    boulder_hrv_intervall(e->hrv.ppi_ms);
  }
}
#endif

static void prv_beginne(time_t utc) {
  s_misst = true;
  s_start = utc;
  s_vorher = 0;
  s_vorher_um = 0;
  s_summe = 0;
  s_anzahl = 0;
  s_ppi_summe = 0;
#ifdef CONFIG_HRM
  // Jede Sekunde, mit HRV. Ablauf nach etwas mehr als dem Fenster: geht das
  // Ende verloren (Neustart), gibt der Sensor von selbst auf.
  s_sitzung = hrm_manager_subscribe_with_callback(
      INSTALL_ID_INVALID, 1, (HRV_FENSTER_MIN + 1) * SECONDS_PER_MINUTE,
      HRMFeature_BPM | HRMFeature_HRV, false /*low_latency*/, prv_hrm_ereignis, NULL);
#endif
}

// Ein Satz der Datei: ein Fenster unter seinem Platz im Ring.
static bool prv_fenster_satz(SettingsFile *datei, SettingsRecordInfo *info, void *kontext) {
  int *neuestes = kontext;
  uint8_t platz;
  Fenster f;
  if (info->key_len != sizeof(platz) || info->val_len != sizeof(f)) {
    return true;
  }
  info->get_key(datei, &platz, sizeof(platz));
  if (platz >= HRV_FENSTER_MAX) {
    return true;
  }
  info->get_val(datei, &f, sizeof(f));
  s_fenster[platz] = f;
  if (*neuestes < 0 || f.start > s_fenster[*neuestes].start) {
    *neuestes = platz;
  }
  return true;
}

// Die Fenster aus der Datei, einmal nach einem Neustart - in einem Durchgang.
static void prv_fenster_laden(void) {
  if (s_fenster_geladen) {
    return;
  }
  s_fenster_geladen = true;
  SettingsFile datei;
  if (settings_file_open(&datei, FENSTER_DATEI, FENSTER_DATEI_GROESSE) != S_SUCCESS) {
    PBL_LOG_WRN("HRV: Fenster nicht lesbar");
    return;
  }
  int neuestes = -1;
  settings_file_each(&datei, prv_fenster_satz, &neuestes);
  settings_file_close(&datei);
  if (neuestes >= 0) {
    s_fenster_naechstes = (neuestes + 1) % HRV_FENSTER_MAX;
  }
}

static void prv_fenster_speichern(uint8_t platz) {
  SettingsFile datei;
  if (settings_file_open(&datei, FENSTER_DATEI, FENSTER_DATEI_GROESSE) != S_SUCCESS) {
    PBL_LOG_WRN("HRV: Fenster nicht gespeichert");
    return;
  }
  if (settings_file_set(&datei, &platz, sizeof(platz), &s_fenster[platz],
                        sizeof(s_fenster[platz])) != S_SUCCESS) {
    PBL_LOG_WRN("HRV: Fenster nicht gespeichert");
  }
  settings_file_close(&datei);
}

// `behalten`: das Fenster zaehlt. Nach einem Zurueckstellen der Uhr nicht -
// sein Beginn laege in der Zukunft der neuen Zeit.
static void prv_beende(bool behalten) {
#ifdef CONFIG_HRM
  if (s_sitzung != HRM_INVALID_SESSION_REF) {
    sys_hrm_manager_unsubscribe(s_sitzung);
    s_sitzung = HRM_INVALID_SESSION_REF;
  }
#endif
  s_misst = false;
  const uint16_t rmssd = boulder_hrv_rmssd();
  if (rmssd == 0 || !behalten) {
    return;
  }
  prv_fenster_laden();
  const uint8_t platz = s_fenster_naechstes;
  s_fenster[platz] = (Fenster){ .start = s_start, .rmssd = rmssd, .puls = boulder_hrv_puls() };
  s_fenster_naechstes = (s_fenster_naechstes + 1) % HRV_FENSTER_MAX;
  prv_fenster_speichern(platz);
  PBL_LOG_INFO("HRV-Fenster: RMSSD %u ms aus %u Intervallen", rmssd, s_anzahl);
}

void boulder_hrv_minute(time_t utc, uint16_t letzte_bewegung, bool spo2_misst) {
  if (s_misst) {
    // Die Uhr wurde zurueckgestellt: das Fenster endet sofort, ohne zu zaehlen.
    // Sonst wartete es, bis die neue Zeit den alten Beginn einholt - so lange
    // gaebe es keine Fenster.
    if (utc < s_start) {
      prv_beende(false);
    } else if (utc - s_start >= HRV_FENSTER_MIN * SECONDS_PER_MINUTE) {
      prv_beende(true);
    }
    return;
  }
  // "Puls aus" gilt auch hier: sonst leuchtete nachts die LED, obwohl der
  // Schalter aus ist.
  const uint16_t takt = boulder_hrv_takt_min();
  if (takt == 0 || (utc / SECONDS_PER_MINUTE) % takt != 0 || !activity_prefs_heart_rate_is_enabled()) {
    return;
  }
  // Nur in Ruhe: wer sich bewegt, liefert Abstaende voller Stoerungen. Und
  // nicht, solange SpO2 misst: es braucht denselben Lichtweg, das Fenster
  // bekaeme kaum Abstaende.
  if (!boulder_nachtruhe(utc, letzte_bewegung) || spo2_misst) {
    return;
  }
  prv_beginne(utc);
}

int boulder_hrv_fenster(time_t von, time_t bis, SchlafHrv *aus, int max) {
  prv_fenster_laden();
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
      .puls = f->puls,
    };
  }
  return n;
}

#ifdef UNITTEST
void boulder_hrv_test_reset(void) {
  memset(s_fenster, 0, sizeof(s_fenster));
  s_fenster_naechstes = 0;
  s_fenster_geladen = false;
  s_misst = false;
  s_start = 0;
  s_vorher = 0;
  s_vorher_um = 0;
  s_summe = 0;
  s_anzahl = 0;
  s_ppi_summe = 0;
#ifdef CONFIG_HRM
  s_sitzung = HRM_INVALID_SESSION_REF;
#endif
}
#endif
