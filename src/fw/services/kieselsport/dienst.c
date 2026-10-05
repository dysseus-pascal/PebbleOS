/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/kieselsport/kieselsport.h"

#include "abschnitt.h"
#include "art.h"
#include "bahnen.h"
#include "dienst.h"
#include "kurve.h"
#include "pausen.h"
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

// EINE ZONE WECHSELT ERST DREI SCHLAEGE JENSEITS DER GRENZE. Ein Puls, der
// um einen Schlag um die Grenze pendelt, brummte sonst alle paar Sekunden -
// nachgestellt: 119 Mal in zehn Minuten. Angezeigt wird die Zone weiter ohne
// Abstand (prv_schnappschuss); nur das Brummen wartet, bis es eindeutig ist.
#define ZONEN_ABSTAND_BPM 3

static int prv_zone_mit_abstand(uint16_t puls) {
  const int roh = puls_zone(puls);
  if (s_letzte_zone <= 0 || roh == s_letzte_zone) {
    return roh;
  }
  // Hinauf gilt die Zone, die auch drei Schlaege tiefer noch gilt, hinab die,
  // die auch drei hoeher noch gilt. Bis dahin bleibt es die alte.
  if (roh > s_letzte_zone) {
    const int z = puls_zone(puls > ZONEN_ABSTAND_BPM ? (uint16_t)(puls - ZONEN_ABSTAND_BPM) : 0);
    return z > s_letzte_zone ? z : s_letzte_zone;
  }
  const int z = puls_zone((uint16_t)(puls + ZONEN_ABSTAND_BPM));
  return z < s_letzte_zone ? z : s_letzte_zone;
}

static void prv_zonenwechsel(void) {
  // Das ist die eine Stelle, an der die Uhr von sich aus etwas sagt - und
  // der Grund, warum man sie beim Sport ueberhaupt anschaut. Nur ein frischer
  // Wert darf brummen: ein alter wechselt keine Zone.
  const int zone = training_puls_frisch() ? prv_zone_mit_abstand(training_puls()) : 0;
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

// EIN VERGESSENES TRAINING ENDET VON SELBST. Steht es eine halbe Stunde am
// Stueck in der Pause, ist niemand mehr dabei: es wird gespeichert wie mit
// der Taste, die aktive Zeit endet beim Beginn der Pause. Sonst liefen
// Sensoren und Sekundentakt die Nacht durch, und das Training kaeme erst
// ans Telefon, wenn jemand die Uhr wieder in die Hand nimmt.
#define AUTO_ENDE_S (30 * 60)
static uint32_t s_pause_s;   //< Takte am Stueck in der Pause

static void prv_speichern(void *daten);

static void prv_tick(void *daten) {
  ks_sperren();
  if (training_zustand() == LaufAus) {
    ks_entsperren();
    return;
  }
  // Gezaehlt wird am Stueck: jedes Weiter faengt von vorn an.
  s_pause_s = training_zustand() == LaufPause ? s_pause_s + 1 : 0;
  if (s_pause_s >= AUTO_ENDE_S) {
    ks_entsperren();
    PBL_LOG_INFO("Kieselsport: %u min Pause, Training endet von selbst",
                 (unsigned)(AUTO_ENDE_S / 60));
    prv_speichern(NULL);
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
  // AUCH FUER EINEN STOPP: ohne ihn zeichnete das Telefon weiter auf.
  if (!(wartend_da() || stopp_da()) || s_nachsenden_versuch >= ARRAY_LENGTH(NACHSENDEN_MIN)) {
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
  // Ein Stopp, der noch wartet, gehoert zu einem frueheren Training: der
  // Start dieses ersetzt ihn auf dem Telefon.
  stopp_vergessen(0);
  s_letzte_zone = -1;
  s_pause_s = 0;
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
  // Auch Pause-Weiter-Pause zwischen zwei Takten beginnt eine neue Pause.
  s_pause_s = 0;
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

// Ein verworfenes Training: der Stopp in den Persist, bis das Telefon ihn hat.
static void prv_stopp_merken(void) {
  stopp_merken(training_beginn(), (uint32_t)ks_jetzt(), (uint8_t)training_art());
}

static void prv_speichern(void *daten) {
  // Nicht auf dem Stapel: KernelBG hat keinen grossen.
  static char s_liste[KS_LISTE_MAX];
  static uint8_t s_pausen[KS_PAUSEN_BYTES];
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
    const size_t pausen = pausen_als_bytes(s_pausen, sizeof(s_pausen));
    wartend_merken(&t, s_liste, s_pausen, pausen, training_ende());
    kurve_merken(t.beginn);
  } else {
    prv_stopp_merken();
  }
  prv_nachsenden_an();
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
  bool verworfen = false;
  ks_sperren();
  if (training_zustand() != LaufAus) {
    prv_stopp_merken();
    verworfen = true;
    training_verwerfen();
    prv_beendet(KsEndeVerworfen);
  }
  ks_entsperren();
  if (verworfen) {
    prv_nachsenden_an();
  }
}

// --- Oeffentlich ---

void kieselsport_init(void) {
  ks_sperren();
  training_init();
  reps_laden();
  bahnen_laden();
  prv_schnappschuss();
  // Nach einem Neustart: was vorher nicht ankam, wird wieder nachgereicht.
  const bool wartet = wartend_da() || stopp_da();
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

bool kieselsport_maxpuls_gesetzt(void) {
  ks_sperren();
  const bool gesetzt = puls_maximum_gesetzt();
  ks_entsperren();
  return gesetzt;
}

void kieselsport_maxpuls_setze(uint16_t schlaege) {
  ks_sperren();
  puls_setze_maximum(schlaege);
  prv_schnappschuss();
  ks_entsperren();
}

void kieselsport_maxpuls_nicht_setzen(void) {
  ks_sperren();
  puls_maximum_nicht_setzen();
  prv_schnappschuss();
  ks_entsperren();
}

void kieselsport_maxpuls_alt(uint16_t schlaege) {
  ks_sperren();
  puls_maximum_alt(schlaege);
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
// eigene Sperre - fuer einzelnes Lesen reicht sie. NICHT FUER PRUEFEN UND
// DANN LOESCHEN: das Vergessen nach einer Bestaetigung prueft erst, ob noch
// das bestaetigte Training dasteht, und loescht dann mehrere Schluessel.
// Unterbricht die App (hoehere Prioritaet) dabei prv_speichern eines neuen
// Trainings auf KernelBG, stehen dessen erste Schluessel schon da, der
// gepruefte noch nicht - und das Loeschen nimmt sie mit. prv_speichern
// schreibt unter der Sperre des Dienstes; dieselbe nehmen darum diese drei.

int kieselsport_archiv_anzahl(void) {
  return training_archiv_anzahl();
}

bool kieselsport_archiv_lesen(int index, Trainingsstand *aus) {
  return training_archiv_lesen(index, aus);
}

bool kieselsport_wartend_laden(Trainingsstand *t, char *liste, size_t platz) {
  return wartend_laden(t, liste, platz);
}

void kieselsport_wartend_vergessen(uint32_t beginn) {
  ks_sperren();
  wartend_vergessen(beginn);
  ks_entsperren();
}

size_t kieselsport_wartend_pausen(uint8_t *aus, size_t platz) {
  return wartend_pausen(aus, platz);
}

uint32_t kieselsport_wartend_ende(void) {
  return wartend_ende();
}

bool kieselsport_stopp_laden(uint32_t *beginn, uint32_t *zeit, uint8_t *art) {
  return stopp_laden(beginn, zeit, art);
}

void kieselsport_stopp_vergessen(uint32_t beginn) {
  stopp_vergessen(beginn);
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

bool kieselsport_kurve_bestaetigt(uint32_t beginn, uint16_t ab, uint16_t n) {
  ks_sperren();
  const bool passt = kurve_bestaetigt(beginn, ab, n);
  ks_entsperren();
  return passt;
}

bool kieselsport_kurve_luecke(void) {
  return kurve_luecke();
}

void kieselsport_kurve_vergessen(void) {
  ks_sperren();
  kurve_vergessen();
  ks_entsperren();
}

bool kieselsport_kurve_zeiten(uint32_t sekunden[KS_ZONEN + 1]) {
  // Die Zonen haengen am Maximalpuls - der braucht die Sperre.
  ks_sperren();
  const bool etwas = kurve_zeiten(sekunden);
  ks_entsperren();
  return etwas;
}
