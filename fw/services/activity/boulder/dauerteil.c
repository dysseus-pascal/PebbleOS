/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2: der Dauerteil an der Naht zur Kraepelin-Kopie (variante.h), Stand AP3.
//
// STAND AP3: Schlaf - jede Minute in Nachtfelder, Live-Automat und Zyklen (nacht.c), der
// Takt "Nachlauf" im eigenen Callback, der gestaffelte Abschluss je Minute, der
// Live-Zustand fuer Pebble (D6). Noch neutral: keine Fahrt (AP6: kontext.c,
// nachfrage.c), kein Ring nach dem Boot (AP4: speicher.c), kein Abo "Aktiv" (AP6), kein
// Training in "gueltig" (AP6). Den Takt als eigenen Cron (Luecken, Ruhezeiten) bringt AP4.

#include "pbl/services/activity/boulder/variante.h"

#include "pbl/services/activity/boulder/live.h"
#include "pbl/services/activity/boulder/nacht.h"
#include "pbl/services/activity/boulder/takt.h"
#include "pbl/services/activity/boulder/zyklen.h"
#include "pbl/services/time.h"
#include "pbl/util/units.h"

static time_t s_letzte_minute_utc;

// Das Nachtfenster, in dem `utc` liegt: ab 20 Uhr Ortszeit, mit dem Versatz AM
// FENSTERBEGINN (b26, M: nacht.c:193-209) - sonst rutschte der Beginn in der Nacht der
// Zeitumstellung um drei Uhr um eine Stunde, mitten in der Rechnung.
static time_t prv_fenster(time_t utc) {
  const time_t lokal = time_utc_to_local(utc);
  const int minute_des_tages = (int)((lokal % PBL_SEC_PER_DAY) / PBL_SEC_PER_MIN);
  time_t beginn_lokal = lokal - (lokal % PBL_SEC_PER_DAY) + 20 * PBL_SEC_PER_HOUR;
  if (minute_des_tages < 20 * PBL_MIN_PER_HOUR) {
    beginn_lokal -= PBL_SEC_PER_DAY;
  }
  time_t b = utc - (lokal - beginn_lokal);
  b = beginn_lokal - (time_utc_to_local(b) - b);
  return b;
}

// Der Takt der Nacht in einem eigenen, verwerfbaren Callback (2.4): die Fensterrechnung
// gehoert nicht in den Minutenteil. Faellt er weg, holt ihn der naechste faellige nach.
static void prv_nachlauf_cb(void *data);
static BoulderAufgabe s_aufgabe_nachlauf = BOULDER_AUFGABE("Boulder Nachlauf", prv_nachlauf_cb);

static void prv_nachlauf_cb(void *data) {
  boulder_aufgabe_laeuft(&s_aufgabe_nachlauf);
  boulder_nacht_takt();
}

void boulder_dauerteil_minute(const BoulderMinute *minute) {
  s_letzte_minute_utc = minute->utc;
  const time_t beginn = prv_fenster(minute->utc);
  const int index = (int)((minute->utc - beginn) / PBL_SEC_PER_MIN);
  if (boulder_nacht_minute(beginn, index, minute->vmc, minute->puls, !minute->nicht_getragen)) {
    boulder_aufgabe_einreihen(&s_aufgabe_nachlauf);
  }
  boulder_zyklen_abschluss_schritt(!boulder_live_schlaeft());
}

bool boulder_dauerteil_ring_fenster(time_t *von_utc, time_t *bis_utc) {
  (void)von_utc;
  (void)bis_utc;
  return false;
}

void boulder_dauerteil_ring_minute(const BoulderMinute *minute) {
  (void)minute;
}

time_t boulder_dauerteil_fahrt_offen_seit(void) {
  return 0;
}

bool boulder_dauerteil_fahrt_minute(time_t minute_utc) {
  (void)minute_utc;
  return false;
}

bool boulder_dauerteil_fahrt_laeuft(void) {
  return false;
}

bool boulder_dauerteil_fahrt_ueberdeckt(time_t start_utc, uint32_t laenge_s) {
  (void)start_utc;
  (void)laenge_s;
  return false;
}

time_t boulder_dauerteil_letzter_schlaf_utc(void) {
  // D6 (Spezifikation 4.5): Schlaeft der Automat, das Ende der offenen Sitzung - Pebble
  // meldet dann "LightSleep" (activity_sessions.c:398). Sonst die Minutenwende der letzten
  // Minute: nach zwei Minuten ohne Verlaengerung meldet Pebble "Awake".
  const time_t offen = boulder_live_schlaeft() ? boulder_zyklen_offen_ende() : 0;
  if (offen) {
    return offen;
  }
  return s_letzte_minute_utc ? s_letzte_minute_utc + 60 : 0;
}

bool boulder_dauerteil_aktiv_abo_laeuft(void) {
  return false;
}

void boulder_dauerteil_aktiv_abo_pausieren(bool pausiert) {
  (void)pausiert;
}

void boulder_dauerteil_tracking_aus(void) {
}
