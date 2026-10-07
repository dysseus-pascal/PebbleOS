/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "pbl/services/activity/boulder/kalorien.h"

#include "pbl/kernel/mutex.h"

#include "stubs_logging.h"

#include <setjmp.h>
#include <stdlib.h>

// T5.1 (Spezifikation Schritt 2, 6.1): kalorien.c gegen Boulders Rechnung. Die Sollwerte stammen
// nicht aus dieser Datei, sondern aus Boulders eigenem Code: Trainingsanalyse.kt aus Boulder
// 34fc13cb (Zeilen 95-159, 174, 544 unveraendert ausgeschnitten), mit kotlinc ausgefuehrt auf
// den Faellen aus TrainingsanalyseTest.kt:74-153 und einigen mehr (Orakel im Scratchpad von AP1,
// a1-orakel/). Die Pulsreihen unten sind dieselben Eingaben wie dort.
//
// Gemessen wird in Kalorien (1/1000 kcal). Die Spezifikation verlangt hoechstens 1 %; geprueft
// wird enger (0,05 %, mindestens 2 cal), damit auch ein kleiner Fehler in der Formel auffaellt.
// Uebrig bleibt nur die Rundung auf ganze Kalorien und bei "Andere" die VO2max-Mitte auf 0,01.

// Umfeld: Profil (activity_prefs), Ruhepuls der Nacht, Pebbles Stopp
static uint16_t s_gewicht_dag = 7000;
static uint8_t s_alter = 41;
static ActivityGender s_geschlecht = ActivityGenderMale;
uint16_t activity_prefs_get_weight_dag(void) { return s_gewicht_dag; }
uint8_t activity_prefs_get_age_years(void) { return s_alter; }
ActivityGender activity_prefs_get_gender(void) { return s_geschlecht; }
extern void fake_ruhepuls_setzen(uint8_t bpm);

static int s_stopps;
static bool s_stopp_ergebnis;
static bool s_verworfen_im_stopp;
bool workout_service_stop_workout(void) {
  s_stopps++;
  s_verworfen_im_stopp = boulder_training_verworfen();
  return s_stopp_ergebnis;
}

// ---------------------------------------------------------------------------------------
// Die Sperre des Zaehlers, mit zwei Tasks
//
// Pebble schreibt den Zaehler des Trainings auf KernelMain, Kieselsports Takt liest ihn auf
// KernelBG. KernelMain hat die hoehere Prioritaet und kann KernelBG mitten im Lesen unterbrechen.
// Der Test laeuft in einem Faden; die Attrappe laesst KernelMain im schlimmsten Moment dran:
// sobald KernelBG die Sperre haelt, also mitten in der Kopie. Greift KernelMain dann zur selben
// Sperre, muesste es warten: Sein Aufruf springt zurueck (longjmp, vor jeder Wirkung) und laeuft,
// sobald KernelBG freigibt, von vorn - wie nach dem Warten.

typedef enum { AufKernelBG, AufKernelMain } Task;
static Task s_task;
static struct pbl_mutex *s_gesperrt;  //< kalorien.c hat eine Sperre
static Task s_halter;
static int s_tiefe;
static void (*s_kernel_main)(void);  //< kommt dran, sobald KernelBG sperrt
static bool s_kernel_main_wartet;
static int s_unterbrochen;
static jmp_buf s_warten;

static void prv_kernel_main_laeuft(void) {
  s_task = AufKernelMain;
  if (setjmp(s_warten) == 0) {
    s_kernel_main();
    s_kernel_main = NULL;
  }
  s_task = AufKernelBG;
}

void pbl_mutex_init(struct pbl_mutex *m) {}
void pbl_mutex_deinit(struct pbl_mutex *m) {}
int pbl_mutex_lock_lr(struct pbl_mutex *m, pbl_timeout_t timeout, uintptr_t lr) {
  cl_assert(!s_gesperrt || s_gesperrt == m);
  s_gesperrt = m;
  if (s_tiefe > 0 && s_halter != s_task) {
    cl_assert(s_task == AufKernelMain);
    s_kernel_main_wartet = true;
    longjmp(s_warten, 1);
  }
  s_halter = s_task;
  s_tiefe++;
  if (s_task == AufKernelBG && s_kernel_main && !s_kernel_main_wartet) {
    s_unterbrochen++;
    prv_kernel_main_laeuft();
  }
  return 0;
}
void pbl_mutex_unlock(struct pbl_mutex *m) {
  cl_assert(s_tiefe > 0 && s_halter == s_task);
  if (--s_tiefe == 0 && s_kernel_main_wartet) {
    s_kernel_main_wartet = false;
    prv_kernel_main_laeuft();
  }
}
bool pbl_mutex_is_owner(const struct pbl_mutex *m) {
  return s_tiefe > 0 && s_halter == s_task;
}
void pbl_mutex_assert_held(const struct pbl_mutex *m, bool held) {}

// ---------------------------------------------------------------------------------------
// Eingaben wie in Boulders Test

// Die Radfahrt vom 29.9.2026, 17:37-18:00: Puls je Minute (TrainingsanalyseTest.kt:65-68)
static const uint8_t FAHRT[] = {
  95, 94, 126, 121, 120, 122, 121, 128, 130, 132, 128, 122,
  129, 125, 140, 139, 137, 140, 135, 125, 129, 125, 143, 145,
};
#define FAHRT_N (sizeof(FAHRT) / sizeof(FAHRT[0]))

typedef struct {
  int32_t t;
  uint8_t bpm;
} Punkt;

// Boulder rechnet den letzten Wert mit 10 s (Trainingsanalyse.kt:133). Auf der Uhr endet der
// letzte Wert am Ende des Trainings; hier also 10 s nach ihm.
static int32_t prv_rechne(const Punkt *punkte, int n, const BoulderKalorienProfil *profil) {
  BoulderKalorienZaehler z;
  boulder_kalorien_zaehler_neu(&z, profil);
  int32_t summe = 0;
  for (int i = 0; i < n; i++) {
    summe += boulder_kalorien_zaehler_puls(&z, punkte[i].bpm, punkte[i].t);
  }
  summe += boulder_kalorien_zaehler_bis(&z, punkte[n - 1].t + 10);
  // Was P1 Schritt fuer Schritt bekommt, ist die gerundete Summe: keine Drift.
  cl_assert_equal_i(summe, boulder_kalorien_zaehler_summe(&z, punkte[n - 1].t + 10));
  return summe;
}

static int32_t prv_fahrt(uint16_t gewicht_dag, uint8_t alter, ActivityGender g, uint8_t ruhe,
                         uint8_t max) {
  Punkt p[FAHRT_N];
  for (unsigned i = 0; i < FAHRT_N; i++) {
    p[i] = (Punkt){.t = 60 * i, .bpm = FAHRT[i]};
  }
  BoulderKalorienProfil profil;
  cl_assert(boulder_kalorien_profil(&profil, gewicht_dag, alter, g, ruhe, max));
  return prv_rechne(p, FAHRT_N, &profil);
}

// Sekundenpuls wie auf der Uhr (Orakel "sekunden"): 20 min, Stufen und Intervalle, einzelne
// fehlende Sekunden und eine Luecke von 70 s.
static int prv_sekundenreihe(Punkt *p) {
  int n = 0;
  for (int s = 0; s < 1200; s++) {
    if (s % 97 == 50 || (s >= 400 && s <= 470)) {
      continue;
    }
    p[n++] = (Punkt){.t = s, .bpm = 100 + (s / 20) % 60 + (((s / 120) % 2) ? 25 : 0)};
  }
  return n;
}

static void prv_wie_boulder(int32_t cal, double boulder_kcal) {
  const double soll = boulder_kcal * 1000.0;
  const double abweichung = abs((int)(cal - (int32_t)(soll + 0.5)));
  printf("  %d cal, Boulder %.3f cal, Abweichung %.4f %%\n", (int)cal, soll,
         soll > 0 ? 100.0 * abweichung / soll : 0.0);
  cl_assert(abweichung <= 0.01 * soll);  // Kriterium 5: hoechstens 1 %
  cl_assert(abweichung <= 2 || abweichung <= 0.0005 * soll);
}

void test_boulder_kalorien__initialize(void) {
  s_gewicht_dag = 7000;
  s_alter = 41;
  s_geschlecht = ActivityGenderMale;
  fake_ruhepuls_setzen(54);
  boulder_kalorien_maxpuls(0);
  s_stopps = 0;
  s_stopp_ergebnis = true;
  s_verworfen_im_stopp = false;
  s_task = AufKernelBG;
  s_kernel_main = NULL;
  s_kernel_main_wartet = false;
  s_unterbrochen = 0;
}

void test_boulder_kalorien__cleanup(void) {
  // Keine Sperre bleibt gehalten, KernelMain wartet auf nichts mehr.
  cl_assert_equal_i(s_tiefe, 0);
  cl_assert(!s_kernel_main_wartet);
}

// ---------------------------------------------------------------------------------------
// T5.1: die Faelle aus Boulders Test, gegen Boulders Code

void test_boulder_kalorien__fahrt_wie_boulder(void) {
  prv_wie_boulder(prv_fahrt(7000, 41, ActivityGenderMale, 54, 190), 156.174847);
  // Ohne Maximum Tanaka (179,3): derselbe Puls ist mehr Anstrengung
  prv_wie_boulder(prv_fahrt(7000, 41, ActivityGenderMale, 54, 0), 169.511406);
}

void test_boulder_kalorien__maximalpuls_gilt_unveraendert(void) {
  // 65 Jahre, Tanaka 162,5: auch ein Wert ueber Tanaka + 25 gilt (Herzgrenzen prueft vorher)
  prv_wie_boulder(prv_fahrt(7000, 65, ActivityGenderMale, 54, 190), 106.738205);
  prv_wie_boulder(prv_fahrt(7000, 65, ActivityGenderMale, 54, 185), 110.812182);
  prv_wie_boulder(prv_fahrt(7000, 65, ActivityGenderMale, 54, 0), 133.791667);
}

void test_boulder_kalorien__ruhepuls_ausserhalb_ist_60(void) {
  prv_wie_boulder(prv_fahrt(7000, 41, ActivityGenderMale, 0, 190), 149.858217);
  prv_wie_boulder(prv_fahrt(7000, 41, ActivityGenderMale, 120, 190), 149.858217);
  prv_wie_boulder(prv_fahrt(7000, 41, ActivityGenderMale, 29, 190), 149.858217);
  BoulderKalorienProfil p;
  cl_assert(boulder_kalorien_profil(&p, 7000, 41, ActivityGenderMale, 30, 190));
  cl_assert_equal_i(p.ruhe_z, 300);
  cl_assert(boulder_kalorien_profil(&p, 7000, 41, ActivityGenderMale, 110, 190));
  cl_assert_equal_i(p.ruhe_z, 1100);
}

void test_boulder_kalorien__unter_dem_ruhepuls_nichts_aktiv(void) {
  Punkt p[61];
  for (int i = 0; i <= 60; i++) {
    p[i] = (Punkt){.t = 10 * i, .bpm = 45};
  }
  BoulderKalorienProfil profil;
  cl_assert(boulder_kalorien_profil(&profil, 7000, 41, ActivityGenderMale, 54, 190));
  cl_assert_equal_i(prv_rechne(p, 61, &profil), 0);
}

// Boulders lueckenZaehlenNicht: Zwei Werte 10 min auseinander zaehlen wie 70 s dichter Puls.
void test_boulder_kalorien__luecke_hoechstens_60s(void) {
  BoulderKalorienProfil profil;
  cl_assert(boulder_kalorien_profil(&profil, 7000, 41, ActivityGenderMale, 54, 190));
  const Punkt luecke[] = {{0, 130}, {600, 130}};
  Punkt dicht[7];
  for (int i = 0; i < 7; i++) {
    dicht[i] = (Punkt){.t = 10 * i, .bpm = 130};
  }
  const int32_t a = prv_rechne(luecke, 2, &profil);
  cl_assert_equal_i(a, prv_rechne(dicht, 7, &profil));
  prv_wie_boulder(a, 8.246652);
}

void test_boulder_kalorien__geschlecht_alter_gewicht(void) {
  prv_wie_boulder(prv_fahrt(7000, 41, ActivityGenderFemale, 54, 190), 110.627451);
  prv_wie_boulder(prv_fahrt(7000, 41, ActivityGenderOther, 54, 190), 133.401149);
  prv_wie_boulder(prv_fahrt(8235, 33, ActivityGenderMale, 58, 0), 206.010401);
  prv_wie_boulder(prv_fahrt(5500, 18, ActivityGenderFemale, 48, 200), 112.170118);
  prv_wie_boulder(prv_fahrt(7000, 90, ActivityGenderMale, 54, 0), 134.979167);
  // Ueber dem Maximum gilt die VO2max (Anteil 1)
  prv_wie_boulder(prv_fahrt(7000, 41, ActivityGenderMale, 54, 130), 269.734377);
}

void test_boulder_kalorien__sekundenpuls_mit_luecken(void) {
  static Punkt p[1200];
  const int n = prv_sekundenreihe(p);
  cl_assert_equal_i(n, 1118);
  BoulderKalorienProfil profil;
  cl_assert(boulder_kalorien_profil(&profil, 7539, 30, ActivityGenderMale, 52, 188));
  prv_wie_boulder(prv_rechne(p, n, &profil), 207.887107);
  cl_assert(boulder_kalorien_profil(&profil, 7539, 30, ActivityGenderOther, 52, 0));
  prv_wie_boulder(prv_rechne(p, n, &profil), 181.930300);
}

// Boulders vo2maxMitte (TrainingsanalyseTest.kt:150-156 und das Orakel)
void test_boulder_kalorien__vo2max_mitte(void) {
  BoulderKalorienProfil p;
  cl_assert(boulder_kalorien_profil(&p, 7000, 41, ActivityGenderMale, 54, 190));
  cl_assert_equal_i(p.vo2max_c, 3964);
  cl_assert(boulder_kalorien_profil(&p, 7000, 18, ActivityGenderMale, 54, 190));
  cl_assert_equal_i(p.vo2max_c, 4800);
  cl_assert(boulder_kalorien_profil(&p, 7000, 90, ActivityGenderMale, 54, 190));
  cl_assert_equal_i(p.vo2max_c, 2440);
  cl_assert(boulder_kalorien_profil(&p, 7000, 41, ActivityGenderFemale, 54, 190));
  cl_assert_equal_i(p.vo2max_c, 2910);
  cl_assert(boulder_kalorien_profil(&p, 7000, 41, ActivityGenderOther, 54, 190));
  cl_assert_equal_i(p.vo2max_c, 3437);
  cl_assert(boulder_kalorien_profil(&p, 7000, 33, ActivityGenderOther, 54, 190));
  cl_assert_equal_i(p.vo2max_c, 3788);
  // Tanaka in Zehnteln: 208 - 0,7 * 41 = 179,3
  cl_assert(boulder_kalorien_profil(&p, 7000, 41, ActivityGenderMale, 54, 0));
  cl_assert_equal_i(p.max_z, 1793);
}

// Boulders ohneGewichtOderMitUnsinnNichts: kein Profil, keine Pulskalorien
void test_boulder_kalorien__ohne_profil_nichts(void) {
  BoulderKalorienProfil p;
  cl_assert(!boulder_kalorien_profil(&p, 0, 41, ActivityGenderMale, 54, 190));
  cl_assert(!boulder_kalorien_profil(&p, 1999, 41, ActivityGenderMale, 54, 190));
  cl_assert(boulder_kalorien_profil(&p, 2000, 41, ActivityGenderMale, 54, 190));
  cl_assert(!boulder_kalorien_profil(&p, 7000, 0, ActivityGenderMale, 54, 190));
  cl_assert(!boulder_kalorien_profil(&p, 7000, 41, ActivityGenderMale, 54, 50));
  // Das Maximum muss mehr als 20 bpm ueber dem Ruhepuls liegen (Orakel: 74 null, 75 gilt)
  cl_assert(!boulder_kalorien_profil(&p, 7000, 41, ActivityGenderMale, 54, 74));
  cl_assert_equal_i(p.gewicht_dag, 0);
  cl_assert(boulder_kalorien_profil(&p, 7000, 41, ActivityGenderMale, 54, 75));
  prv_wie_boulder(prv_fahrt(7000, 41, ActivityGenderMale, 54, 75), 293.035167);
}

// ---------------------------------------------------------------------------------------
// F5 a = B: Pebbles Schritt-kcal nur fuer Sekunden ohne gueltigen Puls

void test_boulder_kalorien__schritte_nur_ohne_puls(void) {
  BoulderKalorienProfil profil;
  cl_assert(boulder_kalorien_profil(&profil, 7000, 41, ActivityGenderMale, 54, 190));
  BoulderKalorienZaehler z;
  boulder_kalorien_zaehler_neu(&z, &profil);
  // Vor dem ersten Pulswert: Pebbles Wert ganz
  cl_assert_equal_i(boulder_kalorien_zaehler_schritte(&z, 1000, 10), 1000);
  // Ab dem Pulswert bei 10 deckt der Puls: kein Schritt-kcal
  const int32_t puls = boulder_kalorien_zaehler_puls(&z, 54, 10);  // Ruhepuls: 0 aktiv
  cl_assert_equal_i(puls, 0);
  cl_assert_equal_i(boulder_kalorien_zaehler_schritte(&z, 1000, 20), 0);
  // Der Wert gilt bis 70; von [20, 100) sind 30 s ohne Puls: 900 * 30 / 80
  cl_assert_equal_i(boulder_kalorien_zaehler_schritte(&z, 900, 100), 338);
  // Danach ganz, auch zwei Meldungen in derselben Sekunde
  cl_assert_equal_i(boulder_kalorien_zaehler_schritte(&z, 500, 110), 500);
  cl_assert_equal_i(boulder_kalorien_zaehler_schritte(&z, 7, 110), 7);
  // [110, 120) war noch ohne Puls; ab dem Wert bei 120 wieder gedeckt, auch in derselben Sekunde
  cl_assert_equal_i(boulder_kalorien_zaehler_puls(&z, 54, 120), 0);
  cl_assert_equal_i(boulder_kalorien_zaehler_schritte(&z, 7, 120), 7);
  cl_assert_equal_i(boulder_kalorien_zaehler_schritte(&z, 5, 120), 0);
  cl_assert_equal_i(boulder_kalorien_zaehler_schritte(&z, 9, 130), 0);
  cl_assert_equal_i(boulder_kalorien_zaehler_summe(&z, 130), 1000 + 338 + 500 + 7 + 7);
}

// Ohne brauchbares Profil gilt keine Sekunde als gedeckt: Pebbles Rechnung wie bisher.
void test_boulder_kalorien__ohne_profil_wie_pebble(void) {
  BoulderKalorienProfil profil;
  cl_assert(!boulder_kalorien_profil(&profil, 0, 41, ActivityGenderMale, 54, 190));
  BoulderKalorienZaehler z;
  boulder_kalorien_zaehler_neu(&z, &profil);
  int32_t summe = 0;
  for (int t = 0; t < 600; t++) {
    summe += boulder_kalorien_zaehler_puls(&z, 160, t);
    if (t % 10 == 9) {
      summe += boulder_kalorien_zaehler_schritte(&z, 1234, t);
    }
  }
  cl_assert_equal_i(summe, 60 * 1234);
}

// Stellt der Nutzer die Uhr zurueck, faellt die Summe nicht, und es geht ab der neuen Zeit weiter.
void test_boulder_kalorien__uhr_zurueck(void) {
  BoulderKalorienProfil profil;
  cl_assert(boulder_kalorien_profil(&profil, 7000, 41, ActivityGenderMale, 54, 190));
  BoulderKalorienZaehler z;
  boulder_kalorien_zaehler_neu(&z, &profil);
  for (int t = 0; t < 100; t++) {
    cl_assert(boulder_kalorien_zaehler_puls(&z, 150, t) >= 0);
  }
  const int32_t vorher = boulder_kalorien_zaehler_summe(&z, 100);
  for (int t = 50; t < 150; t++) {
    cl_assert(boulder_kalorien_zaehler_puls(&z, 150, t) >= 0);
  }
  cl_assert(boulder_kalorien_zaehler_bis(&z, 150) >= 0);
  // 99 s vor dem Sprung, 100 s danach
  BoulderKalorienZaehler dicht;
  boulder_kalorien_zaehler_neu(&dicht, &profil);
  for (int t = 0; t < 199; t++) {
    boulder_kalorien_zaehler_puls(&dicht, 150, t);
  }
  cl_assert(vorher > 0);
  cl_assert_equal_i(boulder_kalorien_zaehler_summe(&z, 150),
                    boulder_kalorien_zaehler_summe(&dicht, 199));
}

// Eine Stunde zurueck, danach kein Puls mehr, nur Schritte: Der letzte Wert gilt auch dann
// hoechstens 60 s, und danach zaehlen die Schritte (F5 a = B). Ohne das Zurueckstellen der Zeiten
// galte der Wert die ganze Stunde, und die Schritte fielen weg.
void test_boulder_kalorien__uhr_eine_stunde_zurueck(void) {
  BoulderKalorienProfil profil;
  cl_assert(boulder_kalorien_profil(&profil, 7000, 41, ActivityGenderMale, 54, 190));
  BoulderKalorienZaehler z;
  boulder_kalorien_zaehler_neu(&z, &profil);
  for (int t = 0; t < 3700; t++) {
    boulder_kalorien_zaehler_puls(&z, 150, t);
  }
  // Die Uhr springt um eine Stunde zurueck; die naechste Meldung ist eine Schrittmeldung bei 110.
  for (int t = 110; t <= 700; t += 10) {
    boulder_kalorien_zaehler_schritte(&z, 1000, t);
  }
  // 3699 s vor dem Sprung, 60 s danach
  BoulderKalorienZaehler dicht;
  boulder_kalorien_zaehler_neu(&dicht, &profil);
  for (int t = 0; t < 3759; t++) {
    boulder_kalorien_zaehler_puls(&dicht, 150, t);
  }
  // Gedeckt bis 170: die Meldungen bei 110 ... 170 nicht, 180 ... 700 ganz
  cl_assert_equal_i(z.schritt_cal, 53 * 1000);
  cl_assert_equal_i(boulder_kalorien_zaehler_summe(&z, 700),
                    boulder_kalorien_zaehler_summe(&dicht, 3759) + 53 * 1000);
}

// ---------------------------------------------------------------------------------------
// Das Training: Schnappschuss beim Start

void test_boulder_kalorien__start_haelt_das_profil_fest(void) {
  boulder_kalorien_maxpuls(190);
  boulder_kalorien_start();
  // Was sich im Training aendert, gilt erst beim naechsten Start
  s_gewicht_dag = 9000;
  s_alter = 20;
  fake_ruhepuls_setzen(40);
  boulder_kalorien_maxpuls(0);
  int32_t summe = 0;
  for (unsigned i = 0; i < FAHRT_N; i++) {
    summe += boulder_kalorien_puls(FAHRT[i], 60 * i);
  }
  summe += boulder_kalorien_ende(60 * (FAHRT_N - 1) + 10);
  prv_wie_boulder(summe, 156.174847);
  cl_assert_equal_i(boulder_kalorien_bisher(60 * (FAHRT_N - 1) + 10), summe);
  // Neuer Start: neues Profil, Zaehler auf 0
  s_gewicht_dag = 7000;
  s_alter = 41;
  fake_ruhepuls_setzen(54);
  boulder_kalorien_start();
  cl_assert_equal_i(boulder_kalorien_bisher(1000), 0);
  summe = 0;
  for (unsigned i = 0; i < FAHRT_N; i++) {
    summe += boulder_kalorien_puls(FAHRT[i], 60 * i);
  }
  summe += boulder_kalorien_ende(60 * (FAHRT_N - 1) + 10);
  prv_wie_boulder(summe, 169.511406);
}

// Der Ruhepuls kommt aus der Variante, das Geschlecht aus Pebbles Profil.
void test_boulder_kalorien__start_liest_ruhepuls_und_geschlecht(void) {
  s_geschlecht = ActivityGenderFemale;
  fake_ruhepuls_setzen(0);  // keine Nacht: 60
  boulder_kalorien_maxpuls(190);
  boulder_kalorien_start();
  int32_t summe = 0;
  for (unsigned i = 0; i < FAHRT_N; i++) {
    summe += boulder_kalorien_puls(FAHRT[i], 60 * i);
  }
  summe += boulder_kalorien_ende(60 * (FAHRT_N - 1) + 10);
  prv_wie_boulder(summe, 106.153026);
}

// ---------------------------------------------------------------------------------------
// Der Takt liest, waehrend Pebble schreibt (Gegenpruefung Schritt 2)
//
// Kieselsports Takt liest die Summe jede Sekunde fuer die Anzeige; beendet Pebble das Training
// selbst, geht der letzte gelesene Wert in die Zusammenfassung. Er muss ein ganzer Stand sein:
// der vor oder der nach Pebbles Haken, nie ein Gemisch aus beiden.

static int32_t s_kernel_main_zuwachs;

static void prv_pebble_puls(void) {
  // Ein Pulswert, den Pebble 5 s vor dem Stand des Takts verbucht: Er aendert die Summe, die der
  // Takt bei 200 s liest.
  s_kernel_main_zuwachs = boulder_kalorien_puls(170, 195);
}

static void prv_pebble_schritte(void) {
  // Eine Schrittmeldung nach 50 s ohne Puls: Pebbles Schritt-kcal zaehlen (F5 a = B).
  s_kernel_main_zuwachs = boulder_kalorien_schritte(4000, 200);
}

static void prv_takt_liest_ganz(void (*pebble)(void)) {
  boulder_kalorien_maxpuls(190);
  boulder_kalorien_start();
  int32_t summe = 0;
  for (int t = 0; t < 100; t += 10) {
    summe += boulder_kalorien_puls(120, t);
  }
  // Der letzte Wert (90 s) deckt bis 150 s, danach ist der Takt ohne Puls.
  const int32_t vorher = boulder_kalorien_bisher(200);
  cl_assert(vorher > 0);
  cl_assert_equal_i(s_unterbrochen, 0);

  s_kernel_main = pebble;
  const int32_t gelesen = boulder_kalorien_bisher(200);
  cl_assert_(s_unterbrochen == 1, "der Takt las den Zaehler ohne Sperre");
  cl_assert_(s_kernel_main == NULL, "Pebbles Haken lief nicht, nachdem der Takt freigab");
  // Pebble hat gewartet, bis der Takt seine Kopie hatte: Der Takt sah den Stand davor, ganz.
  cl_assert_equal_i(gelesen, vorher);
  // Danach steht Pebbles Wert drin, und die Summe ist wieder die gerundete.
  cl_assert(s_kernel_main_zuwachs > 0);
  summe += s_kernel_main_zuwachs;
  cl_assert(boulder_kalorien_bisher(200) > vorher);
  summe += boulder_kalorien_ende(200);
  cl_assert_equal_i(boulder_kalorien_bisher(200), summe);
}

void test_boulder_kalorien__takt_liest_ganz_waehrend_pulswert(void) {
  prv_takt_liest_ganz(prv_pebble_puls);
}

void test_boulder_kalorien__takt_liest_ganz_waehrend_schritten(void) {
  prv_takt_liest_ganz(prv_pebble_schritte);
}

// ---------------------------------------------------------------------------------------
// Verwerfen (F3 = A): der Merker gilt nur in genau diesem Stopp

void test_boulder_kalorien__verwerfen_merker_nur_im_stopp(void) {
  cl_assert(!boulder_training_verworfen());
  cl_assert(boulder_training_verwerfen());
  cl_assert_equal_i(s_stopps, 1);
  cl_assert(s_verworfen_im_stopp);
  cl_assert(!boulder_training_verworfen());
  // Kein Training: Pebble lehnt ab, das Ergebnis kommt durch, der Merker faellt trotzdem
  s_stopp_ergebnis = false;
  cl_assert(!boulder_training_verwerfen());
  cl_assert(!boulder_training_verworfen());
}
