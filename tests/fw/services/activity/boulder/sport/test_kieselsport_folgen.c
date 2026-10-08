/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kieselsport_umfeld.h"

#include "services/activity/boulder/sport/puls.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

void test_kieselsport_folgen__initialize(void) {
  umfeld_initialize();
}

void test_kieselsport_folgen__cleanup(void) {
  umfeld_cleanup();
}

// =============================================================================================
// T7.3 (Spezifikation Schritt 2, 6.1; Kriterium 7): Zufallsfolgen
//
// 1000 Folgen aus Start, Pause, Weiter, Speichern, Verwerfen, Laden (Pebble pausiert selbst),
// Abstecken, Auto-Ende (30 min Pause), Neustart (auch mitten im Training und im 30-s-Fenster)
// und Zeitsprung (vor und zurueck), dazwischen Sekunden mit Puls, ohne Puls und "nicht am Arm".
// Jede Folge laeuft gegen Pebbles echten Workout-Dienst und die echte Sitzungsliste, im Umfeld
// von test_kieselsport (kieselsport_umfeld.h, mit der Sperrfolge T7.2 je Folge). Rot bei einem Assert, einem Haenger oder einer doppelten Sitzung. Geprueft wird nach JEDER
// Sekunde und am Ende jeder Folge:
// - HAENGER: Pebbles Training steht laenger als 31 s ohne Kieselsports (V4), Kieselsport laeuft
//   ohne Pebbles Training, sein Sekundentakt laeuft ohne beide, das Puls-Abo bleibt nach dem Ende
//   (T7.4) oder steht in der Pause nicht auf 10 s, der Persist bleibt offen.
// - EINE UHR (Regel 2.5.3, 2.5.4): Kieselsports Pause ist Pebbles Pause, seine Dauer ist Pebbles
//   Dauer - ausser nach einem Zeitsprung zurueck im laufenden Training: Dann wird Pebbles Dauer
//   kuerzer, und Kieselsport wartet, bis sie aufholt.
// - SITZUNGEN: Pebble meldet jedes gespeicherte Training genau einmal, ein verworfenes und eines,
//   das ein Neustart verloren hat, nie, und sonst nichts. Bekannt und ausgenommen: Ein Neustart
//   im 30-s-Fenster kostet Pebbles Sitzung (V4); nach einem Zeitsprung zurueck kann Pebbles
//   Dauer unter 60 s liegen, dann speichert Pebble nicht.
// Die Folgen sind fest (eigener Zufallsgenerator mit Startwert je Folge); eine rote nennt Folge,
// Schritt und Ereignis.

#define FOLGEN 1000
#define FOLGE_TRAININGS_MAX 48

typedef struct {
  time_t pebble_beginn;  //< Pebbles start_utc: die Uhrzeit, als Kieselsport Pebble startete
  KsEnde ende;           //< Keins: laeuft noch oder ein Neustart hat es verloren
  bool v4_verloren;      //< Neustart, waehrend Pebbles Stopp noch ausstand
  bool zurueck;          //< Zeitsprung zurueck, waehrend Pebbles Training lief
} FolgeTraining;

typedef struct {
  int nr;
  int schritt;
  const char *was;
  uint32_t zufall;
  FolgeTraining t[FOLGE_TRAININGS_MAX];
  int t_anzahl;
  int aktuell;          //< das Training, das Kieselsport fuehrt, sonst -1
  int v4;               //< das gespeicherte, dessen Stopp bei Pebble aussteht, sonst -1
  uint8_t ende_nr;
  bool am_kabel;
  int ohne_ks_s;
  uint32_t sekunden;
  uint8_t akku;
} FolgeZustand;
static FolgeZustand s_folge;

// WAS DIE FOLGEN ABDECKEN, ueber alle gezaehlt: Jeder Fall aus der Spezifikation muss oft
// vorkommen, sonst prueft der Test ihn nicht wirklich (Pruefung am Ende).
static struct {
  int auto_ende;          //< Trainings, die das Auto-Ende gespeichert hat
  int neustart_training;  //< Neustarts mitten im Training
  int neustart_v4;        //< Neustarts im 30-s-Fenster vor Pebbles Stopp
  int start_v4;           //< Starts im 30-s-Fenster (Regel 2.5.7)
  int laden_training;     //< Laden im laufenden Training (Pebbles Pause, Regel 2.5.4)
  int zurueck_training;   //< Zeitsprung zurueck im laufenden Training
  int vor_training;       //< Zeitsprung vor im Training
  int unter_minute;       //< Speichern unter 60 s, also verworfen
  int verworfen;          //< Verwerfen nach 60 s und mehr
} s_abdeckung;

static void prv_folge_fehler(const char *fmt, ...) {
  char text[200];
  va_list args;
  va_start(args, fmt);
  vsnprintf(text, sizeof(text), fmt, args);
  va_end(args);
  char meldung[320];
  snprintf(meldung, sizeof(meldung), "Folge %d, Schritt %d (%s): %s", s_folge.nr,
           s_folge.schritt, s_folge.was, text);
  cl_fail(meldung);
}

#define FOLGE_PRUEFE(bed, ...)       \
  do {                               \
    if (!(bed)) {                    \
      prv_folge_fehler(__VA_ARGS__); \
    }                                \
  } while (0)

static uint32_t prv_zufall(uint32_t n) {
  // xorshift32: fest je Startwert, unabhaengig von der Bibliothek des Rechners.
  uint32_t x = s_folge.zufall;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  s_folge.zufall = x;
  return x % n;
}

// Kieselsports Sekundentakt (nicht Pebbles Timer) ist angemeldet.
static bool prv_ks_takt_an(void) {
  for (int i = 0; i < 4; i++) {
    if (s_sekunden_takte[i] && s_sekunden_takte[i]->cb != prv_workout_timer_cb) return true;
  }
  return false;
}

// Ein Ende, das der Dienst gemeldet hat, dem Training zuordnen.
static void prv_folge_enden_sehen(void) {
  const KsStand s = prv_stand();
  if (s.ende_nr != s_folge.ende_nr) {
    s_folge.ende_nr = s.ende_nr;
    if (s_folge.aktuell >= 0) {
      s_folge.t[s_folge.aktuell].ende = s.ende;
      if (s.ende == KsEndeGespeichert && workout_service_is_workout_ongoing()) {
        s_folge.v4 = s_folge.aktuell;
      }
      s_folge.aktuell = -1;
    }
  }
  if (s_folge.v4 >= 0 && !workout_service_is_workout_ongoing()) {
    s_folge.v4 = -1;
  }
}

static void prv_folge_invarianten(void) {
  const KsStand s = prv_stand();
  const bool pebble = workout_service_is_workout_ongoing();
  FOLGE_PRUEFE(s.zustand == LaufAus || pebble, "Kieselsport laeuft ohne Pebbles Training");
  if (s.zustand != LaufAus) {
    FOLGE_PRUEFE((s.zustand == LaufPause) == workout_service_is_paused(),
                 "Pause: Kieselsport %d, Pebble %d", s.zustand, workout_service_is_paused());
    if (s_folge.aktuell >= 0 && !s_folge.t[s_folge.aktuell].zurueck) {
      FOLGE_PRUEFE((int32_t)s.t.dauer_s == prv_pebble_dauer(), "Dauer: Kieselsport %u, Pebble %d",
                   (unsigned)s.t.dauer_s, (int)prv_pebble_dauer());
    }
    const uint32_t takt =
        (s.zustand == LaufPause) ? 10 : (s_folge.akku < KS_AKKU_SPARSAM ? 5 : 1);
    FOLGE_PRUEFE(prv_sensor_takt() == takt, "Abo-Takt %u statt %u", (unsigned)prv_sensor_takt(),
                 (unsigned)takt);
  } else {
    FOLGE_PRUEFE(prv_sensor_takt() == 0, "Abo mit %u s nach dem Ende",
                 (unsigned)prv_sensor_takt());
    FOLGE_PRUEFE(!prv_ks_takt_an() || pebble, "Sekundentakt ohne Training");
    FOLGE_PRUEFE(s_offen == 0, "Persist offen (%d)", s_offen);
  }
  // Entscheid 2 vom 08.10.2026: Die Variante streicht Schritte genau dann, wenn Kieselsport ein
  // Training ohne Schritte LAUFEN hat - nicht in der Pause (auch Pebbles Lade-Pause), nicht im
  // 30-s-Fenster nach dem Speichern, nie ohne Kieselsport. Gestrichen wird nur in Pebbles Training
  // mit dem Merker (kontext.c). Die Arten nach V1 (entscheidungen.md), nicht aus art.c gelesen.
  static const bool s_ohne[ArtAnzahl] = {
    [ArtLaufen] = false, [ArtBike] = true,  [ArtWandern] = false, [ArtKraft] = true,
    [ArtMTB] = true,     [ArtYoga] = true,  [ArtSchwimmen] = true,
  };
  const bool gestrichen = pebble && g_fake_ohne_schritte;
  const bool soll = s.zustand == LaufLaeuft && s.art < ArtAnzahl && s_ohne[s.art];
  FOLGE_PRUEFE(gestrichen == soll, "Schritte gestrichen %d, soll %d (Zustand %d, Art %d)",
               gestrichen, soll, s.zustand, s.art);
  s_folge.ohne_ks_s = (pebble && s.zustand == LaufAus) ? s_folge.ohne_ks_s + 1 : 0;
  FOLGE_PRUEFE(s_folge.ohne_ks_s <= 31, "Pebbles Training steht seit %d s ohne Kieselsport",
               s_folge.ohne_ks_s);
}

typedef enum {
  MitPuls,
  OhnePuls,
  NichtAmArm,
} FolgePuls;

// Eine Sekunde wie oben (prv_sekunde), mit dem, was Activity am Arm misst - am Kabel nichts:
// Activity zeichnet nicht auf, und der Sensor ist aus (hrm_manager.c, prv_can_turn_sensor_on).
static void prv_folge_sekunde(FolgePuls puls, uint8_t bpm) {
  if (!s_folge.am_kabel) {
    if (puls == MitPuls) {
      // Schritte alle fuenf Sekunden wie Activitys Meldungen in Gruppen - jede Meldung schreibt
      // zwei Logzeilen, und die Folgen haben hunderttausende Sekunden.
      if (s_folge.sekunden % 5 == 0) prv_schritte(10);
      prv_messwert(bpm, HRMQuality_Good);
    } else if (puls == NichtAmArm) {
      prv_messwert(0, HRMQuality_OffWrist);
    }
  }
  prv_sekunde();
  if (++s_folge.sekunden % 60 == 0) {
    prv_minute();
  }
  prv_folge_enden_sehen();
  prv_folge_invarianten();
}

// Ein Neustart: weg ist alles im RAM - Zeitgeber, KernelBG-Schlange, die Abos im HRM-Manager,
// Pebbles Training und Kieselsports (kieselsport_test_neustart). Persist und Sitzungsliste
// bleiben, wie im Flash.
static void prv_folge_neustart(void) {
  if (s_folge.aktuell >= 0) {
    s_abdeckung.neustart_training++;
    s_folge.aktuell = -1;
  }
  if (s_folge.v4 >= 0) {
    s_abdeckung.neustart_v4++;
    s_folge.t[s_folge.v4].v4_verloren = true;
    s_folge.v4 = -1;
  }
  fake_system_task_callbacks_cleanup();
  memset(s_sekunden_takte, 0, sizeof(s_sekunden_takte));
  memset(s_minuten_takte, 0, sizeof(s_minuten_takte));
  for (int i = 0; i < 4; i++) s_wecker[i].laeuft = false;
  memset(&s_abo, 0, sizeof(s_abo));
  s_offen = 0;
  g_fake_ohne_schritte = false;
  workout_service_reset();
  workout_service_init();
  // Das Training im RAM verfaellt auf KernelBG (seine Sensoren gehoeren dorthin, plattform.c).
  stub_pebble_tasks_set_current(PebbleTask_KernelBackground);
  prv_neustart();
  s_folge.ende_nr = prv_stand().ende_nr;
}

static void prv_folge_start(void) {
  const bool lief = kieselsport_laeuft();
  const Sportart art = (Sportart)prv_zufall(ArtAnzahl);
  const uint32_t beginn = prv_zufall(2) ? (uint32_t)rtc_get_time() : 0;
  prv_starte(art, beginn);
  if (!lief && kieselsport_laeuft()) {
    // Ein Stopp aus dem 30-s-Fenster ist mit dem Start erledigt (Regel 2.5.7).
    s_abdeckung.start_v4 += s_folge.v4 >= 0;
    s_folge.v4 = -1;
    cl_assert(s_folge.t_anzahl < FOLGE_TRAININGS_MAX);
    s_folge.t[s_folge.t_anzahl] = (FolgeTraining){.pebble_beginn = rtc_get_time()};
    s_folge.aktuell = s_folge.t_anzahl++;
  }
  prv_folge_enden_sehen();
}

static void prv_folge_warten(void) {
  const int n = 1 + (int)prv_zufall(600);
  const uint32_t art = prv_zufall(20);
  const FolgePuls puls = (art < 15) ? MitPuls : (art < 18 ? OhnePuls : NichtAmArm);
  for (int i = 0; i < n; i++) {
    prv_folge_sekunde(puls, (uint8_t)(60 + prv_zufall(136)));
  }
}

static void prv_folge_schritt(void) {
  const uint32_t w = prv_zufall(100);
  if (w < 14) {
    s_folge.was = "Start";
    prv_folge_start();
  } else if (w < 24) {
    s_folge.was = "Pause";
    if (prv_stand().zustand == LaufLaeuft) prv_pause();
  } else if (w < 34) {
    s_folge.was = "Weiter";
    if (prv_stand().zustand == LaufPause) prv_pause();
  } else if (w < 44) {
    s_folge.was = "Speichern";
    const int t = s_folge.aktuell;
    prv_speichern();
    prv_folge_enden_sehen();
    s_abdeckung.unter_minute += t >= 0 && s_folge.t[t].ende == KsEndeVerworfen;
  } else if (w < 48) {
    s_folge.was = "Verwerfen";
    s_abdeckung.verworfen += s_folge.aktuell >= 0 && prv_stand().t.dauer_s >= 60;
    prv_verwerfen();
  } else if (w < 54) {
    // Am Kabel stoppt Activity, und Pebble pausiert sein Training (workout_service.c:333-339).
    // Abstecken setzt es nicht fort.
    s_folge.am_kabel = !s_folge.am_kabel;
    s_folge.was = s_folge.am_kabel ? "Laden" : "Abstecken";
    if (s_folge.am_kabel) {
      s_abdeckung.laden_training += s_folge.aktuell >= 0 && !workout_service_is_paused();
      stub_pebble_tasks_set_current(PebbleTask_KernelMain);
      PebbleActivityEvent e = {.type = PebbleActivityEvent_TrackingStopped};
      workout_service_activity_event_handler(&e);
      stub_pebble_tasks_set_current(PebbleTask_App);
    }
  } else if (w < 57) {
    s_folge.was = "Auto-Ende";
    const int t = s_folge.aktuell;
    if (prv_stand().zustand == LaufLaeuft) prv_pause();
    for (int i = 0; i < 30 * 60 + 2; i++) prv_folge_sekunde(OhnePuls, 0);
    s_abdeckung.auto_ende += t >= 0 && s_folge.t[t].ende == KsEndeGespeichert;
  } else if (w < 62) {
    s_folge.was = "Neustart";
    prv_folge_neustart();
  } else if (w < 67) {
    static const int s_spruenge[] = {PBL_SEC_PER_HOUR, -PBL_SEC_PER_HOUR, 45, -45,
                                     PBL_SEC_PER_DAY, -10 * PBL_SEC_PER_MIN};
    const int sprung = s_spruenge[prv_zufall(ARRAY_LENGTH(s_spruenge))];
    s_folge.was = (sprung > 0) ? "Zeitsprung vor" : "Zeitsprung zurueck";
    if (sprung < 0 && s_folge.aktuell >= 0 && !workout_service_is_paused()) {
      s_folge.t[s_folge.aktuell].zurueck = true;
      s_abdeckung.zurueck_training++;
    }
    s_abdeckung.vor_training += sprung > 0 && s_folge.aktuell >= 0;
    fake_rtc_increment_time(sprung);
  } else {
    s_folge.was = "Warten";
    prv_folge_warten();
  }
}

// Am Ende: Jedes Training endet, nach 150 s steht nichts mehr, und die Sitzungen stimmen.
static void prv_folge_abschluss(void) {
  s_folge.was = "Abschluss";
  s_folge.am_kabel = false;
  if (kieselsport_laeuft()) {
    if (prv_zufall(2)) {
      prv_speichern();
    } else {
      prv_verwerfen();
    }
    prv_folge_enden_sehen();
  }
  for (int i = 0; i < 150; i++) prv_folge_sekunde(OhnePuls, 0);
  FOLGE_PRUEFE(!kieselsport_laeuft(), "Kieselsport laeuft noch");
  FOLGE_PRUEFE(!workout_service_is_workout_ongoing(), "Pebbles Training laeuft noch");
  FOLGE_PRUEFE(!prv_ks_takt_an(), "Sekundentakt laeuft noch");
  FOLGE_PRUEFE(prv_sensor_takt() == 0, "Abo besteht noch");

  // Je Beginn: so viele Meldungen, wie die Trainings mit diesem Beginn erwarten lassen. Ein
  // Beginn kann sich wiederholen (ein Neustart im selben Augenblick, ein Zeitsprung zurueck).
  cl_assert(s_meldungen <= GEMELDET_MAX);
  const char *spur = getenv("FOLGE_SPUR");
  if (spur && atoi(spur) == s_folge.nr) {
    for (int i = 0; i < s_folge.t_anzahl; i++) {
      printf("\n  Training %d: Pebble ab %ld, Ende %d, V4 verloren %d, zurueck %d", i,
             (long)s_folge.t[i].pebble_beginn, s_folge.t[i].ende, s_folge.t[i].v4_verloren,
             s_folge.t[i].zurueck);
    }
  }
  int erklaert = 0;
  for (int i = 0; i < s_folge.t_anzahl; i++) {
    const time_t beginn = s_folge.t[i].pebble_beginn;
    bool schon = false;
    for (int j = 0; j < i; j++) schon |= s_folge.t[j].pebble_beginn == beginn;
    if (schon) continue;
    int min = 0, max = 0;
    for (int j = i; j < s_folge.t_anzahl; j++) {
      const FolgeTraining *t = &s_folge.t[j];
      if (t->pebble_beginn != beginn || t->ende != KsEndeGespeichert || t->v4_verloren) continue;
      max++;
      min += !t->zurueck;
    }
    int gemeldet = 0;
    for (int m = 0; m < s_meldungen; m++) gemeldet += s_gemeldet_beginn[m] == beginn;
    FOLGE_PRUEFE(gemeldet >= min && gemeldet <= max,
                 "Training ab %ld: %d Meldungen, erwartet %d bis %d", (long)beginn, gemeldet, min,
                 max);
    erklaert += gemeldet;
  }
  FOLGE_PRUEFE(erklaert == s_meldungen, "%d Meldungen, %d gehoeren zu keinem Training",
               s_meldungen, s_meldungen - erklaert);
  FOLGE_PRUEFE(prv_sitzungen() <= s_meldungen, "%d Sitzungen bei %d Meldungen", prv_sitzungen(),
               s_meldungen);
}

// FOLGE_SPUR=<nr> in der Umgebung druckt die Schritte dieser Folge (zum Nachvollziehen einer roten).
static void prv_folge_spur(void) {
  const char *spur = getenv("FOLGE_SPUR");
  if (!spur || atoi(spur) != s_folge.nr) return;
  const KsStand s = prv_stand();
  printf("\n  %2d %-18s t=%ld ks=%d dauer=%u ende=%d/%u pebble=%d pause=%d dauer=%d meld=%d",
         s_folge.schritt, s_folge.was, (long)rtc_get_time(), s.zustand, (unsigned)s.t.dauer_s,
         s.ende, s.ende_nr, workout_service_is_workout_ongoing(), workout_service_is_paused(),
         (int)prv_pebble_dauer(), s_meldungen);
}

static uint32_t s_folgen_sekunden;  //< alle Folgen zusammen

static void prv_folge(int nr, int *trainings, int *gespeichert) {
  umfeld_initialize();
  s_folge = (FolgeZustand){.nr = nr, .zufall = 0x9E3779B9u ^ (uint32_t)(nr * 2654435761u),
                              .aktuell = -1, .v4 = -1, .was = "Beginn"};
  s_folge.akku = prv_zufall(10) ? 80 : 15;
  s_akku = s_folge.akku;
  memset(s_gemeldet_beginn, 0, sizeof(s_gemeldet_beginn));
  s_folge.ende_nr = prv_stand().ende_nr;
  const int schritte = 10 + (int)prv_zufall(21);
  for (s_folge.schritt = 0; s_folge.schritt < schritte; s_folge.schritt++) {
    prv_folge_schritt();
    // Ein Ende, das ein Befehl ausgeloest hat, gehoert noch zu diesem Training - bevor der naechste
    // Schritt ein neues startet oder die Uhr verstellt.
    prv_folge_enden_sehen();
    prv_folge_spur();
  }
  prv_folge_abschluss();
  s_folgen_sekunden += s_folge.sekunden;
  *trainings += s_folge.t_anzahl;
  for (int i = 0; i < s_folge.t_anzahl; i++) *gespeichert += s_folge.t[i].ende == KsEndeGespeichert;
  umfeld_cleanup();
}

void test_kieselsport_folgen__t7_3_zufallsfolgen(void) {
  int trainings = 0, gespeichert = 0;
  s_folgen_sekunden = 0;
  memset(&s_abdeckung, 0, sizeof(s_abdeckung));
  for (int nr = 0; nr < FOLGEN; nr++) {
    prv_folge(nr, &trainings, &gespeichert);
  }
  printf("\nT7.3: %d Folgen, %u h, %d Trainings, davon %d gespeichert. Auto-Ende %d, Neustart im "
         "Training %d, im 30-s-Fenster %d, Start im 30-s-Fenster %d, Laden %d, Zeitsprung zurueck "
         "%d, vor %d, unter 60 s %d, verworfen %d\n",
         FOLGEN, (unsigned)(s_folgen_sekunden / PBL_SEC_PER_HOUR), trainings, gespeichert,
         s_abdeckung.auto_ende, s_abdeckung.neustart_training, s_abdeckung.neustart_v4,
         s_abdeckung.start_v4, s_abdeckung.laden_training, s_abdeckung.zurueck_training,
         s_abdeckung.vor_training, s_abdeckung.unter_minute, s_abdeckung.verworfen);
  // Die Folgen sollen wirklich trainieren und speichern, nicht nur Befehle ins Leere geben, und
  // jeden Fall der Spezifikation oft treffen.
  cl_assert(trainings > FOLGEN);
  cl_assert(gespeichert > FOLGEN / 3);
  cl_assert(s_abdeckung.auto_ende >= 20);
  cl_assert(s_abdeckung.neustart_training >= 20);
  cl_assert(s_abdeckung.neustart_v4 >= 3);
  cl_assert(s_abdeckung.start_v4 >= 3);
  cl_assert(s_abdeckung.laden_training >= 20);
  cl_assert(s_abdeckung.zurueck_training >= 20);
  cl_assert(s_abdeckung.vor_training >= 20);
  cl_assert(s_abdeckung.unter_minute >= 20);
  cl_assert(s_abdeckung.verworfen >= 20);
  umfeld_initialize();
}
