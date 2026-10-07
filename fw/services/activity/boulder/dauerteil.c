/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2: der Dauerteil an der Naht zur Kraepelin-Kopie (variante.h), Stand AP5.
//
// STAND AP3: Schlaf - jede Minute in Nachtfelder, Live-Automat und Zyklen (nacht.c), der
// Takt "Nachlauf" im eigenen Callback, der gestaffelte Abschluss je Minute, der
// Live-Zustand fuer Pebble (D6). Noch neutral: keine Fahrt (AP6: kontext.c,
// nachfrage.c), kein Ring nach dem Boot (AP4: speicher.c), kein Abo "Aktiv" (AP6), kein
// Training in "gueltig" (AP6). Den Takt als eigenen Cron (Luecken, Ruhezeiten) bringt AP4.
//
// STAND AP5: Messstufe, das Puls-Abo (Nachtpuls, HRV-Fenster) und der SpO2-Ring haengen
// am Minutenteil; ihre Arbeit (Lesen, Setter) laeuft in eigenen Callbacks. AP4 kann den
// Anstoss in den eigenen Takt verlegen.
//
// STAND AP7: Jede Minute geht auch an die Weckregel (wecker.c); "Tracking aus" macht sie
// "nicht bereit".
//
// STAND AP4: Der erste init nach dem Boot fuellt den Eingang der Nacht aus Pebbles
// Minutendatei (D14) und reiht "Laden" ein (speicher.c); bis dahin gehen die Minuten nur in
// den Eingang. Der eigene Takt (Cron, Sekunde 10) traegt fehlende Minuten als ungueltig ein -
// auch am Kabel, wenn kein Minutenteil laeuft, wird die Nacht so fertig -, fuehrt das
// Protokoll, stoesst die Ablage an und gibt den Schub ans Telefon.
//
// STAND AP6: Velo und Auto (kontext.c) - jede Minute geht an die Kandidatin, die Nachfrage
// laeuft auf KernelMain (nachfrage.c), ihre Antwort holt der Takt ab, falls ihr Callback
// wegfiel. Die Naht D3-D5, D10 und D15 fragt kontext.c. Waehrend eines Trainings sind die
// Minuten fuer die Nacht ungueltig (2.6).

#include "pbl/services/activity/boulder/variante.h"

#include "pbl/cron/cron.h"
#include <pbl/drivers/rtc.h>
#include "pbl/services/activity/boulder/ausgabe.h"
#include "pbl/services/activity/boulder/diagnose.h"
#include "pbl/services/activity/boulder/eingang.h"
#include "pbl/services/activity/boulder/kontext.h"
#include "pbl/services/activity/boulder/live.h"
#include "pbl/services/activity/boulder/nachlauf.h"
#include "pbl/services/activity/boulder/speicher.h"
#include "pbl/services/activity/boulder/messstufe.h"
#include "pbl/services/activity/boulder/nacht.h"
#include "pbl/services/activity/boulder/puls.h"
#include "pbl/services/activity/boulder/schlafanalyse.h"
#include "pbl/services/activity/boulder/spo2.h"
#include "pbl/services/activity/boulder/takt.h"
#include "pbl/services/activity/boulder/umzug.h"
#include "pbl/services/activity/boulder/wecker.h"
#include "pbl/services/activity/boulder/wecklauf.h"
#include "pbl/services/activity/boulder/zyklen.h"
#ifdef CONFIG_BOULDER_KIESELSPORT
#include "pbl/services/activity/boulder/sport/kieselsport.h"
#endif
#include "pbl/services/battery/battery_state.h"
#include "pbl/services/time.h"
#include "pbl/util/units.h"
#include <pbl/logging/logging.h>

PBL_LOG_MODULE_DECLARE(service_activity, CONFIG_SERVICE_ACTIVITY_LOG_LEVEL);

static time_t s_letzte_minute_utc;

#if defined(CONFIG_BOULDER_PRUEFLAUF)
// Pruefbau (prueflauf.h): waehrend eines Laufs zaehlen nur seine Minuten und sein Takt.
static bool s_prueflauf;
static bool s_aus_prueflauf;
#define PRUEFLAUF_AKTIV() (s_prueflauf)
#define PRUEFLAUF_SPERRT() (s_prueflauf && !s_aus_prueflauf)
#else
#define PRUEFLAUF_AKTIV() false
#define PRUEFLAUF_SPERRT() false
#endif

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
  BOULDER_AUFGABE_LAEUFT(&s_aufgabe_nachlauf);
  boulder_nacht_takt();
}

// ---------------------------------------------------------------------------------------
// Start, Laden, Takt (AP4)

static bool s_gestartet;    //< der erste init seit dem Boot war da
static bool s_laden_offen;  //< "Laden" steht noch aus
static bool s_test_ohne_cron;  //< Tests, die Pebbles Timer selbst treiben (test_boulder_wecker)

static void prv_takt_cb(void *data);
static BoulderAufgabe s_aufgabe_takt = BOULDER_AUFGABE("Boulder Takt", prv_takt_cb);

static void prv_takt_cb(void *data) {
  BOULDER_AUFGABE_LAEUFT(&s_aufgabe_takt);
  if (PRUEFLAUF_SPERRT()) {
    return;
  }
  boulder_dauerteil_takt(rtc_get_time());
}

// Auf NewTimers: nur einreihen (verwerfbar). Faellt der Takt weg, holt der naechste nach.
static void prv_takt_cron(struct pbl_cron_job *job, void *data) {
  boulder_aufgabe_einreihen(&s_aufgabe_takt);
  pbl_cron_job_schedule(job);
}

// SEKUNDE 10: nach Pebbles Minutenlauf (Sekunde 0), damit eine eben fertige Minute nicht als
// Luecke gilt.
static struct pbl_cron_job s_takt_job = {
  .minute = PBL_CRON_MINUTE_ANY,
  .hour = PBL_CRON_HOUR_ANY,
  .mday = PBL_CRON_MDAY_ANY,
  .month = PBL_CRON_MONTH_ANY,
  .offset_seconds = 10,
  .cb = prv_takt_cron,
};

static void prv_laden_cb(void *data);
static BoulderAufgabe s_aufgabe_laden = BOULDER_AUFGABE("Boulder Laden", prv_laden_cb);

// "Laden", einmal je Boot nach dem ersten init. Plant auch den Takt: der Cron-Dienst laeuft
// dann sicher, und init bleibt leicht.
static void prv_laden_cb(void *data) {
  BOULDER_AUFGABE_LAEUFT(&s_aufgabe_laden);
  if (!s_laden_offen) {
    return;
  }
  s_laden_offen = false;
  boulder_speicher_laden();
#ifdef CONFIG_BOULDER_KIESELSPORT
  // KIESELSPORT OHNE PEBBLE-HAKEN (Schritt 2, Regel 2.5.5): Der erste Takt der Variante startet
  // den Dienst, damit Liegengebliebenes nach einem Neustart auch ohne App-Start ans Telefon geht.
  kieselsport_init();
#endif
  if (!s_test_ohne_cron) {
    pbl_cron_job_schedule(&s_takt_job);
  }
}

void boulder_dauerteil_takt(time_t jetzt) {
  boulder_kontext_takt(jetzt);
  boulder_puls_takt();
  if (s_laden_offen) {
    boulder_aufgabe_einreihen(&s_aufgabe_laden);
    return;
  }
  // Im Pruefbau ist `jetzt` die Zeit der Pruefnacht: der Tagessatz folgt nur der echten Uhr.
  if (!PRUEFLAUF_AKTIV()) {
    boulder_diagnose_takt(jetzt);
  }
  const time_t minute = jetzt - (jetzt % PBL_SEC_PER_MIN);
  // LUECKEN: Eine Minute, die zwei Minuten nach ihrem Ende keinen Satz hat (am Kabel, Activity
  // aus, Uhr gestellt), ist ungueltig - der Eingang traegt alle uebersprungenen so ein, wenn
  // die juengste kommt. Ein spaeter Satz dazu wird verworfen (der erste gilt).
  const time_t luecke = minute - 3 * PBL_SEC_PER_MIN;
  if (!s_letzte_minute_utc || luecke > s_letzte_minute_utc) {
    const time_t beginn = prv_fenster(luecke);
    const int index = (int)((luecke - beginn) / PBL_SEC_PER_MIN);
    if (boulder_nacht_minute(beginn, index, 0, 0, false)) {
      boulder_aufgabe_einreihen(&s_aufgabe_nachlauf);
    }
  }
  // Das Protokoll: alle 15 Minuten ab Fensterbeginn Akku und Messstufe.
  const time_t fenster = prv_fenster(minute);
  const int t = (int)((minute - fenster) / PBL_SEC_PER_MIN);
  if (t % AUSGABE_PROTOKOLL_TAKT == 0) {
    const BatteryChargeState akku = battery_get_charge_state();
    uint16_t auf;
    const bool schlaf = boulder_nacht_stufe() != NachtErledigt &&
                        boulder_nachlauf_lauf_aufwachen(&auf);
    boulder_ausgabe_protokoll(fenster, t / AUSGABE_PROTOKOLL_TAKT, akku.charge_percent,
                              boulder_messstufe(), akku.is_plugged, schlaf);
  }
  boulder_ausgabe_takt(jetzt);
  if (t % 15 == 3) {
    boulder_speicher_anstossen(false);
  }
}

// Nur gemessen: der Anteil der Variante am Minutenteil (Ziel 2 ms, Spezifikation 7.1).
static BoulderAufgabe s_messung_minute = BOULDER_AUFGABE("Boulder Minute Variante", NULL);

void boulder_dauerteil_minute(const BoulderMinute *minute) {
  BOULDER_AUFGABE_LAEUFT(&s_messung_minute);
  if (PRUEFLAUF_SPERRT()) {
    return;
  }
  s_letzte_minute_utc = minute->utc;
  boulder_wecker_minute(minute->utc, minute->vmc);
  // Lebt Pebbles Lauf noch? Nur ein Merker hier, die Arbeit auf KernelBG (wecklauf.h).
  boulder_wecklauf_minute();
  const time_t beginn = prv_fenster(minute->utc);
  const int index = (int)((minute->utc - beginn) / PBL_SEC_PER_MIN);
  // Ein Training ist kein Schlaf, auch wenn es ruhig ist (Yoga, Kraft).
  const bool gueltig = !minute->nicht_getragen && !boulder_kontext_training_laeuft();
  if (boulder_nacht_minute(beginn, index, minute->vmc, minute->puls, gueltig)) {
    boulder_aufgabe_einreihen(&s_aufgabe_nachlauf);
  }
  boulder_zyklen_abschluss_schritt(!boulder_live_schlaeft());
  boulder_kontext_minute(minute, boulder_nacht_ruhepuls());
  // Messung (AP5): die laufende Minute beginnt jetzt. "Nachtruhe" (5.2): im Nachtfenster
  // und die eben fertige Minute ruhig - wer um sieben herumlaeuft, misst mit dem Tagestakt.
  const int naechste = index + 1;
  const bool im_nachtfenster = naechste < NACHT_MINUTEN || naechste >= NACHT_FENSTER_MINUTEN;
  boulder_spo2_merken(minute->utc, minute->spo2);
  boulder_messstufe_minute();
  boulder_puls_minute(minute->utc + PBL_SEC_PER_MIN,
                      im_nachtfenster && minute->vmc <= SCHLAF_SCHWELLE_VMC);
}

void boulder_dauerteil_hrv_fenster(time_t beginn_utc, uint16_t rmssd, uint8_t puls) {
  const time_t fenster = prv_fenster(beginn_utc);
  const int minute = (int)((beginn_utc - fenster) / PBL_SEC_PER_MIN);
  if (!boulder_nacht_hrv(fenster, minute, rmssd, puls)) {
    PBL_LOG_DBG("Boulder: HRV-Fenster Minute %d gehoert zu keiner laufenden Nacht", minute);
  }
}

// Der erste init seit dem Boot (D14): den Eingang des laufenden Nachtfensters
// aus der Minutendatei fuellen lassen, "Laden" einreihen. init bleibt leicht: kein Lesen der
// eigenen Datei hier (Spezifikation 2.4).
bool boulder_dauerteil_ring_fenster(time_t *von_utc, time_t *bis_utc) {
  if (s_gestartet) {
    return false;
  }
  s_gestartet = true;
  const time_t jetzt = rtc_get_time();
  const time_t beginn = prv_fenster(jetzt);
  boulder_nacht_ring_neu(beginn);
  s_laden_offen = true;
  boulder_aufgabe_einreihen(&s_aufgabe_laden);
  const time_t nacht_ende = beginn + (time_t)NACHT_MINUTEN * PBL_SEC_PER_MIN;
  const time_t bis = jetzt - (jetzt % PBL_SEC_PER_MIN);
  *von_utc = beginn;
  *bis_utc = (bis < nacht_ende) ? bis : nacht_ende;
  return *bis_utc > *von_utc;
}

void boulder_dauerteil_ring_minute(const BoulderMinute *minute) {
  const time_t beginn = prv_fenster(minute->utc);
  boulder_nacht_ring_minute(beginn, (int)((minute->utc - beginn) / PBL_SEC_PER_MIN),
                            minute->vmc, minute->puls, !minute->nicht_getragen);
}

time_t boulder_dauerteil_fahrt_offen_seit(void) {
  return boulder_kontext_offen_seit();
}

bool boulder_dauerteil_fahrt_minute(time_t minute_utc) {
  return boulder_kontext_fahrt_minute(minute_utc);
}

bool boulder_dauerteil_fahrt_laeuft(void) {
  return boulder_kontext_fahrt_laeuft();
}

bool boulder_dauerteil_fahrt_ueberdeckt(time_t start_utc, uint32_t laenge_s) {
  return boulder_kontext_ueberdeckt(start_utc, laenge_s);
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
  return boulder_kontext_aktiv();
}

void boulder_dauerteil_aktiv_abo_pausieren(bool pausiert) {
  boulder_puls_aktiv_pausieren(pausiert);
}

void boulder_dauerteil_tracking_aus(void) {
  boulder_kontext_tracking_aus();
  boulder_wecker_vergessen();
  boulder_puls_tracking_aus();
}

void boulder_dauerteil_training(bool laeuft) {
  boulder_kontext_training(laeuft);
}

void boulder_dauerteil_sitzungen_beenden(void) {
  boulder_kontext_sitzung_beenden();
}

void boulder_dauerteil_test_neustart(void) {
  s_letzte_minute_utc = 0;
  s_gestartet = false;
  s_laden_offen = false;
  s_aufgabe_nachlauf.steht_an = false;
  s_aufgabe_laden.steht_an = false;
  s_aufgabe_takt.steht_an = false;
  boulder_nacht_zuruecksetzen();
  boulder_kontext_test_zuruecksetzen();
  boulder_wecker_vergessen();
  boulder_ausgabe_test_neustart();
  boulder_speicher_test_neustart();
  boulder_umzug_test_neustart();
}

void boulder_dauerteil_test_ohne_cron(bool ohne) {
  s_test_ohne_cron = ohne;
}

#if defined(CONFIG_BOULDER_PRUEFLAUF)
#include "pbl/services/activity/boulder/prueflauf.h"

void boulder_dauerteil_prueflauf(bool laeuft) {
  s_prueflauf = laeuft;
}

void boulder_dauerteil_prueflauf_minute(const BoulderMinute *minute) {
  s_aus_prueflauf = true;
  boulder_dauerteil_minute(minute);
  s_aus_prueflauf = false;
}

time_t boulder_dauerteil_fenster(time_t utc) {
  return prv_fenster(utc);
}
#endif
