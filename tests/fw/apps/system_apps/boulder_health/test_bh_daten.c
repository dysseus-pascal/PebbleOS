/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "apps/system/boulder_health/daten.h"

#include "pbl/services/activity/boulder/nacht.h"

#include "pbl/services/clock.h"
#include "shell/prefs_syscalls.h"
#include "syscall/syscall.h"

#include <string.h>

// Das Laden der Health-App (Spezifikation Schritt 3, 2.6.2): Phase 1 liest jede Quelle einmal,
// Phase 2 nur, was nach dem Boot noch fehlte, hoechstens BH_NACHFRAGEN mal; der Minutentakt liest
// Nacht und Naechte nur bei einer neuen Nacht. Nachgebaut sind die Syscalls; daten.c und
// rechnung.c (fuer Tag und Wochentag) sind echt.

// --- Die Syscalls, mitgezaehlt ---

enum {
  Lage,
  Nacht,
  Spo2,
  Naechte,
  Herz,
  Woche,
  Metrik,
  Sitzungen,
  Uhr,
  AnzahlQuellen,
};
static int s_gelesen[AnzahlQuellen];

static bool s_lage_da;
static BoulderLage s_lage;
static BoulderLesen s_nacht_antwort;
static bool s_naechte_da;
static BoulderNaechte s_naechte;
static bool s_herz_da;
static bool s_woche_da;
static uint32_t s_spo2_anzahl;
static time_t s_spo2_von, s_spo2_bis;
static time_t s_jetzt;
static int s_gmtoff;
static time_t s_umstellung; //!< ab hier eine Stunde weniger Versatz (Ende der Sommerzeit), 0 = nie
static NachtBild s_bild;
static UnitsDistance s_einheit;
static bool s_h24;

bool sys_boulder_lage(BoulderLage *aus) {
  s_gelesen[Lage]++;
  *aus = s_lage;
  return s_lage_da;
}
BoulderLesen sys_boulder_nacht(NachtBild *aus) {
  s_gelesen[Nacht]++;
  *aus = s_bild;
  return s_nacht_antwort;
}
uint32_t sys_boulder_spo2(time_t von, time_t bis, Spo2Wert *aus, uint32_t max) {
  s_gelesen[Spo2]++;
  s_spo2_von = von;
  s_spo2_bis = bis;
  cl_assert_equal_i(max, SPO2_WERTE_MAX);
  for (uint32_t i = 0; i < s_spo2_anzahl; i++) {
    aus[i] = (Spo2Wert){.zeit = von + (time_t)i, .prozent = 95};
  }
  return s_spo2_anzahl;
}
bool sys_boulder_naechte(BoulderNaechte *aus) {
  s_gelesen[Naechte]++;
  *aus = s_naechte;
  return s_naechte_da;
}
bool sys_boulder_herz(BoulderHerz *aus) {
  s_gelesen[Herz]++;
  memset(aus, 0, sizeof(*aus));
  return s_herz_da;
}
bool sys_boulder_woche(BoulderWoche *aus) {
  s_gelesen[Woche]++;
  memset(aus, 0, sizeof(*aus));
  return s_woche_da;
}
bool sys_activity_get_metric(ActivityMetric metric, uint32_t history_len, int32_t *history) {
  s_gelesen[Metrik]++;
  cl_assert_equal_i(history_len, 1);
  switch (metric) {
    case ActivityMetricStepCount:
      *history = 6230;
      break;
    case ActivityMetricDistanceMeters:
      *history = 4800;
      break;
    case ActivityMetricActiveSeconds:
      *history = 42 * 60;
      break;
    case ActivityMetricRestingKCalories:
      *history = 1500;
      break;
    case ActivityMetricActiveKCalories:
      *history = 390;
      break;
    case ActivityMetricHeartRateRawBPM:
      *history = 61;
      break;
    case ActivityMetricHeartRateRawUpdatedTimeUTC:
      *history = (int32_t)(s_jetzt - 30);
      break;
    default:
      cl_fail("Metrik, die die App nicht braucht");
  }
  return true;
}
bool sys_activity_get_sessions(uint32_t *session_entries, ActivitySession *sessions) {
  s_gelesen[Sitzungen]++;
  cl_assert_equal_i(*session_entries, ACTIVITY_MAX_ACTIVITY_SESSIONS_COUNT);
  *session_entries = 2;
  sessions[0] = (ActivitySession){.type = ActivitySessionType_Walk, .length_min = 20};
  sessions[1] = (ActivitySession){.type = ActivitySessionType_Nap, .length_min = 25};
  return true;
}
time_t sys_get_time(void) {
  s_gelesen[Uhr]++;
  return s_jetzt;
}
struct tm *sys_localtime_r(const time_t *timep, struct tm *result) {
  // 12.10.2026, 14:05 Ortszeit, Sommerzeit Mitteleuropa
  memset(result, 0, sizeof(*result));
  result->tm_year = 126;
  result->tm_yday = 284;
  result->tm_hour = 14;
  result->tm_min = 5;
  result->tm_gmtoff = (s_umstellung && *timep >= s_umstellung) ? s_gmtoff - 3600 : s_gmtoff;
  return result;
}
bool clock_is_24h_style(void) {
  return s_h24;
}
UnitsDistance sys_shell_prefs_get_units_distance(void) {
  return s_einheit;
}

void test_bh_daten__initialize(void) {
  memset(s_gelesen, 0, sizeof(s_gelesen));
  s_lage_da = true;
  memset(&s_lage, 0, sizeof(s_lage));
  s_lage.fassung = BOULDER_LAGE_FASSUNG;
  s_lage.tracking = true;
  s_lage.nacht_stufe = 2;
  s_lage.fenster_utc = 1791662400;
  s_nacht_antwort = BoulderLesenDa;
  s_naechte_da = true;
  memset(&s_naechte, 0, sizeof(s_naechte));
  s_herz_da = true;
  s_woche_da = true;
  s_spo2_anzahl = 3;
  s_jetzt = 1791807900;
  s_gmtoff = 2 * 60 * 60;
  s_umstellung = 0;
  memset(&s_bild, 0, sizeof(s_bild));
  s_bild.schlaf_minuten = 472;
  s_einheit = UnitsDistance_KM;
  s_h24 = true;
}

void test_bh_daten__cleanup(void) {
}

static void prv_gelesen(int lage, int nacht, int spo2, int naechte, int herz, int woche) {
  cl_assert_equal_i(s_gelesen[Lage], lage);
  cl_assert_equal_i(s_gelesen[Nacht], nacht);
  cl_assert_equal_i(s_gelesen[Spo2], spo2);
  cl_assert_equal_i(s_gelesen[Naechte], naechte);
  cl_assert_equal_i(s_gelesen[Herz], herz);
  cl_assert_equal_i(s_gelesen[Woche], woche);
}

// Phase 1 liest jede Quelle genau einmal und uebernimmt, was Pebble und die Variante liefern.
void test_bh_daten__laden(void) {
  BhDaten d;
  memset(&d, 0xAB, sizeof(d));
  bh_daten_laden(&d);
  prv_gelesen(1, 1, 1, 1, 1, 1);
  cl_assert_equal_i(s_gelesen[Sitzungen], 1);
  cl_assert_equal_i(d.offen, 0);
  cl_assert_equal_i(d.nachfragen, 0);
  cl_assert_equal_i(d.nacht.schlaf_minuten, 472);
  cl_assert(d.lage_da && d.naechte_da && d.herz_da && d.woche_da);
  cl_assert_equal_i(d.schritte, 6230);
  cl_assert_equal_i(d.meter, 4800);
  cl_assert_equal_i(d.aktiv_s, 42 * 60);
  cl_assert_equal_i(d.kcal, 1890); // V12: Ruhe- und Aktivkalorien
  cl_assert_equal_i(d.puls, 61);
  cl_assert_equal_i(d.puls_zeit, s_jetzt - 30);
  cl_assert_equal_i(d.sitzungen_anzahl, 2);
  // SpO2 der letzten 24 h, bis jetzt eingeschlossen.
  cl_assert_equal_i(d.spo2_anzahl, 3);
  cl_assert_equal_i(s_spo2_von, s_jetzt - 24 * 60 * 60);
  cl_assert_equal_i(s_spo2_bis, s_jetzt + 1);
  // Uhr und Einstellungen
  cl_assert_equal_i(d.jetzt, s_jetzt);
  cl_assert_equal_i(d.heute, bh_tag(126, 284));
  cl_assert_equal_i(d.minute, 14 * 60 + 5);
  cl_assert_equal_i(d.wochentag, 0); // Montag, 12.10.2026
  cl_assert(d.h24);
  cl_assert(!d.meilen);
}

void test_bh_daten__einheit_und_uhrzeit(void) {
  s_einheit = UnitsDistance_Miles;
  s_h24 = false;
  BhDaten d;
  bh_daten_laden(&d);
  cl_assert(d.meilen);
  cl_assert(!d.h24);
}

// Nach dem Boot sagen S1 "spaeter" und S2, S5, S6, S7 false, bis KernelBG geladen hat. Phase 2 holt
// nur diese nach, bis alles da ist.
void test_bh_daten__nachladen(void) {
  s_nacht_antwort = BoulderLesenSpaeter;
  s_naechte_da = false;
  s_herz_da = false;
  BhDaten d;
  bh_daten_laden(&d);
  cl_assert_equal_i(d.offen, BhQuelleNacht | BhQuelleNaechte | BhQuelleHerz);
  prv_gelesen(1, 1, 1, 1, 1, 1);

  cl_assert(bh_daten_nachladen(&d)); // noch alles offen
  prv_gelesen(1, 2, 1, 2, 2, 1);

  s_nacht_antwort = BoulderLesenDa;
  s_herz_da = true;
  cl_assert(bh_daten_nachladen(&d)); // Naechte fehlen noch
  cl_assert_equal_i(d.offen, BhQuelleNaechte);
  prv_gelesen(1, 3, 1, 3, 3, 1);

  s_naechte_da = true;
  cl_assert(!bh_daten_nachladen(&d));
  cl_assert_equal_i(d.offen, 0);
  prv_gelesen(1, 3, 1, 4, 3, 1);

  // Ist nichts offen, liest Phase 2 nichts.
  cl_assert(!bh_daten_nachladen(&d));
  prv_gelesen(1, 3, 1, 4, 3, 1);
}

// "Keine Nacht" ist eine Antwort, kein Grund nachzufragen.
void test_bh_daten__keine_nacht(void) {
  s_nacht_antwort = BoulderLesenKeine;
  BhDaten d;
  bh_daten_laden(&d);
  cl_assert_equal_i(d.offen, 0);
}

// Kommt eine Quelle nie, hoert Phase 2 nach BH_NACHFRAGEN auf.
void test_bh_daten__nachladen_hoert_auf(void) {
  s_lage_da = false;
  s_woche_da = false;
  BhDaten d;
  bh_daten_laden(&d);
  cl_assert_equal_i(d.offen, BhQuelleLage | BhQuelleWoche);
  int runden = 0;
  while (bh_daten_nachladen(&d)) {
    runden++;
    cl_assert(runden < 10);
  }
  cl_assert_equal_i(runden + 1, BH_NACHFRAGEN);
  cl_assert_equal_i(s_gelesen[Lage], 1 + BH_NACHFRAGEN);
  cl_assert_equal_i(s_gelesen[Woche], 1 + BH_NACHFRAGEN);
  cl_assert(!bh_daten_nachladen(&d));
  cl_assert_equal_i(s_gelesen[Lage], 1 + BH_NACHFRAGEN);
}

// Der Minutentakt liest Lage, Herz, Woche, SpO2 und Pebbles Werte; Nacht und Naechte nur, wenn die
// Lage eine neue Nacht meldet oder sie noch offen sind.
void test_bh_daten__auffrischen(void) {
  BhDaten d;
  bh_daten_laden(&d);
  memset(s_gelesen, 0, sizeof(s_gelesen));

  bh_daten_auffrischen(&d);
  prv_gelesen(1, 0, 1, 0, 1, 1);
  cl_assert_equal_i(s_gelesen[Sitzungen], 1);
  cl_assert_equal_i(s_gelesen[Uhr], 1);

  s_lage.nacht_stufe = 3; // die Nacht ist fertig
  bh_daten_auffrischen(&d);
  prv_gelesen(2, 1, 2, 1, 2, 2);

  s_lage.fenster_utc += 24 * 60 * 60; // ein neues Nachtfenster
  bh_daten_auffrischen(&d);
  prv_gelesen(3, 2, 3, 2, 3, 3);

  bh_daten_auffrischen(&d);
  prv_gelesen(4, 2, 4, 2, 4, 4);
}

void test_bh_daten__auffrischen_holt_offenes(void) {
  s_nacht_antwort = BoulderLesenSpaeter;
  BhDaten d;
  bh_daten_laden(&d);
  memset(s_gelesen, 0, sizeof(s_gelesen));
  bh_daten_auffrischen(&d);
  cl_assert_equal_i(s_gelesen[Nacht], 1);
  // Ohne Lage beim Start liest die erste Minute alles.
  test_bh_daten__initialize();
  s_lage_da = false;
  bh_daten_laden(&d);
  s_lage_da = true;
  memset(s_gelesen, 0, sizeof(s_gelesen));
  bh_daten_auffrischen(&d);
  prv_gelesen(1, 1, 1, 1, 1, 1);
}

// Die Naechte kommen als Ring: tage[neu] ist der juengste Wachtag, die aelteren stehen rueckwaerts
// davor, ueber den Anfang hinaus.
void test_bh_daten__nachttage_aus_dem_ring(void) {
  s_naechte.fassung = BOULDER_NAECHTE_FASSUNG;
  s_naechte.anzahl = 4;
  s_naechte.neu = 1;
  s_naechte.tage[1] = (BoulderNachtEintrag){.tag = 9900, .hrv = 50, .ruhepuls = 54};
  s_naechte.tage[0] = (BoulderNachtEintrag){.tag = 9899, .hrv = 0, .ruhepuls = 55};
  s_naechte.tage[NAECHTE_MAX - 1] = (BoulderNachtEintrag){.tag = 9897, .hrv = 48, .ruhepuls = 0};
  s_naechte.tage[NAECHTE_MAX - 2] = (BoulderNachtEintrag){.tag = 9896, .hrv = 47, .ruhepuls = 56};
  s_naechte.tage[2] = (BoulderNachtEintrag){.tag = 1, .hrv = 99, .ruhepuls = 99}; // ausserhalb
  BhDaten d;
  bh_daten_laden(&d);
  BhNachtTag tage[NAECHTE_MAX];
  cl_assert_equal_i(bh_daten_nachttage(&d, tage), 4);
  cl_assert_equal_i(tage[0].tag, 9900);
  cl_assert_equal_i(tage[0].hrv, 50);
  cl_assert_equal_i(tage[0].ruhepuls, 54);
  cl_assert_equal_i(tage[1].tag, 9899);
  cl_assert_equal_i(tage[2].tag, 9897);
  cl_assert_equal_i(tage[3].tag, 9896);
  cl_assert_equal_i(tage[3].ruhepuls, 56);
}

void test_bh_daten__nachttage_ohne_liste(void) {
  s_naechte.anzahl = 3;
  s_naechte_da = false;
  BhDaten d;
  bh_daten_laden(&d);
  BhNachtTag tage[NAECHTE_MAX];
  cl_assert_equal_i(bh_daten_nachttage(&d, tage), 0);
  // Ein ungueltiger Ring liefert nichts statt Unsinn.
  test_bh_daten__initialize();
  s_naechte.anzahl = 3;
  s_naechte.neu = NAECHTE_MAX;
  bh_daten_laden(&d);
  cl_assert_equal_i(bh_daten_nachttage(&d, tage), 0);
}

// Der Puls jetzt: nur die beiden Metriken, sonst nichts.
void test_bh_daten__puls(void) {
  BhDaten d;
  bh_daten_laden(&d);
  memset(s_gelesen, 0, sizeof(s_gelesen));
  bh_daten_puls(&d);
  cl_assert_equal_i(s_gelesen[Metrik], 2);
  prv_gelesen(0, 0, 0, 0, 0, 0);
  cl_assert_equal_i(d.puls, 61);
}

// --- Gegenpruefung Schritt 3 ---

#define HEUTE        9781       // 12.10.2026
#define NACHT_BEGINN 1791748800 // 11.10.2026 20:00 UTC = 22:00 MESZ

// Eine Nacht als S1: aufgewacht um 07:00 Ortszeit des 12.10. (Wachtag HEUTE).
static void prv_bild(uint16_t schlaf, uint16_t hrv, uint8_t ruhepuls, bool fertig) {
  memset(&s_bild, 0, sizeof(s_bild));
  s_bild.beginn_utc = NACHT_BEGINN;
  s_bild.utc_to_local = 2 * 60 * 60;
  s_bild.einschlafen = 17;
  s_bild.aufwachen = 540;
  s_bild.schlaf_minuten = schlaf;
  s_bild.hrv = hrv;
  s_bild.ruhepuls = ruhepuls;
  s_bild.fertig = fertig;
  s_bild.anzahl = 1;
  s_bild.abschnitte[0] = (NachtDlsAbschnitt){.von = 17, .bis = 540, .phase = 1};
}

// Die Liste mit zwei Wachtagen bis `neu`; der juengste mit `min` Minuten Schlaf.
static void prv_liste(uint16_t neu, uint16_t hrv, uint8_t ruhepuls, uint16_t min_ruhe,
                      uint16_t min_hrv) {
  s_naechte.fassung = BOULDER_NAECHTE_FASSUNG;
  s_naechte.anzahl = 2;
  s_naechte.neu = 1;
  s_naechte.tage[0] = (BoulderNachtEintrag){.tag = neu - 1, .hrv = 51, .ruhepuls = 53};
  s_naechte.tage[1] = (BoulderNachtEintrag){.tag = neu, .hrv = hrv, .ruhepuls = ruhepuls};
  s_naechte.min_ruhe = min_ruhe;
  s_naechte.min_hrv = min_hrv;
}

// C2: Eine fertige Nacht steht erst zwei Stunden nach dem Aufwachen endgueltig in der Liste; bis
// dahin gilt ihr Wachtag schon, wie bei Boulder, das sie bei "fertig" bekommt.
void test_bh_daten__nachttage_mit_der_fertigen_nacht(void) {
  prv_liste(HEUTE - 1, 50, 54, 470, 470);
  prv_bild(430, 61, 50, true);
  BhDaten d;
  bh_daten_laden(&d);
  BhNachtTag tage[NAECHTE_MAX];
  cl_assert_equal_i(bh_daten_nachttage(&d, tage), 3);
  cl_assert_equal_i(tage[0].tag, HEUTE);
  cl_assert_equal_i(tage[0].hrv, 61);
  cl_assert_equal_i(tage[0].ruhepuls, 50);
  cl_assert_equal_i(tage[1].tag, HEUTE - 1);
  uint16_t schlaf = 0;
  bool fertig = false;
  cl_assert(bh_daten_schlaf(&d, HEUTE, &schlaf, &fertig));
  cl_assert_equal_i(schlaf, 430);
  cl_assert(fertig);
  cl_assert(!bh_daten_schlaf(&d, HEUTE - 2, &schlaf, &fertig));
}

// C4: Nach einer Folgenacht gilt fuer den Tag die laengste Nacht (Boulders jeTag), auch wenn S1
// die kuerzere ist; ist S1 laenger, gilt S1.
void test_bh_daten__folgenacht_die_laengste_gilt(void) {
  prv_liste(HEUTE, 55, 49, 450, 450);
  prv_bild(105, 70, 58, true);
  BhDaten d;
  bh_daten_laden(&d);
  BhNachtTag tage[NAECHTE_MAX];
  cl_assert_equal_i(bh_daten_nachttage(&d, tage), 2);
  cl_assert_equal_i(tage[0].hrv, 55);
  cl_assert_equal_i(tage[0].ruhepuls, 49);
  uint16_t schlaf = 0;
  bool fertig = false;
  cl_assert(bh_daten_schlaf(&d, HEUTE, &schlaf, &fertig));
  cl_assert_equal_i(schlaf, 450);
  cl_assert(fertig);

  test_bh_daten__initialize();
  prv_liste(HEUTE, 55, 49, 450, 450);
  prv_bild(500, 70, 58, false);
  bh_daten_laden(&d);
  cl_assert_equal_i(bh_daten_nachttage(&d, tage), 2);
  cl_assert_equal_i(tage[0].hrv, 70);
  cl_assert_equal_i(tage[0].ruhepuls, 58);
  cl_assert(bh_daten_schlaf(&d, HEUTE, &schlaf, &fertig));
  cl_assert_equal_i(schlaf, 500);
  cl_assert(!fertig); // S1 waechst noch
}

// Die HRV aus der laengsten Nacht mit HRV (jeTagMitHrv), der Ruhepuls aus der laengsten.
void test_bh_daten__hrv_aus_der_laengsten_nacht_mit_hrv(void) {
  prv_liste(HEUTE, 0, 49, 450, 0);
  prv_bild(120, 66, 58, true);
  BhDaten d;
  bh_daten_laden(&d);
  BhNachtTag tage[NAECHTE_MAX];
  bh_daten_nachttage(&d, tage);
  cl_assert_equal_i(tage[0].hrv, 66);
  cl_assert_equal_i(tage[0].ruhepuls, 49);
}

// Eine Nacht, die aelter ist als der juengste Tag der Liste, aendert nichts.
void test_bh_daten__aeltere_nacht_aendert_nichts(void) {
  prv_liste(HEUTE + 1, 55, 49, 450, 450);
  prv_bild(500, 70, 58, true);
  BhDaten d;
  bh_daten_laden(&d);
  BhNachtTag tage[NAECHTE_MAX];
  cl_assert_equal_i(bh_daten_nachttage(&d, tage), 2);
  cl_assert_equal_i(tage[0].tag, HEUTE + 1);
  cl_assert_equal_i(tage[0].hrv, 55);
}

// C5: In der Nacht der Zeitumstellung gilt fuer jede Uhrzeit die Zone in ihrem Zeitpunkt, wie bei
// Boulder - nicht der Versatz des Fensterbeginns, den die Nacht traegt.
void test_bh_daten__uhrzeit_in_der_nacht_der_umstellung(void) {
  // 24./25.10.2026: um 03:00 MESZ (01:00 UTC) wird es 02:00 MEZ.
  s_umstellung = 1792890000;
  prv_bild(430, 61, 50, true);
  s_bild.beginn_utc = 1792872000; // 24.10. 20:00 UTC = 22:00 MESZ
  s_bild.aufwachen = 570;         // 25.10. 05:30 UTC = 06:30 MEZ
  BhDaten d;
  bh_daten_laden(&d);
  cl_assert_equal_i(bh_nacht_uhrzeit(&d.nacht, 17), 22 * 60 + 17);
  cl_assert_equal_i(bh_nacht_uhrzeit(&d.nacht, 570), 6 * 60 + 30);
  cl_assert_equal_i(bh_nacht_wachtag(&d.nacht), HEUTE + 13);
}

// C6: Das Night-Detail braucht SpO2 der ganzen letzten Nacht, auch wenn sie mehr als 24 h zurueck
// begann (abends am Tag danach).
void test_bh_daten__spo2_ab_dem_beginn_der_nacht(void) {
  prv_bild(430, 61, 50, true);
  s_jetzt = NACHT_BEGINN + 25 * 60 * 60;
  BhDaten d;
  bh_daten_laden(&d);
  cl_assert_equal_i(s_spo2_von, NACHT_BEGINN + 17 * 60);
  // Liegt die Nacht in den 24 h, bleibt es bei ihnen.
  s_jetzt = NACHT_BEGINN + 12 * 60 * 60;
  bh_daten_laden(&d);
  cl_assert_equal_i(s_spo2_von, s_jetzt - 24 * 60 * 60);
}

// K6: Eine fertige Nacht kann bis "endgueltig" wachsen, ohne dass sich die Stufe der Lage aendert.
// Solange liest der Minutentakt S1 und S5 jedes Mal, sonst nicht.
void test_bh_daten__auffrischen_liest_die_fertige_nacht(void) {
  s_lage.nacht_stufe = NachtFertig;
  BhDaten d;
  bh_daten_laden(&d);
  memset(s_gelesen, 0, sizeof(s_gelesen));
  bh_daten_auffrischen(&d);
  bh_daten_auffrischen(&d);
  cl_assert_equal_i(s_gelesen[Nacht], 2);
  cl_assert_equal_i(s_gelesen[Naechte], 2);
  s_lage.nacht_stufe = NachtOffen;
  bh_daten_auffrischen(&d); // die Stufe aendert sich: einmal
  bh_daten_auffrischen(&d);
  cl_assert_equal_i(s_gelesen[Nacht], 3);
}
