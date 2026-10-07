/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/activity/boulder/nacht.h"

#include "pbl/services/activity/boulder/ausgabe.h"
#include "pbl/services/activity/boulder/eingang.h"
#include "pbl/services/activity/boulder/live.h"
#include "pbl/services/activity/boulder/nachlauf.h"
#include "pbl/services/activity/boulder/schlafanalyse.h"
#include "pbl/services/activity/boulder/zyklen.h"
#include <pbl/logging/logging.h>

#include <string.h>

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
// AP4: nach dem Boot bis "Laden" nur der Eingang (Ring); das Fenster des Rings.
static bool s_wartet;
static time_t s_ring_beginn;
// AP4: die zuletzt endgueltige Nacht des Fensters, fuer den Kopf (Abschluss nach Neustart).
static bool s_alt;
static uint16_t s_alt_ab;
static uint16_t s_alt_auf;
static uint16_t s_alt_fest;
static uint16_t s_aufgeben;  //< Takte am Fensterende mit einer nicht ganz geschickten Nacht
// V3: die Ruhepulse der letzten sieben Naechte (Kalorien, Maximalpuls).
static BoulderRuheRing s_ruhe;

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
  s_wartet = false;
  s_ring_beginn = 0;
  s_alt = false;
  s_alt_ab = 0;
  s_alt_auf = 0;
  s_alt_fest = 0;
  s_aufgeben = 0;
  memset(&s_ruhe, 0, sizeof(s_ruhe));
  boulder_eingang_neu();
  boulder_nachlauf_neu(0, 0);
  boulder_live_neu();
  boulder_zyklen_zuruecksetzen();
}

// V3: der Ruhepuls der Nacht ab `nacht`. Dieselbe Nacht ersetzt ihren Wert (jede Endfassung bis
// "endgueltig"), eine neue verdraengt die aelteste.
static void prv_ruhe_merken(time_t nacht, uint8_t puls) {
  if (s_ruhe.nacht != (uint32_t)nacht) {
    s_ruhe.neu = (uint8_t)((s_ruhe.neu + 1) % NACHT_RUHE_NAECHTE);
    s_ruhe.nacht = (uint32_t)nacht;
  }
  s_ruhe.puls[s_ruhe.neu] = puls;
}

static void prv_fenster_neu(time_t beginn) {
  s_beginn = beginn;
  s_letzte = -1;
  s_ab = 0;
  s_stufe = NachtOffen;
  s_fertig_auf = 0;
  s_fensterende = false;
  s_alt = false;
  s_aufgeben = 0;
  boulder_eingang_neu();
  boulder_live_neu();
  boulder_nachlauf_neu(0, 0);
  boulder_zyklen_nacht(beginn, 0);
}

bool boulder_nacht_minute(time_t beginn_utc, int minute, uint16_t vmc, uint8_t puls,
                          bool gueltig) {
  if (s_wartet) {
    boulder_nacht_ring_minute(beginn_utc, minute, vmc, puls, gueltig);
    return false;
  }
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
  // Eine angefangene, nicht ganz geschickte Nacht geht ganz neu (ausgabe.h).
  bool noetig = s_fensterende || boulder_ausgabe_offen();
  if (s_stufe == NachtOffen) {
    noetig |= gesehen && vorhanden - auf >= NACHT_WACH_FERTIG;
  } else {
    const bool waechst = gesehen && auf > s_fertig_auf;
    const int wach = vorhanden - (waechst ? auf : s_fertig_auf);
    noetig |= (waechst && wach >= NACHT_WACH_FERTIG) || wach >= NACHT_WACH_ENDGUELTIG;
    // JEDE AENDERUNG BIS ENDGUELTIG (b26 rechnete alle 15 Minuten ganz): ein spaeteres
    // Aufwachen erkennt schon der Nachlauf, ein anderer Schlaf erst die Endfassung.
    noetig |= (jetzt % NACHT_NACHSEHEN) == 2;
  }
  if (!noetig) {
    boulder_zyklen_fest(boulder_nachlauf_fest_bis());
    return;
  }
  const SchlafErgebnis *e = boulder_nachlauf_endfassung();
  const time_t satz_beginn = s_beginn + (time_t)s_ab * 60;
  const int wach = e->gefunden ? vorhanden - (s_ab + e->aufwachen) : 0;
  const bool fertig = e->gefunden && (s_fensterende || wach >= NACHT_WACH_FERTIG);
  bool endgueltig = s_fensterende || (e->gefunden && wach >= NACHT_WACH_ENDGUELTIG);
  // Das NachtBild bei jeder Endfassung (b26), die Datei nur "fertig" und bei Aenderung.
  bool ablage = boulder_ausgabe_bild(satz_beginn, e, fertig);
  if (fertig) {
    if (s_stufe != NachtFertig || s_fertig_auf != s_ab + e->aufwachen) {
      PBL_LOG_INFO("Boulder: Nacht fertig, %d min Schlaf", e->schlaf_minuten);
      ablage = true;
    }
    s_stufe = NachtFertig;
    s_fertig_auf = s_ab + e->aufwachen;
    s_ruhepuls = e->ruhepuls ? e->ruhepuls : s_ruhepuls;
    if (e->ruhepuls) {
      prv_ruhe_merken(satz_beginn, e->ruhepuls);
    }
    boulder_zyklen_fertig(s_fertig_auf);
    // Wie b26: nur wenn Beginn, Aufwachen oder Schlaf neu sind.
    if (boulder_ausgabe_noetig(satz_beginn, e->aufwachen, e->schlaf_minuten)) {
      boulder_ausgabe_nacht(satz_beginn, e);
    }
  } else if (boulder_ausgabe_offen()) {
    // Nicht mehr fertig (die Nacht waechst): b26 schickte erst wieder, wenn sie es ist.
    boulder_ausgabe_verwerfen();
  }
  // ENDGUELTIG ERST, WENN DIE NACHT GANZ IM DATALOGGING LIEGT (b26): danach rechnet der
  // Nachlauf die Folgenacht oder den Tag, und die Endfassung dieser Nacht gibt es nicht mehr.
  // Am Fensterende nicht ewig: nach NACHT_AUFGEBEN_TAKTE gibt der Lebenslauf sie auf.
  if (endgueltig && boulder_ausgabe_offen()) {
    if (!s_fensterende || ++s_aufgeben < NACHT_AUFGEBEN_TAKTE) {
      endgueltig = false;
    } else {
      PBL_LOG_WRN("Boulder: Nacht nicht geschickt, aufgegeben");
    }
  }
  if (ablage) {
    boulder_speicher_anstossen(true);
  }
  const uint16_t aufwachen = s_ab + e->aufwachen;
  boulder_zyklen_fest(boulder_nachlauf_fest_bis());
  if (!endgueltig) {
    return;
  }
  boulder_zyklen_endgueltig();
  boulder_ausgabe_endgueltig();
  // Fuer den Kopf: ihr Abschluss laeuft noch ein paar Minuten, ein Neustart setzt ihn fort.
  s_alt = e->gefunden;
  s_alt_ab = s_ab;
  s_alt_auf = aufwachen;
  s_alt_fest = boulder_nachlauf_fest_bis();
  s_aufgeben = 0;
  boulder_speicher_anstossen(true);
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

uint8_t boulder_nacht_ruhepuls_median(void) {
  uint8_t werte[NACHT_RUHE_NAECHTE];
  int n = 0;
  for (int i = 0; i < NACHT_RUHE_NAECHTE; i++) {
    const uint8_t p = s_ruhe.puls[i];
    if (!p) {
      continue;
    }
    // Einfuegen sortiert: hoechstens sieben Werte.
    int k = n++;
    while (k > 0 && werte[k - 1] > p) {
      werte[k] = werte[k - 1];
      k--;
    }
    werte[k] = p;
  }
  if (!n) {
    return 0;
  }
  // Bei gerader Zahl das Mittel der beiden mittleren, abgeschnitten wie Boulders toInt().
  return (n % 2) ? werte[n / 2] : (uint8_t)((werte[n / 2 - 1] + werte[n / 2]) / 2);
}

void boulder_nacht_ruhe_ablage(BoulderRuheRing *ring) {
  *ring = s_ruhe;
}

void boulder_nacht_ruhe_laden(const BoulderRuheRing *ring) {
  if (ring->neu >= NACHT_RUHE_NAECHTE) {
    return;
  }
  s_ruhe = *ring;
}

uint16_t boulder_nacht_nickerchen_gemeldet(void) {
  return s_gemeldet;
}

uint16_t boulder_nacht_nickerchen_abgelehnt(void) {
  return s_abgelehnt;
}

// ---------------------------------------------------------------------------------------
// Nach dem Boot (AP4)

void boulder_nacht_ring_neu(time_t beginn) {
  boulder_nacht_zuruecksetzen();
  s_wartet = true;
  s_ring_beginn = beginn;
}

void boulder_nacht_ring_minute(time_t beginn_utc, int minute, uint16_t vmc, uint8_t puls,
                               bool gueltig) {
  if (!s_wartet) {
    return;
  }
  if (beginn_utc != s_ring_beginn) {
    // Ohne Minuten aus der Datei gilt das Fenster der ersten Minute (Boot kurz vor 20 Uhr,
    // Uhr gestellt); sonst gehoert die Minute nicht zu dieser Nacht.
    if (s_ring_beginn && boulder_eingang()->letzte >= 0) {
      return;
    }
    s_ring_beginn = beginn_utc;
    boulder_eingang_neu();
  }
  if (minute >= 0 && minute < NACHT_MINUTEN) {
    boulder_eingang_minute(minute, vmc, puls, gueltig);
  }
}

bool boulder_nacht_wartet(void) {
  return s_wartet;
}

time_t boulder_nacht_fenster(void) {
  return s_wartet ? s_ring_beginn : s_beginn;
}

bool boulder_nacht_rest_noetig(void) {
  return !s_wartet && s_beginn && s_stufe != NachtErledigt && boulder_live_lauf_beginn() >= 0;
}

void boulder_nacht_kopf(BoulderKopf *k) {
  k->fenster = (uint32_t)s_beginn;
  k->stufe = (uint8_t)s_stufe;
  k->ab = s_ab;
  k->fertig_auf = s_fertig_auf;
  k->ruhepuls = s_ruhepuls;
  k->tag_ab = s_tag_ab;
  k->alt_offen = s_alt && boulder_zyklen_abschluss_offen();
  if (k->alt_offen) {
    k->alt_ab = s_alt_ab;
    k->alt_auf = s_alt_auf;
    k->alt_fest = s_alt_fest;
  }
}

// Die Naechte des Fensters noch einmal so, wie sie im Betrieb liefen - stumm, dann meldet
// boulder_zyklen_nach_boot sie mit ihren Starts. Gleiches Buch, gleiche Starts: Pebble sieht
// dieselben Sitzungen, nichts doppelt.
void boulder_nacht_laden(const BoulderKopf *k, uint16_t fest) {
  s_wartet = false;
  const BoulderEingang *e = boulder_eingang();
  if (!s_ring_beginn || (!k && e->letzte < 0)) {
    // Nichts zu laden: die naechste Minute beginnt das Fenster wie sonst.
    s_beginn = 0;
    return;
  }
  s_beginn = s_ring_beginn;
  s_letzte = e->letzte;
  s_ab = 0;
  s_stufe = NachtOffen;
  s_fertig_auf = 0;
  s_fensterende = false;
  s_tag_ab = 0;
  s_alt = false;
  s_aufgeben = 0;
  if (k) {
    s_stufe = (k->stufe <= NachtErledigt) ? (NachtStufe)k->stufe : NachtOffen;
    s_ab = (k->ab < NACHT_MINUTEN) ? k->ab : 0;
    s_fertig_auf = k->fertig_auf;
    s_ruhepuls = k->ruhepuls;
    s_tag_ab = k->tag_ab;
    s_alt = k->alt_offen;
    s_alt_ab = k->alt_ab;
    s_alt_auf = k->alt_auf;
    s_alt_fest = k->alt_fest;
    boulder_nachlauf_fortsetzen(s_ab, s_ab ? NACHT_FOLGENACHT : 0, fest);
  } else {
    boulder_nachlauf_neu(0, 0);
  }
  boulder_live_neu();
  boulder_zyklen_zuruecksetzen();
  boulder_zyklen_stumm(true);
  if (s_alt) {
    boulder_zyklen_nacht(s_beginn, s_alt_ab);
    boulder_zyklen_fertig(s_alt_auf);
    boulder_zyklen_fest(s_alt_fest);
    boulder_zyklen_endgueltig();
  }
  if (s_stufe == NachtErledigt) {
    boulder_eingang_neu();  // die Felder gehoeren dem Tag
    s_letzte = NACHT_MINUTEN - 1;
  } else {
    for (int m = 0; m <= s_letzte; m++) {
      boulder_live_minute(m);
    }
    boulder_zyklen_nacht(s_beginn, s_ab);
    if (s_stufe == NachtFertig) {
      boulder_zyklen_fertig(s_fertig_auf);
    }
    boulder_zyklen_fest(boulder_nachlauf_fest_bis());
    if (s_letzte >= 0) {
      boulder_zyklen_live(boulder_live_schlaeft(), boulder_live_lauf_beginn(),
                          (uint16_t)(s_letzte + 1));
    }
  }
  boulder_zyklen_nach_boot();
  PBL_LOG_INFO("Boulder: Nacht geladen, Stufe %d, Minute %d", (int)s_stufe, (int)s_letzte);
}
