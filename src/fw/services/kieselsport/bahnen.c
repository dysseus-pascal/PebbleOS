/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "bahnen.h"
#include "abschnitt.h"

#include "schluessel.h"

#include "pbl/util/trig.h"

// Wie stark geglaettet wird. Ein Zug dauert eine Sekunde, eine Bahn zwanzig -
// die Mittelung muss den Zug schlucken und die Wende stehen lassen.
#define GLAETTUNG 6

// Erst ab dieser Abweichung gilt die Richtung als gewechselt, und erst nach
// dieser Zeit als neue Bahn. 120 Grad statt 90, damit ein Schlenker nicht
// schon eine Wende ist; acht Sekunden, weil niemand schneller wendet.
#define WENDE_GRAD 120
#define MIN_BAHN_S 8

// Bis der Kompass etwas taugt, vergehen ein paar Sekunden. So lange wird die
// Achse des Beckens gesucht und nichts gezaehlt.
#define EINSCHWINGEN_S 10

static bool s_an;
static bool s_pausiert;
static uint16_t s_becken = 25;

static int32_t s_sx, s_sy;        //< geglaettete Richtung als Vektor
static bool s_hat_richtung;
static int32_t s_achse;           //< die Richtung der ersten Bahn
static bool s_hat_achse;
static bool s_hin;                //< schwimmt gerade in Achsenrichtung?
static uint16_t s_bahnen;
static uint16_t s_letzte_wende;
static bool s_bereit;

/** Winkelabstand in Grad, immer 0..180. */
static int32_t prv_abstand_grad(int32_t a, int32_t b) {
  int32_t d = (a - b) % TRIG_MAX_ANGLE;
  if (d < 0) d += TRIG_MAX_ANGLE;
  if (d > TRIG_MAX_ANGLE / 2) d = TRIG_MAX_ANGLE - d;
  return d * 360 / TRIG_MAX_ANGLE;
}

void bahnen_kompass(int32_t richtung, bool bereit) {
  if (!s_an || s_pausiert) return;

  // BEREIT HEISST KALIBRIERT ODER BEIM KALIBRIEREN (plattform.c). Eine Uhr
  // ohne Magnetometer meldet "nicht verfuegbar", und dann soll der Schirm
  // sagen, dass hier nichts zu zaehlen ist, statt ruhig "0 Bahnen" zu
  // zeigen.
  s_bereit = bereit;
  if (!s_bereit) return;

  // ALS VEKTOR MITTELN, NICHT ALS ZAHL. Der Mittelwert aus 350 und 10 Grad
  // ist 0 und nicht 180 - wer Winkel wie Zahlen mittelt, bekommt bei jedem
  // Nulldurchgang eine Wende, die es nicht gab.
  const int32_t x = cos_lookup(richtung);
  const int32_t y = sin_lookup(richtung);
  if (!s_hat_richtung) {
    s_sx = x;
    s_sy = y;
    s_hat_richtung = true;
    return;
  }
  s_sx += (x - s_sx) / GLAETTUNG;
  s_sy += (y - s_sy) / GLAETTUNG;
}

static int32_t prv_richtung(void) {
  // GETEILT DURCH VIER, sonst passt es nicht. sin_lookup reicht bis 65535,
  // atan2_lookup nimmt int16_t - ungeteilt liefe der Wert ueber und die
  // Richtung spraenge quer durchs Becken. Auf das Verhaeltnis hat das
  // Teilen keinen Einfluss, und nur darauf kommt es an.
  return atan2_lookup((int16_t)(s_sy / 4), (int16_t)(s_sx / 4));
}

void bahnen_laden(void) {
  if (ks_persist_exists(PERSIST_BECKEN)) {
    const int32_t meter = ks_persist_read_int(PERSIST_BECKEN);
    if (meter >= 5 && meter <= 100) s_becken = (uint16_t)meter;
  }
}

void bahnen_start(void) {
  s_an = true;
  s_pausiert = false;
  s_sx = s_sy = 0;
  s_hat_richtung = false;
  s_hat_achse = false;
  s_achse = 0;
  s_hin = true;
  s_bahnen = 0;
  s_letzte_wende = 0;
  s_bereit = false;
  bahnen_laden();
  ks_kompass(true);
}

void bahnen_stoppe(uint16_t sekunde) {
  if (!s_an) return;
  // DIE LETZTE BAHN ZAEHLT MIT. Wer am Beckenrand ankommt und aufhoert, hat
  // sie geschwommen - nur folgt ihr keine Wende mehr, an der man sie
  // erkennen koennte.
  if (s_hat_achse && sekunde > s_letzte_wende + MIN_BAHN_S) {
    s_bahnen++;
    abschnitt_hinzu(s_letzte_wende, 1, (uint16_t)(sekunde - s_letzte_wende));
  }
  ks_kompass(false);
  s_an = false;
}

void bahnen_pause(bool an) { s_pausiert = an; }

void bahnen_tick(uint16_t sekunde) {
  if (!s_an || s_pausiert || !s_bereit || !s_hat_richtung) return;

  const int32_t jetzt = prv_richtung();

  if (!s_hat_achse) {
    // Die Achse ist die Richtung, in die zuerst geschwommen wurde. Sie muss
    // nicht Norden sein und nicht stimmen - sie muss nur die erste sein.
    if (sekunde >= EINSCHWINGEN_S) {
      s_achse = jetzt;
      s_hat_achse = true;
      s_hin = true;
      s_letzte_wende = sekunde;
    }
    return;
  }

  const int32_t ab = prv_abstand_grad(jetzt, s_achse);
  const bool hin_jetzt = ab < 180 - WENDE_GRAD ? true : (ab > WENDE_GRAD ? false : s_hin);

  if (hin_jetzt != s_hin && sekunde >= s_letzte_wende + MIN_BAHN_S) {
    s_bahnen++;
    abschnitt_hinzu(s_letzte_wende, 1, (uint16_t)(sekunde - s_letzte_wende));
    s_letzte_wende = sekunde;
    s_hin = hin_jetzt;
  }
}

uint16_t bahnen_anzahl(void) {
  return s_bahnen;
}

uint32_t bahnen_meter(void) { return (uint32_t)bahnen_anzahl() * s_becken; }

bool bahnen_bereit(void) {
  return s_bereit;
}

void bahnen_becken_setze(uint16_t meter) {
  if (meter < 5 || meter > 100) return;
  s_becken = meter;
  ks_persist_write_int(PERSIST_BECKEN, meter);
}
uint16_t bahnen_becken(void) { return s_becken; }
