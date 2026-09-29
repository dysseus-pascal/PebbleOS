/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "training.h"
#include "puls.h"
#include "reps.h"
#include "bahnen.h"
#include "abschnitt.h"

#include "schluessel.h"

static Laufzustand s_zustand = LaufAus;
static Sportart s_art = ArtLaufen;
static uint32_t s_beginn = 0;
static uint32_t s_sekunden = 0;

// Die Staende der Uhr beim Start - die Summen sind die Differenz dazu.
// DIE TAGESZAEHLER DER UHR, AUFSUMMIERT. Nicht "jetzt minus Anfang": um
// Mitternacht springen Schritte, Meter und Kalorien der Uhr auf null, und die
// Differenz waere negativ - ein Training ueber Mitternacht verlor alles davor.
// Jede Sekunde kommt dazu, was der Zaehler seit der letzten gewachsen ist;
// faellt er, zaehlt er ab null weiter.
typedef struct {
  uint32_t letzt;
  uint32_t summe;
} Zaehler;
static Zaehler s_zaehler[3];

static uint32_t prv_metrik(KsZaehler z);

static void prv_zaehler_start(void) {
  for (int z = 0; z < 3; z++) {
    s_zaehler[z] = (Zaehler){ .letzt = prv_metrik((KsZaehler)z), .summe = 0 };
  }
}

// Ab jetzt weiterzaehlen, die Summe behalten - nach einer Pause.
static void prv_zaehler_start_ab_jetzt(void) {
  for (int z = 0; z < 3; z++) {
    s_zaehler[z].letzt = prv_metrik((KsZaehler)z);
  }
}

static void prv_zaehler_tick(void) {
  for (int z = 0; z < 3; z++) {
    const uint32_t jetzt = prv_metrik((KsZaehler)z);
    s_zaehler[z].summe += (jetzt >= s_zaehler[z].letzt) ? jetzt - s_zaehler[z].letzt : jetzt;
    s_zaehler[z].letzt = jetzt;
  }
}

static uint32_t s_puls_summe = 0;
static uint32_t s_puls_messungen = 0;
static uint16_t s_puls_max = 0;

// Der Tagesstand eines Zaehlers. Um Mitternacht faengt er neu an; ein
// Training ueber Mitternacht zaehlt dann nur, was danach kam.
static uint32_t prv_metrik(KsZaehler z) {
  return ks_heute(z);
}

uint16_t training_puls(void) {
  return puls_lesen();
}

bool training_puls_frisch(void) {
  return puls_frisch();
}

void training_init(void) {
  puls_init();
}

uint16_t training_sekunde(void) { return (uint16_t)s_sekunden; }

Laufzustand training_zustand(void) { return s_zustand; }
Sportart training_art(void) { return s_art; }

void training_starte(Sportart art) {
  training_starte_ab(art, 0);
}

void training_starte_ab(Sportart art, uint32_t beginn) {
  s_art = art;
  abschnitt_leeren();
  s_zustand = LaufLaeuft;
  // DER BEGINN KOMMT VON DER APP, nicht von hier: sie hat ihn schon ans
  // Telefon gemeldet, und die Spurdatei dort heisst nach ihm. Zwei Uhren
  // in derselben Sekunde koennten auseinanderliegen.
  s_beginn = beginn ? beginn : (uint32_t)ks_jetzt();
  s_sekunden = 0;
  prv_zaehler_start();
  s_puls_summe = 0;
  s_puls_messungen = 0;
  s_puls_max = 0;
  puls_dicht_messen();
  puls_beobachten();
  ks_puls_szene(art, true);

  // DIE ZAEHLER NUR DORT, WO SIE ETWAS MESSEN. Ein Beschleunigungsmesser,
  // der beim Laufen mitlaeuft, zaehlte Schritte als Wiederholungen; ein
  // Kompass beim Krafttraining kostete nur Strom.
  const ArtInfo *info = art_info(art);
  if (info->reps) reps_start();
  if (info->bahnen) bahnen_start();
  // YOGA MISST HRV. Der Puls bleibt daneben; die Zonen sagen bei Yoga wenig.
  if (art == ArtYoga) puls_hrv_start();
}

void training_pause_umschalten(void) {
  if (s_zustand == LaufLaeuft) {
    s_zustand = LaufPause;
    // In der Pause reicht der normale Takt - eine Pause ist kein Training.
    puls_normal_messen();
    reps_pause(true);
    bahnen_pause(true);
  } else if (s_zustand == LaufPause) {
    s_zustand = LaufLaeuft;
    // Was in der Pause lief, zaehlt nicht: sonst kaeme es mit dem ersten Takt
    // danach auf einmal dazu.
    prv_zaehler_start_ab_jetzt();
    puls_dicht_messen();
    reps_pause(false);
    bahnen_pause(false);
  }
}

void training_tick(void) {
  if (s_zustand != LaufLaeuft) return;
  s_sekunden++;
  // DEN TAKT JEDE MINUTE NACHSTELLEN. Die Messrate ist eine Bitte ans
  // System, keine Zusage; schaltet es sie zwischendurch zurueck, faellt der
  // Puls still auf den Ruhetakt - und niemand saehe es der Zahl an.
  if (s_sekunden % 60 == 0) puls_dicht_messen();

  prv_zaehler_tick();

  const ArtInfo *info = art_info(s_art);
  if (info->reps) reps_tick((uint16_t)s_sekunden);
  if (info->bahnen) bahnen_tick((uint16_t)s_sekunden);

  // NUR FRISCHE WERTE ZAEHLEN. Ein alter, den der Sensor eine halbe Stunde
  // lang wiederholt, zoege den Schnitt auf eine Zahl, die nie gemessen wurde.
  const uint16_t puls = training_puls();
  if (puls > 0 && training_puls_frisch()) {
    s_puls_summe += puls;
    s_puls_messungen++;
    if (puls > s_puls_max) s_puls_max = puls;
  }
}

Trainingsstand training_stand(void) {
  Trainingsstand t = {
    .art = (uint8_t)s_art,
    .beginn = s_beginn,
    .dauer_s = s_sekunden,
    .puls_max = s_puls_max,
    .puls_mittel = s_puls_messungen ? (uint16_t)(s_puls_summe / s_puls_messungen) : 0,
  };

  const ArtInfo *info = art_info(s_art);
  if (info->reps) {
    t.saetze = reps_saetze();
    // Die laufenden zaehlen mit: wer waehrend eines Satzes hinschaut, soll
    // nicht die Zahl von vorhin sehen.
    t.reps = (uint16_t)(abschnitt_summe() + reps_laufend());
  }
  if (info->bahnen) {
    t.bahnen = bahnen_anzahl();
    t.meter = bahnen_meter();
  }
  if (s_art == ArtYoga) t.hrv_ms = puls_hrv_rmssd();
  if (info->schritte) t.schritte = s_zaehler[KsZaehlerSchritte].summe;
  if (info->distanz) t.meter = s_zaehler[KsZaehlerMeter].summe;
  // KALORIEN GELTEN FUER JEDE ART. Sie haengen am Puls und an der Bewegung,
  // nicht an Schritten - beim Krafttraining sind sie die einzige Zahl neben
  // Zeit und Puls, die etwas sagt.
  t.kcal = s_zaehler[KsZaehlerKcal].summe;
  return t;
}

// --- Archiv ---

static void prv_ins_archiv(const Trainingsstand *t) {
  int naechst = ks_persist_exists(PERSIST_ARCHIV_NAECHST)
      ? ks_persist_read_int(PERSIST_ARCHIV_NAECHST) : 0;
  int anzahl = ks_persist_exists(PERSIST_ARCHIV_ANZAHL)
      ? ks_persist_read_int(PERSIST_ARCHIV_ANZAHL) : 0;

  ks_persist_write_data(PERSIST_ARCHIV_BASIS + naechst, t, sizeof(*t));
  naechst = (naechst + 1) % KS_ARCHIV_MAX;
  if (anzahl < KS_ARCHIV_MAX) anzahl++;

  ks_persist_write_int(PERSIST_ARCHIV_NAECHST, naechst);
  ks_persist_write_int(PERSIST_ARCHIV_ANZAHL, anzahl);
}

Trainingsstand training_stoppe(void) {
  // ZUERST DIE ZAEHLER SCHLIESSEN, DANN DEN STAND HOLEN: der letzte Satz und
  // die letzte Bahn entstehen erst beim Abschliessen.
  const ArtInfo *info = art_info(s_art);
  if (info->reps) reps_stoppe((uint16_t)s_sekunden);
  if (info->bahnen) bahnen_stoppe((uint16_t)s_sekunden);
  const Trainingsstand t = training_stand();
  s_zustand = LaufAus;
  puls_normal_messen();
  puls_hrv_stop();
  puls_ignorieren();
  ks_puls_szene(s_art, false);
  // EIN TRAINING UNTER EINER MINUTE IST KEINES. Ein Fehlgriff im Menü soll
  // das Archiv nicht mit Eintraegen fuellen, die niemand gemacht hat.
  if (t.dauer_s >= 60) prv_ins_archiv(&t);
  return t;
}

void training_verwerfen(void) {
  const ArtInfo *info = art_info(s_art);
  if (info->reps) reps_stoppe((uint16_t)s_sekunden);
  if (info->bahnen) bahnen_stoppe((uint16_t)s_sekunden);
  s_zustand = LaufAus;
  puls_normal_messen();
  puls_hrv_stop();
  puls_ignorieren();
  ks_puls_szene(s_art, false);
  abschnitt_leeren();
}

uint32_t training_beginn(void) { return s_beginn; }

int training_archiv_anzahl(void) {
  return ks_persist_exists(PERSIST_ARCHIV_ANZAHL) ? ks_persist_read_int(PERSIST_ARCHIV_ANZAHL) : 0;
}

bool training_archiv_lesen(int index, Trainingsstand *aus) {
  // ERST NULLEN. Eintraege aelterer Fassungen sind kuerzer als der heutige
  // Stand; ohne das stuenden in den neuen Feldern Reste vom Stapel.
  memset(aus, 0, sizeof(*aus));
  const int anzahl = training_archiv_anzahl();
  if (index < 0 || index >= anzahl) return false;
  const int naechst = ks_persist_exists(PERSIST_ARCHIV_NAECHST)
      ? ks_persist_read_int(PERSIST_ARCHIV_NAECHST) : 0;
  // Rueckwaerts vom zuletzt geschriebenen Platz: index 0 ist das juengste.
  int platz = (naechst - 1 - index + 2 * KS_ARCHIV_MAX) % KS_ARCHIV_MAX;
  if (!ks_persist_exists(PERSIST_ARCHIV_BASIS + platz)) return false;
  ks_persist_read_data(PERSIST_ARCHIV_BASIS + platz, aus, sizeof(*aus));
  return true;
}
