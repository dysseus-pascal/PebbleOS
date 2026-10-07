/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2, Schritt 2: Pulskalorien und Verwerfen (kalorien.h, P1 in workout_service.c).

#include "pbl/services/activity/boulder/kalorien.h"

#include "pbl/services/activity/boulder/nacht.h"
#include "pbl/services/activity/workout_service.h"
#include "pbl/util/math.h"

// Boulders Grenzen (B: Trainingsanalyse.kt:99-104, :136-142)
#define RUHEPULS_MIN 30
#define RUHEPULS_MAX 110
#define RUHEPULS_ERSATZ 60
#define GEWICHT_MIN_DAG 2000   //< 20 kg
#define RESERVE_MIN_Z 200      //< das Maximum muss mehr als 20 bpm ueber dem Ruhepuls liegen
#define VO2_RUHE_C 350         //< 3,5 ml/kg/min, der Ruheanteil, der nicht aktiv ist
// Ein Pulswert gilt bis zum naechsten, hoechstens so lange (B: :131-133). Eine Luecke des Sensors
// zaehlt so nicht voll, und danach rechnet Pebbles Schrittformel (F5 a = B).
#define PULS_GILT_S 60

// kcal je Sekunde = Anteil * (VO2max - 3,5) ml/kg/min * kg / 1000 * 5 kcal/l / 60. Mit
// VO2max in 0,01, kg in dag und Kalorien (1/1000 kcal) wird daraus der Nenner 120000.
#define NENNER_CAL 120000LL

// VO2max-Mitte (FRIEND, Kaminsky 2015) je Jahrzehnt, Mitte 25 ... 75, in 0,1 ml/kg/min
// (B: :145-146).
static const int16_t VO2MAX_MANN[] = {480, 424, 378, 326, 282, 244};
static const int16_t VO2MAX_FRAU[] = {376, 309, 279, 243, 217, 190};
#define VO2MAX_STUETZEN 6

static int prv_vo2max_c(const int16_t *tabelle, uint8_t alter) {
  // Linear zwischen den Jahrzehnten, ausserhalb der erste oder letzte Wert (B: :148-153). Mit
  // ganzen Jahren liegt jeder Wert exakt auf 0,01.
  const int jahre = CLIP((int)alter - 25, 0, 10 * (VO2MAX_STUETZEN - 1));
  const int i = MIN(jahre / 10, VO2MAX_STUETZEN - 2);
  return 10 * tabelle[i] + (tabelle[i + 1] - tabelle[i]) * (jahre - 10 * i);
}

bool boulder_kalorien_profil(BoulderKalorienProfil *profil, uint16_t gewicht_dag, uint8_t alter,
                             ActivityGender geschlecht, uint8_t ruhepuls, uint8_t maxpuls) {
  *profil = (BoulderKalorienProfil){0};
  if (gewicht_dag < GEWICHT_MIN_DAG || alter == 0) {
    return false;
  }
  const int ruhe =
      (ruhepuls >= RUHEPULS_MIN && ruhepuls <= RUHEPULS_MAX) ? ruhepuls : RUHEPULS_ERSATZ;
  // Ohne eingestelltes Maximum Tanaka: 208 - 0,7 * Alter, in Zehnteln exakt.
  const int max_z = maxpuls ? 10 * maxpuls : 2080 - 7 * (int)alter;
  if (max_z <= 10 * ruhe + RESERVE_MIN_Z) {
    return false;
  }
  const int mann = prv_vo2max_c(VO2MAX_MANN, alter);
  const int frau = prv_vo2max_c(VO2MAX_FRAU, alter);
  int vo2max;
  if (geschlecht == ActivityGenderMale) {
    vo2max = mann;
  } else if (geschlecht == ActivityGenderFemale) {
    vo2max = frau;
  } else {
    // "Andere": die Mitte zwischen beiden (B: :157), auf 0,01 gerundet
    vo2max = (mann + frau + 1) / 2;
  }
  *profil = (BoulderKalorienProfil){
    .gewicht_dag = gewicht_dag,
    .vo2max_c = vo2max,
    .ruhe_z = 10 * ruhe,
    .max_z = max_z,
  };
  return true;
}

// ---------------------------------------------------------------------------------------
// Zaehler

// Der Anteil eines Pulswerts an der Reserve, als Zaehler ueber (max - ruhe). Unter dem Ruhepuls
// bleibt nur der Ruheumsatz (0), ueber dem Maximum die VO2max (B: :108-109).
static int32_t prv_anteil(const BoulderKalorienProfil *p, uint8_t bpm) {
  return CLIP(10 * (int32_t)bpm - p->ruhe_z, 0, p->max_z - p->ruhe_z);
}

static bool prv_gedeckt(const BoulderKalorienZaehler *z, int32_t t) {
  return z->bpm && t < z->puls_s + PULS_GILT_S;
}

// Die Zeit bis t verbuchen: Was der letzte Pulswert deckt, geht in den Anteil, der Rest zaehlt
// als ohne Puls.
static void prv_verbuchen(BoulderKalorienZaehler *z, int32_t t) {
  if (t < z->stand_s) {
    // Die Uhr wurde zurueckgestellt (oder Pebbles Dauer hinkt beim Stopp um einen Tick nach).
    // Verbuchtes bleibt, ab t geht es weiter, wie Pebbles Dauer auch.
    z->stand_s = t;
    z->puls_s = MIN(z->puls_s, t);
    z->bewegung_s = MIN(z->bewegung_s, t);
    return;
  }
  int32_t gedeckt = 0;
  if (z->bpm) {
    gedeckt = CLIP(MIN(t, z->puls_s + PULS_GILT_S) - z->stand_s, 0, t - z->stand_s);
    z->anteil_s += (int64_t)gedeckt * prv_anteil(&z->profil, z->bpm);
  }
  z->ohne_puls_s += (t - z->stand_s) - gedeckt;
  z->stand_s = t;
}

static int32_t prv_summe(const BoulderKalorienZaehler *z) {
  const BoulderKalorienProfil *p = &z->profil;
  int64_t puls = 0;
  if (p->gewicht_dag) {
    const int64_t zaehler = z->anteil_s * (p->vo2max_c - VO2_RUHE_C) * p->gewicht_dag;
    const int64_t nenner = NENNER_CAL * (p->max_z - p->ruhe_z);
    puls = (zaehler + nenner / 2) / nenner;
  }
  return (int32_t)puls + z->schritt_cal;
}

// Was die gerundete Summe seit dem letzten Aufruf zugelegt hat: So bleibt Pebbles Summe ganzer
// Kalorien gleich der exakten, gerundet, ohne dass sich Rundungen ueber Stunden aufaddieren.
static int32_t prv_melden(BoulderKalorienZaehler *z) {
  const int32_t summe = prv_summe(z);
  const int32_t zuwachs = summe - z->gemeldet_cal;
  z->gemeldet_cal = summe;
  return zuwachs;
}

void boulder_kalorien_zaehler_neu(BoulderKalorienZaehler *z, const BoulderKalorienProfil *profil) {
  *z = (BoulderKalorienZaehler){.profil = *profil};
}

int32_t boulder_kalorien_zaehler_puls(BoulderKalorienZaehler *z, uint8_t bpm, int32_t t) {
  prv_verbuchen(z, t);
  // Ohne Profil deckt kein Pulswert: Jede Sekunde rechnet dann mit Pebbles Schrittformel.
  if (z->profil.gewicht_dag && bpm) {
    z->bpm = bpm;
    z->puls_s = t;
  }
  return prv_melden(z);
}

int32_t boulder_kalorien_zaehler_schritte(BoulderKalorienZaehler *z, int32_t cal, int32_t t) {
  prv_verbuchen(z, t);
  // Pebbles Schritt-kcal gelten fuer die Zeit seit der vorigen Meldung. Davon zaehlt der Teil
  // ohne Puls, gleichmaessig verteilt: Pebble rechnet auch nur mit Strecke und Zeit.
  const int32_t dauer = t - z->bewegung_s;
  int32_t gut;
  if (dauer > 0) {
    const int32_t ohne = CLIP(z->ohne_puls_s - z->ohne_puls_bei_bewegung_s, 0, dauer);
    gut = (int32_t)(((int64_t)cal * ohne + dauer / 2) / dauer);
  } else {
    gut = prv_gedeckt(z, t) ? 0 : cal;
  }
  z->schritt_cal += gut;
  z->bewegung_s = t;
  z->ohne_puls_bei_bewegung_s = z->ohne_puls_s;
  return prv_melden(z);
}

int32_t boulder_kalorien_zaehler_bis(BoulderKalorienZaehler *z, int32_t t) {
  prv_verbuchen(z, t);
  return prv_melden(z);
}

int32_t boulder_kalorien_zaehler_summe(const BoulderKalorienZaehler *z, int32_t t) {
  BoulderKalorienZaehler kopie = *z;
  prv_verbuchen(&kopie, t);
  return prv_summe(&kopie);
}

// ---------------------------------------------------------------------------------------
// Das Training

static BoulderKalorienZaehler s_training;
static uint8_t s_maxpuls;
// Nur waehrend boulder_training_verwerfen() den Stopp ruft. So haengt der Merker an genau
// diesem Stopp und kann kein spaeteres Training treffen.
static volatile bool s_verwerfen;

void boulder_kalorien_maxpuls(uint8_t bpm) {
  s_maxpuls = bpm;
}

void boulder_kalorien_start(void) {
  // Der Schnappschuss beim Start: Aendert der Nutzer das Profil im Training, rechnet das
  // laufende Training weiter mit dem alten, wie Boulder mit dem Profil vor dem Training.
  // TODO(AP4, V3): Ruhepuls als Median der letzten 7 Naechte statt der letzten Nacht.
  BoulderKalorienProfil profil;
  boulder_kalorien_profil(&profil, activity_prefs_get_weight_dag(), activity_prefs_get_age_years(),
                          activity_prefs_get_gender(), boulder_nacht_ruhepuls(), s_maxpuls);
  boulder_kalorien_zaehler_neu(&s_training, &profil);
}

int32_t boulder_kalorien_puls(uint8_t bpm, int32_t aktiv_s) {
  return boulder_kalorien_zaehler_puls(&s_training, bpm, aktiv_s);
}

int32_t boulder_kalorien_schritte(int32_t cal, int32_t aktiv_s) {
  return boulder_kalorien_zaehler_schritte(&s_training, cal, aktiv_s);
}

int32_t boulder_kalorien_ende(int32_t aktiv_s) {
  return boulder_kalorien_zaehler_bis(&s_training, aktiv_s);
}

int32_t boulder_kalorien_bisher(int32_t aktiv_s) {
  return boulder_kalorien_zaehler_summe(&s_training, aktiv_s);
}

// ---------------------------------------------------------------------------------------
// Verwerfen

bool boulder_training_verwerfen(void) {
  s_verwerfen = true;
  const bool gestoppt = workout_service_stop_workout();
  s_verwerfen = false;
  return gestoppt;
}

bool boulder_training_verworfen(void) {
  return s_verwerfen;
}
