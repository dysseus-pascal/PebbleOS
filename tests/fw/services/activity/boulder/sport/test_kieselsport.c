/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kieselsport_umfeld.h"

// Die Tests des Dienstes (AP2) im Umfeld aus kieselsport_umfeld.h: Pebbles echter Workout-Dienst,
// die echte Sitzungsliste, die Sperrfolge (T7.2) nach jedem Test.

void test_kieselsport__initialize(void) {
  umfeld_initialize();
}

void test_kieselsport__cleanup(void) {
  umfeld_cleanup();
}

// =============================================================================================
// Die Tests aus b25 (main: tests/fw/services/kieselsport/test_kieselsport.c), portiert

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
  prv_neustart();
  cl_assert_equal_i(kieselsport_maxpuls(), 200);
  cl_assert_equal_i(kieselsport_zonengrenze(3), 140);
}

void test_kieselsport__laufen_von_start_bis_telefon(void) {
  kieselsport_starte(ArtLaufen, BEGINN - 5);
  prv_kernel_bg();
  // Sensor im Sekundentakt (Kieselsports Abo, F2 = A), Laufmodell und keine Bewegungserkennung
  // daneben (beides Pebble).
  cl_assert_equal_i(prv_sensor_takt(), 1);
  cl_assert_equal_i(s_szene, HRMActivityScene_Run);
  cl_assert_equal_s(s_erkennung, "0");
  cl_assert_equal_i(s_offen, 1);
  cl_assert(!s_accel_handler);
  cl_assert(workout_service_is_workout_ongoing());

  for (int i = 0; i < 120; i++) {
    prv_sekunde_mit_puls(i < 60 ? 130 : 150);
  }
  KsStand s = prv_stand();
  cl_assert_equal_i(s.zustand, LaufLaeuft);
  cl_assert_equal_i(s.t.beginn, BEGINN - 5);
  cl_assert_equal_i(s.t.dauer_s, 120);
  cl_assert_equal_i(s.t.dauer_s, prv_pebble_dauer());
  cl_assert_equal_i(s.t.schritte, 240);
  cl_assert(s.t.kcal > 0);
  cl_assert_equal_i(s.t.puls_mittel, 140);
  cl_assert_equal_i(s.t.puls_max, 150);
  cl_assert(s.frisch);
  cl_assert_equal_i(s.zone, 3);
  cl_assert_equal_i(s.minute[120 % 60], 150);

  prv_speichern();
  s = prv_stand();
  cl_assert_equal_i(s.zustand, LaufAus);
  cl_assert_equal_i(s.ende, KsEndeGespeichert);
  cl_assert_equal_i(prv_sensor_takt(), 0);
  cl_assert_equal_i(s_offen, 0);

  // V4: Pebbles Stopp, Sitzung und Meldung kommen 30 s spaeter; bis dahin steht Pebbles Training
  // in der Pause.
  cl_assert(workout_service_is_paused());
  prv_sekunden_leer(29);
  cl_assert_equal_i(prv_sitzungen(), 0);
  cl_assert_equal_i(s_meldungen, 0);
  prv_sekunde();
  cl_assert(!workout_service_is_workout_ongoing());
  cl_assert_equal_s(s_erkennung, "01");
  cl_assert_equal_i(s_szene, HRMActivityScene_Default);
  cl_assert(!prv_takt_an());

  // Im Verlauf der Uhr (Pebbles Sitzung) und fuers Telefon; kein Archiv mehr (V7).
  cl_assert_equal_i(prv_sitzungen(), 1);
  cl_assert_equal_i(prv_sitzung(0)->type, ActivitySessionType_Run);
  cl_assert(prv_sitzung(0)->manual);
  cl_assert_equal_i(prv_sitzung(0)->length_min, 2);
  cl_assert_equal_i(s_meldungen, 1);
  cl_assert(!prv_archiv_beschrieben());
  Trainingsstand t;
  char liste[KS_LISTE_MAX];
  cl_assert(kieselsport_wartend_laden(&t, liste, sizeof(liste)));
  cl_assert_equal_i(t.schritte, 240);
  cl_assert_equal_s(liste, "");
  // Zusammenfassung (10005) und Pebbles Sitzung tragen dieselben kcal und Schritte.
  cl_assert_equal_i(t.kcal, prv_sitzung(0)->step_data.active_kcalories);
  cl_assert_equal_i(t.schritte, prv_sitzung(0)->step_data.steps);
  cl_assert(t.meter > 0);
  cl_assert_equal_i(t.meter, prv_sitzung(0)->step_data.distance_meters);

  // Die Kurve: Sekunde 0 bis 120 in Zehnern, also 13 Werte; die erste ist leer, weil der Takt
  // erst nach Sekunde eins zaehlt.
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
  prv_starte(ArtLaufen, 0);
  for (int i = 0; i < 60; i++) {
    prv_sekunde_mit_puls(130);
  }
  prv_speichern();
  uint32_t zeiten[KS_ZONEN + 1];
  cl_assert(kieselsport_kurve_zeiten(zeiten));
  cl_assert_equal_i(zeiten[2], 60);
  cl_assert_equal_i(zeiten[3], 0);
}

void test_kieselsport__zonenwechsel_brummt(void) {
  prv_starte(ArtBike, 0);
  // Velo ist bei Pebble ein offenes Training mit dem Modell fuer hohe Belastung (V5).
  cl_assert_equal_i(s_szene, HRMActivityScene_HighIntensity);
  for (int i = 0; i < 10; i++) {
    prv_sekunde_mit_puls(120);  // Zone 2
  }
  cl_assert_equal_i(s_brumm_zahl, 0);
  prv_sekunde_mit_puls(140);  // Zone 3: hoch, zwei kurze
  cl_assert_equal_i(s_brumm_zahl, 1);
  cl_assert_equal_i(s_brumm_laenge, 3);
  prv_sekunde_mit_puls(141);
  cl_assert_equal_i(s_brumm_zahl, 1);
  prv_sekunde_mit_puls(120);  // runter, einer
  cl_assert_equal_i(s_brumm_zahl, 2);
  cl_assert_equal_i(s_brumm_laenge, 1);

  // Ein alter Wert wechselt keine Zone: nach 90 Sekunden ohne Meldung ist der Puls nicht mehr
  // frisch.
  prv_sekunden_leer(91);
  KsStand s = prv_stand();
  cl_assert(!s.frisch);
  cl_assert_equal_i(s.zone, 0);
  cl_assert_equal_i(s_brumm_zahl, 2);
}

void test_kieselsport__pause_haelt_die_zeit_an(void) {
  prv_starte(ArtWandern, 0);
  cl_assert_equal_i(s_szene, HRMActivityScene_Walk);
  for (int i = 0; i < 30; i++) {
    prv_sekunde_mit_puls(100);
  }
  prv_pause();
  cl_assert_equal_i(prv_stand().zustand, LaufPause);
  cl_assert(workout_service_is_paused());
  // In der Pause misst der Sensor seltener.
  cl_assert_equal_i(prv_sensor_takt(), 10);
  prv_sekunden_leer(30);
  prv_pause();
  cl_assert(!workout_service_is_paused());
  cl_assert_equal_i(prv_sensor_takt(), 1);
  for (int i = 0; i < 30; i++) {
    prv_sekunde_mit_puls(100);
  }
  cl_assert_equal_i(prv_stand().t.dauer_s, 60);
  cl_assert_equal_i(prv_pebble_dauer(), 60);
}

void test_kieselsport__unter_einer_minute_verworfen(void) {
  prv_starte(ArtLaufen, 0);
  for (int i = 0; i < 59; i++) {
    prv_sekunde_mit_puls(100);
  }
  const uint8_t vorher = prv_stand().ende_nr;
  prv_speichern();
  KsStand s = prv_stand();
  cl_assert_equal_i(s.ende, KsEndeVerworfen);
  cl_assert_equal_i(s.ende_nr, (uint8_t)(vorher + 1));
  // Pebble ist sofort gestoppt, ohne Sitzung und Meldung; kein Archiv (V7).
  cl_assert(!workout_service_is_workout_ongoing());
  prv_sekunden_leer(120);
  cl_assert_equal_i(prv_sitzungen(), 0);
  cl_assert_equal_i(s_meldungen, 0);
  cl_assert_equal_i(s_dls_schub, 0);
  cl_assert(!prv_archiv_beschrieben());
  Trainingsstand t;
  char liste[8];
  cl_assert(!kieselsport_wartend_laden(&t, liste, sizeof(liste)));
}

void test_kieselsport__schwacher_akku_misst_sparsam(void) {
  s_akku = 15;
  prv_starte(ArtLaufen, 0);
  cl_assert_equal_i(prv_sensor_takt(), 5);
  cl_assert(prv_stand().sparsam);
}

void test_kieselsport__puls_frisch_geglaettet_und_vom_handgelenk(void) {
  prv_starte(ArtLaufen, 0);
  // Misstraut der Sensor seiner Messung, gilt der Median der letzten drei: die Spitze 180 zieht
  // nicht mit.
  prv_messwert(100, HRMQuality_Poor);
  prv_messwert(180, HRMQuality_Poor);
  prv_messwert(102, HRMQuality_Poor);
  prv_sekunde();
  cl_assert_equal_i(prv_stand().puls, 102);
  // Unsinn faellt weg.
  prv_messwert(250, HRMQuality_Excellent);
  prv_sekunde();
  cl_assert_equal_i(prv_stand().puls, 102);
  // Nicht am Handgelenk: sofort kein Puls.
  prv_messwert(0, HRMQuality_OffWrist);
  prv_sekunde();
  cl_assert_equal_i(prv_stand().puls, 0);
  cl_assert(!prv_stand().frisch);
}

// Ein Arm, der eine Hantel hebt: eine Wiederholung je Sekunde, 25 Messungen je Sekunde, in
// Paketen zu zehn wie vom Beschleunigungsmesser - auf KernelBG, wo der Dienst sich anmeldete.
static int s_messung;
static void prv_accel(AccelData *paket, uint32_t n) {
  cl_assert(s_accel_handler);
  stub_pebble_tasks_set_current(PebbleTask_KernelBackground);
  s_accel_handler(paket, n);
  stub_pebble_tasks_set_current(PebbleTask_App);
}
static void prv_arm(int sekunden, bool bewegt) {
  for (int s = 0; s < sekunden; s++) {
    AccelData paket[25];
    for (int i = 0; i < 25; i++, s_messung++) {
      const double phase = 2.0 * 3.14159265358979 * (double)s_messung / 25.0;
      paket[i] = (AccelData){
        .x = 0,
        .y = 0,
        .z = (int16_t)(1000 + (bewegt ? 450.0 * sin(phase) : 0.0)),
      };
    }
    prv_accel(paket, 10);
    prv_accel(paket + 10, 10);
    prv_accel(paket + 20, 5);
    prv_sekunde();
  }
}

void test_kieselsport__kraft_zaehlt_saetze_und_brummt_nach_der_pause(void) {
  kieselsport_pausenziel_setze(8);
  prv_starte(ArtKraft, 0);
  cl_assert(s_accel_handler);
  s_messung = 0;
  // Erst eine halbe Minute zur Bank: ein Training unter einer Minute gaebe es sonst am Ende
  // nicht.
  prv_arm(30, false);
  prv_arm(12, true);
  KsStand s = prv_stand();
  cl_assert(!s.ruht);
  cl_assert(s.reps_laufend >= 10 && s.reps_laufend <= 13);
  // Ruhe: nach vier Sekunden ist der Satz zu Ende, nach acht weiteren brummt die Uhr - einmal.
  const int vorher = s_brumm_zahl;
  prv_arm(15, false);
  s = prv_stand();
  cl_assert(s.ruht);
  cl_assert_equal_i(s.t.saetze, 1);
  cl_assert_equal_i(s_brumm_zahl, vorher + 1);
  cl_assert_equal_i(s_brumm_letztes[0], 80);

  // Ein zweiter Satz, der mit Stop endet, zaehlt auch.
  prv_arm(6, true);
  prv_speichern();
  cl_assert(!s_accel_handler);
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

// Die Richtung vom Kompass, auf KernelMain wie dort angemeldet.
static void prv_schwimmen(int sekunden, int32_t richtung) {
  for (int s = 0; s < sekunden; s++) {
    cl_assert(s_kompass_handler);
    stub_pebble_tasks_set_current(PebbleTask_KernelMain);
    const CompassHeadingData d = {.magnetic_heading = richtung,
                                  .compass_status = CompassStatusCalibrated};
    s_kompass_handler(d);
    s_kompass_handler(d);
    stub_pebble_tasks_set_current(PebbleTask_App);
    prv_sekunde();
  }
}

void test_kieselsport__schwimmen_zaehlt_bahnen(void) {
  kieselsport_becken_setze(25);
  prv_starte(ArtSchwimmen, 0);
  cl_assert(s_kompass_handler);
  // V5: kein Modell fuer Schwimmen - das allgemeine statt Pebbles hoher Belastung.
  cl_assert_equal_i(s_szene, HRMActivityScene_Default);
  prv_schwimmen(30, 0);
  prv_schwimmen(30, TRIG_MAX_ANGLE / 2);
  prv_schwimmen(30, 0);
  KsStand s = prv_stand();
  cl_assert(s.bahnen_bereit);
  cl_assert_equal_i(s.t.bahnen, 2);
  // Die letzte Bahn zaehlt beim Stop mit.
  prv_speichern();
  cl_assert(!s_kompass_handler);
  Trainingsstand t;
  char liste[KS_LISTE_MAX];
  cl_assert(kieselsport_wartend_laden(&t, liste, sizeof(liste)));
  cl_assert_equal_i(t.bahnen, 3);
  cl_assert_equal_i(t.meter, 75);
}

void test_kieselsport__yoga_misst_hrv(void) {
  prv_starte(ArtYoga, 0);
  cl_assert(prv_sensor_hrv());
  cl_assert_equal_i(s_szene, HRMActivityScene_Default);
  for (int i = 0; i < 40; i++) {
    prv_abstand((i % 2) ? 1040 : 1000);
  }
  prv_sekunde();
  KsStand s = prv_stand();
  cl_assert_equal_i(s.t.hrv_ms, 40);
  cl_assert_equal_i(s.hrv_anzahl, 39);
  prv_verwerfen();
  cl_assert(!prv_sensor_hrv());
  cl_assert_equal_i(prv_sensor_takt(), 0);
}

// b25: archiv_haelt_die_letzten_zwanzig. Das Archiv entfaellt (V7); seine Schluessel bleiben
// reserviert und werden nie mehr geschrieben, auch nicht nach 22 Trainings.
void test_kieselsport__archiv_entfaellt(void) {
  for (int n = 0; n < 22; n++) {
    prv_starte(ArtLaufen, BEGINN + n * 1000);
    for (int i = 0; i < 60; i++) {
      prv_sekunde_mit_puls(100);
    }
    prv_speichern();
  }
  prv_sekunden_leer(30);
  cl_assert(!prv_archiv_beschrieben());
  // Jedes Training steht genau einmal in Pebbles Sitzungsliste.
  cl_assert_equal_i(prv_sitzungen(), 22);
}

void test_kieselsport__lange_liste_in_zwei_haelften(void) {
  char lang[KS_LISTE_MAX];
  for (int i = 0; i < 250; i++) {
    lang[i] = (char)('a' + i % 26);
  }
  lang[250] = 0;
  const Trainingsstand t = {.art = ArtKraft, .beginn = BEGINN, .dauer_s = 600};
  wartend_merken(&t, lang, NULL, 0, 0);
  Trainingsstand gelesen;
  char liste[KS_LISTE_MAX];
  cl_assert(wartend_laden(&gelesen, liste, sizeof(liste)));
  cl_assert_equal_s(liste, lang);
  wartend_vergessen(BEGINN);
  cl_assert(!wartend_laden(&gelesen, liste, sizeof(liste)));
}

void test_kieselsport__nachsenden_nach_2_10_30_minuten(void) {
  prv_starte(ArtLaufen, 0);
  for (int i = 0; i < 60; i++) {
    prv_sekunde_mit_puls(100);
  }
  prv_speichern();
  cl_assert(prv_minutentakt_an());
  int geholt_bei[3] = {0};
  for (int minute = 1; minute <= 45 && prv_minutentakt_an(); minute++) {
    const int vorher = s_geholt;
    prv_minute();
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
  prv_starte(ArtLaufen, 0);
  for (int i = 0; i < 60; i++) {
    prv_sekunde_mit_puls(100);
  }
  prv_speichern();
  kieselsport_wartend_vergessen(0);
  prv_minute();
  cl_assert(!prv_minutentakt_an());
  cl_assert_equal_i(s_geholt, 0);
}

// b25 summierte Pebbles Tageszaehler selbst ueber Mitternacht. Jetzt zaehlt Pebbles
// Workout-Dienst; er setzt bei fallender Tagessumme neu an (workout_service.c:190-193).
void test_kieselsport__training_ueber_mitternacht_verliert_nichts(void) {
  prv_starte(ArtLaufen, BEGINN);
  for (int i = 0; i < 60; i++) {
    prv_sekunde_mit_puls(130);
  }
  // Mitternacht: die Tagessumme der Uhr beginnt bei null.
  s_schritte_tag = 0;
  for (int i = 0; i < 60; i++) {
    prv_sekunde_mit_puls(130);
  }
  KsStand s = prv_stand();
  cl_assert_equal_i(s.t.schritte, 240);
}

void test_kieselsport__pause_zaehlt_keine_schritte(void) {
  prv_starte(ArtLaufen, BEGINN);
  for (int i = 0; i < 30; i++) {
    prv_sekunde_mit_puls(130);
  }
  prv_pause();
  // In der Pause laeuft man zum Kiosk: die Uhr zaehlt, das Training nicht.
  prv_schritte(500);
  prv_pause();
  for (int i = 0; i < 30; i++) {
    prv_sekunde_mit_puls(130);
  }
  cl_assert_equal_i(prv_stand().t.schritte, 120);
}

// --- Schritt 2, AP4: Arten ohne Schritte (V1) und das Maximum fuer die Velofahrt (F7) ---

// Was beim Setzen des Merkers galt: Task, Kieselsports Sperre, Pebbles Training.
static struct {
  PebbleTask task;
  bool sperre;
  bool training;
  char erkennung[sizeof(s_erkennung)];
} s_beim_merker;
static void prv_beim_merker(bool ohne) {
  s_beim_merker.task = pebble_task_get_current();
  s_beim_merker.sperre = pbl_mutex_is_owner(&s_kieselsport_sperre);
  s_beim_merker.training = workout_service_is_workout_ongoing();
  memcpy(s_beim_merker.erkennung, s_erkennung, sizeof(s_erkennung));
}

// T4.1, Kieselsports Seite (V1, vom Nutzer geaendert): Schritte zaehlen nur Laufen und Wandern.
// Fuer Velo, MTB, Kraft, Yoga und Schwimmen setzt Kieselsport nach Pebbles Start den Merker der
// Variante (D17) - auf KernelBG, ohne seine Sperre (Regel 2.5.2). Die Wirkung (Tagessumme, Minute,
// Rate) prueft test_boulder_fahrt an der echten Variante.
void test_kieselsport__t4_1_arten_ohne_schritte(void) {
  // Die Entscheidung des Nutzers (entscheidungen.md, V1), nicht art.c abgeschrieben.
  static const bool ohne[ArtAnzahl] = {
    [ArtLaufen] = false, [ArtBike] = true,  [ArtWandern] = false, [ArtKraft] = true,
    [ArtMTB] = true,     [ArtYoga] = true,  [ArtSchwimmen] = true,
  };
  g_fake_ohne_schritte_beim_aufruf = prv_beim_merker;
  for (int art = 0; art < ArtAnzahl; art++) {
    g_fake_ohne_schritte_aufrufe = 0;
    memset(s_erkennung, 0, sizeof(s_erkennung));
    memset(&s_beim_merker, 0, sizeof(s_beim_merker));
    prv_starte((Sportart)art, BEGINN + art * 600);
    cl_assert_equal_i(g_fake_ohne_schritte_aufrufe, 1);
    cl_assert_equal_b(g_fake_ohne_schritte, ohne[art]);
    cl_assert_equal_i(s_beim_merker.task, PebbleTask_KernelBackground);
    cl_assert(!s_beim_merker.sperre);
    // Nach Pebbles Start: das Training laeuft, die Auto-Erkennung ist schon aus (D15).
    cl_assert(s_beim_merker.training);
    cl_assert_equal_s(s_beim_merker.erkennung, "0");
    prv_sekunden_leer(5);
    prv_verwerfen();
    cl_assert(!kieselsport_laeuft());
  }
}

// F7: Die bestaetigte Velofahrt rechnet mit Kieselsports geprueftem Maximum (V2) wie ein Training.
// Sie hat keinen Start, darum meldet der Dienst es nach dem Laden und nach jeder Aenderung.
void test_kieselsport__maximum_fuer_die_fahrt(void) {
  s_alter = 40;  // Tanaka 180
  fake_ruhepuls_setzen(55);
  BoulderKalorienProfil p;
  kieselsport_maxpuls_setze(175);
  cl_assert(boulder_kalorien_profil_jetzt(&p));
  cl_assert_equal_i(p.max_z, 1750);
  // Ueber Tanaka + 25 gilt Tanaka (V2), auch fuer die Fahrt.
  kieselsport_maxpuls_setze(214);
  cl_assert(boulder_kalorien_profil_jetzt(&p));
  cl_assert_equal_i(p.max_z, 1800);
  kieselsport_maxpuls_nicht_setzen();
  kieselsport_maxpuls_setze(178);
  // Nach einem Neustart weiss kalorien.c nichts mehr; das Laden der Einstellungen meldet es.
  boulder_kalorien_maxpuls(0);
  prv_neustart();
  cl_assert(boulder_kalorien_profil_jetzt(&p));
  cl_assert_equal_i(p.max_z, 1780);
  // Der Ruhepuls des Profils ist der der Variante (V3: Median der sieben Naechte).
  cl_assert_equal_i(p.ruhe_z, 550);
}

// --- Maximalpuls: gesetzt oder nach Alter ---

void test_kieselsport__ohne_einstellung_rechnet_die_uhr_nach_alter(void) {
  // Ohne Alter die alte Vorgabe; mit Alter Tanaka, gerundet wie Boulder.
  cl_assert(!kieselsport_maxpuls_gesetzt());
  cl_assert_equal_i(kieselsport_maxpuls(), 190);
  s_alter = 41;
  cl_assert_equal_i(kieselsport_maxpuls(), 179);  // 208 - 28,7 = 179,3
  cl_assert_equal_i(kieselsport_zonengrenze(1), 89);
  s_alter = 35;
  cl_assert_equal_i(kieselsport_maxpuls(), 184);  // 183,5 rundet auf
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
  prv_neustart();
  cl_assert(kieselsport_maxpuls_gesetzt());
  cl_assert_equal_i(kieselsport_maxpuls(), 190);
  // Zurueck auf "nach Alter": der Wert fliegt aus dem Persist, damit aeltere Firmware ihre
  // Vorgabe liest.
  kieselsport_maxpuls_nicht_setzen();
  cl_assert(!ks_persist_exists(PERSIST_MAXPULS));
  prv_neustart();
  cl_assert(!kieselsport_maxpuls_gesetzt());
  cl_assert_equal_i(kieselsport_maxpuls(), 179);
}

void test_kieselsport__alte_190_ohne_vermerk_heisst_nicht_gesetzt(void) {
  s_alter = 41;
  // So hinterliessen Store-App, Konfigseite und Boulder bis 0.42 den Persist.
  ks_persist_write_int(PERSIST_MAXPULS, 190);
  prv_neustart();
  cl_assert(!kieselsport_maxpuls_gesetzt());
  cl_assert_equal_i(kieselsport_maxpuls(), 179);
  // Jeder andere alte Wert war eingestellt.
  ks_persist_write_int(PERSIST_MAXPULS, 185);
  prv_neustart();
  cl_assert(kieselsport_maxpuls_gesetzt());
  cl_assert_equal_i(kieselsport_maxpuls(), 185);
  // Schrieb eine aeltere Firmware nach "nicht gesetzt" einen Wert, gilt er.
  kieselsport_maxpuls_nicht_setzen();
  ks_persist_write_int(PERSIST_MAXPULS, 175);
  prv_neustart();
  cl_assert(kieselsport_maxpuls_gesetzt());
  cl_assert_equal_i(kieselsport_maxpuls(), 175);
}

void test_kieselsport__schaetzung_nach_alter_ohne_vermerk_bleibt_nach_alter(void) {
  // So hinterliess Boulder 0.43 bis 0.45 den Persist bei "From age": seine Schaetzung nach
  // Tanaka als Zahl.
  ks_persist_write_int(PERSIST_MAXPULS, 179);
  // Beim Start der Uhr sind die Gesundheitseinstellungen noch nicht geladen.
  s_alter = 30;
  prv_neustart();
  // Bis zur Entscheidung gilt die alte Zahl - und noch nichts im Persist.
  cl_assert_equal_i(kieselsport_maxpuls(), 179);
  cl_assert(!ks_persist_exists(PERSIST_MAXPULS_GESETZT));
  // Geladen; die App fragt beim Melden ans Telefon.
  s_alter = 41;
  cl_assert(!kieselsport_maxpuls_gesetzt());
  cl_assert(!ks_persist_exists(PERSIST_MAXPULS));
  // Festgehalten: mit dem Alter folgt die Uhr, auch nach einem Neustart.
  s_alter = 45;
  prv_neustart();
  cl_assert(!kieselsport_maxpuls_gesetzt());
  cl_assert_equal_i(kieselsport_maxpuls(), 177);  // 176,5
}

void test_kieselsport__eingestellte_zahl_ohne_vermerk_bleibt_eingestellt(void) {
  ks_persist_write_int(PERSIST_MAXPULS, 185);
  s_alter = 30;
  prv_neustart();
  s_alter = 41;
  // Ein Training braucht die Zonen: sein Start entscheidet.
  prv_starte(ArtLaufen, BEGINN);
  cl_assert(ks_persist_exists(PERSIST_MAXPULS_GESETZT));
  cl_assert_equal_i(ks_persist_read_int(PERSIST_MAXPULS_GESETZT), 1);
  prv_verwerfen();
  // Trifft Tanaka die Zahl spaeter zufaellig, bleibt sie eingestellt.
  s_alter = 33;  // 184,9
  prv_neustart();
  cl_assert(kieselsport_maxpuls_gesetzt());
  cl_assert_equal_i(kieselsport_maxpuls(), 185);
}

void test_kieselsport__zahl_ohne_vermerk_wie_bisher(void) {
  s_alter = 41;
  // Das Echo der eigenen Meldung aendert nichts: die Uhr bleibt beim Alter.
  kieselsport_maxpuls_alt(179);
  cl_assert(!kieselsport_maxpuls_gesetzt());
  s_alter = 42;
  cl_assert_equal_i(kieselsport_maxpuls(), 179);  // 178,6
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
  prv_pause();
  prv_sekunden_leer(sekunden);
  prv_pause();
}

static uint16_t prv_u16(const uint8_t *b) { return (uint16_t)(b[0] | (b[1] << 8)); }

void test_kieselsport__pausen_gehen_mit_der_zusammenfassung(void) {
  prv_starte(ArtBike, BEGINN);
  prv_sekunden(30);
  prv_pause_fuer(120);  // Kaffee
  prv_sekunden(40);
  prv_pause_fuer(5);  // Ampel
  prv_sekunden(20);
  // Gespeichert wird aus der Pause: dort endet die aktive Zeit.
  prv_pause();
  const time_t letzte_pause = rtc_get_time();
  prv_sekunden_leer(30);
  prv_speichern();

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
  // Pebbles Sitzung traegt dieselbe aktive Zeit (Regel 2.5.3), nur in ganzen Minuten.
  prv_sekunden_leer(30);
  cl_assert_equal_i(prv_sitzungen(), 1);
  cl_assert_equal_i(prv_sitzung(0)->length_min, 90 / 60);

  // Bestaetigt: alles weg, auch Pausen und Ende.
  kieselsport_wartend_vergessen(BEGINN);
  cl_assert_equal_i(kieselsport_wartend_pausen(b, sizeof(b)), 0);
  cl_assert_equal_i(kieselsport_wartend_ende(), 0);
}

void test_kieselsport__ohne_pausen_nur_kopf(void) {
  prv_starte(ArtLaufen, BEGINN);
  prv_sekunden(60);
  prv_speichern();
  uint8_t b[KS_PAUSEN_BYTES];
  // Der Kopf allein heisst: die Uhr kennt Pausen, es gab keine.
  cl_assert_equal_i(kieselsport_wartend_pausen(b, sizeof(b)), 2);
  cl_assert_equal_i(b[1], 0);
  // Ohne Pause gespeichert endet die aktive Zeit beim Speichern.
  cl_assert_equal_i(kieselsport_wartend_ende(), BEGINN + 60);
}

void test_kieselsport__mehr_pausen_als_platz(void) {
  prv_starte(ArtKraft, BEGINN);
  for (int i = 0; i < KS_PAUSEN_MAX + 1; i++) {
    prv_sekunden(2);
    prv_pause_fuer(3);
  }
  prv_sekunden(10);
  prv_speichern();
  uint8_t b[KS_PAUSEN_BYTES];
  // Die Liste ist voll, die Zahl zaehlt weiter - das Telefon sieht die Luecke.
  cl_assert_equal_i(kieselsport_wartend_pausen(b, sizeof(b)), KS_PAUSEN_BYTES);
  cl_assert_equal_i(b[1], KS_PAUSEN_MAX + 1);
}

void test_kieselsport__ein_neues_training_hat_keine_alten_pausen(void) {
  prv_starte(ArtLaufen, BEGINN);
  prv_sekunden(30);
  prv_pause_fuer(10);
  prv_sekunden(30);
  prv_speichern();
  prv_starte(ArtLaufen, BEGINN + 1000);
  prv_sekunden(60);
  prv_speichern();
  uint8_t b[KS_PAUSEN_BYTES];
  cl_assert_equal_i(kieselsport_wartend_pausen(b, sizeof(b)), 2);
}

void test_kieselsport__alte_pausen_gehen_nicht_mit_fremdem_stand(void) {
  prv_starte(ArtBike, BEGINN);
  prv_sekunden(30);
  prv_pause_fuer(60);
  prv_sekunden(30);
  prv_speichern();
  uint8_t b[KS_PAUSEN_BYTES];
  cl_assert_equal_i(kieselsport_wartend_pausen(b, sizeof(b)), 2 + 4);
  // Zurueckgespielte Firmware: sie schickt das Training, loescht nach der Bestaetigung nur 30-32
  // und legt ihr naechstes ohne Pausen ab.
  ks_persist_delete(PERSIST_WARTET);
  ks_persist_delete(PERSIST_WARTET_LISTE_A);
  ks_persist_delete(PERSIST_WARTET_LISTE_B);
  const Trainingsstand fremd = {.art = ArtLaufen, .beginn = BEGINN + 5000, .dauer_s = 600};
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
  prv_starte(ArtWandern, BEGINN);
  prv_sekunden(100);
  prv_pause();
  prv_verwerfen();
  uint32_t beginn = 0, zeit = 0;
  uint8_t art = 0;
  cl_assert(kieselsport_stopp_laden(&beginn, &zeit, &art));
  cl_assert_equal_i(beginn, BEGINN);
  cl_assert_equal_i(zeit, (uint32_t)rtc_get_time());
  cl_assert_equal_i(art, ArtWandern);
  // Ein spaet bestaetigter fremder Stopp loescht ihn nicht, der eigene schon.
  kieselsport_stopp_vergessen(BEGINN - 1);
  cl_assert(kieselsport_stopp_laden(&beginn, &zeit, &art));
  kieselsport_stopp_vergessen(BEGINN);
  cl_assert(!kieselsport_stopp_laden(&beginn, &zeit, &art));

  // Unter einer Minute gespeichert ist auch verworfen.
  prv_starte(ArtLaufen, BEGINN + 500);
  prv_sekunden(20);
  prv_speichern();
  cl_assert(kieselsport_stopp_laden(&beginn, &zeit, &art));
  cl_assert_equal_i(beginn, BEGINN + 500);
  // Ein neues Training ersetzt ihn - sein Start sagt dem Telefon alles.
  prv_starte(ArtLaufen, BEGINN + 900);
  cl_assert(!kieselsport_stopp_laden(&beginn, &zeit, &art));
  // Ein gespeichertes Training traegt den Stopp in der Zusammenfassung.
  prv_sekunden(60);
  prv_speichern();
  cl_assert(!kieselsport_stopp_laden(&beginn, &zeit, &art));
}

void test_kieselsport__nachsenden_auch_fuer_den_stopp(void) {
  prv_starte(ArtLaufen, BEGINN);
  prv_sekunden(30);
  prv_verwerfen();
  cl_assert(prv_minutentakt_an());
  prv_minute();
  prv_minute();
  cl_assert_equal_i(s_geholt, 1);
  // Angekommen: der Takt hoert auf.
  kieselsport_stopp_vergessen(BEGINN);
  prv_minute();
  cl_assert(!prv_minutentakt_an());
  // Auch nach einem Neustart der Uhr.
  prv_starte(ArtLaufen, BEGINN + 100);
  prv_sekunden(10);
  prv_verwerfen();
  memset(s_minuten_takte, 0, sizeof(s_minuten_takte));
  prv_neustart();
  cl_assert(prv_minutentakt_an());
}

// --- Bestaetigungen gehoeren zu ihrem Training ---

static void prv_training(uint32_t beginn, int sekunden) {
  prv_starte(ArtLaufen, beginn);
  prv_sekunden(sekunden);
  prv_speichern();
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

// DAS SPAETE ACK MITTEN IM SPEICHERN. Die App bestaetigt die Zusammenfassung von A, waehrend
// KernelBG gerade B speichert: Pausen und Ende von B stehen schon da, der wartende Stand noch
// nicht - er sagt noch A. Ohne die Sperre passte die Pruefung, und das Loeschen naehme die Pausen
// und das Ende von B mit; B ginge ohne sie ans Telefon.
static void prv_ack_a(void) {
  kieselsport_wartend_vergessen(BEGINN);
}

void test_kieselsport__spaetes_ack_mitten_im_speichern_loescht_nichts_von_b(void) {
  prv_training(BEGINN, 120);
  prv_starte(ArtLaufen, BEGINN + 1000);
  prv_sekunden(60);
  prv_pause_fuer(30);
  prv_sekunden(60);
  s_app_aktion = prv_ack_a;
  s_unterbrich_vor = PERSIST_WARTET;
  prv_speichern();
  cl_assert(s_app_aktion == NULL);  // die App kam dran
  Trainingsstand t;
  char liste[KS_LISTE_MAX];
  cl_assert(kieselsport_wartend_laden(&t, liste, sizeof(liste)));
  cl_assert_equal_i(t.beginn, BEGINN + 1000);
  uint8_t b[KS_PAUSEN_BYTES];
  cl_assert(kieselsport_wartend_pausen(b, sizeof(b)) > 2);
  cl_assert(kieselsport_wartend_ende() != 0);
}

// Dasselbe fuer die Kurve: die App bestaetigt das letzte Stueck der Kurve von A, waehrend
// KernelBG die Stuecke von B schon geschrieben hat, Anzahl und Beginn aber noch nicht. Ohne die
// Sperre loeschte kurve_vergessen die Stuecke von B, und deren Kurve haette eine Luecke - sie
// wuerde verworfen.
static void prv_kurven_ack_a(void) {
  kieselsport_kurve_bestaetigt(BEGINN, 0, 42);
}

void test_kieselsport__spaetes_kurven_ack_mitten_im_speichern_loescht_nichts_von_b(void) {
  prv_training(BEGINN, 410);
  cl_assert_equal_i(kieselsport_kurve_anzahl(), 42);
  prv_starte(ArtLaufen, BEGINN + 1000);
  prv_sekunden(300);
  s_app_aktion = prv_kurven_ack_a;
  s_unterbrich_vor = PERSIST_KURVE_ANZAHL;
  prv_speichern();
  cl_assert(s_app_aktion == NULL);
  cl_assert(kieselsport_kurve_wartet());
  cl_assert_equal_i(kieselsport_kurve_beginn(), BEGINN + 1000);
  cl_assert(!kieselsport_kurve_luecke());
  uint8_t stueck[300];
  cl_assert_equal_i(kieselsport_kurve_stueck(stueck, sizeof(stueck)), kieselsport_kurve_anzahl());
}

// DAS KURVENSTUECK MITTEN IM SPEICHERN. Die App liest das naechste Stueck der Kurve von A,
// waehrend KernelBG B speichert: die Werte von B stehen schon da, Anzahl, Beginn und Zeiger noch
// die von A. Einzeln gelesen ginge der Puls von B unter dem Beginn von A hinaus, und das Telefon
// truege ihn dort ein.
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
  prv_starte(ArtLaufen, BEGINN);
  for (int i = 0; i < 410; i++) {
    prv_sekunde_mit_puls(100);
  }
  prv_speichern();
  cl_assert_equal_i(kieselsport_kurve_anzahl(), 42);
  prv_starte(ArtLaufen, BEGINN + 1000);
  for (int i = 0; i < 300; i++) {
    prv_sekunde_mit_puls(150);
  }
  s_app_aktion = prv_kurve_lesen;
  s_unterbrich_vor = PERSIST_KURVE_ANZAHL;
  prv_speichern();
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
  // Der zweite Schluessel fehlt (ein gescheitertes Schreiben): ab Wert 200 laesst sich nichts
  // mehr schicken.
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
  prv_starte(ArtWandern, BEGINN);
  prv_sekunden(600);
  prv_pause();
  const time_t pause = rtc_get_time();
  prv_sekunden_leer(30 * 60 - 1);
  cl_assert_equal_i(prv_stand().zustand, LaufPause);
  cl_assert(workout_service_is_workout_ongoing());
  prv_sekunde();
  // Gespeichert wie mit der Taste: Verlauf (Pebbles Sitzung, sofort), Telefon.
  KsStand s = prv_stand();
  cl_assert_equal_i(s.zustand, LaufAus);
  cl_assert_equal_i(s.ende, KsEndeGespeichert);
  cl_assert(!workout_service_is_workout_ongoing());
  cl_assert_equal_i(prv_sitzungen(), 1);
  cl_assert_equal_i(prv_sitzung(0)->length_min, 10);
  cl_assert_equal_i(s_meldungen, 1);
  cl_assert(!prv_archiv_beschrieben());
  Trainingsstand t;
  char liste[KS_LISTE_MAX];
  cl_assert(kieselsport_wartend_laden(&t, liste, sizeof(liste)));
  cl_assert_equal_i(t.dauer_s, 600);
  // Die aktive Zeit endete, als die Pause begann.
  cl_assert_equal_i(kieselsport_wartend_ende(), (uint32_t)pause);
  // Sensoren aus, der Takt steht, und der Dienst holt die App zum Senden.
  cl_assert_equal_i(prv_sensor_takt(), 0);
  cl_assert(!prv_takt_an());
  cl_assert(prv_minutentakt_an());
}

void test_kieselsport__nur_eine_pause_am_stueck_zaehlt(void) {
  prv_starte(ArtWandern, BEGINN);
  prv_sekunden(120);
  // Zwanzig Minuten Pause, kurz weiter, wieder zwanzig: kein Ende.
  prv_pause();
  prv_sekunden_leer(20 * 60);
  prv_pause();
  prv_sekunden(10);
  prv_pause();
  prv_sekunden_leer(20 * 60);
  cl_assert_equal_i(prv_stand().zustand, LaufPause);
  // Auch Pause-Weiter-Pause zwischen zwei Takten faengt neu an.
  prv_pause();
  prv_pause();
  prv_sekunden_leer(29 * 60);
  cl_assert_equal_i(prv_stand().zustand, LaufPause);
  cl_assert_equal_i(prv_sitzungen(), 0);
  cl_assert_equal_i(s_meldungen, 0);
}

// --- Brummen nur bei eindeutigem Zonenwechsel ---

void test_kieselsport__zonen_brummen_erst_drei_schlaege_jenseits(void) {
  // Maximalpuls 190: Zone 3 beginnt bei 133.
  cl_assert_equal_i(kieselsport_zonengrenze(3), 133);
  prv_starte(ArtBike, 0);
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
  prv_starte(ArtSchwimmen, BEGINN);
  // Gut 18 Stunden in eine Richtung (vergessen am Beckenrand), dann eine Bahn zurueck und wieder
  // hin.
  const int lang = 65536 + 100;
  prv_schwimmen(lang, 0);
  prv_schwimmen(30, TRIG_MAX_ANGLE / 2);
  prv_schwimmen(30, 0);
  prv_speichern();
  Trainingsstand t;
  char liste[KS_LISTE_MAX];
  cl_assert(kieselsport_wartend_laden(&t, liste, sizeof(liste)));
  cl_assert_equal_i(t.bahnen, 3);
  // Die zweite Bahn begann bei der Wende nach der langen ersten - in vollen Sekunden, nicht
  // 65536 weniger.
  unsigned beginn[3] = {0}, anzahl = 0, dauer[3] = {0};
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
  prv_neustart();
  cl_assert_equal_i(kieselsport_becken(), 50);
  cl_assert_equal_i(kieselsport_empfindlichkeit(), 3);
  cl_assert_equal_i(kieselsport_pausenziel(), 60);
  cl_assert(!kieselsport_maxpuls_gesetzt());
}

// =============================================================================================
// Neu mit dem Weg 2 (Spezifikation Schritt 2, 2.5 und 6.1)

// T2.1: Ein gespeichertes Training steht genau einmal in Pebbles Sitzungsliste, mit Typ, Beginn
// der Uhr und manual; seine Meldung kommt einmal, 30 s nach dem Speichern (V4).
void test_kieselsport__speichern_eine_sitzung_nach_30s(void) {
  prv_starte(ArtBike, BEGINN);
  const time_t pebble_start = rtc_get_time();
  prv_sekunden(150);
  prv_pause();
  prv_sekunden_leer(20);
  prv_speichern();
  for (int i = 0; i < 29; i++) {
    prv_sekunde();
    cl_assert_equal_i(prv_sitzungen(), 0);
  }
  prv_sekunde();
  cl_assert_equal_i(prv_sitzungen(), 1);
  cl_assert_equal_i(s_meldungen, 1);
  cl_assert_equal_i(prv_sitzung(0)->type, ActivitySessionType_Open);
  cl_assert_equal_i(prv_sitzung(0)->start_utc, pebble_start);
  cl_assert(prv_sitzung(0)->manual);
  cl_assert(!prv_sitzung(0)->ongoing);
  cl_assert_equal_i(prv_sitzung(0)->length_min, 2);
  // Danach nichts mehr: kein zweiter Stopp, keine zweite Sitzung.
  prv_sekunden_leer(120);
  cl_assert_equal_i(prv_sitzungen(), 1);
  cl_assert_equal_i(s_meldungen, 1);
  // V8: 90 s nach dem Stopp geht das Datalogging einmal hinaus.
  cl_assert_equal_i(s_dls_schub, 1);
}

// V8 nur bei Platz (Schritt 1, Regel 2): Ist die KernelBG-Schlange knapp, faellt der Anstoss weg
// (dls_send_all_sessions reiht nicht verwerfbar ein); die Sitzung geht mit Pebbles eigenem
// Datalogging-Takt hinaus.
void test_kieselsport__datalogging_nur_bei_platz(void) {
  prv_training(BEGINN, 70);
  prv_sekunden_leer(30);
  cl_assert_equal_i(prv_sitzungen(), 1);
  system_task_set_available_space(BOULDER_SCHLANGE_FREI - 1);
  prv_sekunden_leer(90);
  cl_assert_equal_i(s_dls_schub, 0);
  system_task_set_available_space(100);
}

// T2.1, Start im 30-s-Fenster (V4, Regel 2.5.7): Pebble lehnte den zweiten Start ab; Kieselsport
// stoppt sein eigenes Training sofort - mit Sitzung - und startet neu. Jedes steht einmal da.
void test_kieselsport__start_im_30s_fenster(void) {
  prv_training(BEGINN, 90);
  prv_sekunden_leer(10);
  cl_assert_equal_i(prv_sitzungen(), 0);
  prv_starte(ArtLaufen, BEGINN + 1000);
  cl_assert_equal_i(prv_stand().zustand, LaufLaeuft);
  cl_assert_equal_i(prv_sitzungen(), 1);
  cl_assert_equal_i(s_meldungen, 1);
  cl_assert(workout_service_is_workout_ongoing());
  prv_sekunden(70);
  prv_speichern();
  prv_sekunden_leer(30);
  cl_assert_equal_i(prv_sitzungen(), 2);
  cl_assert(prv_sitzung(0)->start_utc != prv_sitzung(1)->start_utc);
  cl_assert_equal_i(s_meldungen, 2);
}

// V4 bei einem Zeitsprung zurueck im 30-s-Fenster: Pebbles Stopp kommt im naechsten Takt, nicht
// erst, wenn die Uhr die alte Zeit wieder erreicht.
void test_kieselsport__zeitsprung_zurueck_im_30s_fenster(void) {
  prv_training(BEGINN, 90);
  prv_sekunden_leer(5);
  fake_rtc_increment_time(-PBL_SEC_PER_HOUR);
  cl_assert_equal_i(prv_sitzungen(), 0);
  prv_sekunde();
  cl_assert(!workout_service_is_workout_ongoing());
  cl_assert_equal_i(prv_sitzungen(), 1);
  cl_assert_equal_i(s_meldungen, 1);
  cl_assert(!prv_takt_an());
}

// T3.1 (Persist-Teil zu AP1, F3 = A): Verwerfen nach mehr als 60 s - keine Sitzung, keine
// Meldung, Pebble gestoppt, der Stopp im Persist (35).
void test_kieselsport__verwerfen_ohne_sitzung_mit_stopp(void) {
  prv_starte(ArtLaufen, BEGINN);
  prv_sekunden(200);
  prv_pause();
  prv_verwerfen();
  cl_assert(!workout_service_is_workout_ongoing());
  cl_assert_equal_s(s_erkennung, "01");
  prv_sekunden_leer(120);
  cl_assert_equal_i(prv_sitzungen(), 0);
  cl_assert_equal_i(s_meldungen, 0);
  cl_assert(ks_persist_exists(PERSIST_STOPP));
  cl_assert(!ks_persist_exists(PERSIST_WARTET));
  // Danach speichert ein Training wieder.
  prv_training(BEGINN + 1000, 70);
  prv_sekunden_leer(30);
  cl_assert_equal_i(prv_sitzungen(), 1);
}

// Regel 2.5.7: Laeuft ein fremdes Training (Pebbles Workout-App), lehnt Pebble ab. Kieselsport
// laeuft nicht an und legt einen Stopp fuers Telefon ab, das Zustand 1 schon bekommen hat.
void test_kieselsport__fremdes_training_lehnt_ab(void) {
  stub_pebble_tasks_set_current(PebbleTask_App);
  cl_assert(workout_service_start_workout(ActivitySessionType_Walk));
  const uint8_t vorher = prv_stand().ende_nr;
  prv_starte(ArtLaufen, BEGINN + 7);
  KsStand s = prv_stand();
  cl_assert_equal_i(s.zustand, LaufAus);
  cl_assert_equal_i(s.ende, KsEndeVerworfen);
  cl_assert_equal_i(s.ende_nr, (uint8_t)(vorher + 1));
  uint32_t beginn = 0, zeit = 0;
  uint8_t art = 9;
  cl_assert(kieselsport_stopp_laden(&beginn, &zeit, &art));
  cl_assert_equal_i(beginn, BEGINN + 7);
  cl_assert_equal_i(art, ArtLaufen);
  cl_assert(prv_minutentakt_an());
  cl_assert_equal_i(prv_sensor_takt(), 0);
  // Pebbles Training laeuft unberuehrt weiter.
  ActivitySessionType typ;
  cl_assert(workout_service_get_current_workout_type(&typ));
  cl_assert_equal_i(typ, ActivitySessionType_Walk);
  cl_assert(workout_service_stop_workout());
}

// Regel 2.5.4: Pebble pausiert beim Laden selbst. Kieselsports Takt traegt die Pause bei Pebbles
// Sekunde ein; nach 30 min endet das Training von selbst, mit Pebbles Sitzung.
void test_kieselsport__lade_pause_von_pebble(void) {
  prv_starte(ArtBike, BEGINN);
  prv_sekunden(300);
  stub_pebble_tasks_set_current(PebbleTask_KernelMain);
  PebbleActivityEvent laden = {.type = PebbleActivityEvent_TrackingStopped};
  workout_service_activity_event_handler(&laden);
  stub_pebble_tasks_set_current(PebbleTask_App);
  const time_t pause = rtc_get_time();
  prv_sekunde();
  KsStand s = prv_stand();
  cl_assert_equal_i(s.zustand, LaufPause);
  cl_assert_equal_i(s.t.dauer_s, 300);
  cl_assert_equal_i(prv_sensor_takt(), 10);
  prv_sekunden_leer(30 * 60);
  s = prv_stand();
  cl_assert_equal_i(s.zustand, LaufAus);
  cl_assert_equal_i(s.ende, KsEndeGespeichert);
  cl_assert_equal_i(prv_sitzungen(), 1);
  cl_assert_equal_i(prv_sitzung(0)->length_min, 5);
  Trainingsstand t;
  char liste[KS_LISTE_MAX];
  cl_assert(kieselsport_wartend_laden(&t, liste, sizeof(liste)));
  cl_assert_equal_i(t.dauer_s, 300);
  // Die Pause beginnt bei Pebbles Sekunde 300, gesehen im naechsten Takt.
  cl_assert(kieselsport_wartend_ende() >= (uint32_t)pause &&
            kieselsport_wartend_ende() <= (uint32_t)pause + 1);
}

// Regel 2.5.3: Pebbles Timer laesst bei belegter Sperre einen Tick aus. Kieselsport verarbeitet
// die Differenz: keine Sekunde geht verloren oder doppelt, Kurve und Dauer folgen Pebble.
void test_kieselsport__ausgelassener_tick_von_pebble(void) {
  prv_starte(ArtLaufen, BEGINN);
  prv_sekunden(9);
  // Sekunde 10: Pebbles Timer (zuerst angemeldet) laesst den Tick aus (workout_service.c:
  // 292-300), Kieselsports Takt laeuft.
  RegularTimerInfo *pebble = s_sekunden_takte[0];
  cl_assert(pebble && pebble->cb == prv_workout_timer_cb);
  s_sekunden_takte[0] = NULL;
  prv_sekunde_mit_puls(140);
  s_sekunden_takte[0] = pebble;
  cl_assert_equal_i(prv_pebble_dauer(), 9);
  cl_assert_equal_i(prv_stand().t.dauer_s, 9);
  prv_sekunde_mit_puls(140);
  cl_assert_equal_i(prv_stand().t.dauer_s, 11);
  cl_assert_equal_i(prv_pebble_dauer(), 11);
  prv_sekunden(50);
  prv_speichern();
  Trainingsstand t;
  char liste[KS_LISTE_MAX];
  cl_assert(kieselsport_wartend_laden(&t, liste, sizeof(liste)));
  cl_assert_equal_i(t.dauer_s, 61);
  // Der Kurvenwert bei Sekunde 10 fehlt nicht.
  uint8_t stueck[8];
  cl_assert_equal_i(kieselsport_kurve_stueck(stueck, sizeof(stueck)), 7);
  cl_assert_equal_i(stueck[1], 140);
}

// Pebble beendet das Training selbst (Pebbles Workout-App, ihre Verlassen-Regel): Kieselsport
// legt seine Zusammenfassung zu Pebbles Sitzung und stoppt nicht ein zweites Mal.
void test_kieselsport__pebble_beendet_selbst(void) {
  prv_starte(ArtLaufen, BEGINN);
  prv_sekunden(100);
  cl_assert(workout_service_stop_workout());
  cl_assert_equal_i(prv_sitzungen(), 1);
  prv_sekunde();
  KsStand s = prv_stand();
  cl_assert_equal_i(s.zustand, LaufAus);
  cl_assert_equal_i(s.ende, KsEndeGespeichert);
  Trainingsstand t;
  char liste[KS_LISTE_MAX];
  cl_assert(kieselsport_wartend_laden(&t, liste, sizeof(liste)));
  cl_assert_equal_i(t.dauer_s, 100);
  prv_sekunden_leer(60);
  cl_assert_equal_i(prv_sitzungen(), 1);
  cl_assert_equal_i(s_meldungen, 1);
  cl_assert(!prv_takt_an());
}

// K5 bis in die Zusammenfassung (10005): Boulders Fahrt aus AP1 (test_kieselsport_kalorien,
// fahrt_in_der_sitzung, B: 156,17 kcal) ueber Kieselsport gestartet, mit Kieselsports
// Maximalpuls 190 (Alter 41: Tanaka 179, 190 liegt unter 179 + 25 und gilt).
void test_kieselsport__kcal_der_fahrt_in_zusammenfassung_und_sitzung(void) {
  static const uint8_t FAHRT[] = {
    95, 94, 126, 121, 120, 122, 121, 128, 130, 132, 128, 122,
    129, 125, 140, 139, 137, 140, 135, 125, 129, 125, 143, 145,
  };
  s_alter = 41;
  fake_ruhepuls_setzen(54);
  kieselsport_maxpuls_setze(190);
  prv_starte(ArtBike, BEGINN);
  for (unsigned i = 0; i < sizeof(FAHRT); i++) {
    prv_messwert(FAHRT[i], HRMQuality_Good);
    prv_sekunden_leer(i + 1 < sizeof(FAHRT) ? 60 : 10);
  }
  prv_pause();
  prv_sekunden_leer(30);
  prv_speichern();
  Trainingsstand t;
  char liste[KS_LISTE_MAX];
  cl_assert(kieselsport_wartend_laden(&t, liste, sizeof(liste)));
  cl_assert_equal_i(t.kcal, 156);
  prv_sekunden_leer(30);
  cl_assert_equal_i(prv_sitzungen(), 1);
  cl_assert_equal_i(prv_sitzung(0)->step_data.active_kcalories, 156);
  cl_assert_equal_i(s_meldung.step_data.active_kcalories, 156);
}

// V2: Boulders Pruefung des Maximalpulses fuer Zonen und kcal. Gemeldet (ans Telefon) wird
// weiter die Einstellung, gerechnet mit Tanaka, wenn sie nicht passt.
void test_kieselsport__maxpuls_nach_boulders_pruefung(void) {
  s_alter = 50;  // Tanaka 173, Spielraum bis 198
  kieselsport_maxpuls_setze(198);
  cl_assert_equal_i(kieselsport_zonengrenze(5), 178);  // 198 * 0,9
  kieselsport_maxpuls_setze(199);
  cl_assert_equal_i(kieselsport_maxpuls(), 199);
  cl_assert(kieselsport_maxpuls_gesetzt());
  cl_assert_equal_i(kieselsport_zonengrenze(5), 155);  // 173 * 0,9
  cl_assert_equal_i(prv_stand().maxpuls, 173);
  // Unten: hoechstens Ruhepuls + 20 laesst keine Reserve.
  fake_ruhepuls_setzen(90);
  prv_starte(ArtLaufen, BEGINN);  // der Start liest den Ruhepuls
  prv_verwerfen();
  kieselsport_maxpuls_setze(110);
  cl_assert_equal_i(kieselsport_zonengrenze(1), 86);  // 173 * 0,5
  kieselsport_maxpuls_setze(111);
  cl_assert_equal_i(kieselsport_zonengrenze(1), 55);  // 111 * 0,5
  // Ohne brauchbares Alter gibt es kein Tanaka: der eingestellte Wert gilt wie in Boulder.
  s_alter = 0;
  kieselsport_maxpuls_setze(230);
  cl_assert_equal_i(kieselsport_zonengrenze(5), 207);
}

// V2 fuer die kcal: dasselbe Training mit 230 (nicht plausibel bei 41: Tanaka 179) und mit 179
// eingestellt rechnet gleich.
static int32_t prv_kcal_mit(uint16_t maxpuls) {
  kieselsport_maxpuls_setze(maxpuls);
  prv_starte(ArtBike, BEGINN);
  for (int i = 0; i < 300; i++) {
    prv_sekunde_mit_puls(150);
  }
  prv_pause();
  prv_speichern();
  Trainingsstand t;
  char liste[KS_LISTE_MAX];
  cl_assert(kieselsport_wartend_laden(&t, liste, sizeof(liste)));
  kieselsport_wartend_vergessen(0);
  prv_sekunden_leer(30);
  return (int32_t)t.kcal;
}

void test_kieselsport__maxpuls_nach_boulders_pruefung_fuer_kcal(void) {
  s_alter = 41;
  fake_ruhepuls_setzen(54);
  const int32_t tanaka = prv_kcal_mit(179);
  const int32_t zu_hoch = prv_kcal_mit(230);
  const int32_t gilt = prv_kcal_mit(200);
  cl_assert(tanaka > 0);
  cl_assert_equal_i(zu_hoch, tanaka);
  cl_assert(gilt < tanaka);
}

// F2 = A: Kieselsports Abo - 1 s, in der Pause 10 s, sparsam 5 s, jede Minute erneuert mit 5 min
// Ablauf. Pebble legt keines an (cleanup).
void test_kieselsport__abo_takt_und_ablauf(void) {
  prv_starte(ArtLaufen, BEGINN);
  cl_assert_equal_i(prv_sensor_takt(), 1);
  cl_assert_equal_i(s_abo.ablauf, 5 * 60);
  cl_assert_equal_i(s_abo_anmeldungen, 1);
  // Nach Ablauf (der Manager hat es entfernt) meldet die naechste Minute neu an.
  s_abo.belegt = false;
  prv_sekunden(60);
  cl_assert_equal_i(prv_sensor_takt(), 1);
  cl_assert_equal_i(s_abo_anmeldungen, 2);
  s_akku = 15;
  prv_sekunden(60);
  cl_assert_equal_i(prv_sensor_takt(), 5);
  prv_pause();
  cl_assert_equal_i(prv_sensor_takt(), 10);
  prv_pause();
  cl_assert_equal_i(prv_sensor_takt(), 5);
  prv_verwerfen();
  cl_assert_equal_i(prv_sensor_takt(), 0);
}

// Regel 2.5.5: Init ohne Pebble-Haken - beliebig oft, einmal wirksam. Von jeder Task anstossen;
// gearbeitet wird erst auf KernelBG. (AP3: Woran das zu sehen ist, ist jetzt das Nachsenden - die
// Einstellungen laedt seither der erste Zugriff, siehe einstellungen_vor_init.)
void test_kieselsport__init_einmal(void) {
  kieselsport_maxpuls_setze(185);
  ks_persist_write_int(PERSIST_MAXPULS, 175);
  stub_pebble_tasks_set_current(PebbleTask_KernelBackground);
  kieselsport_init();
  stub_pebble_tasks_set_current(PebbleTask_App);
  cl_assert_equal_i(kieselsport_maxpuls(), 185);
  // Nach einem Neustart wartet ein Stopp aufs Telefon: init holt dafuer das Nachsenden an.
  stopp_merken(BEGINN, BEGINN + 60, ArtLaufen);
  memset(s_minuten_takte, 0, sizeof(s_minuten_takte));
  kieselsport_test_neustart();
  kieselsport_init_anstossen();
  cl_assert(!prv_minutentakt_an());
  prv_kernel_bg();
  cl_assert(prv_minutentakt_an());
  cl_assert_equal_i(kieselsport_maxpuls(), 175);
}

// AP3: Die App stoesst init an, und KernelBG kommt erst, wenn die App wartet (Regel 2.5.5).
// Vor dem Neustart eingestellt: 3, 60 s, 50 m; nach dem Neustart stehen im RAM die Vorgaben.
static void prv_neustart_mit_einstellungen(void) {
  kieselsport_empfindlichkeit_setze(2);
  kieselsport_pausenziel_setze(90);
  kieselsport_becken_setze(25);
  ks_persist_write_int(PERSIST_EMPFIND, 3);
  ks_persist_write_int(PERSIST_PAUSENZIEL, 60);
  ks_persist_write_int(PERSIST_BECKEN, 50);
  kieselsport_test_neustart();
  kieselsport_init_anstossen();
}

// Schickt das Telefon vorher Einstellungen, gelten sie trotzdem - auch die Rueckkehr zum
// Vorgabewert, die ein Vergleich mit dem RAM fuer "unveraendert" hielte. Jede Einstellung einmal
// als erster Zugriff nach dem Neustart.
void test_kieselsport__einstellungen_vor_init(void) {
  for (int fall = 0; fall < 3; fall++) {
    prv_neustart_mit_einstellungen();
    if (fall == 0) kieselsport_empfindlichkeit_setze(2);
    if (fall == 1) kieselsport_pausenziel_setze(90);
    if (fall == 2) kieselsport_becken_setze(25);
    prv_kernel_bg();
    if (fall == 0) {
      cl_assert_equal_i(kieselsport_empfindlichkeit(), 2);
      cl_assert_equal_i(ks_persist_read_int(PERSIST_EMPFIND), 2);
    }
    if (fall == 1) {
      cl_assert_equal_i(kieselsport_pausenziel(), 90);
      cl_assert_equal_i(ks_persist_read_int(PERSIST_PAUSENZIEL), 90);
    }
    if (fall == 2) {
      cl_assert_equal_i(kieselsport_becken(), 25);
      cl_assert_equal_i(ks_persist_read_int(PERSIST_BECKEN), 25);
    }
  }
}

// Das Echo ans Telefon vor init nennt die gespeicherten Werte, nicht die Vorgaben.
void test_kieselsport__echo_vor_init(void) {
  prv_neustart_mit_einstellungen();
  cl_assert_equal_i(kieselsport_empfindlichkeit(), 3);
  cl_assert_equal_i(kieselsport_pausenziel(), 60);
  cl_assert_equal_i(kieselsport_becken(), 50);
  prv_kernel_bg();
  cl_assert_equal_i(kieselsport_pausenziel(), 60);
}

// Kriterium 7, "Zeit je Callback" (AP7): Jeder Callback des Dienstes misst sich unter eigenem
// Namen - die Befehle einzeln, der Takt, das Nachsenden und der Pulsrueckruf, der unter der Sperre
// des HRM-Managers laeuft. `boulder zeiten` und der Diagnosesatz zeigen sie (takt.h).
static const BoulderAufgabe *prv_messung(const char *name) {
  BoulderAufgabe *const *liste;
  const int n = boulder_messung_liste(&liste);
  for (int i = 0; i < n; i++) {
    if (liste[i] && !strcmp(liste[i]->name, name)) return liste[i];
  }
  return NULL;
}

static uint32_t prv_laeufe(const char *name) {
  const BoulderAufgabe *a = prv_messung(name);
  return a ? a->laeufe : 0;
}

void test_kieselsport__messung_je_callback(void) {
  static const char *const s_namen[] = {
    "Kieselsport Start", "Kieselsport Pause", "Kieselsport Speichern", "Kieselsport Verwerfen",
    "Kieselsport Takt", "Kieselsport Puls", "Kieselsport Nachsenden",
  };
  uint32_t vorher[ARRAY_LENGTH(s_namen)];
  for (size_t i = 0; i < ARRAY_LENGTH(s_namen); i++) vorher[i] = prv_laeufe(s_namen[i]);
  prv_starte(ArtLaufen, BEGINN);
  prv_sekunden(70);
  prv_pause();
  prv_speichern();
  prv_minute();
  prv_starte(ArtKraft, BEGINN + 1000);
  prv_sekunden(10);
  prv_verwerfen();
  for (size_t i = 0; i < ARRAY_LENGTH(s_namen); i++) {
    cl_assert_(prv_laeufe(s_namen[i]) > vorher[i], s_namen[i]);
  }
  // Der Takt lief jede Sekunde, der Pulsrueckruf mit jedem Wert.
  cl_assert(prv_laeufe("Kieselsport Takt") >= vorher[4] + 80);
  cl_assert(prv_laeufe("Kieselsport Puls") >= vorher[5] + 80);
}

// =============================================================================================
// Nach Schritt 2 (Entscheide vom 08.10.2026)

// Entscheid 2 vom 08.10.2026: Die Variante streicht Schritte nur, solange ein Training ohne
// Schritte LAEUFT. Was sie damit zaehlt, ist Pebbles laufendes Training UND der Merker (kontext.c,
// boulder_kontext_training_ohne_schritte_laeuft); den Merker sieht dieser Test, Pebbles Training
// ist das echte. Die Wirkung auf Tagessumme, Minute und Rate prueft test_boulder_fahrt.
static bool prv_gestrichen(void) {
  return workout_service_is_workout_ongoing() && g_fake_ohne_schritte;
}

// Jede Meldung an die Variante: auf KernelBG und ohne Kieselsports Sperre (Regel 2.5.2).
static int s_merker_falsch_gemeldet;
static void prv_merker_pruefen(bool ohne) {
  s_merker_falsch_gemeldet += pebble_task_get_current() != PebbleTask_KernelBackground ||
                              pbl_mutex_is_owner(&s_kieselsport_sperre);
}

// Velo: laufend 0, Pause zaehlt, Weiter wieder 0, nach dem Speichern zaehlt - auch in den 30 s,
// in denen Pebbles Training noch pausiert dasteht (V4), und danach.
void test_kieselsport__schritte_zaehlen_in_der_pause(void) {
  s_merker_falsch_gemeldet = 0;
  g_fake_ohne_schritte_beim_aufruf = prv_merker_pruefen;
  prv_starte(ArtBike, BEGINN);
  cl_assert(prv_gestrichen());
  prv_sekunden(30);
  cl_assert(prv_gestrichen());
  prv_pause();
  cl_assert(!prv_gestrichen());
  prv_sekunden_leer(20);
  cl_assert(!prv_gestrichen());
  prv_pause();  // weiter
  cl_assert(prv_gestrichen());
  prv_sekunden(40);
  cl_assert(prv_gestrichen());
  prv_pause();
  prv_sekunden_leer(5);
  cl_assert(!prv_gestrichen());
  prv_speichern();
  for (int i = 0; i < 29; i++) {
    cl_assert(workout_service_is_workout_ongoing());
    cl_assert(!prv_gestrichen());
    prv_sekunde();
  }
  prv_sekunde();
  cl_assert(!workout_service_is_workout_ongoing());
  cl_assert(!prv_gestrichen());
  cl_assert_equal_i(prv_sitzungen(), 1);
  cl_assert_equal_i(s_merker_falsch_gemeldet, 0);
}

// Speichern ohne vorherige Pause ist nicht moeglich (die App bietet es nur in der Pause an), der
// Dienst nimmt es aber an - auch dann zaehlt es ab dem Speichern. Verwerfen ebenso.
void test_kieselsport__schritte_zaehlen_nach_speichern_und_verwerfen(void) {
  prv_starte(ArtKraft, BEGINN);
  prv_sekunden(70);
  cl_assert(prv_gestrichen());
  prv_speichern();
  cl_assert(workout_service_is_workout_ongoing());
  cl_assert(!prv_gestrichen());
  prv_sekunden_leer(30);
  cl_assert(!workout_service_is_workout_ongoing());

  prv_starte(ArtSchwimmen, BEGINN + 1000);
  prv_sekunden(70);
  cl_assert(prv_gestrichen());
  prv_verwerfen();
  cl_assert(!g_fake_ohne_schritte);
  cl_assert(!prv_gestrichen());
}

// Pebbles eigene Pause beim Laden (Regel 2.5.4) hebt das Streichen mit dem naechsten Takt auf; ein
// Weiter von anderswo (Pebbles Workout-App) setzt es wieder.
void test_kieselsport__schritte_in_pebbles_pause(void) {
  prv_starte(ArtYoga, BEGINN);
  prv_sekunden(60);
  cl_assert(prv_gestrichen());
  stub_pebble_tasks_set_current(PebbleTask_KernelMain);
  PebbleActivityEvent laden = {.type = PebbleActivityEvent_TrackingStopped};
  workout_service_activity_event_handler(&laden);
  stub_pebble_tasks_set_current(PebbleTask_App);
  prv_sekunde();
  cl_assert_equal_i(prv_stand().zustand, LaufPause);
  cl_assert(!prv_gestrichen());
  cl_assert(workout_service_pause_workout(false));
  prv_sekunde();
  cl_assert_equal_i(prv_stand().zustand, LaufLaeuft);
  cl_assert(prv_gestrichen());
}

// Laufen und Wandern zaehlen Schritte wie bisher: weder laufend noch in der Pause gestrichen.
void test_kieselsport__laufen_und_wandern_zaehlen_immer(void) {
  const Sportart arten[] = {ArtLaufen, ArtWandern};
  for (size_t i = 0; i < ARRAY_LENGTH(arten); i++) {
    prv_starte(arten[i], BEGINN + i * 1000);
    cl_assert(!prv_gestrichen());
    prv_sekunden(40);
    prv_pause();
    cl_assert(!prv_gestrichen());
    prv_pause();
    cl_assert(!prv_gestrichen());
    prv_sekunden(30);
    prv_pause();
    prv_speichern();
    cl_assert(!prv_gestrichen());
    prv_sekunden_leer(30);
  }
}

// P5 (Entscheid 3 vom 08.10.2026): Mit dem Start steht Kieselsport ohne Ablauf oben im Startmenue,
// mit dem Ende (gespeichert, verworfen, von selbst) nur noch Pebbles Frist lang. Die Kennung kommt
// aus der UUID (hier 4242).
void test_kieselsport__startmenue_oben_im_training(void) {
  prv_starte(ArtLaufen, BEGINN);
  cl_assert_equal_i(s_oben_aufrufe, 1);
  cl_assert_equal_i(s_oben_id, 4242);
  cl_assert(s_oben_ohne_ablauf);
  prv_sekunden(70);
  prv_pause();
  prv_pause();
  cl_assert(s_oben_ohne_ablauf);
  prv_speichern();
  cl_assert(!s_oben_ohne_ablauf);
  int aufrufe = s_oben_aufrufe;
  prv_sekunden_leer(30);
  cl_assert_equal_i(s_oben_aufrufe, aufrufe);

  prv_starte(ArtBike, BEGINN + 1000);
  cl_assert(s_oben_ohne_ablauf);
  prv_verwerfen();
  cl_assert_equal_i(s_oben_aufrufe, aufrufe + 2);
  cl_assert(!s_oben_ohne_ablauf);

  // Von selbst nach 30 min Pause: bis dahin oben, danach mit Ablauf.
  prv_starte(ArtWandern, BEGINN + 2000);
  cl_assert(s_oben_ohne_ablauf);
  prv_sekunden(70);
  prv_pause();
  prv_sekunden_leer(30 * 60 - 1);
  cl_assert_equal_i(prv_stand().zustand, LaufPause);
  cl_assert(s_oben_ohne_ablauf);
  prv_sekunden_leer(1);
  cl_assert_equal_i(prv_stand().zustand, LaufAus);
  cl_assert(!s_oben_ohne_ablauf);
  aufrufe = s_oben_aufrufe;
  prv_sekunden_leer(120);
  cl_assert_equal_i(s_oben_aufrufe, aufrufe);
}

// Jede AppMessage der App setzt Pebbles Frist an Kieselsports Stelle (app_message_sender.c,
// app_message_receiver.c: mit Ablauf). Der Dienst markiert jede Minute wieder ohne Ablauf, auch in
// der Pause - sonst rutschte Kieselsport fuenf Minuten nach der letzten Nachricht mitten im
// Training ab. Nach dem Ende nicht mehr.
void test_kieselsport__startmenue_oben_trotz_nachrichten(void) {
  prv_starte(ArtBike, BEGINN);
  cl_assert(s_oben_ohne_ablauf);
  for (int runde = 0; runde < 2; runde++) {
    app_install_mark_prioritized(4242, true /* wie eine gesendete Zustandsmeldung */);
    cl_assert(!s_oben_ohne_ablauf);
    prv_sekunden(60);
    cl_assert(s_oben_ohne_ablauf);
    cl_assert_equal_i(s_oben_id, 4242);
    prv_pause();
  }
  // Nach dem zweiten Umschalten laeuft es wieder; Pause, Speichern, und die Minuten danach.
  prv_pause();
  prv_speichern();
  cl_assert(!s_oben_ohne_ablauf);
  const int aufrufe = s_oben_aufrufe;
  prv_sekunden_leer(120);
  cl_assert(!s_oben_ohne_ablauf);
  cl_assert_equal_i(s_oben_aufrufe, aufrufe);
}

// Lehnt Pebble den Start ab (ein fremdes Training), rueckt Kieselsport nicht nach oben.
void test_kieselsport__startmenue_nicht_oben_ohne_start(void) {
  stub_pebble_tasks_set_current(PebbleTask_App);
  cl_assert(workout_service_start_workout(ActivitySessionType_Walk));
  prv_starte(ArtLaufen, BEGINN + 7);
  cl_assert_equal_i(prv_stand().zustand, LaufAus);
  cl_assert_equal_i(s_oben_aufrufe, 0);
  cl_assert(workout_service_stop_workout());
}
