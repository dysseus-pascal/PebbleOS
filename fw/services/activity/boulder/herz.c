/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2, Schritt 3: Herzband und Zonen von heute (herz.h, Spezifikation 3.8.2, AP4). Nur mit
// der eigenen Health-App; ohne den Schalter bleibt die Datei leer.
#ifdef CONFIG_BOULDER_HEALTH_APP

#include "pbl/services/activity/boulder/herz.h"

#include "pbl/kernel/mutex.h"
#include "pbl/services/activity/boulder/takt.h"
#include "pbl/services/activity/boulder/variante.h"
#include "pbl/services/time.h"
#include "pbl/util/math.h"
#include "pbl/util/testing.h"
#include "pbl/util/units.h"
#include <pbl/drivers/rtc.h>
#include <pbl/logging/logging.h>

#include <string.h>

PBL_LOG_MODULE_DECLARE(service_activity, CONFIG_SERVICE_ACTIVITY_LOG_LEVEL);

// 1.1.2000 0:00 als Ortszeit in Sekunden seit 1970: der Tag 0 (wie naechte.c und die App).
#define TAG_NULL_LOKAL 946684800

#define RUHEPULS_ERSATZ 60 // Boulder: Trainingsanalyse.RUHEPULS_ERSATZ
#define RESERVE_MIN     20 // Boulder: pulsZonen, coerceAtLeast(20)

// So weit zurueck sucht das Nachrechnen Minuten (in Halbstunden vor der juengsten moeglichen):
// das Band selbst und noch einmal 24 h. Endete die juengste Minute bis zu einen Tag vor jetzt
// (Tracking aus, am Kabel), steht ihr Band trotzdem ganz da - wie im laufenden Betrieb.
#define SUCHE_HALBSTUNDEN (2 * HERZ_HALBSTUNDEN)

// EINE BLATTSPERRE (3.4): Unter ihr wird nur gerechnet und kopiert, nichts gerufen. Fuer die Tests
// sichtbar (Sperrfolge, T2.4).
PBL_T_STATIC PBL_MUTEX_DEFINE(s_herz_sperre);
static BoulderHerz s_herz;
// Die Summe der Pulse in der Halbstunde `neueste`: So ist ihr Mittel genau, nicht aus einem
// gerundeten Mittel weitergerechnet. Aeltere Halbstunden bekommen im laufenden Betrieb nichts mehr.
static uint16_t s_summe;
static bool s_bereit;  //< das Nachrechnen lief seit dem Boot (unter der Sperre gesetzt)
static bool s_faellig; //< Nachrechnen steht aus (Boot, neue Grenzen); nur KernelBG

static void prv_nachrechnen_cb(void *data);
static BoulderAufgabe s_aufgabe = BOULDER_AUFGABE("Boulder Herz nachrechnen", prv_nachrechnen_cb);

// ---------------------------------------------------------------------------------------------
// Grenzen und Zonen wie Boulder

void boulder_herz_grenzen(uint8_t ruhepuls_median, uint16_t maxpuls,
                          uint8_t grenzen[HERZ_ZONEN - 1]) {
  const int32_t ruhe = ruhepuls_median ? ruhepuls_median : RUHEPULS_ERSATZ;
  int32_t reserve = (int32_t)maxpuls - ruhe;
  if (reserve < RESERVE_MIN) {
    reserve = RESERVE_MIN;
  }
  // 40, 60 und 80 % der Reserve, gerundet wie roundToInt: Bei einer ganzen Reserve ist ein Zehntel
  // davon nie genau x,5, also gibt es keinen Gleichstand, den Gleitkomma anders riefe.
  static const uint8_t s_zehntel[HERZ_ZONEN - 1] = {4, 6, 8};
  for (int i = 0; i < HERZ_ZONEN - 1; i++) {
    const int32_t grenze = ruhe + (s_zehntel[i] * reserve + 5) / 10;
    grenzen[i] = (uint8_t)((grenze > UINT8_MAX) ? UINT8_MAX : grenze);
  }
}

uint8_t boulder_herz_zone(uint8_t bpm, const uint8_t grenzen[HERZ_ZONEN - 1]) {
  uint8_t zone = 0;
  for (int i = 0; i < HERZ_ZONEN - 1; i++) {
    zone += (bpm >= grenzen[i]);
  }
  return zone;
}

// Die Grenzen, die jetzt gelten: aus der Lage, die der Minutenteil eben veroeffentlicht hat. So
// rechnen App (Zonenfarbe) und Variante (Zonenminuten) mit denselben Zahlen.
static void prv_grenzen_jetzt(uint8_t grenzen[HERZ_ZONEN - 1]) {
  BoulderLage lage;
  boulder_lage_lesen(&lage);
  boulder_herz_grenzen(lage.ruhepuls_median, lage.maxpuls, grenzen);
}

// Tage seit dem 1.1.2000 nach Ortszeit, mit der Zeitzone samt Sommerzeit dieser Minute. 0 ohne
// gueltige Uhrzeit. Pebbles Code: nie unter der Sperre.
static uint16_t prv_tag(time_t utc) {
  const time_t lokal = time_utc_to_local(utc);
  if (lokal < TAG_NULL_LOKAL) {
    return 0;
  }
  const uint32_t tage = (uint32_t)((lokal - TAG_NULL_LOKAL) / PBL_SEC_PER_DAY);
  return (tage > UINT16_MAX) ? UINT16_MAX : (uint16_t)tage;
}

static void prv_platz_leeren(BoulderHerz *h, int platz) {
  h->min[platz] = 0;
  h->max[platz] = 0;
  h->mittel[platz] = 0;
  h->n[platz] = 0;
}

// Gerundet wie Math.round fuer positive Zahlen.
static uint8_t prv_mittel(uint32_t summe, uint32_t n) {
  return (uint8_t)((2 * summe + n) / (2 * n));
}

static void prv_zaehlen(uint16_t *zonen_min, uint8_t zone) {
  if (zonen_min[zone] < UINT16_MAX) {
    zonen_min[zone]++;
  }
}

// Neue Grenzen (eine neue Nacht im Median, ein anderer Maximalpuls): Die Zonen von heute zaehlen
// neu, im Auftrag. Bis dahin zaehlt der laufende Betrieb mit den alten, wie alle Minuten vorher.
static void prv_grenzen_pruefen(void) {
  uint8_t grenzen[HERZ_ZONEN - 1];
  prv_grenzen_jetzt(grenzen);
  if (memcmp(grenzen, s_herz.grenzen, sizeof(grenzen)) != 0) {
    s_faellig = true;
  }
  if (s_faellig) {
    boulder_aufgabe_einreihen(&s_aufgabe);
  }
}

// ---------------------------------------------------------------------------------------------
// Der laufende Betrieb

void boulder_herz_minute(time_t utc, uint8_t puls) {
  // VOR DEM ERSTEN NACHRECHNEN NICHTS: Es liest alle Minuten seit dem Boot aus Datei und Puffer
  // und ersetzt das Band ganz. Und vorher fehlten die Grenzen (die Ruhepulse laedt erst "Laden").
  if (!s_bereit) {
    return;
  }
  prv_grenzen_pruefen();
  const uint32_t halbstunde = (uint32_t)(utc / HERZ_HALBSTUNDE_S);
  const uint16_t tag = prv_tag(utc);

  pbl_mutex_lock(&s_herz_sperre, PBL_FOREVER);
  BoulderHerz *h = &s_herz;
  if (halbstunde > h->neueste) {
    // Eine neue Halbstunde: die uebersprungenen und ihre eigene leer.
    const uint32_t schritte = halbstunde - h->neueste;
    for (uint32_t k = 1; k <= MIN(schritte, (uint32_t)HERZ_HALBSTUNDEN); k++) {
      prv_platz_leeren(h, (int)((h->neueste + k) % HERZ_HALBSTUNDEN));
    }
    h->neueste = halbstunde;
    s_summe = 0;
  }
  if (tag > h->zonen_tag) {
    memset(h->zonen_min, 0, sizeof(h->zonen_min));
    h->zonen_tag = tag;
  }
  if (puls && tag == h->zonen_tag) {
    prv_zaehlen(h->zonen_min, boulder_herz_zone(puls, h->grenzen));
  }
  if (puls && h->neueste - halbstunde < HERZ_HALBSTUNDEN) {
    const int platz = (int)(halbstunde % HERZ_HALBSTUNDEN);
    const uint8_t n = h->n[platz];
    if (n < UINT8_MAX) {
      h->min[platz] = (n == 0) ? puls : MIN(h->min[platz], puls);
      h->max[platz] = MAX(h->max[platz], puls);
      // Die juengste Halbstunde genau aus ihrer Summe, eine aeltere aus dem gerundeten Mittel
      // (herz.h, falsche Reihenfolge).
      const uint32_t summe = (halbstunde == h->neueste) ? (s_summe += puls)
                                                         : (uint32_t)h->mittel[platz] * n + puls;
      h->n[platz] = n + 1;
      h->mittel[platz] = prv_mittel(summe, n + 1);
    }
  }
  pbl_mutex_unlock(&s_herz_sperre);
}

// ---------------------------------------------------------------------------------------------
// Nachrechnen

// Was der Durchlauf sammelt, auf dem Stapel von KernelBG (kein Kernel-RAM, kein Heap). Jeder Platz
// gehoert der juengsten Halbstunde, die ihn belegt; die Reihenfolge der Minuten ist gleich.
typedef struct {
  uint32_t basis;                     //< Halbstunde zum Versatz 0
  uint32_t neueste;                   //< juengste Halbstunde ueberhaupt, 0 = keine Minute
  uint16_t summe[HERZ_HALBSTUNDEN];
  uint8_t versatz[HERZ_HALBSTUNDEN];  //< Halbstunde des Platzes - basis
  uint8_t min[HERZ_HALBSTUNDEN];
  uint8_t max[HERZ_HALBSTUNDEN];
  uint8_t n[HERZ_HALBSTUNDEN];
  uint16_t zonen_min[HERZ_ZONEN];
  uint16_t zonen_tag;
  uint8_t grenzen[HERZ_ZONEN - 1];
  uint32_t minuten;                   //< gesehen, fuer das Protokoll
} HerzRechnung;

// Unter der Kraepelin-Sperre (D18): nur rechnen, keine Sperre, nichts aus Pebble ausser der
// Ortszeit (ohne Sperre, time.c).
static void prv_rechnung_minute(const BoulderMinute *minute, void *kontext) {
  HerzRechnung *r = kontext;
  const uint32_t halbstunde = (uint32_t)(minute->utc / HERZ_HALBSTUNDE_S);
  if (halbstunde < r->basis) {
    return;
  }
  r->minuten++;
  if (halbstunde > r->neueste) {
    r->neueste = halbstunde;
  }
  const uint16_t tag = prv_tag(minute->utc);
  if (tag > r->zonen_tag) {
    memset(r->zonen_min, 0, sizeof(r->zonen_min));
    r->zonen_tag = tag;
  }
  if (!minute->puls) {
    return;
  }
  if (tag == r->zonen_tag) {
    prv_zaehlen(r->zonen_min, boulder_herz_zone(minute->puls, r->grenzen));
  }
  const int platz = (int)(halbstunde % HERZ_HALBSTUNDEN);
  const uint8_t versatz = (uint8_t)(halbstunde - r->basis);
  if (r->n[platz] && versatz < r->versatz[platz]) {
    return;  // der Platz gehoert schon einer juengeren Halbstunde
  }
  if (!r->n[platz] || versatz > r->versatz[platz]) {
    r->versatz[platz] = versatz;
    r->n[platz] = 0;
    r->summe[platz] = 0;
    r->min[platz] = minute->puls;
    r->max[platz] = minute->puls;
  }
  if (r->n[platz] < UINT8_MAX) {
    r->n[platz]++;
    r->summe[platz] += minute->puls;
    r->min[platz] = MIN(r->min[platz], minute->puls);
    r->max[platz] = MAX(r->max[platz], minute->puls);
  }
}

static void prv_nachrechnen_cb(void *data) {
  BOULDER_AUFGABE_LAEUFT(&s_aufgabe);
  if (!s_faellig) {
    return;
  }
  // Fertig sind die Minuten, die vor der laufenden begonnen haben; die laufende kommt mit dem
  // naechsten Minutenteil.
  const time_t jetzt = rtc_get_time();
  const time_t bis = jetzt - (jetzt % PBL_SEC_PER_MIN);
  const uint32_t juengste = (uint32_t)((bis - PBL_SEC_PER_MIN) / HERZ_HALBSTUNDE_S);
  HerzRechnung r = {
    .basis = (juengste >= SUCHE_HALBSTUNDEN) ? juengste - (SUCHE_HALBSTUNDEN - 1) : 0,
  };
  prv_grenzen_jetzt(r.grenzen);
  if (!boulder_kraepelin_minuten((time_t)r.basis * HERZ_HALBSTUNDE_S, bis, prv_rechnung_minute,
                                 &r)) {
    // Ohne Tracking-Teil (am Kabel, Tracking aus) kommt auch keine Minute: Der Takt versucht es
    // wieder, sobald er da ist (herz.h).
    PBL_LOG_INFO("Boulder: Herz nachrechnen geht noch nicht (kein Tracking-Teil)");
    return;
  }
  // Das Band endet mit der juengsten Minute; was davor 24 h oder aelter ist, faellt heraus. Unter
  // der Sperre nur Rechnen und Kopieren (herz.h), und zwar direkt in den Kern: eine zweite Kopie
  // (212 B) auf dem Stapel von KernelBG truege die Kette durch Pebbles Dateicode ueber 1 KB (T7.3).
  pbl_mutex_lock(&s_herz_sperre, PBL_FOREVER);
  BoulderHerz *h = &s_herz;
  memset(h, 0, sizeof(*h));
  h->neueste = r.neueste;
  h->zonen_tag = r.zonen_tag;
  memcpy(h->zonen_min, r.zonen_min, sizeof(h->zonen_min));
  memcpy(h->grenzen, r.grenzen, sizeof(h->grenzen));
  s_summe = 0;
  for (int platz = 0; platz < HERZ_HALBSTUNDEN; platz++) {
    const uint32_t halbstunde = r.basis + r.versatz[platz];
    if (!r.n[platz] || halbstunde + HERZ_HALBSTUNDEN <= r.neueste) {
      continue;
    }
    h->min[platz] = r.min[platz];
    h->max[platz] = r.max[platz];
    h->n[platz] = r.n[platz];
    h->mittel[platz] = prv_mittel(r.summe[platz], r.n[platz]);
    if (halbstunde == r.neueste) {
      s_summe = r.summe[platz];
    }
  }
  s_bereit = true;
  pbl_mutex_unlock(&s_herz_sperre);
  s_faellig = false;
  PBL_LOG_INFO("Boulder: Herz nachgerechnet, %lu Minuten, Grenzen %d/%d/%d",
               (unsigned long)r.minuten, r.grenzen[0], r.grenzen[1], r.grenzen[2]);
}

void boulder_herz_nachrechnen(void) {
  s_faellig = true;
  boulder_aufgabe_einreihen(&s_aufgabe);
}

void boulder_herz_takt(void) {
  // Auch am Kabel, wo kein Minutenteil laeuft: Ein neuer Maximalpuls aendert die Grenzen dort
  // ebenso. Vor dem ersten Nachrechnen steht es ohnehin an.
  if (s_bereit) {
    prv_grenzen_pruefen();
  } else if (s_faellig) {
    boulder_aufgabe_einreihen(&s_aufgabe);
  }
}

// ---------------------------------------------------------------------------------------------

bool boulder_herz_kopieren(BoulderHerz *aus) {
  // UNTER DER SPERRE NUR DIE KOPIE (3.1, Regel 3), direkt in den Puffer des Aufrufers.
  pbl_mutex_lock(&s_herz_sperre, PBL_FOREVER);
  const bool bereit = s_bereit;
  if (bereit) {
    memcpy(aus, &s_herz, sizeof(*aus));
  }
  pbl_mutex_unlock(&s_herz_sperre);
  if (!bereit) {
    memset(aus, 0, sizeof(*aus));
  }
  return bereit;
}

const BoulderHerz *boulder_herz_kern(void) {
  return &s_herz;
}

bool boulder_herz_bereit(void) {
  return s_bereit;
}

void boulder_herz_test_neustart(void) {
  memset(&s_herz, 0, sizeof(s_herz));
  s_summe = 0;
  s_bereit = false;
  s_faellig = false;
  s_aufgabe.steht_an = false;
}

#endif // CONFIG_BOULDER_HEALTH_APP
