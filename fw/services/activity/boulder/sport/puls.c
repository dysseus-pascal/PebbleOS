/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "puls.h"

#include "schluessel.h"

// Die alte Vorgabe. Neben Tanaka gilt sie nur bei einem unsinnigen Alter:
// ohne Abgleich steht in den Gesundheitseinstellungen der Uhr 30, nicht 0.
#define MAXPULS_VORGABE 190
// Was als Maximalpuls durchgeht; alles andere ist ein Tippfehler.
#define MAXPULS_MIN 101
#define MAXPULS_MAX 239

// Sekunden zwischen zwei Messungen waehrend eines Trainings.
//
// EINE SEKUNDE, ALSO DAUERND. Mit fuenf Sekunden liess der Sensor am Lenker
// und am Hang den Wert stehen, und die Zone stimmte nie. Eine Sekunde ist
// der Takt der eingebauten Workout-App - und ein Training ist die eine
// Stunde am Tag, in der der Akku dafuer da ist.
#define TAKT_TRAINING_S 1
#define TAKT_SPARSAM_S 5
// In der Pause: oft genug, dass die Anzeige lebt, selten genug, dass eine
// lange Pause nicht kostet wie ein Training.
#define TAKT_PAUSE_S 10

// Ausserhalb dieser Grenzen ist ein Wert ein Messfehler, kein Herz.
#define BPM_MIN 30
#define BPM_MAX 230

// DER ANGEZEIGTE PULS IST DER MEDIAN DER LETZTEN DREI, solange der Sensor
// seiner Messung nicht traut. Bewegung macht Spitzen, und eine Spitze sollte
// keine Zone wechseln und nicht brummen. Traut er ihr (guete >= 3), gilt der
// rohe Wert - sonst hinge die Anzeige einem Zwischensprint hinterher. So
// macht es auch die Workout-App der Firmware.
#define GLAETTEN_N 3
#define GUETE_GUT 3

// 0 heisst: nicht gesetzt - dann gilt Tanaka (prv_tanaka).
static uint16_t s_maximum;
// Fuer Boulders Pruefung (V2): der Ruhepuls der Variante, 0 = keiner.
static uint8_t s_ruhepuls;
// Eine Zahl aelterer Firmware ohne Vermerk, noch nicht entschieden (siehe
// prv_klaeren). 0 = nichts offen.
static uint16_t s_offen;
static bool s_sparsam;

static const uint8_t s_prozent[KS_ZONEN] = { 50, 60, 70, 80, 90 };

// --- Der Sensor ---

static bool s_beobachtet;
static bool s_dicht;
static bool s_hrv_an;
// Das Abo, wie es sein soll, und ob der Dienst es (wieder) stellen muss.
static bool s_sensor_faellig;
static uint16_t s_sensor_takt;
static bool s_sensor_hrv;

static uint16_t s_bpm;
static time_t s_frisch_seit;
static uint8_t s_ring[GLAETTEN_N];
static uint8_t s_ring_anzahl;
static uint8_t s_ring_kopf;

static void prv_sensor(void) {
  // Nur vormerken: gestellt wird nach der Sperre (puls_sensor_faellig).
  s_sensor_faellig = true;
  if (!s_beobachtet) {
    s_sensor_takt = 0;
    s_sensor_hrv = false;
    return;
  }
  s_sensor_takt = !s_dicht ? TAKT_PAUSE_S : (s_sparsam ? TAKT_SPARSAM_S : TAKT_TRAINING_S);
  s_sensor_hrv = s_hrv_an;
}

bool puls_sensor_faellig(uint16_t *takt_s, bool *hrv) {
  if (!s_sensor_faellig) {
    return false;
  }
  s_sensor_faellig = false;
  *takt_s = s_sensor_takt;
  *hrv = s_sensor_hrv;
  return true;
}

void puls_setze_sparsam(bool sparsam) {
  if (sparsam == s_sparsam) {
    return;
  }
  s_sparsam = sparsam;
  prv_sensor();
}

bool puls_sparsam(void) { return s_sparsam; }

static bool prv_plausibel(int32_t schlaege) {
  return schlaege >= MAXPULS_MIN && schlaege <= MAXPULS_MAX;
}

void puls_init(void) {
  s_maximum = 0;
  s_offen = 0;
  if (!ks_persist_exists(PERSIST_MAXPULS)) {
    return;
  }
  const int32_t gelesen = ks_persist_read_int(PERSIST_MAXPULS);
  if (!prv_plausibel(gelesen)) {
    return;
  }
  const bool vermerkt = ks_persist_exists(PERSIST_MAXPULS_GESETZT) &&
                        ks_persist_read_int(PERSIST_MAXPULS_GESETZT) == 1;
  if (vermerkt) {
    s_maximum = (uint16_t)gelesen;
    return;
  }
  // OHNE VERMERK ENTSCHEIDET ERST DER ERSTE GEBRAUCH (prv_klaeren): beim
  // Start der Uhr laeuft dieser Dienst vor dem Laden der
  // Gesundheitseinstellungen, das Alter waere noch die Vorgabe 30.
  s_offen = (uint16_t)gelesen;
}

// TANAKA, 208 - 0,7 x Alter, gerundet wie Boulder (Herzgrenzen.tanaka) -
// in Zehnteln gerechnet, damit beide dieselbe Zahl sehen.
static uint16_t prv_tanaka(void) {
  const uint8_t alter = ks_alter();
  if (alter < 10 || alter > 100) {
    return MAXPULS_VORGABE;
  }
  return (uint16_t)((2080 - 7 * (int)alter + 5) / 10);
}

// WAS MEINTE DIE ZAHL OHNE VERMERK? 190 war die Vorgabe (Store-App,
// Konfigseite, Boulder bis 0.42), und bei "From age" schickte Boulder 0.43
// bis 0.45 seine Schaetzung nach Tanaka - beides heisst "nicht gesetzt".
// Jede andere Zahl war eingestellt. Gefragt wird erst, wenn die Zahl
// gebraucht wird (Training, Meldung ans Telefon, neue Einstellung); dann
// stimmt das Alter. Die Antwort kommt in den Persist, damit sie nicht
// kippt, wenn sich das Alter spaeter aendert.
static void prv_klaeren(void) {
  if (!s_offen) {
    return;
  }
  const uint16_t alt = s_offen;
  s_offen = 0;
  if (alt == MAXPULS_VORGABE || alt == prv_tanaka()) {
    puls_maximum_nicht_setzen();
  } else {
    puls_setze_maximum(alt);
  }
}

uint16_t puls_maximum(void) {
  if (s_maximum) {
    return s_maximum;
  }
  // Noch offen: bis zur Entscheidung die alte Zahl, wie bisher - nur die
  // alte Vorgabe nicht, die heisst in jedem Fall "nach Alter".
  if (s_offen && s_offen != MAXPULS_VORGABE) {
    return s_offen;
  }
  return prv_tanaka();
}

// BOULDERS PRUEFUNG (V2, Herzgrenzen.kt:37-47): Ein eingestellter Wert ueber Tanaka + 25 ist
// eher ein Tippfehler als ein Herz, einer bis Ruhepuls + 20 laesst keine Reserve. Dann rechnet
// die Uhr wie Boulder mit Tanaka - sonst brummte sie nach anderen Zonen, als Boulder zeigt, und
// ihre kcal wichen ab (Befund 7). Ohne brauchbares Alter gibt es kein Tanaka, dann gilt der
// eingestellte Wert wie in Boulder. Ohne Ruhepuls 60 (Boulder: ruhe ?: RUHEPULS_ERSATZ).
#define TANAKA_SPIELRAUM 25
#define RESERVE_MIN 20
#define RUHEPULS_ERSATZ 60

static bool prv_alter_brauchbar(void) {
  const uint8_t alter = ks_alter();
  return alter >= 10 && alter <= 100;
}

uint16_t puls_maximum_wirksam(void) {
  const uint16_t gemeldet = puls_maximum();
  // Nicht gesetzt (oder noch offen und die alte Vorgabe): puls_maximum ist schon Tanaka.
  const bool eingestellt = s_maximum || (s_offen && s_offen != MAXPULS_VORGABE);
  if (!eingestellt || !prv_alter_brauchbar()) {
    return gemeldet;
  }
  const int ruhe = s_ruhepuls ? s_ruhepuls : RUHEPULS_ERSATZ;
  const uint16_t tanaka = prv_tanaka();
  if (gemeldet > ruhe + RESERVE_MIN && gemeldet <= tanaka + TANAKA_SPIELRAUM) {
    return gemeldet;
  }
  return tanaka;
}

void puls_setze_ruhepuls(uint8_t bpm) {
  s_ruhepuls = bpm;
}

bool puls_maximum_gesetzt(void) {
  prv_klaeren();
  return s_maximum != 0;
}

void puls_setze_maximum(uint16_t schlaege) {
  // Steht derselbe Wert schon fest, schreibt das Echo des Telefons nichts.
  // Nicht bei offener Zahl: dann fehlt der Vermerk im Persist noch.
  if (!prv_plausibel(schlaege) || (s_maximum == schlaege && !s_offen)) {
    return;
  }
  s_offen = 0;
  s_maximum = schlaege;
  ks_persist_write_int(PERSIST_MAXPULS, schlaege);
  ks_persist_write_int(PERSIST_MAXPULS_GESETZT, 1);
}

void puls_maximum_nicht_setzen(void) {
  // Schon "nach Alter" und so im Persist vermerkt: nichts zu schreiben.
  if (!s_maximum && !s_offen && !ks_persist_exists(PERSIST_MAXPULS) &&
      ks_persist_exists(PERSIST_MAXPULS_GESETZT) && ks_persist_read_int(PERSIST_MAXPULS_GESETZT) == 0) {
    return;
  }
  s_offen = 0;
  s_maximum = 0;
  // LOESCHEN, NICHT 190 SCHREIBEN: eine aeltere Firmware nach einem
  // Zurueckspielen liest dann ihre Vorgabe, und ein Wert, den sie spaeter
  // schreibt, wird hier wieder gelesen (siehe prv_klaeren).
  ks_persist_delete(PERSIST_MAXPULS);
  ks_persist_write_int(PERSIST_MAXPULS_GESETZT, 0);
}

void puls_maximum_alt(uint16_t schlaege) {
  prv_klaeren();
  // Das Echo der eigenen Meldung: ein Telefon ohne "nicht gesetzt" schickt
  // beim Speichern aller Einstellungen zurueck, was die Uhr gemeldet hat.
  if (!s_maximum && schlaege == prv_tanaka()) {
    return;
  }
  puls_setze_maximum(schlaege);
}

uint16_t puls_zonengrenze(int zone) {
  if (zone < 1 || zone > KS_ZONEN) {
    return 0;
  }
  return (uint16_t)((uint32_t)puls_maximum_wirksam() * s_prozent[zone - 1] / 100);
}

int puls_zone(uint16_t bpm) {
  if (bpm == 0) {
    return 0;
  }
  for (int z = KS_ZONEN; z >= 1; z--) {
    if (bpm >= puls_zonengrenze(z)) {
      return z;
    }
  }
  return 0;
}

// --- HRV ---
//
// RMSSD = Wurzel aus dem Mittel der quadrierten Differenzen aufeinander
// folgender Intervalle, laufend gezaehlt, die Wurzel erst am Ende. Ein
// Intervall, das um mehr als ein Drittel vom vorigen abweicht, ist ein
// verpasster Schlag, kein Herz, und faellt weg.
static uint16_t s_hrv_vorher;
static uint32_t s_hrv_summe;
static uint16_t s_hrv_anzahl;

static void prv_hrv_intervall(uint16_t ppi) {
  if (ppi < 300 || ppi > 2000) {
    return;
  }
  if (s_hrv_vorher != 0) {
    const int d = (int)ppi - (int)s_hrv_vorher;
    const int grenze = (int)s_hrv_vorher / 3;
    if (d > -grenze && d < grenze && s_hrv_summe < 0xF0000000u) {
      s_hrv_summe += (uint32_t)(d * d);
      if (s_hrv_anzahl < 0xFFFF) {
        s_hrv_anzahl++;
      }
    }
  }
  s_hrv_vorher = ppi;
}

static uint32_t prv_wurzel(uint32_t x) {
  uint32_t r = 0;
  uint32_t bit = 1u << 30;
  while (bit > x) {
    bit >>= 2;
  }
  while (bit) {
    if (x >= r + bit) {
      x -= r + bit;
      r = (r >> 1) + bit;
    } else {
      r >>= 1;
    }
    bit >>= 2;
  }
  return r;
}

void puls_hrv_start(void) {
  s_hrv_vorher = 0;
  s_hrv_summe = 0;
  s_hrv_anzahl = 0;
  s_hrv_an = true;
  prv_sensor();
}

void puls_hrv_stop(void) {
  if (!s_hrv_an) {
    return;
  }
  s_hrv_an = false;
  prv_sensor();
}

uint16_t puls_hrv_rmssd(void) {
  // Unter dreissig Intervallen ist es Rauschen, keine Messung.
  if (s_hrv_anzahl < 30) {
    return 0;
  }
  return (uint16_t)prv_wurzel(s_hrv_summe / s_hrv_anzahl);
}

uint16_t puls_hrv_anzahl(void) { return s_hrv_anzahl; }

// --- Was der Sensor meldet ---

static uint8_t prv_median(void) {
  uint8_t puffer[GLAETTEN_N];
  for (uint8_t i = 0; i < s_ring_anzahl; i++) {
    puffer[i] = s_ring[(s_ring_kopf + GLAETTEN_N - s_ring_anzahl + i) % GLAETTEN_N];
  }
  for (uint8_t i = 1; i < s_ring_anzahl; i++) {
    const uint8_t v = puffer[i];
    int j = i - 1;
    while (j >= 0 && puffer[j] > v) {
      puffer[j + 1] = puffer[j];
      j--;
    }
    puffer[j + 1] = v;
  }
  return puffer[s_ring_anzahl / 2];
}

void puls_ereignis_bpm(uint8_t bpm, int8_t guete) {
  if (!s_beobachtet) {
    return;
  }
  if (guete < 0) {
    // NICHT AM HANDGELENK: kein Puls, und zwar sofort - nicht erst, wenn
    // der alte Wert nach anderthalb Minuten verblasst.
    s_bpm = 0;
    s_frisch_seit = 0;
    s_ring_anzahl = 0;
    return;
  }
  if (bpm < BPM_MIN || bpm > BPM_MAX) {
    return;
  }
  if (guete >= GUETE_GUT) {
    s_ring_anzahl = 0;
    s_bpm = bpm;
  } else {
    s_ring[s_ring_kopf] = bpm;
    s_ring_kopf = (uint8_t)((s_ring_kopf + 1) % GLAETTEN_N);
    if (s_ring_anzahl < GLAETTEN_N) {
      s_ring_anzahl++;
    }
    s_bpm = prv_median();
  }
  s_frisch_seit = ks_jetzt();
}

void puls_ereignis_ppi(uint16_t ppi_ms) {
  if (s_hrv_an && ppi_ms) {
    prv_hrv_intervall(ppi_ms);
  }
}

void puls_beobachten(void) {
  // Ein Training braucht die Zonen: spaetestens jetzt ist klar, was gilt.
  prv_klaeren();
  s_bpm = 0;
  s_frisch_seit = 0;
  s_ring_anzahl = 0;
  s_ring_kopf = 0;
  s_beobachtet = true;
  prv_sensor();
}

void puls_ignorieren(void) {
  s_beobachtet = false;
  s_hrv_an = false;
  prv_sensor();
}

uint16_t puls_lesen(void) {
  return s_bpm;
}

bool puls_frisch(void) {
  if (s_bpm == 0 || s_frisch_seit == 0) {
    return false;
  }
  return (ks_jetzt() - s_frisch_seit) <= KS_PULS_ALT_S;
}

void puls_dicht_messen(void) {
  s_dicht = true;
  prv_sensor();
}

void puls_normal_messen(void) {
  s_dicht = false;
  prv_sensor();
}

void puls_auffrischen(void) {
  prv_sensor();
}
