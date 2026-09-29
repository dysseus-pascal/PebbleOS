/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "pbl/services/kieselsport/kieselsport.h"
#include "services/kieselsport/bahnen.h"
#include "services/kieselsport/puls.h"
#include "services/kieselsport/reps.h"
#include "services/kieselsport/wartend.h"

#include "pbl/services/regular_timer.h"
#include "pbl/util/trig.h"

#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_system_task.h"

#include <math.h>

// Der Trainingsdienst aus Kieselsport, in der Firmware. Die Plattform ist hier
// nachgebaut: ein Persist im Speicher, Zaehler, die der Test stellt, und ein
// Sensor, dessen Werte der Test einspeist - wie plattform.c es unter der
// Sperre tut.

// --- Zeit und Takt ---

static time_t s_jetzt;
static RegularTimerInfo *s_takt;
static RegularTimerInfo *s_minutentakt;

time_t ks_jetzt(void) { return s_jetzt; }

void regular_timer_add_seconds_callback(RegularTimerInfo *cb) { s_takt = cb; }
void regular_timer_add_minutes_callback(RegularTimerInfo *cb) { s_minutentakt = cb; }
bool regular_timer_remove_callback(RegularTimerInfo *cb) {
  if (s_takt == cb) {
    s_takt = NULL;
  }
  if (s_minutentakt == cb) {
    s_minutentakt = NULL;
  }
  return true;
}
bool regular_timer_is_scheduled(RegularTimerInfo *cb) {
  return s_takt == cb || s_minutentakt == cb;
}

// --- Persist ---

#define EINTRAEGE_MAX 64
typedef struct {
  bool belegt;
  uint32_t key;
  size_t laenge;
  uint8_t daten[256];
} Eintrag;
static Eintrag s_persist[EINTRAEGE_MAX];
static int s_offen;

static Eintrag *prv_finde(uint32_t key) {
  for (int i = 0; i < EINTRAEGE_MAX; i++) {
    if (s_persist[i].belegt && s_persist[i].key == key) {
      return &s_persist[i];
    }
  }
  return NULL;
}

bool ks_persist_exists(uint32_t key) { return prv_finde(key) != NULL; }

int ks_persist_read_data(uint32_t key, void *puffer, size_t groesse) {
  Eintrag *e = prv_finde(key);
  if (!e) {
    return 0;
  }
  const size_t n = e->laenge < groesse ? e->laenge : groesse;
  memcpy(puffer, e->daten, n);
  return (int)n;
}

void ks_persist_write_data(uint32_t key, const void *daten, size_t groesse) {
  Eintrag *e = prv_finde(key);
  for (int i = 0; !e && i < EINTRAEGE_MAX; i++) {
    if (!s_persist[i].belegt) {
      e = &s_persist[i];
    }
  }
  cl_assert(e);
  e->belegt = true;
  e->key = key;
  e->laenge = groesse > 256 ? 256 : groesse;
  memcpy(e->daten, daten, e->laenge);
}

int32_t ks_persist_read_int(uint32_t key) {
  int32_t wert = 0;
  ks_persist_read_data(key, &wert, sizeof(wert));
  return wert;
}

void ks_persist_write_int(uint32_t key, int32_t wert) {
  ks_persist_write_data(key, &wert, sizeof(wert));
}

void ks_persist_delete(uint32_t key) {
  Eintrag *e = prv_finde(key);
  if (e) {
    e->belegt = false;
  }
}

void ks_persist_offen_halten(bool an) { s_offen += an ? 1 : -1; }

// --- Uhr ---

static uint32_t s_zaehler[3];
uint32_t ks_heute(KsZaehler zaehler) { return s_zaehler[zaehler]; }

static uint16_t s_sensor_takt;
static bool s_sensor_hrv;
void ks_puls_sensor(uint16_t takt_s, bool hrv) {
  s_sensor_takt = takt_s;
  s_sensor_hrv = hrv;
}

static int s_szene;   //< 0 = allgemein, sonst Art + 1
void ks_puls_szene(Sportart art, bool an) { s_szene = an ? (int)art + 1 : 0; }

static bool s_accel;
void ks_accel(bool an) { s_accel = an; }
static bool s_kompass;
void ks_kompass(bool an) { s_kompass = an; }

static int s_brumm_zahl;
static uint32_t s_brumm_letztes[4];
static uint32_t s_brumm_laenge;
void ks_brummen(const uint32_t *dauer_ms, uint32_t anzahl) {
  s_brumm_zahl++;
  s_brumm_laenge = anzahl;
  memcpy(s_brumm_letztes, dauer_ms, anzahl * sizeof(uint32_t));
}

static uint8_t s_akku = 80;
uint8_t ks_akku_prozent(bool *am_kabel) {
  *am_kabel = false;
  return s_akku;
}

static bool s_erkennen = true;
void ks_bewegung_erkennen(bool an) { s_erkennen = an; }

static int s_geholt;
void ks_app_holen(void) { s_geholt++; }
static bool s_oben;
void ks_startmenue_oben(bool laeuft) { s_oben = laeuft; }

static int s_gemeldet_zahl;
static Trainingsstand s_gemeldet;
void ks_sitzung_melden(const Trainingsstand *t) {
  s_gemeldet_zahl++;
  s_gemeldet = *t;
}

// --- Hilfen ---

#define BEGINN 1790445600  // 26.9.2026, 18:00 UTC

static void prv_sekunde(void) {
  s_jetzt++;
  cl_assert(s_takt);
  s_takt->cb(s_takt->cb_data);
}

// Eine Sekunde Training mit frischem Puls und zwei Schritten.
static void prv_sekunde_mit_puls(uint8_t bpm) {
  s_jetzt++;
  s_zaehler[KsZaehlerSchritte] += 2;
  puls_ereignis_bpm(bpm, 4);
  s_takt->cb(s_takt->cb_data);
}

static KsStand prv_stand(void) {
  KsStand s;
  kieselsport_stand(&s);
  return s;
}

void test_kieselsport__initialize(void) {
  memset(s_persist, 0, sizeof(s_persist));
  memset(s_zaehler, 0, sizeof(s_zaehler));
  s_jetzt = BEGINN;
  s_takt = NULL;
  s_minutentakt = NULL;
  s_geholt = 0;
  s_offen = 0;
  s_sensor_takt = 0;
  s_sensor_hrv = false;
  s_szene = 0;
  s_accel = false;
  s_kompass = false;
  s_brumm_zahl = 0;
  s_akku = 80;
  s_erkennen = true;
  s_gemeldet_zahl = 0;
  kieselsport_init();
}

void test_kieselsport__cleanup(void) {
  if (kieselsport_laeuft()) {
    kieselsport_verwerfen();
  }
}

void test_kieselsport__zonen_am_maximalpuls(void) {
  cl_assert_equal_i(kieselsport_maxpuls(), 190);
  cl_assert_equal_i(kieselsport_zonengrenze(1), 95);
  cl_assert_equal_i(kieselsport_zonengrenze(5), 171);
  cl_assert_equal_i(kieselsport_zone(94), 0);
  cl_assert_equal_i(kieselsport_zone(150), 3);
  // Unsinnige Werte bleiben draussen, gueltige ueberleben den Neustart.
  kieselsport_maxpuls_setze(90);
  cl_assert_equal_i(kieselsport_maxpuls(), 190);
  kieselsport_maxpuls_setze(200);
  kieselsport_init();
  cl_assert_equal_i(kieselsport_maxpuls(), 200);
  cl_assert_equal_i(kieselsport_zonengrenze(3), 140);
}

void test_kieselsport__laufen_von_start_bis_telefon(void) {
  s_zaehler[KsZaehlerSchritte] = 5000;
  s_zaehler[KsZaehlerKcal] = 300;
  kieselsport_starte(ArtLaufen, BEGINN - 5);
  // Sensor im Sekundentakt, Laufmodell, keine Bewegungserkennung daneben.
  cl_assert_equal_i(s_sensor_takt, 1);
  cl_assert_equal_i(s_szene, ArtLaufen + 1);
  cl_assert(!s_erkennen);
  cl_assert_equal_i(s_offen, 1);
  cl_assert(!s_accel);
  cl_assert(s_oben);

  for (int i = 0; i < 120; i++) {
    if (i == 100) {
      s_zaehler[KsZaehlerKcal] = 320;
    }
    prv_sekunde_mit_puls(i < 60 ? 130 : 150);
  }
  KsStand s = prv_stand();
  cl_assert_equal_i(s.zustand, LaufLaeuft);
  cl_assert_equal_i(s.t.beginn, BEGINN - 5);
  cl_assert_equal_i(s.t.dauer_s, 120);
  cl_assert_equal_i(s.t.schritte, 240);
  cl_assert_equal_i(s.t.kcal, 20);
  cl_assert_equal_i(s.t.puls_mittel, 140);
  cl_assert_equal_i(s.t.puls_max, 150);
  cl_assert(s.frisch);
  cl_assert_equal_i(s.zone, 3);
  cl_assert_equal_i(s.minute[120 % 60], 150);

  kieselsport_speichern();
  s = prv_stand();
  cl_assert_equal_i(s.zustand, LaufAus);
  cl_assert_equal_i(s.ende, KsEndeGespeichert);
  cl_assert_equal_i(s_sensor_takt, 0);
  cl_assert_equal_i(s_szene, 0);
  cl_assert(s_erkennen);
  cl_assert_equal_i(s_offen, 0);
  cl_assert(!s_takt);
  cl_assert(!s_oben);

  // Im Verlauf der Uhr, im Archiv, und fuers Telefon.
  cl_assert_equal_i(s_gemeldet_zahl, 1);
  cl_assert_equal_i(s_gemeldet.dauer_s, 120);
  cl_assert_equal_i(kieselsport_archiv_anzahl(), 1);
  Trainingsstand t;
  char liste[KS_LISTE_MAX];
  cl_assert(kieselsport_wartend_laden(&t, liste, sizeof(liste)));
  cl_assert_equal_i(t.schritte, 240);
  cl_assert_equal_s(liste, "");

  // Die Kurve: Sekunde 0 bis 120 in Zehnern, also 13 Werte; die erste ist
  // leer, weil der Takt erst nach Sekunde eins zaehlt.
  cl_assert(kieselsport_kurve_wartet());
  cl_assert_equal_i(kieselsport_kurve_anzahl(), 13);
  cl_assert_equal_i(kieselsport_kurve_beginn(), BEGINN - 5);
  uint8_t stueck[8];
  cl_assert_equal_i(kieselsport_kurve_stueck(stueck, sizeof(stueck)), 8);
  cl_assert_equal_i(stueck[0], 0);
  cl_assert_equal_i(stueck[1], 130);
  cl_assert_equal_i(stueck[7], 150);
  kieselsport_kurve_bestaetigt(8);
  cl_assert_equal_i(kieselsport_kurve_ab(), 8);
  cl_assert_equal_i(kieselsport_kurve_stueck(stueck, sizeof(stueck)), 5);
  kieselsport_kurve_bestaetigt(5);
  cl_assert(!kieselsport_kurve_wartet());
  // Zeiten je Zone aus der Kurve: 130 ist Zone 2, 150 Zone 3.
  kieselsport_starte(ArtLaufen, 0);
  for (int i = 0; i < 60; i++) {
    prv_sekunde_mit_puls(130);
  }
  kieselsport_speichern();
  uint32_t zeiten[KS_ZONEN + 1];
  cl_assert(kieselsport_kurve_zeiten(zeiten));
  cl_assert_equal_i(zeiten[2], 60);
  cl_assert_equal_i(zeiten[3], 0);
}

void test_kieselsport__zonenwechsel_brummt(void) {
  kieselsport_starte(ArtBike, 0);
  cl_assert_equal_i(s_szene, ArtBike + 1);
  for (int i = 0; i < 10; i++) {
    prv_sekunde_mit_puls(120);   // Zone 2
  }
  cl_assert_equal_i(s_brumm_zahl, 0);
  prv_sekunde_mit_puls(140);     // Zone 3: hoch, zwei kurze
  cl_assert_equal_i(s_brumm_zahl, 1);
  cl_assert_equal_i(s_brumm_laenge, 3);
  prv_sekunde_mit_puls(141);
  cl_assert_equal_i(s_brumm_zahl, 1);
  prv_sekunde_mit_puls(120);     // runter, einer
  cl_assert_equal_i(s_brumm_zahl, 2);
  cl_assert_equal_i(s_brumm_laenge, 1);

  // Ein alter Wert wechselt keine Zone: nach 90 Sekunden ohne Meldung ist
  // der Puls nicht mehr frisch.
  for (int i = 0; i < 91; i++) {
    prv_sekunde();
  }
  KsStand s = prv_stand();
  cl_assert(!s.frisch);
  cl_assert_equal_i(s.zone, 0);
  cl_assert_equal_i(s_brumm_zahl, 2);
}

void test_kieselsport__pause_haelt_die_zeit_an(void) {
  kieselsport_starte(ArtWandern, 0);
  for (int i = 0; i < 30; i++) {
    prv_sekunde_mit_puls(100);
  }
  kieselsport_pause_umschalten();
  cl_assert_equal_i(prv_stand().zustand, LaufPause);
  // In der Pause misst der Sensor seltener.
  cl_assert_equal_i(s_sensor_takt, 10);
  for (int i = 0; i < 30; i++) {
    prv_sekunde();
  }
  kieselsport_pause_umschalten();
  cl_assert_equal_i(s_sensor_takt, 1);
  for (int i = 0; i < 30; i++) {
    prv_sekunde_mit_puls(100);
  }
  cl_assert_equal_i(prv_stand().t.dauer_s, 60);
}

void test_kieselsport__unter_einer_minute_verworfen(void) {
  kieselsport_starte(ArtLaufen, 0);
  for (int i = 0; i < 59; i++) {
    prv_sekunde_mit_puls(100);
  }
  const uint8_t vorher = prv_stand().ende_nr;
  kieselsport_speichern();
  KsStand s = prv_stand();
  cl_assert_equal_i(s.ende, KsEndeVerworfen);
  cl_assert_equal_i(s.ende_nr, (uint8_t)(vorher + 1));
  cl_assert_equal_i(kieselsport_archiv_anzahl(), 0);
  cl_assert_equal_i(s_gemeldet_zahl, 0);
  Trainingsstand t;
  char liste[8];
  cl_assert(!kieselsport_wartend_laden(&t, liste, sizeof(liste)));
}

void test_kieselsport__schwacher_akku_misst_sparsam(void) {
  s_akku = 15;
  kieselsport_starte(ArtLaufen, 0);
  cl_assert_equal_i(s_sensor_takt, 5);
  cl_assert(prv_stand().sparsam);
}

void test_kieselsport__puls_frisch_geglaettet_und_vom_handgelenk(void) {
  kieselsport_starte(ArtLaufen, 0);
  // Misstraut der Sensor seiner Messung, gilt der Median der letzten drei:
  // die Spitze 180 zieht nicht mit.
  puls_ereignis_bpm(100, 1);
  puls_ereignis_bpm(180, 1);
  puls_ereignis_bpm(102, 1);
  prv_sekunde();
  cl_assert_equal_i(prv_stand().puls, 102);
  // Unsinn faellt weg.
  puls_ereignis_bpm(250, 4);
  prv_sekunde();
  cl_assert_equal_i(prv_stand().puls, 102);
  // Nicht am Handgelenk: sofort kein Puls.
  puls_ereignis_bpm(0, -1);
  prv_sekunde();
  cl_assert_equal_i(prv_stand().puls, 0);
  cl_assert(!prv_stand().frisch);
}

// Ein Arm, der eine Hantel hebt: eine Wiederholung je Sekunde, 25 Messungen
// je Sekunde, in Paketen zu zehn wie vom Beschleunigungsmesser.
static int s_messung;
static void prv_arm(int sekunden, bool bewegt) {
  for (int s = 0; s < sekunden; s++) {
    KsAccel paket[25];
    for (int i = 0; i < 25; i++, s_messung++) {
      const double phase = 2.0 * 3.14159265358979 * (double)s_messung / 25.0;
      paket[i] = (KsAccel){
        .x = 0,
        .y = 0,
        .z = (int16_t)(1000 + (bewegt ? 450.0 * sin(phase) : 0.0)),
      };
    }
    reps_daten(paket, 10);
    reps_daten(paket + 10, 10);
    reps_daten(paket + 20, 5);
    prv_sekunde();
  }
}

void test_kieselsport__kraft_zaehlt_saetze_und_brummt_nach_der_pause(void) {
  kieselsport_pausenziel_setze(8);
  kieselsport_starte(ArtKraft, 0);
  cl_assert(s_accel);
  s_messung = 0;
  // Erst eine halbe Minute zur Bank: ein Training unter einer Minute gaebe
  // es sonst am Ende nicht.
  prv_arm(30, false);
  prv_arm(12, true);
  KsStand s = prv_stand();
  cl_assert(!s.ruht);
  cl_assert(s.reps_laufend >= 10 && s.reps_laufend <= 13);
  // Ruhe: nach vier Sekunden ist der Satz zu Ende, nach acht weiteren brummt
  // die Uhr - einmal.
  const int vorher = s_brumm_zahl;
  prv_arm(15, false);
  s = prv_stand();
  cl_assert(s.ruht);
  cl_assert_equal_i(s.t.saetze, 1);
  cl_assert_equal_i(s_brumm_zahl, vorher + 1);
  cl_assert_equal_i(s_brumm_letztes[0], 80);

  // Ein zweiter Satz, der mit Stop endet, zaehlt auch.
  prv_arm(6, true);
  kieselsport_speichern();
  cl_assert(!s_accel);
  Trainingsstand t;
  char liste[KS_LISTE_MAX];
  cl_assert(kieselsport_wartend_laden(&t, liste, sizeof(liste)));
  cl_assert_equal_i(t.saetze, 2);
  // Zwei Abschnitte "beginn:anzahl:dauer;".
  int strichpunkte = 0;
  for (const char *c = liste; *c; c++) {
    strichpunkte += (*c == ';');
  }
  cl_assert_equal_i(strichpunkte, 2);
  cl_assert(t.reps >= 14 && t.reps <= 20);
}

static void prv_schwimmen(int sekunden, int32_t richtung) {
  for (int s = 0; s < sekunden; s++) {
    bahnen_kompass(richtung, true);
    bahnen_kompass(richtung, true);
    prv_sekunde();
  }
}

void test_kieselsport__schwimmen_zaehlt_bahnen(void) {
  kieselsport_becken_setze(25);
  kieselsport_starte(ArtSchwimmen, 0);
  cl_assert(s_kompass);
  prv_schwimmen(30, 0);
  prv_schwimmen(30, TRIG_MAX_ANGLE / 2);
  prv_schwimmen(30, 0);
  KsStand s = prv_stand();
  cl_assert(s.bahnen_bereit);
  cl_assert_equal_i(s.t.bahnen, 2);
  // Die letzte Bahn zaehlt beim Stop mit.
  kieselsport_speichern();
  cl_assert(!s_kompass);
  Trainingsstand t;
  char liste[KS_LISTE_MAX];
  cl_assert(kieselsport_wartend_laden(&t, liste, sizeof(liste)));
  cl_assert_equal_i(t.bahnen, 3);
  cl_assert_equal_i(t.meter, 75);
}

void test_kieselsport__yoga_misst_hrv(void) {
  kieselsport_starte(ArtYoga, 0);
  cl_assert(s_sensor_hrv);
  for (int i = 0; i < 40; i++) {
    puls_ereignis_ppi((i % 2) ? 1040 : 1000);
  }
  prv_sekunde();
  KsStand s = prv_stand();
  cl_assert_equal_i(s.t.hrv_ms, 40);
  cl_assert_equal_i(s.hrv_anzahl, 39);
  kieselsport_verwerfen();
  cl_assert(!s_sensor_hrv);
  cl_assert_equal_i(s_sensor_takt, 0);
}

void test_kieselsport__archiv_haelt_die_letzten_zwanzig(void) {
  for (int n = 0; n < 22; n++) {
    kieselsport_starte(ArtLaufen, BEGINN + n * 1000);
    for (int i = 0; i < 60; i++) {
      prv_sekunde_mit_puls(100);
    }
    kieselsport_speichern();
  }
  cl_assert_equal_i(kieselsport_archiv_anzahl(), 20);
  Trainingsstand t;
  cl_assert(kieselsport_archiv_lesen(0, &t));
  cl_assert_equal_i(t.beginn, BEGINN + 21 * 1000);
  cl_assert(kieselsport_archiv_lesen(19, &t));
  cl_assert_equal_i(t.beginn, BEGINN + 2 * 1000);
  cl_assert(!kieselsport_archiv_lesen(20, &t));
}

void test_kieselsport__lange_liste_in_zwei_haelften(void) {
  char lang[KS_LISTE_MAX];
  for (int i = 0; i < 250; i++) {
    lang[i] = (char)('a' + i % 26);
  }
  lang[250] = 0;
  const Trainingsstand t = { .art = ArtKraft, .beginn = BEGINN, .dauer_s = 600 };
  wartend_merken(&t, lang);
  Trainingsstand gelesen;
  char liste[KS_LISTE_MAX];
  cl_assert(wartend_laden(&gelesen, liste, sizeof(liste)));
  cl_assert_equal_s(liste, lang);
  wartend_vergessen();
  cl_assert(!wartend_laden(&gelesen, liste, sizeof(liste)));
}

void test_kieselsport__nachsenden_nach_2_10_30_minuten(void) {
  kieselsport_starte(ArtLaufen, 0);
  for (int i = 0; i < 60; i++) {
    prv_sekunde_mit_puls(100);
  }
  kieselsport_speichern();
  cl_assert(s_minutentakt);
  int geholt_bei[3] = { 0 };
  for (int minute = 1; minute <= 45 && s_minutentakt; minute++) {
    const int vorher = s_geholt;
    s_minutentakt->cb(s_minutentakt->cb_data);
    if (s_geholt > vorher) {
      geholt_bei[s_geholt - 1] = minute;
    }
  }
  cl_assert_equal_i(s_geholt, 3);
  cl_assert_equal_i(geholt_bei[0], 2);
  cl_assert_equal_i(geholt_bei[1], 10);
  cl_assert_equal_i(geholt_bei[2], 30);
}

void test_kieselsport__nachsenden_hoert_auf_wenn_angekommen(void) {
  kieselsport_starte(ArtLaufen, 0);
  for (int i = 0; i < 60; i++) {
    prv_sekunde_mit_puls(100);
  }
  kieselsport_speichern();
  kieselsport_wartend_vergessen();
  s_minutentakt->cb(s_minutentakt->cb_data);
  cl_assert(!s_minutentakt);
  cl_assert_equal_i(s_geholt, 0);
}

void test_kieselsport__training_ueber_mitternacht_verliert_nichts(void) {
  s_zaehler[KsZaehlerSchritte] = 5000;
  s_zaehler[KsZaehlerKcal] = 300;
  kieselsport_starte(ArtLaufen, BEGINN);
  for (int i = 0; i < 60; i++) {
    prv_sekunde_mit_puls(130);
  }
  // Mitternacht: die Tageszaehler der Uhr beginnen bei null. Die Differenz
  // zum Anfang waere negativ gewesen, und das Training haette alles verloren.
  s_zaehler[KsZaehlerSchritte] = 0;
  s_zaehler[KsZaehlerKcal] = 0;
  for (int i = 0; i < 60; i++) {
    prv_sekunde_mit_puls(130);
  }
  KsStand s = prv_stand();
  cl_assert_equal_i(s.t.schritte, 240);
  cl_assert_equal_i(s.t.kcal, 0);
}
