/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "puls.h"

#include "schluessel.h"

#define MAXPULS_VORGABE 190

// Sekunden zwischen zwei Messungen waehrend eines Trainings.
//
// EINE SEKUNDE, ALSO DAUERND. Mit fuenf Sekunden liess der Sensor am Lenker
// und am Hang den Wert stehen, und die Zone stimmte nie. Eine Sekunde ist
// der Takt der eingebauten Workout-App - und ein Training ist die eine
// Stunde am Tag, in der der Akku dafuer da ist.
#define TAKT_TRAINING_S 1
#define TAKT_SPARSAM_S 5
// In der Pause: oft genug, dass die Anzeige lebt, selten genug, dass eine
// lange Pause nicht kostet wie ein Training.
#define TAKT_PAUSE_S 10

// Ausserhalb dieser Grenzen ist ein Wert ein Messfehler, kein Herz.
#define BPM_MIN 30
#define BPM_MAX 230

// DER ANGEZEIGTE PULS IST DER MEDIAN DER LETZTEN DREI, solange der Sensor
// seiner Messung nicht traut. Bewegung macht Spitzen, und eine Spitze sollte
// keine Zone wechseln und nicht brummen. Traut er ihr (guete >= 3), gilt der
// rohe Wert - sonst hinge die Anzeige einem Zwischensprint hinterher. So
// macht es auch die Workout-App der Firmware.
#define GLAETTEN_N 3
#define GUETE_GUT 3

static uint16_t s_maximum = MAXPULS_VORGABE;
static bool s_sparsam;

static const uint8_t s_prozent[KS_ZONEN] = { 50, 60, 70, 80, 90 };

// --- Der Sensor ---

static bool s_beobachtet;
static bool s_dicht;
static bool s_hrv_an;

static uint16_t s_bpm;
static time_t s_frisch_seit;
static uint8_t s_ring[GLAETTEN_N];
static uint8_t s_ring_anzahl;
static uint8_t s_ring_kopf;

static void prv_sensor(void) {
  if (!s_beobachtet) {
    ks_puls_sensor(0, false);
    return;
  }
  const uint16_t takt = !s_dicht ? TAKT_PAUSE_S : (s_sparsam ? TAKT_SPARSAM_S : TAKT_TRAINING_S);
  ks_puls_sensor(takt, s_hrv_an);
}

void puls_setze_sparsam(bool sparsam) {
  if (sparsam == s_sparsam) {
    return;
  }
  s_sparsam = sparsam;
  prv_sensor();
}

bool puls_sparsam(void) { return s_sparsam; }

void puls_init(void) {
  s_maximum = MAXPULS_VORGABE;
  if (ks_persist_exists(PERSIST_MAXPULS)) {
    const int32_t gelesen = ks_persist_read_int(PERSIST_MAXPULS);
    if (gelesen > 100 && gelesen < 240) {
      s_maximum = (uint16_t)gelesen;
    }
  }
}

uint16_t puls_maximum(void) { return s_maximum; }

void puls_setze_maximum(uint16_t schlaege) {
  if (schlaege <= 100 || schlaege >= 240) {
    return;
  }
  s_maximum = schlaege;
  ks_persist_write_int(PERSIST_MAXPULS, schlaege);
}

uint16_t puls_zonengrenze(int zone) {
  if (zone < 1 || zone > KS_ZONEN) {
    return 0;
  }
  return (uint16_t)((uint32_t)s_maximum * s_prozent[zone - 1] / 100);
}

int puls_zone(uint16_t bpm) {
  if (bpm == 0) {
    return 0;
  }
  for (int z = KS_ZONEN; z >= 1; z--) {
    if (bpm >= puls_zonengrenze(z)) {
      return z;
    }
  }
  return 0;
}

// --- HRV ---
//
// RMSSD = Wurzel aus dem Mittel der quadrierten Differenzen aufeinander
// folgender Intervalle, laufend gezaehlt, die Wurzel erst am Ende. Ein
// Intervall, das um mehr als ein Drittel vom vorigen abweicht, ist ein
// verpasster Schlag, kein Herz, und faellt weg.
static uint16_t s_hrv_vorher;
static uint32_t s_hrv_summe;
static uint16_t s_hrv_anzahl;

static void prv_hrv_intervall(uint16_t ppi) {
  if (ppi < 300 || ppi > 2000) {
    return;
  }
  if (s_hrv_vorher != 0) {
    const int d = (int)ppi - (int)s_hrv_vorher;
    const int grenze = (int)s_hrv_vorher / 3;
    if (d > -grenze && d < grenze && s_hrv_summe < 0xF0000000u) {
      s_hrv_summe += (uint32_t)(d * d);
      if (s_hrv_anzahl < 0xFFFF) {
        s_hrv_anzahl++;
      }
    }
  }
  s_hrv_vorher = ppi;
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

void puls_hrv_start(void) {
  s_hrv_vorher = 0;
  s_hrv_summe = 0;
  s_hrv_anzahl = 0;
  s_hrv_an = true;
  prv_sensor();
}

void puls_hrv_stop(void) {
  if (!s_hrv_an) {
    return;
  }
  s_hrv_an = false;
  prv_sensor();
}

uint16_t puls_hrv_rmssd(void) {
  // Unter dreissig Intervallen ist es Rauschen, keine Messung.
  if (s_hrv_anzahl < 30) {
    return 0;
  }
  return (uint16_t)prv_wurzel(s_hrv_summe / s_hrv_anzahl);
}

uint16_t puls_hrv_anzahl(void) { return s_hrv_anzahl; }

// --- Was der Sensor meldet ---

static uint8_t prv_median(void) {
  uint8_t puffer[GLAETTEN_N];
  for (uint8_t i = 0; i < s_ring_anzahl; i++) {
    puffer[i] = s_ring[(s_ring_kopf + GLAETTEN_N - s_ring_anzahl + i) % GLAETTEN_N];
  }
  for (uint8_t i = 1; i < s_ring_anzahl; i++) {
    const uint8_t v = puffer[i];
    int j = i - 1;
    while (j >= 0 && puffer[j] > v) {
      puffer[j + 1] = puffer[j];
      j--;
    }
    puffer[j + 1] = v;
  }
  return puffer[s_ring_anzahl / 2];
}

void puls_ereignis_bpm(uint8_t bpm, int8_t guete) {
  if (!s_beobachtet) {
    return;
  }
  if (guete < 0) {
    // NICHT AM HANDGELENK: kein Puls, und zwar sofort - nicht erst, wenn
    // der alte Wert nach anderthalb Minuten verblasst.
    s_bpm = 0;
    s_frisch_seit = 0;
    s_ring_anzahl = 0;
    return;
  }
  if (bpm < BPM_MIN || bpm > BPM_MAX) {
    return;
  }
  if (guete >= GUETE_GUT) {
    s_ring_anzahl = 0;
    s_bpm = bpm;
  } else {
    s_ring[s_ring_kopf] = bpm;
    s_ring_kopf = (uint8_t)((s_ring_kopf + 1) % GLAETTEN_N);
    if (s_ring_anzahl < GLAETTEN_N) {
      s_ring_anzahl++;
    }
    s_bpm = prv_median();
  }
  s_frisch_seit = ks_jetzt();
}

void puls_ereignis_ppi(uint16_t ppi_ms) {
  if (s_hrv_an && ppi_ms) {
    prv_hrv_intervall(ppi_ms);
  }
}

void puls_beobachten(void) {
  s_bpm = 0;
  s_frisch_seit = 0;
  s_ring_anzahl = 0;
  s_ring_kopf = 0;
  s_beobachtet = true;
  prv_sensor();
}

void puls_ignorieren(void) {
  s_beobachtet = false;
  s_hrv_an = false;
  prv_sensor();
}

uint16_t puls_lesen(void) {
  return s_bpm;
}

bool puls_frisch(void) {
  if (s_bpm == 0 || s_frisch_seit == 0) {
    return false;
  }
  return (ks_jetzt() - s_frisch_seit) <= KS_PULS_ALT_S;
}

void puls_dicht_messen(void) {
  s_dicht = true;
  prv_sensor();
}

void puls_normal_messen(void) {
  s_dicht = false;
  prv_sensor();
}

void puls_auffrischen(void) {
  prv_sensor();
}
