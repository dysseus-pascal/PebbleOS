/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "reps.h"
#include "abschnitt.h"

#include "schluessel.h"

// 25 Messungen je Sekunde, in Paketen zu zehn (plattform.c). Dichter braucht
// es nicht: eine Wiederholung dauert eine gute Sekunde, und jedes Paket
// kostet Strom.

// Ab wie viel Ruhe ein Satz als beendet gilt - und ab wann der naechste
// beginnt. Vier Sekunden, weil kurzes Absetzen mitten im Satz vorkommt.
#define RUHE_S 4
#define RUHE_SCHWELLE 55     //< milli-g, gemittelte Abweichung
#define AKTIV_SCHWELLE 130

// Kuerzer als das ist keine Wiederholung, laenger als das keine mehr.
#define MIN_ABSTAND 12       //< Messungen, also rund eine halbe Sekunde

// WIE LANGE DER AUSSCHLAG ANHALTEN MUSS, bevor er als Wiederholung zaehlt.
//
// NACHGEMESSEN, NICHT GESCHAETZT: im Emulator eingespeistes Schuetteln mit
// 12,5 Hz - jede Messung das andere Vorzeichen - ergab 21 Wiederholungen in
// zehn Sekunden. Die Sperrzeit allein deckelt eben nur auf zwei je Sekunde;
// sie sagt nichts darueber, ob die Bewegung eine Bewegung war.
//
// Drei Messungen sind 0,12 Sekunden. Eine gehobene Hantel steht laenger
// oben als das (gemessen: acht Messungen bei einer Wiederholung je
// Sekunde), ein Zittern nie: bei 12,5 Hz dauert jede Halbwelle eine
// einzige Messung.
#define HALTEN 3

static bool s_an;
static bool s_pausiert;
static uint8_t s_empfind = 2;
static uint16_t s_pausenziel = 90;

static int32_t s_basis;          //< gleitender Mittelwert der Staerke
static int32_t s_bewegung;       //< gleitender Mittelwert der Abweichung
static bool s_oben;              //< ueber der oberen Schwelle gewesen?
static uint8_t s_haelt;          //< Messungen in Folge ueber der Schwelle
static uint16_t s_seit_rep;      //< Messungen seit der letzten Wiederholung

static uint16_t s_reps;
static uint16_t s_saetze;
static uint32_t s_satz_beginn;
static uint16_t s_ruhe_zaehler;
static bool s_ruht;
static uint32_t s_ruhe_beginn;
static uint16_t s_ruhe_dauer;
static bool s_gebrummt;
static bool s_brumm_faellig;   //< die Pause ist um - der Dienst soll brummen

static int32_t prv_schwelle(void) {
  // Fein zaehlt kleine Bewegungen mit, traege nur deutliche.
  switch (s_empfind) {
    case 1: return 260;
    case 3: return 110;
    default: return 180;
  }
}

/** Ganzzahlige Wurzel - der Uebersetzer bekommt hier keine Fliesskommazahl. */
static int32_t prv_wurzel(int32_t x) {
  if (x <= 0) return 0;
  int32_t w = x, letzte = 0;
  // Newton, sechs Runden reichen fuer alles, was ein Beschleunigungsmesser
  // liefert; die Abbruchbedingung faengt den Rest.
  for (int i = 0; i < 12 && w != letzte; i++) {
    letzte = w;
    w = (w + x / w) / 2;
  }
  return w;
}

void reps_daten(const KsAccel *daten, uint32_t anzahl) {
  if (!s_an || s_pausiert) return;
  for (uint32_t i = 0; i < anzahl; i++) {
    if (daten[i].did_vibrate) continue;  //< das Brummen ist keine Bewegung
    const int32_t x = daten[i].x, y = daten[i].y, z = daten[i].z;
    const int32_t staerke = prv_wurzel(x * x + y * y + z * z);

    // Der gleitende Mittelwert IST die Schwerkraft samt Koerperhaltung. Ihn
    // abzuziehen macht aus "wo ist unten" ein "was bewegt sich".
    if (s_basis == 0) s_basis = staerke;
    s_basis += (staerke - s_basis) / 16;
    const int32_t abw = staerke - s_basis;
    const int32_t betrag = abw < 0 ? -abw : abw;
    s_bewegung += (betrag - s_bewegung) / 8;

    if (s_seit_rep < 0xFFFF) s_seit_rep++;

    // Ein Schmitt-Trigger, keine blosse Schwelle: gezaehlt wird erst, wenn
    // die Bewegung vorher unten WAR. Sonst zaehlte ein Zittern an der
    // Schwelle zwanzig Wiederholungen in einer Sekunde.
    const int32_t schwelle = prv_schwelle();
    if (abw > schwelle) {
      if (s_haelt < 0xFF) s_haelt++;
    } else {
      s_haelt = 0;
    }
    if (!s_oben && s_haelt >= HALTEN) {
      s_oben = true;
      if (s_seit_rep >= MIN_ABSTAND) {
        s_reps++;
        s_seit_rep = 0;
        if (s_ruht) {
          // Bewegung in der Pause heisst: der naechste Satz hat begonnen.
          s_ruht = false;
          s_gebrummt = false;
        }
      }
    } else if (s_oben && abw < -schwelle) {
      s_oben = false;
    }
  }
}

void reps_laden(void) {
  if (ks_persist_exists(PERSIST_EMPFIND)) {
    const int32_t stufe = ks_persist_read_int(PERSIST_EMPFIND);
    if (stufe >= 1 && stufe <= 3) s_empfind = (uint8_t)stufe;
  }
  if (ks_persist_exists(PERSIST_PAUSENZIEL)) {
    s_pausenziel = (uint16_t)ks_persist_read_int(PERSIST_PAUSENZIEL);
  }
}

void reps_start(void) {
  s_an = true;
  s_pausiert = false;
  s_basis = 0;
  s_bewegung = 0;
  s_oben = false;
  s_haelt = 0;
  s_seit_rep = MIN_ABSTAND;
  s_reps = 0;
  s_saetze = 0;
  s_satz_beginn = 0;
  s_ruhe_zaehler = 0;
  s_ruht = false;
  s_ruhe_beginn = 0;
  s_ruhe_dauer = 0;
  s_gebrummt = false;
  s_brumm_faellig = false;

  reps_laden();
  ks_accel(true);
}

void reps_stoppe(uint32_t sekunde) {
  if (!s_an) return;
  // DEN OFFENEN SATZ NOCH ABSCHLIESSEN. Wer nach der letzten Wiederholung
  // sofort auf Stop drueckt, haette ihn sonst umsonst gemacht.
  if (!s_ruht && s_reps >= 2) {
    abschnitt_hinzu(s_satz_beginn, s_reps,
                    sekunde > s_satz_beginn ? sekunde - s_satz_beginn : 0);
    s_saetze++;
    s_reps = 0;
  }
  ks_accel(false);
  s_an = false;
}

void reps_pause(bool an) { s_pausiert = an; }


void reps_tick(uint32_t sekunde) {
  if (!s_an || s_pausiert) return;

  if (s_bewegung < RUHE_SCHWELLE) {
    if (s_ruhe_zaehler < 0xFFFF) s_ruhe_zaehler++;
  } else if (s_bewegung > AKTIV_SCHWELLE) {
    s_ruhe_zaehler = 0;
  }

  if (!s_ruht && s_ruhe_zaehler >= RUHE_S) {
    // EIN SATZ AB ZWEI WIEDERHOLUNGEN. Eine einzelne gezaehlte Bewegung ist
    // meistens ein Griff zur Flasche.
    if (s_reps >= 2) {
      const uint32_t ende = sekunde > RUHE_S ? sekunde - RUHE_S : sekunde;
      abschnitt_hinzu(s_satz_beginn, s_reps, ende > s_satz_beginn ? ende - s_satz_beginn : 0);
      s_saetze++;
    }
    s_ruht = true;
    s_ruhe_beginn = sekunde;
    s_ruhe_dauer = 0;
    s_gebrummt = false;
    s_reps = 0;
  }

  // Die Anzeige fasst zwei Byte; eine Pause ueber 18 Stunden steht bei 65535.
  const uint32_t ruhe = s_ruht && sekunde > s_ruhe_beginn ? sekunde - s_ruhe_beginn : 0;
  s_ruhe_dauer = ruhe > UINT16_MAX ? UINT16_MAX : (uint16_t)ruhe;

  if (s_ruht && !s_gebrummt && s_pausenziel > 0 &&
      sekunde >= s_ruhe_beginn + s_pausenziel) {
    // EINMAL BRUMMEN, WENN DIE PAUSE UM IST. Das ist der eine Dienst, den die
    // Uhr hier leistet und das Telefon nicht: man hat die Hantel in der Hand
    // und schaut nirgendwo hin. Vorgemerkt, gebrummt wird im Dienst.
    s_brumm_faellig = true;
    s_gebrummt = true;
  }


  // Ein Satz beginnt mit seiner ERSTEN Wiederholung, nicht mit dem Druck
  // auf Start: dazwischen liegt das Hinlaufen zur Bank.
  if (!s_ruht && s_reps == 1) s_satz_beginn = sekunde;
}

// NUR IM PRUEFBAU. Der Emulator hat keinen bewegten Arm; ohne erfundene
// Zahlen laesst sich der Kraft-Schirm nie ansehen. Vierzig Sekunden Satz,
// zwanzig Sekunden Pause, immer im Wechsel.

uint16_t reps_laufend(void) {
  return s_reps;
}

bool reps_ruht(void) {
  return s_ruht;
}

uint16_t reps_ruhe_s(void) {
  return s_ruhe_dauer;
}

uint16_t reps_saetze(void) {
  return s_saetze;
}

bool reps_brumm_holen(void) {
  const bool war = s_brumm_faellig;
  s_brumm_faellig = false;
  return war;
}

void reps_empfindlichkeit_setze(uint8_t stufe) {
  // NUR WAS SICH AENDERT, KOMMT IN DEN FLASH. Boulder schickt beim Speichern
  // alle Einstellungen, auch die gleich gebliebenen, und jede Zeile waere
  // sonst ein neuer Satz in der Datei.
  if (stufe < 1 || stufe > 3 || stufe == s_empfind) return;
  s_empfind = stufe;
  ks_persist_write_int(PERSIST_EMPFIND, stufe);
}
uint8_t reps_empfindlichkeit(void) { return s_empfind; }

void reps_pausenziel_setze(uint16_t sekunden) {
  if (sekunden == s_pausenziel) return;
  s_pausenziel = sekunden;
  ks_persist_write_int(PERSIST_PAUSENZIEL, sekunden);
}
uint16_t reps_pausenziel(void) { return s_pausenziel; }
