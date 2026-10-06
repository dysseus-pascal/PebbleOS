/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2: Velo und Auto (kontext.h, Spezifikation 3.3-3.7).

#include "pbl/services/activity/boulder/kontext.h"

#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/activity_private.h"
#include "pbl/services/activity/boulder/puls.h"
#include "pbl/services/activity/boulder/speicher.h"
#include "pbl/services/activity/boulder/takt.h"
#include "pbl/services/time.h"
#include "pbl/util/units.h"
#include <pbl/logging/logging.h>

#include <inttypes.h>
#include <string.h>

PBL_LOG_MODULE_DECLARE(service_activity, CONFIG_SERVICE_ACTIVITY_LOG_LEVEL);

typedef struct {
  time_t utc;
  uint8_t schritte;  //< gezaehlt (D5: auch waehrend der Fahrt)
  uint8_t lage;
  uint8_t puls;
  uint8_t art;       //< BoulderFahrtArt der Minute
} KontextMinute;

static struct {
  KontextMinute ring[KONTEXT_FENSTER];
  uint8_t kopf;      //< naechster Platz
  uint8_t anzahl;    //< lueckenlose Minuten im Ring
  time_t letzte_utc;
  uint8_t velo_folge;
  bool aktiv;        //< Abo "Aktiv" gewuenscht (3.7)
  time_t offen_seit; //< fuer die Kopie, im Minutenteil berechnet (D3)
  // Die Fahrt
  BoulderFahrtZustand zustand;
  BoulderFahrtArt art;
  uint8_t nr;
  time_t beginn;
  time_t letzte_artige;  //< Beginn der letzten fahrtartigen Minute
  time_t ende;           //< 0 = laeuft, sonst die Minute nach der letzten fahrtartigen
  uint8_t keine_folge;   //< nicht fahrtartige Minuten seit der letzten fahrtartigen
  uint8_t fragen;
  time_t frage_um;       //< 0 = kein Dialog offen
  bool nach_laden;       //< nach dem Boot fortgesetzt: der erste Abstand ist kein Sprung
  // Die vorige bestaetigte Fahrt: ihre Minuten koennen noch im Puffer liegen.
  time_t vorige_von;
  time_t vorige_bis;
} s_k;

// Von KernelMain (Dialog zu) an KernelBG: Bit 15 = gesetzt, 8-14 = Fahrt, 0-7 = Antwort.
static volatile uint16_t s_merker;
static volatile bool s_training;

static void prv_antwort_cb(void *data);
static BoulderAufgabe s_aufgabe_antwort = BOULDER_AUFGABE("Boulder Antwort", prv_antwort_cb);

// ---------------------------------------------------------------------------------------
// Merkmale

static uint8_t prv_ruhepuls(uint8_t nacht) {
  if (nacht) {
    return nacht;
  }
  const uint8_t pref = activity_prefs_heart_get_resting_hr();
  return pref ? pref : KONTEXT_RUHEPULS_SONST;
}

static BoulderFahrtArt prv_art(const BoulderMinute *m, uint8_t ruhe) {
  if (m->geladen || m->nicht_getragen || s_training) {
    return FahrtArtKeine;
  }
  // Der Puls zaehlt nur, wenn die Minute einen hat (Stufe 1: alle 10 min; ab der Vorstufe
  // jede Sekunde, 3.7).
  const bool puls_hoch = m->puls && m->puls >= ruhe + KONTEXT_PULS_UEBER;
  const bool puls_tief = m->puls && !puls_hoch;
  if (m->schritte < KONTEXT_VELO_SCHRITTE_UNTER && m->vmc >= KONTEXT_VELO_VMC_AB && !puls_tief) {
    return FahrtArtVelo;
  }
  if (m->schritte <= KONTEXT_AUTO_SCHRITTE_BIS && m->vmc >= KONTEXT_AUTO_VMC_AB &&
      m->vmc < KONTEXT_VELO_VMC_AB && !puls_hoch) {
    return FahrtArtAuto;
  }
  return FahrtArtKeine;
}

// Die i-te Minute des Rings, 0 = die aelteste der lueckenlosen.
static const KontextMinute *prv_ring(int i) {
  const int platz = (s_k.kopf + KONTEXT_FENSTER - s_k.anzahl + i) % KONTEXT_FENSTER;
  return &s_k.ring[platz];
}

static time_t prv_erste_artige(void) {
  for (int i = 0; i < s_k.anzahl; i++) {
    if (prv_ring(i)->art != FahrtArtKeine) {
      return prv_ring(i)->utc;
    }
  }
  return 0;
}

// Die Kandidatin aus den letzten zehn Minuten (3.3); FahrtArtKeine = keine.
static BoulderFahrtArt prv_kandidatin(uint8_t ruhe) {
  if (s_k.anzahl < KONTEXT_FENSTER) {
    return FahrtArtKeine;
  }
  int velo = 0, autos = 0, lage = 0, n_puls = 0;
  uint8_t puls[KONTEXT_FENSTER];
  for (int i = 0; i < KONTEXT_FENSTER; i++) {
    const KontextMinute *m = &s_k.ring[i];
    velo += (m->art == FahrtArtVelo);
    autos += (m->art == FahrtArtAuto);
    int gleich = 0;
    for (int j = 0; j < KONTEXT_FENSTER; j++) {
      gleich += (s_k.ring[j].lage == m->lage);
    }
    lage = (gleich > lage) ? gleich : lage;
    if (m->puls) {
      // Einfuegen sortiert: hoechstens zehn Werte.
      int k = n_puls++;
      while (k > 0 && puls[k - 1] > m->puls) {
        puls[k] = puls[k - 1];
        k--;
      }
      puls[k] = m->puls;
    }
  }
  const bool mit_puls = n_puls >= KONTEXT_FENSTER_PULSWERTE;
  const int median = mit_puls ? puls[n_puls / 2] : 0;
  if (velo >= KONTEXT_VELO_ARTIG && lage >= KONTEXT_VELO_LAGE &&
      (!mit_puls || median >= ruhe + KONTEXT_VELO_FENSTER_PULS)) {
    return FahrtArtVelo;
  }
  if (autos >= KONTEXT_AUTO_ARTIG && lage >= KONTEXT_AUTO_LAGE &&
      (!mit_puls || median < ruhe + KONTEXT_PULS_UEBER)) {
    return FahrtArtAuto;
  }
  return FahrtArtKeine;
}

// ---------------------------------------------------------------------------------------
// Sitzung (F5) und Ende

// Velo als Pebbles "Open", nicht manuell; Schrittdaten 0 (F7: 0 aktive kcal in Schritt 1).
static void prv_sitzung(bool laufend) {
  if (s_k.art != FahrtArtVelo || s_k.zustand != FahrtBestaetigt) {
    return;
  }
  const time_t bis = s_k.ende ? s_k.ende : s_k.letzte_artige + PBL_SEC_PER_MIN;
  ActivitySession s = {
    .type = ActivitySessionType_Open,
    .start_utc = s_k.beginn,
    .length_min = (uint16_t)((bis - s_k.beginn) / PBL_SEC_PER_MIN),
    .ongoing = laufend,
  };
  activity_sessions_prv_add_activity_session(&s);
}

static void prv_fahrt_weg(void) {
  if (s_k.zustand == FahrtBestaetigt) {
    s_k.vorige_von = s_k.beginn;
    s_k.vorige_bis = s_k.ende;
    boulder_speicher_anstossen(false);
  }
  s_k.zustand = FahrtKeine;
  s_k.art = FahrtArtKeine;
  s_k.ende = 0;
  s_k.frage_um = 0;
  s_k.fragen = 0;
  s_k.keine_folge = 0;
  s_k.nach_laden = false;
}

// Die Schritte der Minuten ab `ab` (Schwanz nach der letzten fahrtartigen): waehrend der Fahrt
// gingen sie an der Summe vorbei (D5) oder wurden beim "Ja" mit abgezogen (D4) - nur heute.
static uint32_t prv_schwanz_schritte(time_t ab) {
  const time_t heute = time_util_get_midnight_of(s_k.letzte_utc);
  uint32_t summe = 0;
  for (int i = 0; i < s_k.anzahl; i++) {
    const KontextMinute *m = prv_ring(i);
    if (m->utc >= ab && m->utc >= heute) {
      summe += m->schritte;
    }
  }
  return summe;
}

// Die Fahrt hat ihr Ende (`ende` gesetzt). Bestaetigt: Schwanz zurueck, Sitzung schliessen.
// Kandidatin: bleibt offen bis zum Entscheid (zweite Frage). Abgelehnt: weg.
static void prv_beendet(const char *grund) {
  PBL_LOG_INFO("Boulder: Fahrt %u Ende (%s), %" PRIi32 " min", s_k.nr, grund,
               (int32_t)((s_k.ende - s_k.beginn) / PBL_SEC_PER_MIN));
  switch (s_k.zustand) {
    case FahrtBestaetigt:
      boulder_kraepelin_fahrt_zurueckgeben(prv_schwanz_schritte(s_k.ende));
      prv_sitzung(false);
      prv_fahrt_weg();
      break;
    case FahrtAbgelehnt:
      prv_fahrt_weg();
      break;
    case FahrtKandidatin:
    case FahrtKeine:
      break;
  }
}

// "aus" (3.3: Training, Tracking aus, Zeitsprung): eine unbestaetigte Kandidatin verfaellt
// ohne Frage, ihre Minuten gehen ungestrichen hinaus.
static void prv_kandidatin_verwerfen(const char *grund) {
  if (s_k.zustand != FahrtKandidatin) {
    return;
  }
  PBL_LOG_INFO("Boulder: Fahrt %u verworfen (%s)", s_k.nr, grund);
  s_k.zustand = FahrtAbgelehnt;
  prv_fahrt_weg();
}

static void prv_ende_setzen(const char *grund) {
  if (s_k.zustand == FahrtKeine || s_k.ende) {
    return;
  }
  s_k.ende = s_k.letzte_artige + PBL_SEC_PER_MIN;
  prv_beendet(grund);
}

// ---------------------------------------------------------------------------------------
// Nachfrage und Antwort

static void prv_fragen(time_t jetzt) {
  s_k.fragen++;
  s_k.frage_um = jetzt;
  PBL_LOG_INFO("Boulder: Fahrt %u, Frage %u (%s)", s_k.nr, s_k.fragen,
               s_k.art == FahrtArtVelo ? "Velo" : "Auto");
  boulder_nachfrage_stellen(s_k.art, s_k.nr);
}

// "Ja": erst streichen (die Minuten sind noch zurueckgehalten), dann bestaetigen.
static void prv_bestaetigen(void) {
  const time_t bis = s_k.ende ? s_k.ende : (time_t)INT32_MAX;
  boulder_kraepelin_fahrt_streichen(s_k.beginn, bis);
  s_k.zustand = FahrtBestaetigt;
  PBL_LOG_INFO("Boulder: Fahrt %u bestaetigt (%s)", s_k.nr,
               s_k.art == FahrtArtVelo ? "Velo" : "Auto");
  if (s_k.ende) {
    prv_sitzung(false);
    prv_fahrt_weg();
  } else {
    prv_sitzung(true);
    boulder_speicher_anstossen(false);
  }
}

static void prv_aktiv_setzen(void);
static void prv_offen_berechnen(void);

static void prv_antwort_verarbeiten(uint8_t nr, BoulderNachfrageAntwort antwort) {
  if (nr != s_k.nr || s_k.zustand != FahrtKandidatin || !s_k.frage_um) {
    PBL_LOG_INFO("Boulder: Antwort %d fuer Fahrt %u kommt zu spaet", (int)antwort, nr);
    return;
  }
  s_k.frage_um = 0;
  if (antwort == NachfrageJa) {
    prv_bestaetigen();
    return;
  }
  // F4: ohne Antwort auf die zweite Frage gilt "Nein"; nach der ersten wartet die zweite.
  if (antwort == NachfrageNein || s_k.fragen >= 2) {
    PBL_LOG_INFO("Boulder: Fahrt %u nicht bestaetigt (%s)", s_k.nr,
                 antwort == NachfrageNein ? "Nein" : "keine Antwort");
    s_k.zustand = FahrtAbgelehnt;
    if (s_k.ende) {
      prv_fahrt_weg();
    }
  }
}

static void prv_merker_abholen(void) {
  const uint16_t merker = s_merker;
  if (!(merker & 0x8000)) {
    return;
  }
  s_merker = 0;
  prv_antwort_verarbeiten((uint8_t)((merker >> 8) & 0x7F), (BoulderNachfrageAntwort)(merker & 0xFF));
  // Gleich wirken, nicht erst in der naechsten Minute: das Abo "Aktiv" endet mit dem "Nein",
  // und nach dem Entscheid haelt die Kopie nichts mehr zurueck.
  prv_aktiv_setzen();
  prv_offen_berechnen();
}

static void prv_antwort_cb(void *data) {
  BOULDER_AUFGABE_LAEUFT(&s_aufgabe_antwort);
  prv_merker_abholen();
}

void boulder_kontext_antwort(uint8_t nr, BoulderNachfrageAntwort antwort) {
  s_merker = (uint16_t)(0x8000 | ((nr & 0x7F) << 8) | (uint8_t)antwort);
  // Faellt der Callback weg, holt der Takt den Merker ab (2.4, Regel 2).
  boulder_aufgabe_einreihen(&s_aufgabe_antwort);
}

void boulder_kontext_takt(time_t jetzt) {
  prv_merker_abholen();
  (void)jetzt;
}

// Die Fragen einer Kandidatin (F4): die erste beim Entstehen, die zweite am Fahrtende,
// spaetestens wenn das Zurueckhalten 45 min erreicht. Nie zwei Dialoge zugleich.
static void prv_nachfrage(time_t jetzt) {
  if (s_k.zustand != FahrtKandidatin) {
    return;
  }
  if (s_k.frage_um) {
    // Ohne Rueckmeldung (Dialog nie erschienen, Merker verloren): als zu ohne Antwort.
    if (jetzt - s_k.frage_um >= KONTEXT_DIALOG_SICHER_MIN * PBL_SEC_PER_MIN) {
      PBL_LOG_WRN("Boulder: Fahrt %u, Dialog ohne Rueckmeldung", s_k.nr);
      prv_antwort_verarbeiten(s_k.nr, NachfrageOhneAntwort);
    }
    return;
  }
  const time_t zurueck = jetzt + PBL_SEC_PER_MIN - s_k.beginn;
  if (s_k.fragen == 0 ||
      (s_k.fragen == 1 &&
       (s_k.ende || zurueck >= KONTEXT_ZWEITE_FRAGE_MIN * PBL_SEC_PER_MIN))) {
    prv_fragen(jetzt);
  }
}

// ---------------------------------------------------------------------------------------
// Minutenteil

static void prv_aktiv_setzen(void) {
  const bool velo_fahrt = (s_k.zustand == FahrtKandidatin || s_k.zustand == FahrtBestaetigt) &&
                          s_k.art == FahrtArtVelo && !s_k.ende;
  // Nach "Nein" aus, auch wenn die Fahrt weitergeht (3.7).
  const bool an = !s_training && s_k.zustand != FahrtAbgelehnt &&
                  (s_k.velo_folge >= KONTEXT_VORSTUFE || velo_fahrt);
  if (an == s_k.aktiv) {
    return;
  }
  s_k.aktiv = an;
  boulder_puls_aktiv(an);
  if (s_training) {
    // Das Training hat seine eigene Szene gesetzt (workout_service.c:490).
    boulder_szene_vergessen();
  } else {
    boulder_szene_aktiv(an);
  }
}

static void prv_offen_berechnen(void) {
  switch (s_k.zustand) {
    case FahrtKandidatin:
      s_k.offen_seit = s_k.beginn;
      break;
    case FahrtBestaetigt:
      // Der Schwanz: ob er zur Fahrt gehoert, zeigt erst die naechste Minute.
      s_k.offen_seit = s_k.keine_folge ? s_k.letzte_artige + PBL_SEC_PER_MIN : 0;
      break;
    case FahrtAbgelehnt:
      s_k.offen_seit = 0;
      break;
    case FahrtKeine:
      s_k.offen_seit = prv_erste_artige();
      break;
  }
}

void boulder_kontext_minute(const BoulderMinute *minute, uint8_t ruhepuls) {
  const uint8_t ruhe = prv_ruhepuls(ruhepuls);
  const time_t vorher = s_k.nach_laden ? s_k.letzte_artige : s_k.letzte_utc;
  // Nach dem Boot ist die gerettete letzte Minute bis zu einer Ablage (15 min) alt.
  const time_t sprung_s = (KONTEXT_SPRUNG_MIN + (s_k.nach_laden ? 15 : 0)) * PBL_SEC_PER_MIN;
  const bool sprung = vorher && (minute->utc <= vorher || minute->utc - vorher > sprung_s);
  const bool lueckenlos = s_k.letzte_utc && minute->utc == s_k.letzte_utc + PBL_SEC_PER_MIN;
  s_k.nach_laden = false;
  // Erst die Fahrt beenden (sie braucht den Ring fuer den Schwanz), dann den Ring leeren.
  if (sprung) {
    prv_kandidatin_verwerfen("Zeitsprung");
    prv_ende_setzen("Zeitsprung");
  }
  s_k.letzte_utc = minute->utc;
  if (!lueckenlos) {
    s_k.anzahl = 0;
    s_k.velo_folge = 0;
  }

  const BoulderFahrtArt art = prv_art(minute, ruhe);
  s_k.ring[s_k.kopf] = (KontextMinute){
    .utc = minute->utc, .schritte = minute->schritte, .lage = minute->lage,
    .puls = minute->puls, .art = (uint8_t)art,
  };
  s_k.kopf = (s_k.kopf + 1) % KONTEXT_FENSTER;
  s_k.anzahl = (s_k.anzahl < KONTEXT_FENSTER) ? s_k.anzahl + 1 : KONTEXT_FENSTER;
  s_k.velo_folge = (art == FahrtArtVelo) ? s_k.velo_folge + 1 : 0;

  if (s_k.zustand == FahrtKeine) {
    BoulderFahrtArt kandidatin = s_training ? FahrtArtKeine : prv_kandidatin(ruhe);
    // Mindestdauer 10 min (Move IQ): acht artige Minuten reichen erst, wenn die erste davon
    // zehn Minuten zurueckliegt - sonst fragte die Uhr schon nach acht Minuten Stop-and-go.
    if (kandidatin != FahrtArtKeine &&
        minute->utc + PBL_SEC_PER_MIN - prv_erste_artige() < KONTEXT_FENSTER * PBL_SEC_PER_MIN) {
      kandidatin = FahrtArtKeine;
    }
    if (kandidatin != FahrtArtKeine) {
      s_k.zustand = FahrtKandidatin;
      s_k.art = kandidatin;
      s_k.nr = (uint8_t)((s_k.nr + 1) & 0x7F);
      s_k.beginn = prv_erste_artige();
      s_k.letzte_artige = minute->utc;
      s_k.keine_folge = 0;
      s_k.ende = 0;
      s_k.fragen = 0;
      s_k.frage_um = 0;
      PBL_LOG_INFO("Boulder: Fahrt %u Kandidatin (%s) ab %" PRIu32, s_k.nr,
                   kandidatin == FahrtArtVelo ? "Velo" : "Auto", (uint32_t)s_k.beginn);
    }
  } else if (!s_k.ende) {
    if (art != FahrtArtKeine) {
      s_k.letzte_artige = minute->utc;
      s_k.keine_folge = 0;
    } else {
      s_k.keine_folge++;
    }
    if (s_training) {
      prv_ende_setzen("Training");
    } else if (s_k.keine_folge >= KONTEXT_ENDE) {
      prv_ende_setzen("Ende");
    } else if (minute->utc + PBL_SEC_PER_MIN - s_k.beginn >=
               KONTEXT_HOECHSTENS_MIN * PBL_SEC_PER_MIN) {
      prv_ende_setzen("6 h");
    } else if (art != FahrtArtKeine) {
      prv_sitzung(true);
    }
  }
  if (s_training) {
    // Nicht im Training fragen.
    prv_kandidatin_verwerfen("Training");
  }
  prv_nachfrage(minute->utc + PBL_SEC_PER_MIN);
  prv_aktiv_setzen();
  prv_offen_berechnen();
}

void boulder_kontext_training(bool laeuft) {
  s_training = laeuft;
}

bool boulder_kontext_training_laeuft(void) {
  return s_training;
}

void boulder_kontext_sitzung_beenden(void) {
  if (s_k.zustand == FahrtBestaetigt && !s_k.ende) {
    prv_ende_setzen("Activity aus");
  }
}

void boulder_kontext_tracking_aus(void) {
  // D13: ein offener Entscheid gilt als "Nein" - die Kopie schreibt ihre Minuten ungestrichen.
  prv_kandidatin_verwerfen("Tracking aus");
  prv_ende_setzen("Tracking aus");
  if (s_k.zustand != FahrtKeine) {
    prv_fahrt_weg();
  }
  s_k.anzahl = 0;
  s_k.velo_folge = 0;
  s_k.letzte_utc = 0;
  s_k.offen_seit = 0;
  if (s_k.aktiv) {
    s_k.aktiv = false;
    boulder_puls_aktiv(false);
    if (!s_training) {
      boulder_szene_aktiv(false);
    }
  }
}

// ---------------------------------------------------------------------------------------
// Naht der Kopie

time_t boulder_kontext_offen_seit(void) {
  return s_k.offen_seit;
}

bool boulder_kontext_fahrt_minute(time_t minute_utc) {
  if (s_k.zustand == FahrtBestaetigt && minute_utc >= s_k.beginn) {
    const time_t bis = s_k.ende ? s_k.ende : s_k.letzte_artige + PBL_SEC_PER_MIN;
    if (minute_utc < bis) {
      return true;
    }
  }
  return minute_utc >= s_k.vorige_von && minute_utc < s_k.vorige_bis;
}

bool boulder_kontext_fahrt_laeuft(void) {
  return s_k.zustand == FahrtBestaetigt && !s_k.ende;
}

bool boulder_kontext_ueberdeckt(time_t start_utc, uint32_t laenge_s) {
  const time_t ende = start_utc + (time_t)laenge_s;
  if (s_k.zustand == FahrtKandidatin || s_k.zustand == FahrtBestaetigt) {
    const time_t bis = s_k.ende ? s_k.ende : s_k.letzte_utc + PBL_SEC_PER_MIN;
    if (start_utc < bis && ende > s_k.beginn) {
      return true;
    }
  }
  return start_utc < s_k.vorige_bis && ende > s_k.vorige_von;
}

bool boulder_kontext_aktiv(void) {
  return s_k.aktiv;
}

bool boulder_kontext_frage_offen(void) {
  return s_k.frage_um != 0;
}

// ---------------------------------------------------------------------------------------
// Neustart (7.4): eine bestaetigte, laufende Fahrt geht mit gleichem Start weiter; eine
// unbestaetigte Kandidatin ist verworfen (ihre Minuten waren nur im Puffer).

void boulder_kontext_ablage(BoulderFahrtAblage *ablage) {
  memset(ablage, 0, sizeof(*ablage));
  if (s_k.zustand != FahrtBestaetigt || s_k.ende) {
    return;
  }
  ablage->fassung = KONTEXT_ABLAGE_FASSUNG;
  ablage->art = (uint8_t)s_k.art;
  ablage->nr = s_k.nr;
  ablage->beginn = (uint32_t)s_k.beginn;
  ablage->letzte_artige = (uint32_t)s_k.letzte_artige;
}

void boulder_kontext_laden(const BoulderFahrtAblage *ablage) {
  if (ablage->fassung != KONTEXT_ABLAGE_FASSUNG ||
      (ablage->art != FahrtArtVelo && ablage->art != FahrtArtAuto) ||
      ablage->letzte_artige < ablage->beginn || s_k.zustand != FahrtKeine) {
    return;
  }
  s_k.zustand = FahrtBestaetigt;
  s_k.art = (BoulderFahrtArt)ablage->art;
  s_k.nr = ablage->nr;
  s_k.beginn = (time_t)ablage->beginn;
  s_k.letzte_artige = (time_t)ablage->letzte_artige;
  s_k.ende = 0;
  s_k.keine_folge = 0;
  s_k.nach_laden = true;
  PBL_LOG_INFO("Boulder: Fahrt %u nach dem Boot fortgesetzt", s_k.nr);
  // Pebbles Liste kann sie aus ihrer Ablage schon haben; gleicher Start und Typ ersetzt.
  prv_sitzung(true);
}

// ---------------------------------------------------------------------------------------
// Diagnose und Tests

BoulderFahrtZustand boulder_kontext_zustand(void) {
  return s_k.zustand;
}

BoulderFahrtArt boulder_kontext_art(void) {
  return s_k.art;
}

time_t boulder_kontext_beginn(void) {
  return s_k.beginn;
}

uint8_t boulder_kontext_fragen(void) {
  return s_k.fragen;
}

void boulder_kontext_test_zuruecksetzen(void) {
  memset(&s_k, 0, sizeof(s_k));
  s_merker = 0;
  s_training = false;
  s_aufgabe_antwort.steht_an = false;
}
