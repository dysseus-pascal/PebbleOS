/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "pbl/services/kieselsport/kieselsport.h"
#include "services/kieselsport/bahnen.h"
#include "services/kieselsport/puls.h"
#include "services/kieselsport/reps.h"
#include "services/kieselsport/schluessel.h"
#include "services/kieselsport/wartend.h"

#include "pbl/services/regular_timer.h"
#include "pbl/util/trig.h"

#include "stubs_logging.h"
#include "stubs_system_task.h"

#include <math.h>
#include <setjmp.h>

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

// --- Die Sperre des Dienstes, zwischen zwei Aufgaben ---
//
// Der Dienst schreibt auf KernelBG, die App bestaetigt auf ihrer eigenen
// Aufgabe - mit hoeherer Prioritaet: sie kann KernelBG mitten im Speichern
// unterbrechen. Der Test laeuft in einem Faden; s_aufgabe sagt, wer dran ist.
// Greift die App zur Sperre, waehrend KernelBG sie haelt, muesste sie warten:
// hier springt ihr Aufruf zurueck (longjmp, vor jeder Wirkung) und laeuft,
// sobald KernelBG entsperrt, von vorn - wie nach dem Warten. Die Sperre ist
// rekursiv wie die der Firmware.
typedef enum { AufgabeKernelBG, AufgabeApp } Aufgabe;
static Aufgabe s_aufgabe;
static Aufgabe s_halter;
static int s_tiefe;
static jmp_buf s_warten;
static bool s_app_wartet;

// Was die App tut, wenn sie KernelBG unterbricht - kurz bevor dieser Schluessel
// geschrieben wird.
static void (*s_app_aktion)(void);
static uint32_t s_unterbrich_vor;

static void prv_app_laeuft(void) {
  s_aufgabe = AufgabeApp;
  s_app_wartet = false;
  if (setjmp(s_warten) == 0) {
    s_app_aktion();
    s_app_aktion = NULL;
  } else {
    s_app_wartet = true;
  }
  s_aufgabe = AufgabeKernelBG;
}

void pbl_mutex_init(struct pbl_mutex *m) {}
void pbl_mutex_deinit(struct pbl_mutex *m) {}
int pbl_mutex_lock_lr(struct pbl_mutex *m, pbl_timeout_t timeout, uintptr_t lr) {
  if (s_tiefe > 0 && s_halter != s_aufgabe) {
    cl_assert(s_aufgabe == AufgabeApp);
    longjmp(s_warten, 1);
  }
  s_halter = s_aufgabe;
  s_tiefe++;
  return 0;
}
void pbl_mutex_unlock(struct pbl_mutex *m) {
  cl_assert(s_tiefe > 0 && s_halter == s_aufgabe);
  // Gibt KernelBG die Sperre frei, kommt die wartende App dran.
  if (--s_tiefe == 0 && s_app_wartet && s_aufgabe == AufgabeKernelBG) {
    prv_app_laeuft();
  }
}
bool pbl_mutex_is_owner(const struct pbl_mutex *m) { return s_tiefe > 0 && s_halter == s_aufgabe; }
void pbl_mutex_assert_held(const struct pbl_mutex *m, bool held) {}

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

static int s_schreibvorgaenge;   //< jeder ist ein neuer Satz im Flash

void ks_persist_write_data(uint32_t key, const void *daten, size_t groesse) {
  if (s_app_aktion && !s_app_wartet && key == s_unterbrich_vor) {
    prv_app_laeuft();
  }
  s_schreibvorgaenge++;
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

// Das Alter aus den Gesundheitseinstellungen. 0 gibt es auf der Uhr nicht
// (ohne Abgleich steht dort 30); hier steht es fuer ein unsinniges Alter,
// mit dem die Uhr bei 190 bleibt.
static uint8_t s_alter;
uint8_t ks_alter(void) { return s_alter; }

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
  s_alter = 0;
  s_schreibvorgaenge = 0;
  s_aufgabe = AufgabeKernelBG;
  s_tiefe = 0;
  s_app_wartet = false;
  s_app_aktion = NULL;
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
  cl_assert(kieselsport_kurve_bestaetigt(BEGINN - 5, 0, 8));
  cl_assert_equal_i(kieselsport_kurve_ab(), 8);
  cl_assert_equal_i(kieselsport_kurve_stueck(stueck, sizeof(stueck)), 5);
  cl_assert(kieselsport_kurve_bestaetigt(BEGINN - 5, 8, 5));
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
  wartend_merken(&t, lang, NULL, 0, 0);
  Trainingsstand gelesen;
  char liste[KS_LISTE_MAX];
  cl_assert(wartend_laden(&gelesen, liste, sizeof(liste)));
  cl_assert_equal_s(liste, lang);
  wartend_vergessen(BEGINN);
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
  kieselsport_wartend_vergessen(0);
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

void test_kieselsport__pause_zaehlt_keine_schritte(void) {
  s_zaehler[KsZaehlerSchritte] = 1000;
  kieselsport_starte(ArtLaufen, BEGINN);
  for (int i = 0; i < 30; i++) {
    prv_sekunde_mit_puls(130);
  }
  kieselsport_pause_umschalten();
  // In der Pause laeuft man zum Kiosk: die Uhr zaehlt, das Training nicht.
  s_zaehler[KsZaehlerSchritte] += 500;
  kieselsport_pause_umschalten();
  for (int i = 0; i < 30; i++) {
    prv_sekunde_mit_puls(130);
  }
  cl_assert_equal_i(prv_stand().t.schritte, 120);
}

// --- Maximalpuls: gesetzt oder nach Alter ---

void test_kieselsport__ohne_einstellung_rechnet_die_uhr_nach_alter(void) {
  // Ohne Alter die alte Vorgabe; mit Alter Tanaka, gerundet wie Boulder.
  cl_assert(!kieselsport_maxpuls_gesetzt());
  cl_assert_equal_i(kieselsport_maxpuls(), 190);
  s_alter = 41;
  cl_assert_equal_i(kieselsport_maxpuls(), 179);   // 208 - 28,7 = 179,3
  cl_assert_equal_i(kieselsport_zonengrenze(1), 89);
  s_alter = 35;
  cl_assert_equal_i(kieselsport_maxpuls(), 184);   // 183,5 rundet auf
  // Das Alter aendert sich: die Zonen folgen, ohne dass jemand etwas schickt.
  s_alter = 50;
  cl_assert_equal_i(kieselsport_maxpuls(), 173);
  cl_assert(!kieselsport_maxpuls_gesetzt());
}

void test_kieselsport__gemessene_190_ist_einstellbar(void) {
  s_alter = 41;
  kieselsport_maxpuls_setze(190);
  cl_assert(kieselsport_maxpuls_gesetzt());
  cl_assert_equal_i(kieselsport_maxpuls(), 190);
  kieselsport_init();
  cl_assert(kieselsport_maxpuls_gesetzt());
  cl_assert_equal_i(kieselsport_maxpuls(), 190);
  // Zurueck auf "nach Alter": der Wert fliegt aus dem Persist, damit aeltere
  // Firmware ihre Vorgabe liest.
  kieselsport_maxpuls_nicht_setzen();
  cl_assert(!ks_persist_exists(PERSIST_MAXPULS));
  kieselsport_init();
  cl_assert(!kieselsport_maxpuls_gesetzt());
  cl_assert_equal_i(kieselsport_maxpuls(), 179);
}

void test_kieselsport__alte_190_ohne_vermerk_heisst_nicht_gesetzt(void) {
  s_alter = 41;
  // So hinterliessen Store-App, Konfigseite und Boulder bis 0.42 den Persist.
  ks_persist_write_int(PERSIST_MAXPULS, 190);
  kieselsport_init();
  cl_assert(!kieselsport_maxpuls_gesetzt());
  cl_assert_equal_i(kieselsport_maxpuls(), 179);
  // Jeder andere alte Wert war eingestellt.
  ks_persist_write_int(PERSIST_MAXPULS, 185);
  kieselsport_init();
  cl_assert(kieselsport_maxpuls_gesetzt());
  cl_assert_equal_i(kieselsport_maxpuls(), 185);
  // Schrieb eine aeltere Firmware nach "nicht gesetzt" einen Wert, gilt er.
  kieselsport_maxpuls_nicht_setzen();
  ks_persist_write_int(PERSIST_MAXPULS, 175);
  kieselsport_init();
  cl_assert(kieselsport_maxpuls_gesetzt());
  cl_assert_equal_i(kieselsport_maxpuls(), 175);
}

void test_kieselsport__schaetzung_nach_alter_ohne_vermerk_bleibt_nach_alter(void) {
  // So hinterliess Boulder 0.43 bis 0.45 den Persist bei "From age": seine
  // Schaetzung nach Tanaka als Zahl.
  ks_persist_write_int(PERSIST_MAXPULS, 179);
  // Beim Start der Uhr sind die Gesundheitseinstellungen noch nicht geladen.
  s_alter = 30;
  kieselsport_init();
  // Bis zur Entscheidung gilt die alte Zahl - und noch nichts im Persist.
  cl_assert_equal_i(kieselsport_maxpuls(), 179);
  cl_assert(!ks_persist_exists(PERSIST_MAXPULS_GESETZT));
  // Geladen; die App fragt beim Melden ans Telefon.
  s_alter = 41;
  cl_assert(!kieselsport_maxpuls_gesetzt());
  cl_assert(!ks_persist_exists(PERSIST_MAXPULS));
  // Festgehalten: mit dem Alter folgt die Uhr, auch nach einem Neustart.
  s_alter = 45;
  kieselsport_init();
  cl_assert(!kieselsport_maxpuls_gesetzt());
  cl_assert_equal_i(kieselsport_maxpuls(), 177);   // 176,5
}

void test_kieselsport__eingestellte_zahl_ohne_vermerk_bleibt_eingestellt(void) {
  ks_persist_write_int(PERSIST_MAXPULS, 185);
  s_alter = 30;
  kieselsport_init();
  s_alter = 41;
  // Ein Training braucht die Zonen: sein Start entscheidet.
  kieselsport_starte(ArtLaufen, BEGINN);
  cl_assert(ks_persist_exists(PERSIST_MAXPULS_GESETZT));
  cl_assert_equal_i(ks_persist_read_int(PERSIST_MAXPULS_GESETZT), 1);
  kieselsport_verwerfen();
  // Trifft Tanaka die Zahl spaeter zufaellig, bleibt sie eingestellt.
  s_alter = 33;   // 184,9
  kieselsport_init();
  cl_assert(kieselsport_maxpuls_gesetzt());
  cl_assert_equal_i(kieselsport_maxpuls(), 185);
}

void test_kieselsport__zahl_ohne_vermerk_wie_bisher(void) {
  s_alter = 41;
  // Das Echo der eigenen Meldung aendert nichts: die Uhr bleibt beim Alter.
  kieselsport_maxpuls_alt(179);
  cl_assert(!kieselsport_maxpuls_gesetzt());
  s_alter = 42;
  cl_assert_equal_i(kieselsport_maxpuls(), 179);   // 178,6
  // Jede andere Zahl ist eingestellt - auch 190 von einem alten Boulder.
  kieselsport_maxpuls_alt(190);
  cl_assert(kieselsport_maxpuls_gesetzt());
  cl_assert_equal_i(kieselsport_maxpuls(), 190);
  // Ist ein Wert gesetzt, bleibt auch eine gleiche Zahl gesetzt.
  kieselsport_maxpuls_alt(179);
  cl_assert(kieselsport_maxpuls_gesetzt());
  cl_assert_equal_i(kieselsport_maxpuls(), 179);
  // Unsinn bleibt draussen.
  kieselsport_maxpuls_alt(90);
  cl_assert_equal_i(kieselsport_maxpuls(), 179);
}

// --- Pausen und Ende fuer das Telefon ---

static void prv_sekunden(int n) {
  for (int i = 0; i < n; i++) {
    prv_sekunde_mit_puls(120);
  }
}

static void prv_pause_fuer(int sekunden) {
  kieselsport_pause_umschalten();
  for (int i = 0; i < sekunden; i++) {
    prv_sekunde();
  }
  kieselsport_pause_umschalten();
}

static uint16_t prv_u16(const uint8_t *b) { return (uint16_t)(b[0] | (b[1] << 8)); }

void test_kieselsport__pausen_gehen_mit_der_zusammenfassung(void) {
  kieselsport_starte(ArtBike, BEGINN);
  prv_sekunden(30);
  prv_pause_fuer(120);       // Kaffee
  prv_sekunden(40);
  prv_pause_fuer(5);         // Ampel
  prv_sekunden(20);
  // Gespeichert wird aus der Pause: dort endet die aktive Zeit.
  kieselsport_pause_umschalten();
  const time_t letzte_pause = s_jetzt;
  for (int i = 0; i < 30; i++) {
    prv_sekunde();
  }
  kieselsport_speichern();

  Trainingsstand t;
  char liste[KS_LISTE_MAX];
  cl_assert(kieselsport_wartend_laden(&t, liste, sizeof(liste)));
  cl_assert_equal_i(t.dauer_s, 90);
  uint8_t b[KS_PAUSEN_BYTES];
  const size_t n = kieselsport_wartend_pausen(b, sizeof(b));
  cl_assert_equal_i(n, 2 + 2 * 4);
  cl_assert_equal_i(b[0], KS_PAUSEN_FASSUNG);
  cl_assert_equal_i(b[1], 2);
  cl_assert_equal_i(prv_u16(b + 2), 30);
  cl_assert_equal_i(prv_u16(b + 4), 120);
  cl_assert_equal_i(prv_u16(b + 6), 70);
  cl_assert_equal_i(prv_u16(b + 8), 5);
  // Beginn + aktive Zeit + Pausen ist genau das Ende.
  cl_assert_equal_i(kieselsport_wartend_ende(), (uint32_t)letzte_pause);
  cl_assert_equal_i(kieselsport_wartend_ende(), BEGINN + 90 + 125);

  // Bestaetigt: alles weg, auch Pausen und Ende.
  kieselsport_wartend_vergessen(BEGINN);
  cl_assert_equal_i(kieselsport_wartend_pausen(b, sizeof(b)), 0);
  cl_assert_equal_i(kieselsport_wartend_ende(), 0);
}

void test_kieselsport__ohne_pausen_nur_kopf(void) {
  kieselsport_starte(ArtLaufen, BEGINN);
  prv_sekunden(60);
  kieselsport_speichern();
  uint8_t b[KS_PAUSEN_BYTES];
  // Der Kopf allein heisst: die Uhr kennt Pausen, es gab keine.
  cl_assert_equal_i(kieselsport_wartend_pausen(b, sizeof(b)), 2);
  cl_assert_equal_i(b[1], 0);
  // Ohne Pause gespeichert endet die aktive Zeit beim Speichern.
  cl_assert_equal_i(kieselsport_wartend_ende(), BEGINN + 60);
}

void test_kieselsport__mehr_pausen_als_platz(void) {
  kieselsport_starte(ArtKraft, BEGINN);
  for (int i = 0; i < KS_PAUSEN_MAX + 1; i++) {
    prv_sekunden(2);
    prv_pause_fuer(3);
  }
  prv_sekunden(10);
  kieselsport_speichern();
  uint8_t b[KS_PAUSEN_BYTES];
  // Die Liste ist voll, die Zahl zaehlt weiter - das Telefon sieht die Luecke.
  cl_assert_equal_i(kieselsport_wartend_pausen(b, sizeof(b)), KS_PAUSEN_BYTES);
  cl_assert_equal_i(b[1], KS_PAUSEN_MAX + 1);
}

void test_kieselsport__ein_neues_training_hat_keine_alten_pausen(void) {
  kieselsport_starte(ArtLaufen, BEGINN);
  prv_sekunden(30);
  prv_pause_fuer(10);
  prv_sekunden(30);
  kieselsport_speichern();
  kieselsport_starte(ArtLaufen, BEGINN + 1000);
  prv_sekunden(60);
  kieselsport_speichern();
  uint8_t b[KS_PAUSEN_BYTES];
  cl_assert_equal_i(kieselsport_wartend_pausen(b, sizeof(b)), 2);
}

void test_kieselsport__alte_pausen_gehen_nicht_mit_fremdem_stand(void) {
  kieselsport_starte(ArtBike, BEGINN);
  prv_sekunden(30);
  prv_pause_fuer(60);
  prv_sekunden(30);
  kieselsport_speichern();
  uint8_t b[KS_PAUSEN_BYTES];
  cl_assert_equal_i(kieselsport_wartend_pausen(b, sizeof(b)), 2 + 4);
  // Zurueckgespielte Firmware: sie schickt das Training, loescht nach der
  // Bestaetigung nur 30-32 und legt ihr naechstes ohne Pausen ab.
  ks_persist_delete(PERSIST_WARTET);
  ks_persist_delete(PERSIST_WARTET_LISTE_A);
  ks_persist_delete(PERSIST_WARTET_LISTE_B);
  const Trainingsstand fremd = { .art = ArtLaufen, .beginn = BEGINN + 5000, .dauer_s = 600 };
  ks_persist_write_data(PERSIST_WARTET, &fremd, sizeof(fremd));
  // Wieder die neue Firmware: der fremde Stand geht ohne die alten Pausen.
  Trainingsstand t;
  char liste[KS_LISTE_MAX];
  cl_assert(kieselsport_wartend_laden(&t, liste, sizeof(liste)));
  cl_assert_equal_i(t.beginn, BEGINN + 5000);
  cl_assert_equal_i(kieselsport_wartend_pausen(b, sizeof(b)), 0);
  cl_assert_equal_i(kieselsport_wartend_ende(), 0);
}

// --- Der Stopp eines verworfenen Trainings ---

void test_kieselsport__verworfen_merkt_den_stopp(void) {
  kieselsport_starte(ArtWandern, BEGINN);
  prv_sekunden(100);
  kieselsport_pause_umschalten();
  kieselsport_verwerfen();
  uint32_t beginn = 0, zeit = 0;
  uint8_t art = 0;
  cl_assert(kieselsport_stopp_laden(&beginn, &zeit, &art));
  cl_assert_equal_i(beginn, BEGINN);
  cl_assert_equal_i(zeit, (uint32_t)s_jetzt);
  cl_assert_equal_i(art, ArtWandern);
  // Ein spaet bestaetigter fremder Stopp loescht ihn nicht, der eigene schon.
  kieselsport_stopp_vergessen(BEGINN - 1);
  cl_assert(kieselsport_stopp_laden(&beginn, &zeit, &art));
  kieselsport_stopp_vergessen(BEGINN);
  cl_assert(!kieselsport_stopp_laden(&beginn, &zeit, &art));

  // Unter einer Minute gespeichert ist auch verworfen.
  kieselsport_starte(ArtLaufen, BEGINN + 500);
  prv_sekunden(20);
  kieselsport_speichern();
  cl_assert(kieselsport_stopp_laden(&beginn, &zeit, &art));
  cl_assert_equal_i(beginn, BEGINN + 500);
  // Ein neues Training ersetzt ihn - sein Start sagt dem Telefon alles.
  kieselsport_starte(ArtLaufen, BEGINN + 900);
  cl_assert(!kieselsport_stopp_laden(&beginn, &zeit, &art));
  // Ein gespeichertes Training traegt den Stopp in der Zusammenfassung.
  prv_sekunden(60);
  kieselsport_speichern();
  cl_assert(!kieselsport_stopp_laden(&beginn, &zeit, &art));
}

void test_kieselsport__nachsenden_auch_fuer_den_stopp(void) {
  kieselsport_starte(ArtLaufen, BEGINN);
  prv_sekunden(30);
  kieselsport_verwerfen();
  cl_assert(s_minutentakt);
  s_minutentakt->cb(s_minutentakt->cb_data);
  s_minutentakt->cb(s_minutentakt->cb_data);
  cl_assert_equal_i(s_geholt, 1);
  // Angekommen: der Takt hoert auf.
  kieselsport_stopp_vergessen(BEGINN);
  s_minutentakt->cb(s_minutentakt->cb_data);
  cl_assert(!s_minutentakt);
  // Auch nach einem Neustart der Uhr.
  kieselsport_starte(ArtLaufen, BEGINN + 100);
  prv_sekunden(10);
  kieselsport_verwerfen();
  s_minutentakt = NULL;
  kieselsport_init();
  cl_assert(s_minutentakt);
}

// --- Bestaetigungen gehoeren zu ihrem Training ---

static void prv_training(uint32_t beginn, int sekunden) {
  kieselsport_starte(ArtLaufen, beginn);
  prv_sekunden(sekunden);
  kieselsport_speichern();
}

void test_kieselsport__spaete_bestaetigung_loescht_kein_neueres_training(void) {
  prv_training(BEGINN, 120);
  // B kommt dazu, bevor das Telefon A bestaetigt hat - dann dessen ACK.
  prv_training(BEGINN + 1000, 90);
  kieselsport_wartend_vergessen(BEGINN);
  Trainingsstand t;
  char liste[KS_LISTE_MAX];
  cl_assert(kieselsport_wartend_laden(&t, liste, sizeof(liste)));
  cl_assert_equal_i(t.beginn, BEGINN + 1000);
  // Die Pausen und das Ende von B stehen auch noch.
  uint8_t b[KS_PAUSEN_BYTES];
  cl_assert_equal_i(kieselsport_wartend_pausen(b, sizeof(b)), 2);
  // Die eigene Bestaetigung loescht B.
  kieselsport_wartend_vergessen(BEGINN + 1000);
  cl_assert(!kieselsport_wartend_laden(&t, liste, sizeof(liste)));
}

void test_kieselsport__spaete_kurvenbestaetigung_trifft_keine_neue_kurve(void) {
  prv_training(BEGINN, 410);
  uint8_t stueck[300];
  cl_assert_equal_i(kieselsport_kurve_stueck(stueck, sizeof(stueck)), 42);
  // Das Stueck von A ist unterwegs, B wird gespeichert.
  prv_training(BEGINN + 1000, 300);
  cl_assert(!kieselsport_kurve_bestaetigt(BEGINN, 0, 42));
  cl_assert(kieselsport_kurve_wartet());
  cl_assert_equal_i(kieselsport_kurve_beginn(), BEGINN + 1000);
  cl_assert_equal_i(kieselsport_kurve_ab(), 0);
  // Auch ein Stueck von B an falscher Stelle zaehlt nicht.
  cl_assert(!kieselsport_kurve_bestaetigt(BEGINN + 1000, 5, 10));
  cl_assert(kieselsport_kurve_bestaetigt(BEGINN + 1000, 0, 10));
  cl_assert_equal_i(kieselsport_kurve_ab(), 10);
}

// DAS SPAETE ACK MITTEN IM SPEICHERN. Die App bestaetigt die Zusammenfassung von
// A, waehrend KernelBG gerade B speichert: Pausen und Ende von B stehen schon
// da, der wartende Stand noch nicht - er sagt noch A. Ohne die Sperre passte
// die Pruefung, und das Loeschen naehme die Pausen und das Ende von B mit; B
// ginge ohne sie ans Telefon.
static void prv_ack_a(void) {
  kieselsport_wartend_vergessen(BEGINN);
}

void test_kieselsport__spaetes_ack_mitten_im_speichern_loescht_nichts_von_b(void) {
  prv_training(BEGINN, 120);
  kieselsport_starte(ArtLaufen, BEGINN + 1000);
  prv_sekunden(60);
  prv_pause_fuer(30);
  prv_sekunden(60);
  s_app_aktion = prv_ack_a;
  s_unterbrich_vor = PERSIST_WARTET;
  kieselsport_speichern();
  cl_assert(s_app_aktion == NULL);   // die App kam dran
  Trainingsstand t;
  char liste[KS_LISTE_MAX];
  cl_assert(kieselsport_wartend_laden(&t, liste, sizeof(liste)));
  cl_assert_equal_i(t.beginn, BEGINN + 1000);
  uint8_t b[KS_PAUSEN_BYTES];
  cl_assert(kieselsport_wartend_pausen(b, sizeof(b)) > 2);
  cl_assert(kieselsport_wartend_ende() != 0);
}

// Dasselbe fuer die Kurve: die App bestaetigt das letzte Stueck der Kurve von
// A, waehrend KernelBG die Stuecke von B schon geschrieben hat, Anzahl und
// Beginn aber noch nicht. Ohne die Sperre loeschte kurve_vergessen die Stuecke
// von B, und deren Kurve haette eine Luecke - sie wuerde verworfen.
static void prv_kurven_ack_a(void) {
  kieselsport_kurve_bestaetigt(BEGINN, 0, 42);
}

void test_kieselsport__spaetes_kurven_ack_mitten_im_speichern_loescht_nichts_von_b(void) {
  prv_training(BEGINN, 410);
  cl_assert_equal_i(kieselsport_kurve_anzahl(), 42);
  kieselsport_starte(ArtLaufen, BEGINN + 1000);
  prv_sekunden(300);
  s_app_aktion = prv_kurven_ack_a;
  s_unterbrich_vor = PERSIST_KURVE_ANZAHL;
  kieselsport_speichern();
  cl_assert(s_app_aktion == NULL);
  cl_assert(kieselsport_kurve_wartet());
  cl_assert_equal_i(kieselsport_kurve_beginn(), BEGINN + 1000);
  cl_assert(!kieselsport_kurve_luecke());
  uint8_t stueck[300];
  cl_assert_equal_i(kieselsport_kurve_stueck(stueck, sizeof(stueck)), kieselsport_kurve_anzahl());
}

// DAS KURVENSTUECK MITTEN IM SPEICHERN. Die App liest das naechste Stueck der
// Kurve von A, waehrend KernelBG B speichert: die Werte von B stehen schon da,
// Anzahl, Beginn und Zeiger noch die von A. Einzeln gelesen ginge der Puls von B
// unter dem Beginn von A hinaus, und das Telefon truege ihn dort ein.
static uint32_t s_gelesen_beginn;
static uint16_t s_gelesen_ab;
static uint16_t s_gelesen_anzahl;
static uint16_t s_gelesen_n;
static uint8_t s_gelesen[300];
static void prv_kurve_lesen(void) {
  s_gelesen_n = kieselsport_kurve_lesen(&s_gelesen_beginn, &s_gelesen_ab, &s_gelesen_anzahl,
                                        s_gelesen, sizeof(s_gelesen));
}

void test_kieselsport__kurvenstueck_mitten_im_speichern_gehoert_zu_einem_training(void) {
  // A mit Puls 100, 42 Werte; B mit Puls 150, 31 Werte.
  kieselsport_starte(ArtLaufen, BEGINN);
  for (int i = 0; i < 410; i++) {
    prv_sekunde_mit_puls(100);
  }
  kieselsport_speichern();
  cl_assert_equal_i(kieselsport_kurve_anzahl(), 42);
  kieselsport_starte(ArtLaufen, BEGINN + 1000);
  for (int i = 0; i < 300; i++) {
    prv_sekunde_mit_puls(150);
  }
  s_app_aktion = prv_kurve_lesen;
  s_unterbrich_vor = PERSIST_KURVE_ANZAHL;
  kieselsport_speichern();
  cl_assert(s_app_aktion == NULL);
  // Alles, was die App las, gehoert zu B.
  cl_assert_equal_i(s_gelesen_beginn, BEGINN + 1000);
  cl_assert_equal_i(s_gelesen_ab, 0);
  cl_assert_equal_i(s_gelesen_anzahl, 31);
  cl_assert_equal_i(s_gelesen_n, 31);
  cl_assert_equal_i(s_gelesen[20], 150);
}

void test_kieselsport__kurve_mit_luecke(void) {
  // 211 Werte in zwei Schluesseln.
  prv_training(BEGINN, 2100);
  cl_assert_equal_i(kieselsport_kurve_anzahl(), 211);
  cl_assert(!kieselsport_kurve_luecke());
  // Der zweite Schluessel fehlt (ein gescheitertes Schreiben): ab Wert 200
  // laesst sich nichts mehr schicken.
  cl_assert(kieselsport_kurve_bestaetigt(BEGINN, 0, 200));
  ks_persist_delete(PERSIST_KURVE_BASIS + 1);
  uint8_t stueck[300];
  cl_assert_equal_i(kieselsport_kurve_stueck(stueck, sizeof(stueck)), 0);
  cl_assert(kieselsport_kurve_luecke());
  kieselsport_kurve_vergessen();
  cl_assert(!kieselsport_kurve_luecke());
}

// --- Ein vergessenes Training ---

void test_kieselsport__halbe_stunde_pause_beendet_das_training(void) {
  kieselsport_starte(ArtWandern, BEGINN);
  prv_sekunden(600);
  kieselsport_pause_umschalten();
  const time_t pause = s_jetzt;
  for (int i = 0; i < 30 * 60 - 1; i++) {
    prv_sekunde();
  }
  cl_assert_equal_i(prv_stand().zustand, LaufPause);
  prv_sekunde();
  // Gespeichert wie mit der Taste: Archiv, Verlauf, Telefon.
  KsStand s = prv_stand();
  cl_assert_equal_i(s.zustand, LaufAus);
  cl_assert_equal_i(s.ende, KsEndeGespeichert);
  cl_assert_equal_i(s_gemeldet_zahl, 1);
  cl_assert_equal_i(kieselsport_archiv_anzahl(), 1);
  Trainingsstand t;
  char liste[KS_LISTE_MAX];
  cl_assert(kieselsport_wartend_laden(&t, liste, sizeof(liste)));
  cl_assert_equal_i(t.dauer_s, 600);
  // Die aktive Zeit endete, als die Pause begann.
  cl_assert_equal_i(kieselsport_wartend_ende(), (uint32_t)pause);
  // Sensoren aus, der Takt steht, und der Dienst holt die App zum Senden.
  cl_assert_equal_i(s_sensor_takt, 0);
  cl_assert(!s_takt);
  cl_assert(s_minutentakt);
}

void test_kieselsport__nur_eine_pause_am_stueck_zaehlt(void) {
  kieselsport_starte(ArtWandern, BEGINN);
  prv_sekunden(120);
  // Zwanzig Minuten Pause, kurz weiter, wieder zwanzig: kein Ende.
  kieselsport_pause_umschalten();
  for (int i = 0; i < 20 * 60; i++) prv_sekunde();
  kieselsport_pause_umschalten();
  prv_sekunden(10);
  kieselsport_pause_umschalten();
  for (int i = 0; i < 20 * 60; i++) prv_sekunde();
  cl_assert_equal_i(prv_stand().zustand, LaufPause);
  // Auch Pause-Weiter-Pause zwischen zwei Takten faengt neu an.
  kieselsport_pause_umschalten();
  kieselsport_pause_umschalten();
  for (int i = 0; i < 29 * 60; i++) prv_sekunde();
  cl_assert_equal_i(prv_stand().zustand, LaufPause);
  cl_assert_equal_i(s_gemeldet_zahl, 0);
}

// --- Brummen nur bei eindeutigem Zonenwechsel ---

void test_kieselsport__zonen_brummen_erst_drei_schlaege_jenseits(void) {
  // Maximalpuls 190: Zone 3 beginnt bei 133.
  cl_assert_equal_i(kieselsport_zonengrenze(3), 133);
  kieselsport_starte(ArtBike, 0);
  for (int i = 0; i < 10; i++) prv_sekunde_mit_puls(120);
  // Zehn Minuten lang pendelt der Puls um einen Schlag um die Grenze.
  for (int i = 0; i < 600; i++) {
    prv_sekunde_mit_puls((i / 5) % 2 ? 134 : 132);
  }
  cl_assert_equal_i(s_brumm_zahl, 0);
  // Zwei Schlaege drueber reicht nicht, drei schon - einmal hoch.
  prv_sekunde_mit_puls(135);
  cl_assert_equal_i(s_brumm_zahl, 0);
  prv_sekunde_mit_puls(136);
  cl_assert_equal_i(s_brumm_zahl, 1);
  cl_assert_equal_i(s_brumm_laenge, 3);
  // Zurueck an die Grenze: bleibt Zone 3, bis drei Schlaege darunter.
  prv_sekunde_mit_puls(132);
  prv_sekunde_mit_puls(130);
  cl_assert_equal_i(s_brumm_zahl, 1);
  prv_sekunde_mit_puls(129);
  cl_assert_equal_i(s_brumm_zahl, 2);
  cl_assert_equal_i(s_brumm_laenge, 1);
}

// --- Laenger als 18 Stunden ---

void test_kieselsport__bahnen_nach_achtzehn_stunden_an_der_richtigen_stelle(void) {
  kieselsport_becken_setze(25);
  kieselsport_starte(ArtSchwimmen, BEGINN);
  // Gut 18 Stunden in eine Richtung (vergessen am Beckenrand), dann eine
  // Bahn zurueck und wieder hin.
  const int lang = 65536 + 100;
  prv_schwimmen(lang, 0);
  prv_schwimmen(30, TRIG_MAX_ANGLE / 2);
  prv_schwimmen(30, 0);
  kieselsport_speichern();
  Trainingsstand t;
  char liste[KS_LISTE_MAX];
  cl_assert(kieselsport_wartend_laden(&t, liste, sizeof(liste)));
  cl_assert_equal_i(t.bahnen, 3);
  // Die zweite Bahn begann bei der Wende nach der langen ersten - in vollen
  // Sekunden, nicht 65536 weniger.
  unsigned beginn[3] = { 0 }, anzahl = 0, dauer[3] = { 0 };
  const char *c = liste;
  for (int i = 0; i < 3; i++) {
    cl_assert(sscanf(c, "%u:%u:%u;", &beginn[i], &anzahl, &dauer[i]) == 3);
    c = strchr(c, ';') + 1;
  }
  cl_assert(beginn[1] > 65536);
  cl_assert(dauer[0] > 65536);
  cl_assert(dauer[1] >= 20 && dauer[1] <= 40);
}

// --- Gleiche Einstellungen schreiben nichts ---

void test_kieselsport__gleiche_einstellung_schreibt_nichts(void) {
  kieselsport_becken_setze(33);
  kieselsport_empfindlichkeit_setze(3);
  kieselsport_pausenziel_setze(60);
  kieselsport_maxpuls_setze(185);
  const int nach_erstem = s_schreibvorgaenge;
  cl_assert(nach_erstem > 0);
  // Boulder schickt beim Speichern alle Werte noch einmal mit.
  kieselsport_becken_setze(33);
  kieselsport_empfindlichkeit_setze(3);
  kieselsport_pausenziel_setze(60);
  kieselsport_maxpuls_setze(185);
  cl_assert_equal_i(s_schreibvorgaenge, nach_erstem);
  // "Nach Alter" ebenso: einmal geschrieben, dann nicht mehr.
  kieselsport_maxpuls_nicht_setzen();
  const int nach_alter = s_schreibvorgaenge;
  cl_assert(nach_alter > nach_erstem);
  kieselsport_maxpuls_nicht_setzen();
  cl_assert_equal_i(s_schreibvorgaenge, nach_alter);
  // Was sich aendert, kommt weiter an.
  kieselsport_becken_setze(50);
  cl_assert_equal_i(s_schreibvorgaenge, nach_alter + 1);
  kieselsport_init();
  cl_assert_equal_i(kieselsport_becken(), 50);
  cl_assert_equal_i(kieselsport_empfindlichkeit(), 3);
  cl_assert_equal_i(kieselsport_pausenziel(), 60);
  cl_assert(!kieselsport_maxpuls_gesetzt());
}
