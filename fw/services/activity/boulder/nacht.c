/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/activity/boulder/nacht.h"

#include "pbl/services/activity/boulder/eingang.h"
#include "pbl/services/activity/boulder/live.h"
#include "pbl/services/activity/boulder/nachlauf.h"
#include "pbl/services/activity/boulder/schlafanalyse.h"
#include "pbl/services/activity/boulder/zyklen.h"
#include <pbl/logging/logging.h>

PBL_LOG_MODULE_DECLARE(service_activity, CONFIG_SERVICE_ACTIVITY_LOG_LEVEL);

static time_t s_beginn;          //< Minute 0 des Fensters, 0 = keins
static int16_t s_letzte = -1;    //< zuletzt angenommene Minute des Fensters
static uint16_t s_ab;            //< Beginn der laufenden Nacht (0 oder Folgenacht)
static NachtStufe s_stufe;
static uint16_t s_fertig_auf;    //< Aufwachen, als die Nacht fertig wurde (ab Minute 0)
static bool s_fensterende;       //< eine Minute nach der Nacht (oder ein neues Fenster) kam
static uint8_t s_ruhepuls;       //< der letzten fertigen Nacht, fuer die Nickerchen
static uint16_t s_tag_ab;        //< Nickerchen ab dieser Minute des Tages suchen
static uint16_t s_gemeldet;
static uint16_t s_abgelehnt;

void boulder_nacht_zuruecksetzen(void) {
  s_beginn = 0;
  s_letzte = -1;
  s_ab = 0;
  s_stufe = NachtOffen;
  s_fertig_auf = 0;
  s_fensterende = false;
  s_ruhepuls = 0;
  s_tag_ab = 0;
  s_gemeldet = 0;
  s_abgelehnt = 0;
  boulder_live_neu();
  boulder_zyklen_zuruecksetzen();
}

static void prv_fenster_neu(time_t beginn) {
  s_beginn = beginn;
  s_letzte = -1;
  s_ab = 0;
  s_stufe = NachtOffen;
  s_fertig_auf = 0;
  s_fensterende = false;
  boulder_eingang_neu();
  boulder_live_neu();
  boulder_nachlauf_neu(0, 0);
  boulder_zyklen_nacht(beginn, 0);
}

bool boulder_nacht_minute(time_t beginn_utc, int minute, uint16_t vmc, uint8_t puls,
                          bool gueltig) {
  if (beginn_utc != s_beginn) {
    if (s_beginn && beginn_utc < s_beginn) {
      return false;  // Uhr zurueck ueber den Fensterbeginn: nichts verlaengern
    }
    if (s_beginn && s_stufe != NachtErledigt) {
      // Erst die alte Nacht abschliessen (Takt); die Minuten bis dahin gehen verloren.
      s_fensterende = true;
      return true;
    }
    prv_fenster_neu(beginn_utc);
  }
  if (minute <= s_letzte || minute < 0 || minute >= NACHT_FENSTER_MINUTEN) {
    return false;  // doppelt oder zurueck: der erste Satz gilt
  }
  if (minute < NACHT_MINUTEN) {
    if (s_stufe == NachtErledigt) {
      return false;
    }
    boulder_eingang_minute(minute, vmc, puls, gueltig);
    // Der Automat sieht jede Minute, auch die fehlenden (ungueltig = wach).
    for (int k = s_letzte + 1; k <= minute; k++) {
      boulder_live_minute(k);
    }
    s_letzte = (int16_t)minute;
    boulder_zyklen_live(boulder_live_schlaeft(), boulder_live_lauf_beginn(),
                        (uint16_t)(minute + 1));
    const int t = minute + 1;
    return t >= NACHT_RECHNEN_AB && (t % NACHLAUF_TAKT) == 2;
  }
  if (s_stufe != NachtErledigt) {
    s_fensterende = true;
    return true;
  }
  // Tag: dieselben Felder, Minute 0 = 11 Uhr.
  const int tag = minute - NACHT_MINUTEN;
  boulder_eingang_minute(tag, vmc, puls, gueltig);
  s_letzte = (int16_t)minute;
  const int t = tag + 1;
  return t >= NACHT_RECHNEN_AB && (t % NACHLAUF_TAKT) == 2;
}

bool boulder_nacht_hrv(time_t beginn_utc, int minute, uint16_t rmssd, uint8_t puls) {
  // Nur in die laufende Nacht: ein Fenster aus einer vergangenen (Uhr zurueck, Fensterende
  // dazwischen) gehoert nicht in diese Felder.
  if (beginn_utc != s_beginn || s_stufe == NachtErledigt || minute < 0 ||
      minute >= NACHT_MINUTEN) {
    return false;
  }
  return boulder_eingang_hrv(minute, rmssd, puls);
}

// Fertig, waechst, endgueltig (Spezifikation 4.4). Das Aufwachen kommt aus dem Fenster
// des Nachlaufs; entschieden wird erst an der Endfassung (die ganze Nacht, wie b26), denn
// nur sie gibt die Zahlen des Satzes.
static void prv_nacht_takt(void) {
  const uint16_t jetzt = (s_letzte + 1 < NACHT_MINUTEN) ? (uint16_t)(s_letzte + 1)
                                                        : NACHT_MINUTEN;
  boulder_nachlauf_rechnen(jetzt);
  const int vorhanden = boulder_eingang()->ende;  // an vorhandenen Minuten gemessen
  uint16_t auf = 0;
  const bool gesehen = boulder_nachlauf_lauf_aufwachen(&auf);
  bool noetig = s_fensterende;
  if (s_stufe == NachtOffen) {
    noetig |= gesehen && vorhanden - auf >= NACHT_WACH_FERTIG;
  } else {
    const bool waechst = gesehen && auf > s_fertig_auf;
    const int wach = vorhanden - (waechst ? auf : s_fertig_auf);
    noetig |= (waechst && wach >= NACHT_WACH_FERTIG) || wach >= NACHT_WACH_ENDGUELTIG;
  }
  if (!noetig) {
    boulder_zyklen_fest(boulder_nachlauf_fest_bis());
    return;
  }
  const SchlafErgebnis *e = boulder_nachlauf_endfassung();
  const int wach = e->gefunden ? vorhanden - (s_ab + e->aufwachen) : 0;
  const bool fertig = e->gefunden && (s_fensterende || wach >= NACHT_WACH_FERTIG);
  const bool endgueltig = s_fensterende || (e->gefunden && wach >= NACHT_WACH_ENDGUELTIG);
  if (fertig) {
    s_stufe = NachtFertig;
    s_fertig_auf = s_ab + e->aufwachen;
    s_ruhepuls = e->ruhepuls ? e->ruhepuls : s_ruhepuls;
    boulder_zyklen_fertig(s_fertig_auf);
    PBL_LOG_INFO("Boulder: Nacht fertig, %d min Schlaf", e->schlaf_minuten);
  }
  const uint16_t aufwachen = s_ab + e->aufwachen;
  boulder_zyklen_fest(boulder_nachlauf_fest_bis());
  if (!endgueltig) {
    return;
  }
  boulder_zyklen_endgueltig();
  if (!s_fensterende && e->gefunden) {
    // Schlaf danach ist eine eigene Nacht - erst ab NACHT_FOLGENACHT Minuten (b25).
    s_ab = aufwachen;
    s_stufe = NachtOffen;
    boulder_nachlauf_neu(s_ab, NACHT_FOLGENACHT);
    boulder_zyklen_nacht(s_beginn, s_ab);
    return;
  }
  // Fensterende: die Felder gehoeren ab jetzt dem Tag.
  s_stufe = NachtErledigt;
  s_fensterende = false;
  s_tag_ab = 0;
  if (s_letzte < NACHT_MINUTEN - 1) {
    s_letzte = NACHT_MINUTEN - 1;
  }
  boulder_eingang_neu();
  boulder_live_neu();
}

// Median der Pulse in [von, bis) ohne Puffer: hoechstens einmal je Nickerchen.
static uint8_t prv_median_puls(const BoulderEingang *e, int von, int bis, int *anzahl) {
  int n = 0;
  for (int i = von; i < bis; i++) {
    n += (e->gueltig[i] && e->puls[i]);
  }
  *anzahl = n;
  for (int b = 0; b < 256 && n > 0; b++) {
    int bis_b = 0;
    for (int i = von; i < bis; i++) {
      bis_b += (e->gueltig[i] && e->puls[i] && e->puls[i] <= b);
    }
    if (2 * bis_b >= n) {
      return (uint8_t)b;
    }
  }
  return 0;
}

// Nickerchen (Spezifikation 4.7): das Fenster des Tages mit der b26-Rechnung; gemeldet erst
// nach 15 Minuten wach, als abgeschlossene Sleep-Sitzung ohne R. Pebbles post_process macht
// daraus Nap, wenn sie in 12-21 Uhr liegt und hoechstens drei Stunden dauert.
static void prv_tag_takt(void) {
  const BoulderEingang *e = boulder_eingang();
  const int jetzt = s_letzte + 1 - NACHT_MINUTEN;
  int von = jetzt - NACHLAUF_FENSTER;
  von = (von < s_tag_ab) ? s_tag_ab : von;
  const int ende = (e->ende < jetzt) ? e->ende : jetzt;
  if (ende - von < NICKERCHEN_MIN_SCHLAF) {
    return;
  }
  SchlafArbeit *arbeit;
  SchlafErgebnis *r;
  boulder_nachlauf_rechenplatz(&arbeit, &r);
  const SchlafEingabe eingabe = {
    .bewegung = &e->bewegung[von],
    .puls = &e->puls[von],
    .gueltig = &e->gueltig[von],
    .anzahl = (uint16_t)(ende - von),
    .schwelle = SCHLAF_SCHWELLE_VMC,
    .min_schlaf = NICKERCHEN_MIN_SCHLAF,
  };
  if (!schlafanalyse_werte(&eingabe, arbeit, r) || ende - (von + r->aufwachen) <
                                                       NACHT_WACH_FERTIG) {
    return;
  }
  const int ein = von + r->einschlafen;
  const int auf = von + r->aufwachen;
  s_tag_ab = (uint16_t)auf;  // dieses Stueck Tag ist entschieden, so oder so
  int pulse = 0;
  const uint8_t median = prv_median_puls(e, ein, auf, &pulse);
  if (!s_ruhepuls || pulse < NICKERCHEN_PULSE_MIN || median > s_ruhepuls + NICKERCHEN_PULS_UEBER) {
    s_abgelehnt++;
    PBL_LOG_INFO("Boulder: kein Nickerchen (%d Pulse, Median %d, Ruhepuls %d)", pulse, median,
                 s_ruhepuls);
    return;
  }
  const time_t start = s_beginn + (time_t)(NACHT_MINUTEN + ein) * 60;
  if (boulder_zyklen_nickerchen(start, (uint16_t)(auf - ein))) {
    s_gemeldet++;
  }
}

void boulder_nacht_takt(void) {
  if (!s_beginn) {
    return;
  }
  if (s_stufe != NachtErledigt) {
    prv_nacht_takt();
  } else if (s_letzte >= NACHT_MINUTEN) {
    prv_tag_takt();
  }
}

NachtStufe boulder_nacht_stufe(void) {
  return s_stufe;
}

uint8_t boulder_nacht_ruhepuls(void) {
  return s_ruhepuls;
}

uint16_t boulder_nacht_nickerchen_gemeldet(void) {
  return s_gemeldet;
}

uint16_t boulder_nacht_nickerchen_abgelehnt(void) {
  return s_abgelehnt;
}
