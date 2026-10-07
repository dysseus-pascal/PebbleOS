/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "training.h"
#include "puls.h"
#include "reps.h"
#include "bahnen.h"
#include "abschnitt.h"
#include "pausen.h"

#include "pbl/util/math.h"

static Laufzustand s_zustand = LaufAus;
static Sportart s_art = ArtLaufen;
static uint32_t s_beginn = 0;
static uint32_t s_sekunden = 0;

// Pebbles Zahlen, wie der Dienst sie zuletzt gelesen hat (training_werte).
static KsWorkout s_pebble;

static uint32_t s_puls_summe = 0;
static uint32_t s_puls_messungen = 0;
static uint16_t s_puls_max = 0;

uint16_t training_puls(void) {
  return puls_lesen();
}

bool training_puls_frisch(void) {
  return puls_frisch();
}

void training_init(void) {
  puls_init();
}

uint32_t training_sekunde(void) { return s_sekunden; }

Laufzustand training_zustand(void) { return s_zustand; }
Sportart training_art(void) { return s_art; }

void training_starte_ab(Sportart art, uint32_t beginn) {
  s_art = art;
  abschnitt_leeren();
  pausen_leeren();
  s_zustand = LaufLaeuft;
  // DER BEGINN KOMMT VON DER APP, nicht von hier und nicht von Pebble: sie hat ihn schon ans
  // Telefon gemeldet, und die Spurdatei dort heisst nach ihm. Pebbles start_utc kann eine
  // Sekunde spaeter liegen (Regel 2.5.3).
  s_beginn = beginn ? beginn : (uint32_t)ks_jetzt();
  s_sekunden = 0;
  s_pebble = (KsWorkout){0};
  s_puls_summe = 0;
  s_puls_messungen = 0;
  s_puls_max = 0;
  puls_dicht_messen();
  puls_beobachten();

  // DIE ZAEHLER NUR DORT, WO SIE ETWAS MESSEN. Ein Beschleunigungsmesser, der beim Laufen
  // mitlaeuft, zaehlte Schritte als Wiederholungen; ein Kompass beim Krafttraining kostete nur
  // Strom.
  const ArtInfo *info = art_info(art);
  if (info->reps) reps_start();
  if (info->bahnen) bahnen_start();
  // YOGA MISST HRV. Der Puls bleibt daneben; die Zonen sagen bei Yoga wenig.
  if (art == ArtYoga) puls_hrv_start();
}

void training_pause_umschalten(void) {
  if (s_zustand == LaufLaeuft) {
    s_zustand = LaufPause;
    // Wann und bei welcher aktiven Sekunde - das Telefon rechnet damit die Kurve, die Saetze und
    // die Bahnen in Uhrzeit um (pausen.h).
    pausen_beginn(s_sekunden, ks_jetzt());
    // In der Pause reicht der normale Takt - eine Pause ist kein Training.
    puls_normal_messen();
    reps_pause(true);
    bahnen_pause(true);
  } else if (s_zustand == LaufPause) {
    s_zustand = LaufLaeuft;
    pausen_ende(ks_jetzt());
    puls_dicht_messen();
    reps_pause(false);
    bahnen_pause(false);
  }
}

void training_werte(const KsWorkout *w) {
  s_pebble = *w;
}

void training_tick(void) {
  if (s_zustand != LaufLaeuft) return;
  s_sekunden++;
  // DEN TAKT JEDE MINUTE NACHSTELLEN. Die Messrate ist eine Bitte ans System, keine Zusage;
  // schaltet es sie zwischendurch zurueck, faellt der Puls still auf den Ruhetakt - und niemand
  // saehe es der Zahl an.
  if (s_sekunden % 60 == 0) puls_dicht_messen();

  const ArtInfo *info = art_info(s_art);
  // IN VIER BYTE, NICHT IN ZWEI: nach 18 Stunden liefe ein uint16 ueber - Saetze und Bahnen
  // stuenden beim Telefon 18 Stunden zu frueh, und eine Bahn ueber die Grenze bekaeme eine
  // falsche Dauer.
  if (info->reps) reps_tick(s_sekunden);
  if (info->bahnen) bahnen_tick(s_sekunden);

  // NUR FRISCHE WERTE ZAEHLEN. Ein alter, den der Sensor eine halbe Stunde lang wiederholt,
  // zoege den Schnitt auf eine Zahl, die nie gemessen wurde.
  const uint16_t puls = training_puls();
  if (puls > 0 && training_puls_frisch()) {
    s_puls_summe += puls;
    s_puls_messungen++;
    if (puls > s_puls_max) s_puls_max = puls;
  }
}

void training_springe(uint32_t sekunde) {
  if (s_zustand == LaufLaeuft && sekunde > s_sekunden) {
    s_sekunden = sekunde;
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
    // Die laufenden zaehlen mit: wer waehrend eines Satzes hinschaut, soll nicht die Zahl von
    // vorhin sehen.
    t.reps = (uint16_t)(abschnitt_summe() + reps_laufend());
  }
  if (info->bahnen) {
    t.bahnen = bahnen_anzahl();
    t.meter = bahnen_meter();
  }
  if (s_art == ArtYoga) t.hrv_ms = puls_hrv_rmssd();
  if (info->schritte) t.schritte = (uint32_t)MAX(s_pebble.schritte, 0);
  if (info->distanz) t.meter = (uint32_t)MAX(s_pebble.meter, 0);
  // KALORIEN GELTEN FUER JEDE ART, aus dem Puls (kalorien.h): beim Krafttraining und auf dem
  // Rad sind sie die einzige Zahl neben Zeit und Puls, die etwas sagt. Gerundet wie Pebbles
  // Sitzung, damit Zusammenfassung und Sitzung dieselbe Zahl tragen.
  t.kcal = (uint32_t)ROUND(MAX(s_pebble.kcal_cal, 0), 1000);
  return t;
}

static void prv_zaehler_schliessen(void) {
  const ArtInfo *info = art_info(s_art);
  if (info->reps) reps_stoppe(s_sekunden);
  if (info->bahnen) bahnen_stoppe(s_sekunden);
  s_zustand = LaufAus;
  puls_normal_messen();
  puls_hrv_stop();
  puls_ignorieren();
}

Trainingsstand training_stoppe(void) {
  // ZUERST DIE ZAEHLER SCHLIESSEN, DANN DEN STAND HOLEN: der letzte Satz und die letzte Bahn
  // entstehen erst beim Abschliessen.
  const ArtInfo *info = art_info(s_art);
  if (info->reps) reps_stoppe(s_sekunden);
  if (info->bahnen) bahnen_stoppe(s_sekunden);
  const Trainingsstand t = training_stand();
  prv_zaehler_schliessen();
  return t;
}

void training_verwerfen(void) {
  prv_zaehler_schliessen();
  abschnitt_leeren();
}

uint32_t training_beginn(void) { return s_beginn; }

uint32_t training_ende(void) {
  // Gespeichert wird aus der Pause: die aktive Zeit endete, als sie begann.
  const time_t pause = pausen_offen_seit();
  return (uint32_t)(pause ? pause : ks_jetzt());
}
