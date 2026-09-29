/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/kieselsport/kieselsport.h"

#include "abschnitt.h"
#include "art.h"
#include "bahnen.h"
#include "dienst.h"
#include "kurve.h"
#include "puls.h"
#include "reps.h"
#include "training.h"
#include "wartend.h"

#include "pbl/kernel/mutex.h"
#include "pbl/services/regular_timer.h"
#include "pbl/services/system_task.h"
#include <pbl/logging/logging.h>

// Der Dienst: was in Kieselsport der Worker war.
//
// ER ZAEHLT DIE SEKUNDEN, liest den Puls, zaehlt Saetze und Bahnen und
// brummt beim Zonenwechsel - ob die App offen ist oder nicht. Anders als der
// Worker darf er brummen; er muss die App dafuer nicht mehr nach vorn holen.
//
// ALLES LAEUFT AUF KERNELBG. Der Sekundentakt kommt vom NewTimer, dessen
// Stapel klein ist, und springt sofort weiter; die Befehle der App ebenso.
// Die App liest nur den Schnappschuss (KsStand), den jeder Takt erneuert.

static PBL_MUTEX_DEFINE(s_sperre);

void ks_sperren(void) {
  pbl_mutex_lock(&s_sperre, PBL_FOREVER);
}

void ks_entsperren(void) {
  pbl_mutex_unlock(&s_sperre);
}

static KsStand s_stand;
static int s_letzte_zone = -1;
static uint8_t s_minute[KS_MINUTE_S];
static uint8_t s_sekunde_der_minute;
static RegularTimerInfo s_takt;

static KsEnde s_ende;
static uint8_t s_ende_nr;

// Die Bestellung der App, bis KernelBG sie abholt.
static Sportart s_bestellt_art;
static uint32_t s_bestellt_beginn;

// --- Brummen ---
//
// BEIM ZONENWECHSEL HOCH ZWEI KURZE, RUNTER EINER - wer laeuft, soll sie
// unterscheiden koennen, ohne hinzusehen. Beim Kraft einmal ein anderes
// Muster, wenn die Pause um ist.
static void prv_brummen_hoch(void) {
  static const uint32_t s_muster[] = { 60, 80, 60 };
  ks_brummen(s_muster, ARRAY_LENGTH(s_muster));
}

static void prv_brummen_runter(void) {
  static const uint32_t s_muster[] = { 100 };
  ks_brummen(s_muster, ARRAY_LENGTH(s_muster));
}

static void prv_brummen_pause_um(void) {
  static const uint32_t s_muster[] = { 80, 100, 80 };
  ks_brummen(s_muster, ARRAY_LENGTH(s_muster));
}

static void prv_zonenwechsel(void) {
  // Das ist die eine Stelle, an der die Uhr von sich aus etwas sagt - und
  // der Grund, warum man sie beim Sport ueberhaupt anschaut. Nur ein frischer
  // Wert darf brummen: ein alter wechselt keine Zone.
  const int zone = training_puls_frisch() ? puls_zone(training_puls()) : 0;
  if (training_zustand() == LaufLaeuft && zone > 0 && s_letzte_zone > 0 &&
      zone != s_letzte_zone) {
    if (zone > s_letzte_zone) {
      prv_brummen_hoch();
    } else {
      prv_brummen_runter();
    }
  }
  if (zone > 0) {
    s_letzte_zone = zone;
  }
  if (reps_brumm_holen()) {
    prv_brummen_pause_um();
  }
}

// AKKU: unter KS_AKKU_SPARSAM Prozent misst der Puls seltener. Beim Start und
// jede Minute nachgesehen - der Stand aendert sich nicht schneller.
static void prv_akku_pruefen(void) {
  bool am_kabel = false;
  const uint8_t prozent = ks_akku_prozent(&am_kabel);
  puls_setze_sparsam(!am_kabel && prozent < KS_AKKU_SPARSAM);
}

// --- Schnappschuss ---

static void prv_schnappschuss(void) {
  const Trainingsstand t = training_stand();
  const uint16_t puls = training_puls();
  const bool frisch = training_puls_frisch();
  s_stand = (KsStand){
    .zustand = training_zustand(),
    .art = training_art(),
    .t = t,
    .puls = puls,
    .frisch = frisch,
    .zone = (uint8_t)(frisch ? puls_zone(puls) : 0),
    .maxpuls = puls_maximum(),
    .reps_laufend = reps_laufend(),
    .ruht = reps_ruht(),
    .ruhe_s = reps_ruhe_s(),
    .bahnen_bereit = bahnen_bereit(),
    .sparsam = puls_sparsam(),
    .hrv_anzahl = puls_hrv_anzahl(),
    .ende = s_ende,
    .ende_nr = s_ende_nr,
  };
  memcpy(s_stand.minute, s_minute, sizeof(s_minute));
}

// --- Takt ---

static void prv_tick(void *daten) {
  ks_sperren();
  if (training_zustand() == LaufAus) {
    ks_entsperren();
    return;
  }
  training_tick();
  if (training_zustand() == LaufLaeuft) {
    const Trainingsstand t = training_stand();
    const uint16_t p = training_puls_frisch() ? training_puls() : 0;
    kurve_tick(t.dauer_s, p);
    s_minute[t.dauer_s % KS_MINUTE_S] = p > 255 ? 255 : (uint8_t)p;
  }
  // JEDE MINUTE, AUCH IN DER PAUSE: der Akku, und die Anmeldung am Sensor,
  // die sonst ablaeuft.
  if (++s_sekunde_der_minute >= 60) {
    s_sekunde_der_minute = 0;
    prv_akku_pruefen();
    puls_auffrischen();
  }
  prv_zonenwechsel();
  prv_schnappschuss();
  ks_entsperren();
}

static void prv_takt(void *daten) {
  // Der NewTimer hat einen kleinen Stapel; gerechnet wird auf KernelBG.
  system_task_add_callback(prv_tick, NULL);
}

static void prv_takt_an(bool an) {
  if (an && !regular_timer_is_scheduled(&s_takt)) {
    s_takt = (RegularTimerInfo){ .cb = prv_takt };
    regular_timer_add_seconds_callback(&s_takt);
  } else if (!an && regular_timer_is_scheduled(&s_takt)) {
    regular_timer_remove_callback(&s_takt);
  }
}

// --- Nachsenden ---
//
// EIN GESPEICHERTES TRAINING, DAS NIE ANKAM. Die App schickt die
// Zusammenfassung und wartet zwanzig Sekunden auf das Telefon; war es nicht
// zu erreichen, geht sie zu, und das Training laege im Persist, bis jemand
// die App wieder oeffnet. Also holt der Dienst sie selbst: nach 2, 10 und 30
// Minuten, dann nicht mehr - ist das Telefon so lange weg, geht es beim
// naechsten Oeffnen.
static const uint8_t NACHSENDEN_MIN[] = { 2, 10, 30 };
static RegularTimerInfo s_nachsenden_takt;
static uint8_t s_nachsenden_versuch;
static uint16_t s_nachsenden_minuten;

static void prv_nachsenden_aus(void) {
  if (regular_timer_is_scheduled(&s_nachsenden_takt)) {
    regular_timer_remove_callback(&s_nachsenden_takt);
  }
}

static void prv_nachsenden(void *daten) {
  ks_sperren();
  if (!wartend_da() || s_nachsenden_versuch >= ARRAY_LENGTH(NACHSENDEN_MIN)) {
    prv_nachsenden_aus();
    ks_entsperren();
    return;
  }
  const bool holen = ++s_nachsenden_minuten >= NACHSENDEN_MIN[s_nachsenden_versuch];
  if (holen) {
    s_nachsenden_versuch++;
  }
  ks_entsperren();
  if (holen) {
    ks_app_holen();
  }
}

static void prv_nachsenden_takt(void *daten) {
  system_task_add_callback(prv_nachsenden, NULL);
}

static void prv_nachsenden_an(void) {
  s_nachsenden_versuch = 0;
  s_nachsenden_minuten = 0;
  if (!regular_timer_is_scheduled(&s_nachsenden_takt)) {
    s_nachsenden_takt = (RegularTimerInfo){ .cb = prv_nachsenden_takt };
    regular_timer_add_minutes_callback(&s_nachsenden_takt);
  }
}

// --- Befehle, auf KernelBG ---

static void prv_starte(void *daten) {
  ks_sperren();
  if (training_zustand() != LaufAus) {
    ks_entsperren();
    return;
  }
  // Waehrend des Trainings bleibt die Datei offen: an seinem Ende wird
  // viel geschrieben.
  ks_persist_offen_halten(true);
  // Die Uhr erkennt keine Bewegung, solange ein Training laeuft - sonst
  // stuende dieselbe Stunde zweimal im Verlauf, einmal als Bewegung.
  ks_bewegung_erkennen(false);
  s_letzte_zone = -1;
  s_sekunde_der_minute = 0;
  memset(s_minute, 0, sizeof(s_minute));
  prv_akku_pruefen();
  training_starte_ab(s_bestellt_art, s_bestellt_beginn);
  kurve_start();
  prv_takt_an(true);
  ks_startmenue_oben(true);
  prv_schnappschuss();
  PBL_LOG_INFO("Kieselsport: Training gestartet, Art %d", (int)s_bestellt_art);
  ks_entsperren();
}

static void prv_pause(void *daten) {
  ks_sperren();
  training_pause_umschalten();
  prv_schnappschuss();
  ks_entsperren();
}

static void prv_beendet(KsEnde ende) {
  prv_takt_an(false);
  ks_startmenue_oben(false);
  ks_bewegung_erkennen(true);
  s_ende = ende;
  s_ende_nr++;
  prv_schnappschuss();
  ks_persist_offen_halten(false);
}

static void prv_speichern(void *daten) {
  // Nicht auf dem Stapel: KernelBG hat keinen grossen.
  static char s_liste[KS_LISTE_MAX];
  ks_sperren();
  if (training_zustand() == LaufAus) {
    ks_entsperren();
    return;
  }
  const Trainingsstand t = training_stoppe();
  // EIN TRAINING UNTER EINER MINUTE IST KEINES - es geht nicht ans Telefon
  // und nicht in den Verlauf.
  const bool behalten = t.dauer_s >= 60;
  if (behalten) {
    s_liste[0] = 0;
    if (abschnitt_anzahl() > 0) {
      abschnitt_als_text(s_liste, sizeof(s_liste));
    }
    wartend_merken(&t, s_liste);
    kurve_merken(t.beginn);
    prv_nachsenden_an();
  }
  abschnitt_leeren();
  prv_beendet(behalten ? KsEndeGespeichert : KsEndeVerworfen);
  ks_entsperren();
  // AUSSERHALB DER SPERRE: die Nachricht schreibt in den Flash, und so
  // lange soll die App nicht auf ihren Schnappschuss warten.
  if (behalten) {
    ks_sitzung_melden(&t);
  }
}

static void prv_verwerfen(void *daten) {
  ks_sperren();
  if (training_zustand() != LaufAus) {
    training_verwerfen();
    prv_beendet(KsEndeVerworfen);
  }
  ks_entsperren();
}

// --- Oeffentlich ---

void kieselsport_init(void) {
  ks_sperren();
  training_init();
  reps_laden();
  bahnen_laden();
  prv_schnappschuss();
  // Nach einem Neustart: was vorher nicht ankam, wird wieder nachgereicht.
  const bool wartet = wartend_da();
  ks_entsperren();
  if (wartet) {
    prv_nachsenden_an();
  }
}

void kieselsport_starte(Sportart art, uint32_t beginn) {
  ks_sperren();
  s_bestellt_art = art < ArtAnzahl ? art : ArtLaufen;
  s_bestellt_beginn = beginn;
  ks_entsperren();
  system_task_add_callback(prv_starte, NULL);
}

void kieselsport_pause_umschalten(void) {
  system_task_add_callback(prv_pause, NULL);
}

void kieselsport_speichern(void) {
  system_task_add_callback(prv_speichern, NULL);
}

void kieselsport_verwerfen(void) {
  system_task_add_callback(prv_verwerfen, NULL);
}

void kieselsport_stand(KsStand *aus) {
  ks_sperren();
  *aus = s_stand;
  ks_entsperren();
}

bool kieselsport_laeuft(void) {
  ks_sperren();
  const bool laeuft = training_zustand() != LaufAus;
  ks_entsperren();
  return laeuft;
}

const ArtInfo *kieselsport_art_info(Sportart art) {
  return art_info(art);
}

int kieselsport_zone(uint16_t bpm) {
  ks_sperren();
  const int zone = puls_zone(bpm);
  ks_entsperren();
  return zone;
}

uint16_t kieselsport_zonengrenze(int zone) {
  ks_sperren();
  const uint16_t grenze = puls_zonengrenze(zone);
  ks_entsperren();
  return grenze;
}

uint16_t kieselsport_maxpuls(void) {
  ks_sperren();
  const uint16_t wert = puls_maximum();
  ks_entsperren();
  return wert;
}

void kieselsport_maxpuls_setze(uint16_t schlaege) {
  ks_sperren();
  puls_setze_maximum(schlaege);
  prv_schnappschuss();
  ks_entsperren();
}

uint8_t kieselsport_empfindlichkeit(void) {
  ks_sperren();
  const uint8_t wert = reps_empfindlichkeit();
  ks_entsperren();
  return wert;
}

void kieselsport_empfindlichkeit_setze(uint8_t stufe) {
  ks_sperren();
  reps_empfindlichkeit_setze(stufe);
  ks_entsperren();
}

uint16_t kieselsport_pausenziel(void) {
  ks_sperren();
  const uint16_t wert = reps_pausenziel();
  ks_entsperren();
  return wert;
}

void kieselsport_pausenziel_setze(uint16_t sekunden) {
  ks_sperren();
  reps_pausenziel_setze(sekunden);
  ks_entsperren();
}

uint16_t kieselsport_becken(void) {
  ks_sperren();
  const uint16_t wert = bahnen_becken();
  ks_entsperren();
  return wert;
}

void kieselsport_becken_setze(uint16_t meter) {
  ks_sperren();
  bahnen_becken_setze(meter);
  ks_entsperren();
}

// Archiv, Zusammenfassung und Kurve liegen nur im Persist, und der hat seine
// eigene Sperre. Die des Dienstes braucht es dafuer nicht.

int kieselsport_archiv_anzahl(void) {
  return training_archiv_anzahl();
}

bool kieselsport_archiv_lesen(int index, Trainingsstand *aus) {
  return training_archiv_lesen(index, aus);
}

bool kieselsport_wartend_laden(Trainingsstand *t, char *liste, size_t platz) {
  return wartend_laden(t, liste, platz);
}

void kieselsport_wartend_vergessen(void) {
  wartend_vergessen();
}

bool kieselsport_kurve_wartet(void) {
  return kurve_wartet();
}

uint32_t kieselsport_kurve_beginn(void) {
  return kurve_beginn();
}

uint16_t kieselsport_kurve_anzahl(void) {
  return kurve_anzahl();
}

uint16_t kieselsport_kurve_ab(void) {
  return kurve_ab();
}

uint16_t kieselsport_kurve_stueck(uint8_t *aus, uint16_t max) {
  return kurve_stueck(aus, max);
}

void kieselsport_kurve_bestaetigt(uint16_t n) {
  kurve_bestaetigt(n);
}

void kieselsport_kurve_vergessen(void) {
  kurve_vergessen();
}

bool kieselsport_kurve_zeiten(uint32_t sekunden[KS_ZONEN + 1]) {
  // Die Zonen haengen am Maximalpuls - der braucht die Sperre.
  ks_sperren();
  const bool etwas = kurve_zeiten(sekunden);
  ks_entsperren();
  return etwas;
}
