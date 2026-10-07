/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2: Velo und Auto (kontext.h, Spezifikation 3.3-3.7).

#include "pbl/services/activity/boulder/kontext.h"

#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/activity_private.h"
#include "pbl/services/activity/boulder/puls.h"
#include "pbl/services/activity/boulder/speicher.h"
#include "pbl/services/activity/boulder/takt.h"
#ifdef CONFIG_BOULDER_KIESELSPORT
#include "pbl/services/activity/boulder/kalorien.h"
#endif
#include "pbl/services/time.h"
#include "pbl/util/math.h"
#include "pbl/util/units.h"
#include <pbl/drivers/rtc.h>
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
  // Eine Minute, die ein Training beruehrte, liegt im Ring: die naechste andere leert ihn.
  bool training_im_ring;
  // F7: Pulskalorien der Velofahrt, in Kalorien
  int32_t kcal_vorher;      //< aus der Ablage vor einem Neustart
  int32_t kcal_cal;         //< bis zum Ende der Sitzung (letzte fahrtartige Minute oder Ende)
  int32_t kcal_minute;      //< bis zum Ende der zuletzt verbuchten Fahrtminute ...
  time_t kcal_minute_ende;  //< ... die hier endet
#ifdef CONFIG_BOULDER_KIESELSPORT
  bool kcal_laeuft;
  BoulderKalorienZaehler kcal;  //< Zeit in Sekunden ab dem Fahrtbeginn
#endif
} s_k;

// Von KernelMain (Dialog zu) an KernelBG: Bit 15 = gesetzt, 8-14 = Fahrt, 0-7 = Antwort.
static volatile uint16_t s_merker;
// Vom Workout-Dienst (D15: unter seiner Sperre, App-Task oder KernelBG) und von Kieselsport
// (KernelBG): nur Merker und Zeiten. Gelesen im Minutenteil, in der Kopie (D17) und im
// HRM-Rueckruf; jedes ist ein Wort.
static volatile bool s_training;
static volatile bool s_ohne_schritte;
static volatile time_t s_training_ab;   //< Start des laufenden oder letzten Trainings
static volatile time_t s_training_bis;  //< Ende des letzten Trainings

static void prv_antwort_cb(void *data);
static BoulderAufgabe s_aufgabe_antwort = BOULDER_AUFGABE("Boulder Antwort", prv_antwort_cb);

// DIE ACTIVITY-SPERRE um jeden Weg, der die Fahrt aendert (Gegenpruefung B3): Das Einstecken
// beendet eine bestaetigte Fahrt auf KernelMain (activity.c prv_charger_event_cb ->
// early_deinit -> boulder_kontext_sitzung_beenden), unter dieser Sperre, waehrend Minutenteil,
// Takt und Antwort auf KernelBG laufen. Ohne gemeinsame Sperre koennte die Fahrt zweimal enden
// (Schritte doppelt zurueck, Sitzung zweimal geschlossen). Die Sperre ist rekursiv. Reihenfolge
// wie in early_deinit: Activity vor Kraepelin vor HRM - darum nie in den Lesern, die die Kopie
// unter ihrer eigenen Sperre ruft (boulder_kontext_fahrt_minute, _ueberdeckt, _offen_seit).
static void prv_sperren(void) {
  pbl_mutex_lock(&activity_private_state()->mutex, PBL_FOREVER);
}

static void prv_entsperren(void) {
  pbl_mutex_unlock(&activity_private_state()->mutex);
}

// ---------------------------------------------------------------------------------------
// Merkmale

static uint8_t prv_ruhepuls(uint8_t nacht) {
  if (nacht) {
    return nacht;
  }
  const uint8_t pref = activity_prefs_heart_get_resting_hr();
  return pref ? pref : KONTEXT_RUHEPULS_SONST;
}

static BoulderFahrtArt prv_art(const BoulderMinute *m, uint8_t ruhe, bool im_training) {
  if (m->geladen || m->nicht_getragen || im_training) {
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

// Velo als Pebbles "Open", nicht manuell. Schritte und Strecke 0 (gestrichen), die aktiven kcal
// aus dem Puls (F7, Schritt 2) bis zum Ende der Sitzung.
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
    .step_data.active_kcalories =
        (uint16_t)MIN(ROUND(MAX(s_k.kcal_cal, 0), ACTIVITY_CALORIES_PER_KCAL), UINT16_MAX),
  };
  activity_sessions_prv_add_activity_session(&s);
}

// ---------------------------------------------------------------------------------------
// Pulskalorien der Velofahrt (F7, Spezifikation 3): derselbe Zaehler und dasselbe Profil wie im
// Training (kalorien.c), Zeit in Sekunden ab dem Fahrtbeginn.
//
// AB DER KANDIDATIN, MIT DEM MINUTENPULS DAVOR: Der Fahrtbeginn liegt bis zu zehn Minuten vor der
// Kandidatin, und das Abo "Aktiv" misst erst ab der Vorstufe jede Sekunde. Fuer die Minuten bis
// zur Kandidatin zaehlt darum ihr Minutenpuls aus dem Ring, danach jeder Sekundenwert. Eine
// Minute ganz ohne Sekundenwert (Abo fuer SpO2 pausiert, Pebbles "Puls bei Aktivitaet" aus)
// bekommt ebenfalls ihren Minutenpuls. Bei "Nein" faellt der Zaehler weg.
//
// DIE SITZUNG ENDET MIT IHRER LETZTEN FAHRTARTIGEN MINUTE, und erst drei Minuten spaeter steht
// das fest. Was der Zaehler in diesen Minuten noch bekam, gehoert nicht dazu: Die Sitzung traegt
// die Summe bis zu ihrem Ende (kcal_cal), festgehalten in der Minute, in der es lag. Ein Wort -
// KernelMain liest es beim Einstecken (boulder_kontext_sitzung_beenden), ohne den Zaehler.

static int32_t prv_kcal_t(time_t utc) {
  return (int32_t)(utc - s_k.beginn);
}

static void prv_kcal_neu(void) {
  s_k.kcal_vorher = 0;
  s_k.kcal_cal = 0;
  s_k.kcal_minute = 0;
  s_k.kcal_minute_ende = 0;
#ifdef CONFIG_BOULDER_KIESELSPORT
  BoulderKalorienProfil profil;
  // Ohne brauchbares Profil (kein Gewicht, kein Alter) bleibt es bei 0 kcal wie in Schritt 1.
  s_k.kcal_laeuft = (s_k.art == FahrtArtVelo) && boulder_kalorien_profil_jetzt(&profil);
  boulder_kalorien_zaehler_neu(&s_k.kcal, &profil);
#endif
}

// Die Summe bis `utc`, mit dem Stand vor einem Neustart.
static int32_t prv_kcal_bis(time_t utc) {
#ifdef CONFIG_BOULDER_KIESELSPORT
  if (s_k.kcal_laeuft) {
    return s_k.kcal_vorher + boulder_kalorien_zaehler_summe(&s_k.kcal, prv_kcal_t(utc));
  }
#endif
  return s_k.kcal_vorher;
}

// Eine Fahrtminute verbuchen: ihr Minutenpuls, wenn kein Sekundenwert in ihr lag.
static void prv_kcal_minute(const KontextMinute *m) {
#ifdef CONFIG_BOULDER_KIESELSPORT
  const int32_t t = prv_kcal_t(m->utc);
  BoulderKalorienZaehler *z = &s_k.kcal;
  if (s_k.kcal_laeuft && m->puls && t >= z->stand_s && (!z->bpm || z->puls_s < t)) {
    boulder_kalorien_zaehler_puls(z, m->puls, t);
  }
#endif
  s_k.kcal_minute_ende = m->utc + PBL_SEC_PER_MIN;
  s_k.kcal_minute = prv_kcal_bis(s_k.kcal_minute_ende);
  if (m->art != FahrtArtKeine) {
    s_k.kcal_cal = s_k.kcal_minute;
  }
}

static void prv_fahrt_weg(void) {
  pbl_mutex_assert_held(&activity_private_state()->mutex, true);
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
#ifdef CONFIG_BOULDER_KIESELSPORT
  s_k.kcal_laeuft = false;
#endif
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

static void prv_ende_bei(time_t ende, const char *grund) {
  pbl_mutex_assert_held(&activity_private_state()->mutex, true);
  if (s_k.zustand == FahrtKeine || s_k.ende) {
    return;
  }
  s_k.ende = ende;
  // Endet sie spaeter als mit ihrer letzten fahrtartigen Minute (am Trainingsstart), gehoeren
  // die Minuten dazwischen zur Fahrt - auch ihre Kalorien. Nur der Minutenteil kommt hierher.
  if (ende != s_k.letzte_artige + PBL_SEC_PER_MIN) {
    s_k.kcal_cal = (ende == s_k.kcal_minute_ende) ? s_k.kcal_minute : prv_kcal_bis(ende);
  }
  prv_beendet(grund);
}

static void prv_ende_setzen(const char *grund) {
  prv_ende_bei(s_k.letzte_artige + PBL_SEC_PER_MIN, grund);
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
#ifdef CONFIG_BOULDER_KIESELSPORT
    s_k.kcal_laeuft = false;  // F7: der Zaehler faellt weg
#endif
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
  prv_sperren();
  prv_antwort_verarbeiten((uint8_t)((merker >> 8) & 0x7F), (BoulderNachfrageAntwort)(merker & 0xFF));
  // Gleich wirken, nicht erst in der naechsten Minute: das Abo "Aktiv" endet mit dem "Nein",
  // und nach dem Entscheid haelt die Kopie nichts mehr zurueck.
  prv_aktiv_setzen();
  prv_offen_berechnen();
  prv_entsperren();
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

static void prv_minute(const BoulderMinute *minute, uint8_t ruhepuls) {
  const uint8_t ruhe = prv_ruhepuls(ruhepuls);
  // EINE MINUTE, DIE EIN TRAINING BERUEHRT, gehoert keiner Fahrt: auch die, in der es begann
  // oder endete. Laege die Minute des Stopps als velo-artig im Ring, begaenne eine Kandidatin
  // nach einem Velo-Training noch in ihm (T4.4).
  const bool im_training = s_training || s_training_bis > minute->utc;
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
  // NACH DEM TRAINING BEGINNT DER RING NEU (Spezifikation G10): Velo-Minuten vor einem kurzen
  // Training zaehlten sonst mit, und die Kandidatin danach begaenne vor dem Training. Die Fahrt
  // ist hier schon zu - sie endete in der ersten Trainingsminute -, ihr Schwanz braucht den
  // Ring nicht mehr.
  if (!im_training && s_k.training_im_ring) {
    s_k.anzahl = 0;
    s_k.velo_folge = 0;
  }
  s_k.training_im_ring = im_training;

  const BoulderFahrtArt art = prv_art(minute, ruhe, im_training);
  s_k.ring[s_k.kopf] = (KontextMinute){
    .utc = minute->utc, .schritte = minute->schritte, .lage = minute->lage,
    .puls = minute->puls, .art = (uint8_t)art,
  };
  s_k.kopf = (s_k.kopf + 1) % KONTEXT_FENSTER;
  s_k.anzahl = (s_k.anzahl < KONTEXT_FENSTER) ? s_k.anzahl + 1 : KONTEXT_FENSTER;
  s_k.velo_folge = (art == FahrtArtVelo) ? s_k.velo_folge + 1 : 0;

  if (s_k.zustand == FahrtKeine) {
    BoulderFahrtArt kandidatin = im_training ? FahrtArtKeine : prv_kandidatin(ruhe);
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
      // F7: der Zaehler ab jetzt, die Minuten seit dem Fahrtbeginn mit ihrem Minutenpuls. Die
      // Sitzung reicht bis zu dieser Minute (letzte_artige), auch wenn sie selbst keine war.
      prv_kcal_neu();
      for (int i = 0; i < s_k.anzahl; i++) {
        if (prv_ring(i)->utc >= s_k.beginn) {
          prv_kcal_minute(prv_ring(i));
        }
      }
      s_k.kcal_cal = s_k.kcal_minute;
    }
  } else if (!s_k.ende) {
    if (art != FahrtArtKeine) {
      s_k.letzte_artige = minute->utc;
      s_k.keine_folge = 0;
    } else {
      s_k.keine_folge++;
    }
    if (!im_training) {
      prv_kcal_minute(prv_ring(s_k.anzahl - 1));
    }
    if (im_training) {
      // DIE FAHRT ENDET AM TRAININGSSTART (Kriterium 4), nicht schon mit ihrer letzten
      // fahrtartigen Minute: Was dazwischen lag (Ampel, Absteigen), gehoert zur Fahrt. Auf die
      // Minute genau, und die Minute des Starts gehoert schon dem Training - so ueberdeckt die
      // Fahrt es nie. Pebble hat ihre Sitzung beim Start schon geschlossen
      // (workout_service.c:453-465); prv_beendet schreibt sie mit diesem Ende neu.
      const time_t ab = s_training_ab;
      const time_t start = ab - (ab % PBL_SEC_PER_MIN);
      prv_ende_bei(MAX(start, s_k.letzte_artige + PBL_SEC_PER_MIN), "Training");
    } else if (s_k.keine_folge >= KONTEXT_ENDE) {
      prv_ende_setzen("Ende");
    } else if (minute->utc + PBL_SEC_PER_MIN - s_k.beginn >=
               KONTEXT_HOECHSTENS_MIN * PBL_SEC_PER_MIN) {
      prv_ende_setzen("6 h");
    } else if (art != FahrtArtKeine) {
      prv_sitzung(true);
    }
  }
  if (im_training) {
    // Nicht im Training fragen.
    prv_kandidatin_verwerfen("Training");
  }
  prv_nachfrage(minute->utc + PBL_SEC_PER_MIN);
  prv_aktiv_setzen();
  prv_offen_berechnen();
}

void boulder_kontext_minute(const BoulderMinute *minute, uint8_t ruhepuls) {
  prv_sperren();
  prv_minute(minute, ruhepuls);
  prv_entsperren();
}

void boulder_kontext_training(bool laeuft) {
  // NUR MERKER UND ZEIT (D15): Der Aufruf kommt unter Pebbles Workout-Sperre, mit Pebbles
  // Workout-App von der App-Task, mit Kieselsport von KernelBG. Die Fahrt beendet der naechste
  // Minutenteil unter der Activity-Sperre - hier keine Sperre und nichts aus Pebble.
  const time_t jetzt = rtc_get_time();
  // Jeder Start und Stopp loescht "ohne Schritte": Kieselsport setzt es nach seinem Start neu,
  // ein anderes Training (Pebbles Workout-App) zaehlt Schritte wie Pebble.
  s_ohne_schritte = false;
  if (laeuft) {
    s_training_ab = jetzt;
  } else if (s_training) {
    s_training_bis = jetzt;
  }
  s_training = laeuft;
}

bool boulder_kontext_training_laeuft(void) {
  return s_training;
}

void boulder_kontext_training_ohne_schritte(bool ohne) {
  s_ohne_schritte = ohne;
}

bool boulder_kontext_training_ohne_schritte_laeuft(void) {
  // Ohne Training gilt der Merker nie: Kam Kieselsports Aufruf erst, als das Training schon
  // wieder zu war, wirkt er nicht, und der naechste Start loescht ihn.
  return s_training && s_ohne_schritte;
}

void boulder_kontext_puls(uint8_t bpm, time_t utc) {
#ifdef CONFIG_BOULDER_KIESELSPORT
  // KernelBG im HRM-Rueckruf: ohne die Activity-Sperre (die Reihenfolge Activity -> HRM bleibt).
  // Den Zaehler aendern sonst nur der Minutenteil und die Callbacks, auch alle auf KernelBG.
  if (!s_k.kcal_laeuft || s_training || s_k.ende ||
      (s_k.zustand != FahrtKandidatin && s_k.zustand != FahrtBestaetigt)) {
    return;
  }
  const int32_t t = prv_kcal_t(utc);
  if (t >= s_k.kcal.stand_s) {
    boulder_kalorien_zaehler_puls(&s_k.kcal, bpm, t);
  }
#else
  (void)bpm;
  (void)utc;
#endif
}

int32_t boulder_kontext_kcal(void) {
  return s_k.kcal_cal;
}

void boulder_kontext_sitzung_beenden(void) {
  prv_sperren();  // haelt early_deinit schon; so gilt die Regel ohne Blick auf den Aufrufer
  if (s_k.zustand == FahrtBestaetigt && !s_k.ende) {
    prv_ende_setzen("Activity aus");
  }
  prv_entsperren();
}

void boulder_kontext_tracking_aus(void) {
  prv_sperren();
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
  prv_entsperren();
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
  prv_sperren();  // nicht halb vor und halb nach dem Ende vom Einstecken
  if (s_k.zustand == FahrtBestaetigt && !s_k.ende) {
    ablage->fassung = KONTEXT_ABLAGE_FASSUNG;
    ablage->art = (uint8_t)s_k.art;
    ablage->nr = s_k.nr;
    ablage->beginn = (uint32_t)s_k.beginn;
    ablage->letzte_artige = (uint32_t)s_k.letzte_artige;
    ablage->kcal_cal = s_k.kcal_cal;
  }
  prv_entsperren();
}

void boulder_kontext_laden(const BoulderFahrtAblage *ablage) {
  if (ablage->fassung != KONTEXT_ABLAGE_FASSUNG ||
      (ablage->art != FahrtArtVelo && ablage->art != FahrtArtAuto) ||
      ablage->letzte_artige < ablage->beginn) {
    return;
  }
  prv_sperren();
  if (s_k.zustand != FahrtKeine) {
    prv_entsperren();
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
  // F7: Der Zaehler beginnt neu, die Kalorien bis zur letzten Ablage bleiben. Die Zeit ohne Uhr
  // zaehlt nicht.
  prv_kcal_neu();
  s_k.kcal_vorher = MAX(ablage->kcal_cal, 0);
  s_k.kcal_cal = s_k.kcal_vorher;
  PBL_LOG_INFO("Boulder: Fahrt %u nach dem Boot fortgesetzt", s_k.nr);
  // Pebbles Liste kann sie aus ihrer Ablage schon haben; gleicher Start und Typ ersetzt.
  prv_sitzung(true);
  prv_entsperren();
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
  s_ohne_schritte = false;
  s_training_ab = 0;
  s_training_bis = 0;
  s_aufgabe_antwort.steht_an = false;
}
