/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/activity/boulder/nacht.h"

#include "applib/data_logging.h"
#include "kernel/kernel_heap.h"
#include "kernel/pbl_malloc.h"
#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/activity_algorithm.h"
#include "pbl/services/activity/activity_private.h"
// Die HRV-Fenster und die Messstufe ueber die Haken: ist ihr Schalter aus, kommt die Antwort
// ohne sie (keine Fenster, Stufe 0).
#include "pbl/services/activity/boulder/haken.h"
#include "pbl/services/battery/battery_state.h"
#include "pbl/services/activity/boulder/schlafanalyse.h"
#include "pbl/services/data_logging/data_logging_service.h"
#include "pbl/services/health_sync_endpoint.h"
#include "pbl/services/new_timer/new_timer.h"
#include "pbl/services/settings/settings_file.h"
#include "pbl/services/system_task.h"
#include "pbl/kernel/mutex.h"
#include "pbl/util/heap.h"
#include "util/time/time.h"
#include <pbl/logging/logging.h>

#ifdef CONFIG_SERVICE_KIESELSPORT
#include "pbl/services/kieselsport/kieselsport.h"
#endif

#include <string.h>

// Das Nachtfenster in Minuten des Tages (Ortszeit): 20 bis 11 Uhr.
#define FENSTER_VON_MIN (20 * MINUTES_PER_HOUR)
#define FENSTER_DAUER_MIN (15 * MINUTES_PER_HOUR)
// So oft wird gerechnet. Jede Minute waere Strom und Flash fuer nichts: die
// Rechnung liest die ganze Nacht aus der Minutendatei. Dazwischen wird nur der
// laufende Schlafblock fortgeschrieben (prv_fortschreiben).
#define TAKT_MIN 15
// So lange wach nach dem letzten Schlaf, und die Nacht gilt als fertig: sie
// wird gemeldet und geschickt, damit sie bald nach dem Aufstehen da ist. Die
// Rechnung laeuft aber weiter - schlaeft man noch einmal ein, waechst dieselbe
// Nacht, und die Uhr schickt sie mit demselben Beginn noch einmal. Frueher
// wurde aus dem Schlaf danach eine zweite Nacht; das Telefon zeigte dann nur
// den Rest.
#define WACH_FERTIG_MIN 15
// So lange wach, und die Nacht ist endgueltig: Schlaf danach ist eine eigene
// Nacht (siehe s_erledigt_bis) - der Morgenschlaf nach zwei Stunden auf. ERST
// DANN SIND IHRE SITZUNGEN ABGESCHLOSSEN: bis dahin kann jede Rechnung den
// Block verschieben, und ein schon fester Block bliebe neben dem neuen stehen.
#define WACH_ENDGUELTIG_MIN 120
// DER MORGENSCHLAF NACH EINER ENDGUELTIGEN NACHT ist erst ab anderthalb
// Stunden eine eigene Nacht. Kuerzer ist es ein Nickerchen: als Nacht stuende
// es neben der echten in der App und in Health Connect. An die Nacht gehaengt
// verschoebe es deren Aufwachen um Stunden und zaehlte das Wachsein dazwischen
// als naechtliches Wachliegen. Lang (Nachtdienst, krank) bleibt es eine Nacht.
#define FOLGENACHT_MIN 90
// Nach dem Fenster noch so lange, um eine offene Nacht abzuschliessen.
#define NACHLAUF_MIN 60
// DIE PLAETZE DER UHR SIND KNAPP: ACTIVITY_MAX_ACTIVITY_SESSIONS_COUNT (32) je
// Tag, geteilt mit Bewegung und Training. Eine Nacht meldet ihren Schlafblock
// und hoechstens so viele Tiefschlafbloecke; liegen mehr Tiefschlaf-Abschnitte
// vor, werden die mit der kleinsten Luecke zusammengelegt (die Luecke zaehlt
// nicht als Tiefschlaf, siehe nicht_schlaf_min).
#define TIEF_JE_NACHT_MAX 7
#define SITZUNGEN_JE_NACHT (1 + TIEF_JE_NACHT_MAX)
// Und das ganze Fenster - Nacht und Folgenacht zusammen - hoechstens so viele.
#define SITZUNGEN_JE_FENSTER (2 * SITZUNGEN_JE_NACHT)
// DIE LETZTEN ZWEI MINUTEN SIND UNSICHER: Cole-Kripke wiegt jede Minute mit
// den zwei folgenden (schlafanalyse.c); am Ende fehlen sie, und eine einzige
// Regung macht dort wach. So kurz wach am Ende gilt fuer den Schlafzustand
// noch als Schlaf - die naechste Rechnung klaert es.
#define RAND_UNSICHER_MIN 2
// So viel Kernel-Heap muss nach der Rechnung noch frei sein. Darunter wartet
// die Nacht eine Minute: andere Teile der Uhr holen ihren Speicher mit
// kernel_malloc_check, und fehlt er dort, startet die Uhr neu.
#define HEAP_RESERVE (8 * 1024)
// So lange nach dem Senden geht der Schub ans Telefon (wie Kieselsport): nicht
// im selben Minutenlauf, der schon gerechnet, geschrieben und geloggt hat.
#define SCHICKEN_NACH_MS (90 * MS_PER_SECOND)
// So viele Trainings merkt sich die Nacht - mehr macht niemand zwischen 20 und
// 11 Uhr, und die aeltesten liegen dann ohnehin vor dem Fenster. Eine lange
// Pause teilt ein Training in zwei.
#define TRAININGS_MAX 6
// EIN VERGESSENES TRAINING LOESCHT NICHT DIE NACHT. Kieselsport beendet ein
// Training nach einer halben Stunde Pause selbst (dienst.c, AUTO_ENDE_S); so
// lange zaehlt die Pause zum Training, laenger auch hier nicht. Ein laufendes
// Training beendet niemand von selbst: es zaehlt hoechstens drei Stunden,
// danach ist es vergessen, und der Schlaf darin zaehlt.
#define PAUSE_MAX_MIN 30
#define TRAINING_MAX_MIN (3 * MINUTES_PER_HOUR)

// DER ARBEITSSPEICHER EINER RECHNUNG, auf dem Heap (rund 20 KB). Die Minuten
// aus der Datei und die Arbeit der Analyse brauchen nie beide zugleich Platz:
// erst wird gelesen, dann gerechnet.
typedef struct {
  uint16_t bewegung[SCHLAF_MINUTEN_MAX];
  uint8_t puls[SCHLAF_MINUTEN_MAX];
  bool gueltig[SCHLAF_MINUTEN_MAX];
  SchlafHrv hrv[SCHLAF_HRV_MAX];
  union {
    HealthMinuteData minuten[SCHLAF_MINUTEN_MAX];
    SchlafArbeit arbeit;
  };
  SchlafErgebnis ergebnis;
  NachtBild bild;
  NachtDlsRecord satz;
  NachtHrvDlsRecord hrv_satz;
} Rechnung;

// DIE KIESELSPORT-TRAININGS, Minute fuer Minute mitgeschrieben: [von, bis).
// Gespeicherte Trainings stehen zwar als Sitzung in der Uhr, aber dort nur die
// des laufenden Tages (nach Mitternacht fehlt das Yoga von 21 Uhr) und ohne
// die Pausen - und ein verworfenes gar nicht.
typedef struct {
  time_t von;
  time_t bis;
} Spanne;
static Spanne s_trainings[TRAININGS_MAX];
static int s_trainings_naechstes;   //< Ringpuffer: hierhin kommt das naechste
static bool s_training_offen;       //< das juengste laeuft noch
static time_t s_zuletzt_gelaufen;   //< die letzte Minute, in der es nicht pausierte
// Ist das Aufwachen schon zu sehen, wird um diese Zeit noch einmal gerechnet,
// ausserhalb des Takts; 0 = nicht. Auch ein Versuch, der am Speicher scheiterte.
static time_t s_nachsehen_um;

// Die laufenden Sitzungen dieser Nacht - sie werden bei jeder Rechnung ersetzt.
static ActivitySession s_gemeldet[SITZUNGEN_JE_NACHT];
static int s_gemeldet_anzahl;
// SCHLAEFT MAN AM ENDE DER LETZTEN RECHNUNG, waechst der Block jede Minute
// mit, ohne neu zu rechnen: sonst saehe die Uhr zwischen zwei Rechnungen
// einen laengst vergangenen Schlaf und hielte einen fuer wach (Smart Alarm,
// Schlafzustand). Indizes in s_gemeldet, -1 = keiner.
static bool s_schlaeft_am_ende;
static int s_block = -1;
static int s_tief_am_ende = -1;
// Feste Sitzungen im Fenster s_fest_fenster, fuer SITZUNGEN_JE_FENSTER.
static int s_fest_im_fenster;
static time_t s_fest_fenster;
// Bis hierher ist gerechnet: das Aufwachen der zuletzt endgueltigen Nacht, oder
// das Ende des Fensters. Die naechste Rechnung beginnt dort.
static time_t s_erledigt_bis;
// Was zuletzt ans Telefon ging - dieselbe Nacht geht nur, wenn sie sich aendert.
static time_t s_gesendet_beginn;
static uint16_t s_gesendet_aufwachen;
static uint16_t s_gesendet_schlaf;
static DataLoggingSession *s_dls;
static DataLoggingSession *s_dls_hrv;
static TimerID s_schicken_takt = TIMER_INVALID_ID;
// DAS PROTOKOLL: Akku und Messstufe bei jedem Rechentakt im Fenster. Am Ende
// zaehlt der Stand beim Einschlafen - nicht der, als die Rechnung den Schlaf
// zum ersten Mal sah; das ist erst nach einer Stunde Schlaf, und nach einem
// Wachliegen spaeter. Die Messstufe ebenso: die beim Senden war die des Morgens.
#define PROTOKOLL_TAKTE ((FENSTER_DAUER_MIN + NACHLAUF_MIN) / TAKT_MIN + 1)
static uint8_t s_akku[PROTOKOLL_TAKTE];
static uint8_t s_messstufe[PROTOKOLL_TAKTE];
static time_t s_protokoll_fenster;
static bool s_geladen;

// DIE LETZTE NACHT FUER DIE HEALTH-APP. Geschrieben auf KernelBG, gelesen aus
// der App - daher die Sperre. Die fertige Nacht liegt zusaetzlich in einer
// Datei, damit sie einen Neustart uebersteht.
#define BILD_DATEI "boulder_nacht"
#define BILD_DATEI_GROESSE 2048
static const char BILD_SCHLUESSEL[] = "letzte";
static PBL_MUTEX_DEFINE(s_bild_sperre);
static NachtBild s_bild;
static bool s_bild_da;
static bool s_bild_gelesen;
static bool s_bild_in_datei;   //< s_bild steht so in der Datei

// WAS EINEN NEUSTART UEBERSTEHEN MUSS, damit die Uhr am Morgen nicht eine
// schon endgueltige Nacht neu rechnet - und sie mit dem Morgenschlaf zu einer
// verschmilzt, neben dem schon festen Block (Ueberlappung) - und eine nicht
// geschickte Nacht nicht fuer geschickt haelt.
#define NACHT_ZUSTAND_VERSION 1
static const char ZUSTAND_SCHLUESSEL[] = "zustand";
typedef struct PBL_PACKED {
  uint8_t version;
  uint32_t fenster;          //< Beginn des Fensters, fuer das die beiden naechsten gelten
  uint32_t erledigt_bis;
  uint8_t fest;
  uint32_t gesendet_beginn;
  uint16_t gesendet_aufwachen;
  uint16_t gesendet_schlaf;
} NachtZustand;
static NachtZustand s_zustand_datei;   //< so steht er in der Datei
static bool s_zustand_geladen;

// ---------------------------------------------------------------------------

// Das Fenster, in dem `utc` liegt - oder das zuletzt vergangene.
void boulder_nacht_fenster(time_t utc, time_t *beginn, time_t *ende) {
  const time_t lokal = time_utc_to_local(utc);
  const int minute_des_tages = (lokal % SECONDS_PER_DAY) / SECONDS_PER_MINUTE;
  const time_t tagesbeginn_lokal = lokal - (lokal % SECONDS_PER_DAY);
  time_t beginn_lokal = tagesbeginn_lokal + FENSTER_VON_MIN * SECONDS_PER_MINUTE;
  if (minute_des_tages < FENSTER_VON_MIN) {
    beginn_lokal -= SECONDS_PER_DAY;
  }
  // DER VERSATZ AM FENSTERBEGINN, nicht der von jetzt: in der Nacht der
  // Zeitumstellung aendert er sich um drei Uhr, und mit ihm rutschte der
  // Beginn der laufenden Nacht um eine Stunde - mitten in der Rechnung.
  time_t b = utc - (lokal - beginn_lokal);
  b = beginn_lokal - (time_utc_to_local(b) - b);
  *beginn = b;
  *ende = *beginn + FENSTER_DAUER_MIN * SECONDS_PER_MINUTE;
}

// Die Minuten [beginn, bis) aus der Minutendatei, IN EINEM GRIFF: jeder Griff
// oeffnet die Datei und geht ihre Saetze vom Anfang an durch. Rueckgabe: bis
// zur letzten vorhandenen Minute - DIE FEHLENDEN AM ENDE ZAEHLEN NICHT: als
// ungueltig gerechnet saehe jede Nacht so aus, als waere man eben aufgewacht.
static int prv_lies(Rechnung *r, time_t beginn, time_t bis) {
  int n = (bis - beginn) / SECONDS_PER_MINUTE;
  if (n > SCHLAF_MINUTEN_MAX) {
    n = SCHLAF_MINUTEN_MAX;
  }
  memset(r->gueltig, 0, sizeof(r->gueltig));
  memset(r->bewegung, 0, sizeof(r->bewegung));
  memset(r->puls, 0, sizeof(r->puls));
  if (n <= 0) {
    return 0;
  }
  uint32_t anzahl = n;
  time_t start = beginn;
  const bool gelesen = activity_algorithm_get_minute_history(r->minuten, &anzahl, &start);
  // Eine volle Datei durchzugehen dauert auf der Uhr: der Waechter soll das
  // nicht fuer ein Haengen halten.
  system_task_watchdog_feed();
  if (!gelesen) {
    PBL_LOG_WRN("Nacht: Minutendatei nicht lesbar");
    return 0;
  }
  int letzte = -1;
  for (uint32_t k = 0; k < anzahl; k++) {
    const int i = (start - beginn) / SECONDS_PER_MINUTE + (int)k;
    if (i < 0) {
      continue;
    }
    if (i >= n) {
      break;
    }
    const HealthMinuteData *m = &r->minuten[k];
    if (m->is_invalid || m->vmc == 0xFFFF) {
      continue;
    }
    r->bewegung[i] = m->vmc;
    r->puls[i] = (m->heart_rate_bpm == 0xFF) ? 0 : m->heart_rate_bpm;
    r->gueltig[i] = true;
    if (i > letzte) {
      letzte = i;
    }
  }
  return letzte + 1;
}

// Jede Minute: laeuft ein Kieselsport-Training (pausiert hoechstens
// PAUSE_MAX_MIN lang), gehoert ihm die eben vergangene Minute. Die Minute, in
// der es endet, zaehlt noch dazu.
static void prv_training_merken(time_t utc) {
#ifdef CONFIG_SERVICE_KIESELSPORT
  KsStand stand;
  kieselsport_stand(&stand);
  if (stand.zustand == LaufLaeuft) {
    s_zuletzt_gelaufen = utc;
  }
  const bool laeuft = stand.zustand == LaufLaeuft ||
      (stand.zustand == LaufPause &&
       utc - s_zuletzt_gelaufen <= PAUSE_MAX_MIN * SECONDS_PER_MINUTE);
#else
  const bool laeuft = false;
#endif
  Spanne *juengstes = &s_trainings[(s_trainings_naechstes + TRAININGS_MAX - 1) % TRAININGS_MAX];
  if (!laeuft) {
    if (s_training_offen) {
      juengstes->bis = utc;
      s_training_offen = false;
    }
    return;
  }
  if (!s_training_offen) {
    juengstes = &s_trainings[s_trainings_naechstes];
    s_trainings_naechstes = (s_trainings_naechstes + 1) % TRAININGS_MAX;
    *juengstes = (Spanne){ .von = utc - SECONDS_PER_MINUTE };
    s_training_offen = true;
  }
  juengstes->bis = utc;
}

// Die Minuten der Trainings zaehlen wie abgelegt: wach, nie Schlaf. Sie
// bleiben vorhanden - ein Training am Morgen ist das Aufstehen. Hoechstens
// TRAINING_MAX_MIN je Training.
static void prv_trainings_raus(Rechnung *r, time_t beginn, int n) {
  for (int k = 0; k < TRAININGS_MAX; k++) {
    const Spanne *s = &s_trainings[k];
    const time_t ende = (s->bis - s->von > TRAINING_MAX_MIN * SECONDS_PER_MINUTE)
        ? s->von + TRAINING_MAX_MIN * SECONDS_PER_MINUTE : s->bis;
    if (ende <= s->von || ende <= beginn) {
      continue;
    }
    const int von = (s->von <= beginn) ? 0 : (s->von - beginn) / SECONDS_PER_MINUTE;
    const int bis = (ende - beginn + SECONDS_PER_MINUTE - 1) / SECONDS_PER_MINUTE;
    for (int i = von; i < bis && i < n; i++) {
      r->gueltig[i] = false;
    }
  }
}

// ---------------------------------------------------------------------------
// Die Sitzungen der Nacht

// Eine Sitzung melden. Rueckgabe: ihr Platz in s_gemeldet, -1 = keiner (fest
// oder nicht gespeichert).
static int prv_neu(ActivitySessionType art, time_t start, int minuten, int nicht_schlaf,
                   bool laufend) {
  if (minuten <= 0) {
    return -1;
  }
  ActivitySession s = {
    .start_utc = start,
    .length_min = minuten,
    .type = art,
    .ongoing = laufend,
    .nicht_schlaf_min = (nicht_schlaf > 0) ? nicht_schlaf : 0,
  };
  if (!activity_sessions_prv_add_activity_session(&s)) {
    PBL_LOG_WRN("Nacht: Sitzung %d nicht gespeichert, die Uhr ist voll", (int)art);
    return -1;
  }
  if (!laufend) {
    s_fest_im_fenster++;
    return -1;
  }
  if (s_gemeldet_anzahl >= SITZUNGEN_JE_NACHT) {
    return -1;
  }
  s_gemeldet[s_gemeldet_anzahl] = s;
  return s_gemeldet_anzahl++;
}

// Die Tiefschlaf-Abschnitte als Bloecke nach `liste`, hoechstens `max`: zu
// viele werden an der kleinsten Luecke zusammengelegt. Rueckgabe: wie viele.
static int prv_tief_bloecke(const SchlafErgebnis *e, SchlafAbschnitt *liste, int max) {
  if (max <= 0) {
    return 0;
  }
  int t = 0;
  for (int k = 0; k < e->abschnitt_anzahl; k++) {
    if (e->abschnitte[k].phase == SchlafPhaseTief) {
      liste[t++] = e->abschnitte[k];
    }
  }
  while (t > max) {
    int k = 0;
    for (int i = 1; i + 1 < t; i++) {
      if (liste[i + 1].von - liste[i].bis < liste[k + 1].von - liste[k].bis) {
        k = i;
      }
    }
    liste[k].bis = liste[k + 1].bis;
    memmove(&liste[k + 1], &liste[k + 2], (t - k - 2) * sizeof(*liste));
    t--;
  }
  return t;
}

// Die Tiefschlafminuten in [von, bis).
static int prv_tief_minuten(const SchlafErgebnis *e, int von, int bis) {
  int summe = 0;
  for (int k = 0; k < e->abschnitt_anzahl; k++) {
    const SchlafAbschnitt *a = &e->abschnitte[k];
    const int a_von = (a->von > von) ? a->von : von;
    const int a_bis = (a->bis < bis) ? a->bis : bis;
    if (a->phase == SchlafPhaseTief && a_bis > a_von) {
      summe += a_bis - a_von;
    }
  }
  return summe;
}

// Die laufenden Sitzungen zuruecknehmen - AUCH EINE, DIE EIN ANDERER TEIL DER UHR
// INZWISCHEN ABGESCHLOSSEN HAT (workout_service.c tut das beim Trainingsstart,
// sobald dort num_sessions berichtigt ist). Die Nacht ist deshalb nicht fertig;
// verschiebt die naechste Rechnung ihren Beginn, stuende sonst ein zweiter Block
// neben dem neuen. Das Loeschen der Firmware hielte dabei die Uhr an
// (PBL_ASSERT) - darum der eigene Weg in activity_sessions.c.
static void prv_zuruecknehmen(void) {
  for (int k = 0; k < s_gemeldet_anzahl; k++) {
    activity_sessions_prv_eigene_zuruecknehmen(&s_gemeldet[k]);
  }
  s_gemeldet_anzahl = 0;
  s_block = -1;
  s_tief_am_ende = -1;
  s_schlaeft_am_ende = false;
}

// DIE NACHT IST EIN BLOCK, vom Einschlafen bis zum Aufwachen; das Wachliegen
// dazwischen bleibt darin und zaehlt nicht zur Schlafsumme (nicht_schlaf_min).
// Im Satz fuer Boulder und im Bild der Health-App steht es weiter als Wach.
// Dazu die Tiefschlafbloecke. Laufend bis zur endgueltigen Nacht: bis dahin
// ersetzt jede Rechnung alle Sitzungen der Nacht.
static void prv_melde(Rechnung *r, time_t beginn, int anzahl, bool endgueltig) {
  const SchlafErgebnis *e = &r->ergebnis;
  prv_zuruecknehmen();
  if (!e->gefunden) {
    return;
  }
  int frei = SITZUNGEN_JE_FENSTER - s_fest_im_fenster;
  if (frei <= 0) {
    PBL_LOG_WRN("Nacht: keine Sitzung mehr frei im Fenster");
    return;
  }
  const bool laufend = !endgueltig;
  const int laenge = e->aufwachen - e->einschlafen;
  s_block = prv_neu(ActivitySessionType_Sleep, beginn + e->einschlafen * SECONDS_PER_MINUTE,
                    laenge, laenge - e->schlaf_minuten, laufend);
  frei--;

  // Die Arbeitsliste der Rechnung ist frei - sie dient als Ablage.
  SchlafAbschnitt *tief = r->arbeit.liste;
  const int n = prv_tief_bloecke(e, tief, (frei < TIEF_JE_NACHT_MAX) ? frei : TIEF_JE_NACHT_MAX);
  for (int k = 0; k < n; k++) {
    int von = tief[k].von;
    // DIE FIRMWARE HAELT ALLE SCHLAFARTEN MIT GLEICHEM BEGINN FUER DIESELBE
    // SITZUNG (activity_sessions.c, any_sleep): ein Tiefschlaf, der mit dem
    // Block beginnt, ueberschriebe ihn. Also eine Minute spaeter.
    if (von == e->einschlafen) {
      von++;
    }
    const int minuten = tief[k].bis - von;
    const int platz = prv_neu(ActivitySessionType_RestfulSleep,
                              beginn + von * SECONDS_PER_MINUTE, minuten,
                              minuten - prv_tief_minuten(e, von, tief[k].bis), laufend);
    if (tief[k].bis == e->aufwachen) {
      s_tief_am_ende = platz;
    }
  }
  s_schlaeft_am_ende = laufend && s_block >= 0 && anzahl - e->aufwachen <= RAND_UNSICHER_MIN;
}

// Eine laufende Sitzung bis `ende` (Minute fuer Minute).
static void prv_bis(ActivitySession *s, time_t ende) {
  if (ende <= s->start_utc) {
    return;
  }
  const time_t minuten = (ende - s->start_utc) / SECONDS_PER_MINUTE;
  if (minuten == s->length_min || minuten > ACTIVITY_SESSION_MAX_LENGTH_MIN ||
      minuten <= s->nicht_schlaf_min) {
    return;
  }
  s->length_min = minuten;
  activity_sessions_prv_add_activity_session(s);
}

// DER SCHLAFBLOCK STEHT DORT, WO DER SCHLAFZUSTAND DER UHR NACHSIEHT: bei der
// zuletzt verarbeiteten Minute der Schlaferkennung (activity_sessions.c sieht
// einen ab mehr als einer Minute Abstand fuer wach an), eine Minute weiter,
// weil sie bis zum naechsten Minutenlauf eine weiter ist. Ein Ende in der
// Zukunft hiesse fuer sie ebenso wach wie eines in der Vergangenheit.
static void prv_fortschreiben(void) {
  if (!s_schlaeft_am_ende) {
    return;
  }
  const time_t verarbeitet = activity_algorithm_get_last_sleep_utc();
  if (verarbeitet == 0) {
    return;
  }
  const time_t ende = verarbeitet + SECONDS_PER_MINUTE;
  prv_bis(&s_gemeldet[s_block], ende);
  if (s_tief_am_ende >= 0) {
    prv_bis(&s_gemeldet[s_tief_am_ende], ende);
  }
}

// Laufende Sitzungen einer Nacht, deren Fenster vorbei ist, abschliessen, wie
// sie sind - etwa wenn das Senden bis zuletzt scheiterte. Sonst nahme sie die
// erste Rechnung der naechsten Nacht zurueck.
static void prv_festschreiben(void) {
  for (int k = 0; k < s_gemeldet_anzahl; k++) {
    s_gemeldet[k].ongoing = false;
    activity_sessions_prv_add_activity_session(&s_gemeldet[k]);
    s_fest_im_fenster++;
  }
  s_gemeldet_anzahl = 0;
  s_block = -1;
  s_tief_am_ende = -1;
  s_schlaeft_am_ende = false;
}

// ---------------------------------------------------------------------------
// Das Bild der Nacht fuer die Uhr

// Unter s_bild_sperre: die zuletzt fertige Nacht aus der Datei, einmal nach
// einem Neustart. Es zaehlt ein Bild dieser Fassung - oder eines von b24, das
// denselben Aufbau hat (nacht.h): sonst fehlte der Health-App nach dem Update
// die letzte Nacht, und prv_zustand_laden faende nicht, was schon geschickt ist.
static void prv_bild_laden(void) {
  if (s_bild_da || s_bild_gelesen) {
    return;
  }
  s_bild_gelesen = true;
  SettingsFile datei;
  if (settings_file_open(&datei, BILD_DATEI, BILD_DATEI_GROESSE) != S_SUCCESS) {
    return;
  }
  if (settings_file_get_len(&datei, BILD_SCHLUESSEL, sizeof(BILD_SCHLUESSEL)) ==
          (int)sizeof(s_bild) &&
      settings_file_get(&datei, BILD_SCHLUESSEL, sizeof(BILD_SCHLUESSEL), &s_bild,
                        sizeof(s_bild)) == S_SUCCESS) {
    if (s_bild.version == NACHT_BILD_VERSION) {
      s_bild_da = true;
      s_bild_in_datei = true;
    } else if (s_bild.version == NACHT_BILD_VERSION_B24) {
      // Umzug: nur die Fassung kommt dazu. In der Datei steht es noch als b24;
      // die naechste fertige Rechnung schreibt es in dieser Fassung.
      s_bild.version = NACHT_BILD_VERSION;
      s_bild_da = true;
      s_bild_in_datei = false;
      PBL_LOG_INFO("Nacht: Bild aus b24 uebernommen");
    } else {
      PBL_LOG_WRN("Nacht: Bild in Fassung %d verworfen", (int)s_bild.version);
      memset(&s_bild, 0, sizeof(s_bild));
    }
  }
  settings_file_close(&datei);
}

static void prv_bild_merken(Rechnung *r, time_t beginn, bool fertig) {
  const SchlafErgebnis *e = &r->ergebnis;
  if (!e->gefunden) {
    return;
  }
  // Die Arbeitsliste der Rechnung ist jetzt frei - sie dient als Ablage.
  SchlafAbschnitt *a = r->arbeit.liste;
  memcpy(a, e->abschnitte, e->abschnitt_anzahl * sizeof(*a));
  const int n = schlafanalyse_verdichten(a, e->abschnitt_anzahl, NACHT_BILD_ABSCHNITTE);

  // Ganz genullt, auch die Fuellbytes: verglichen wird Byte fuer Byte.
  NachtBild *b = &r->bild;
  memset(b, 0, sizeof(*b));
  b->beginn_utc = (uint32_t)beginn;
  b->utc_to_local = time_utc_to_local(beginn) - beginn;
  b->einschlafen = e->einschlafen;
  b->aufwachen = e->aufwachen;
  b->schlaf_minuten = e->schlaf_minuten;
  b->hrv = e->hrv;
  b->ruhepuls = e->ruhepuls;
  b->fertig = fertig;
  b->anzahl = (uint8_t)n;
  b->version = NACHT_BILD_VERSION;
  // Die Minuten je Phase aus der ungekuerzten Liste - die verdichtete
  // verschoebe sie.
  for (int k = 0; k < e->abschnitt_anzahl; k++) {
    if (e->abschnitte[k].phase < NACHT_PHASEN) {
      b->minuten[e->abschnitte[k].phase] += e->abschnitte[k].bis - e->abschnitte[k].von;
    }
  }
  for (int k = 0; k < n; k++) {
    b->abschnitte[k] = (NachtDlsAbschnitt){
      .von = a[k].von, .bis = a[k].bis, .phase = a[k].phase,
    };
  }

  pbl_mutex_lock(&s_bild_sperre, PBL_FOREVER);
  const bool gleich = s_bild_da && memcmp(b, &s_bild, sizeof(*b)) == 0;
  const bool schreiben = fertig && !(gleich && s_bild_in_datei);
  if (!gleich) {
    s_bild = *b;
    s_bild_da = true;
    s_bild_in_datei = false;
  }
  pbl_mutex_unlock(&s_bild_sperre);

  // NUR WAS SICH GEAENDERT HAT, kommt in die Datei, und ausserhalb der Sperre:
  // nach dem Aufwachen rechnet die Uhr bis zu zwei Stunden lang dieselbe Nacht
  // nach, und die Health-App soll waehrenddessen nicht auf den Flash warten.
  if (!schreiben) {
    return;
  }
  SettingsFile datei;
  bool geschrieben = false;
  if (settings_file_open(&datei, BILD_DATEI, BILD_DATEI_GROESSE) == S_SUCCESS) {
    geschrieben = settings_file_set(&datei, BILD_SCHLUESSEL, sizeof(BILD_SCHLUESSEL), b,
                                    sizeof(*b)) == S_SUCCESS;
    settings_file_close(&datei);
  }
  system_task_watchdog_feed();
  if (!geschrieben) {
    PBL_LOG_WRN("Nacht: Bild nicht gespeichert");
    return;
  }
  pbl_mutex_lock(&s_bild_sperre, PBL_FOREVER);
  s_bild_in_datei = memcmp(b, &s_bild, sizeof(*b)) == 0;
  pbl_mutex_unlock(&s_bild_sperre);
}

bool boulder_nacht_letzte(NachtBild *aus) {
  pbl_mutex_lock(&s_bild_sperre, PBL_FOREVER);
  prv_bild_laden();
  const bool da = s_bild_da;
  if (da) {
    *aus = s_bild;
  }
  pbl_mutex_unlock(&s_bild_sperre);
  return da;
}

// ---------------------------------------------------------------------------
// Was einen Neustart uebersteht

static void prv_zustand_laden(time_t beginn) {
  if (s_zustand_geladen) {
    return;
  }
  s_zustand_geladen = true;
  NachtZustand z = { 0 };
  bool da = false;
  SettingsFile datei;
  if (settings_file_open(&datei, BILD_DATEI, BILD_DATEI_GROESSE) == S_SUCCESS) {
    da = settings_file_get_len(&datei, ZUSTAND_SCHLUESSEL, sizeof(ZUSTAND_SCHLUESSEL)) ==
             (int)sizeof(z) &&
         settings_file_get(&datei, ZUSTAND_SCHLUESSEL, sizeof(ZUSTAND_SCHLUESSEL), &z,
                           sizeof(z)) == S_SUCCESS &&
         z.version == NACHT_ZUSTAND_VERSION;
    settings_file_close(&datei);
  }
  if (da) {
    s_zustand_datei = z;
    s_gesendet_beginn = z.gesendet_beginn;
    s_gesendet_aufwachen = z.gesendet_aufwachen;
    s_gesendet_schlaf = z.gesendet_schlaf;
    // Nur fuer dasselbe Fenster: am naechsten Abend beginnt alles neu.
    if ((time_t)z.fenster == beginn) {
      s_erledigt_bis = z.erledigt_bis;
      s_fest_im_fenster = z.fest;
      s_fest_fenster = beginn;
    }
    return;
  }
  // Von einer Fassung ohne diesen Zustand: die zuletzt geschickte Nacht ist die
  // fertige im Bild - sonst ginge dieselbe Nacht noch einmal ans Telefon.
  pbl_mutex_lock(&s_bild_sperre, PBL_FOREVER);
  prv_bild_laden();
  if (s_bild_da && s_bild.fertig) {
    s_gesendet_beginn = s_bild.beginn_utc;
    s_gesendet_aufwachen = s_bild.aufwachen;
    s_gesendet_schlaf = s_bild.schlaf_minuten;
  }
  pbl_mutex_unlock(&s_bild_sperre);
}

// Den Zustand schreiben - nur, wenn er sich geaendert hat.
static void prv_zustand_schreiben(time_t beginn) {
  const NachtZustand z = {
    .version = NACHT_ZUSTAND_VERSION,
    .fenster = (uint32_t)beginn,
    .erledigt_bis = (uint32_t)s_erledigt_bis,
    .fest = (uint8_t)s_fest_im_fenster,
    .gesendet_beginn = (uint32_t)s_gesendet_beginn,
    .gesendet_aufwachen = s_gesendet_aufwachen,
    .gesendet_schlaf = s_gesendet_schlaf,
  };
  if (memcmp(&z, &s_zustand_datei, sizeof(z)) == 0) {
    return;
  }
  SettingsFile datei;
  if (settings_file_open(&datei, BILD_DATEI, BILD_DATEI_GROESSE) != S_SUCCESS) {
    PBL_LOG_WRN("Nacht: Zustand nicht gespeichert");
    return;
  }
  if (settings_file_set(&datei, ZUSTAND_SCHLUESSEL, sizeof(ZUSTAND_SCHLUESSEL), &z,
                        sizeof(z)) == S_SUCCESS) {
    s_zustand_datei = z;
  } else {
    PBL_LOG_WRN("Nacht: Zustand nicht gespeichert");
  }
  settings_file_close(&datei);
}

// ---------------------------------------------------------------------------
// Ans Telefon

// Der protokollierte Wert zum ersten Takt ab `utc`; 0xFF, wenn keiner da ist.
static uint8_t prv_bei(const uint8_t *protokoll, time_t utc) {
  for (int i = (utc - s_protokoll_fenster + TAKT_MIN * SECONDS_PER_MINUTE - 1) /
               (TAKT_MIN * SECONDS_PER_MINUTE);
       i >= 0 && i < PROTOKOLL_TAKTE; i++) {
    if (protokoll[i] != 0xFF) {
      return protokoll[i];
    }
  }
  return 0xFF;
}

// Eine Datalogging-Sitzung, angelegt, wenn noch keine da ist. Das Anlegen
// schickt ein Oeffnen ans Telefon und wartet bis zu vier Sekunden auf Platz
// im Bluetooth-Puffer - vorher den Waechter fuettern, damit zwei Sitzungen in
// einem Lauf nicht zusammen ueber seine Frist kommen.
static bool prv_sitzung(DataLoggingSession **sitzung, uint32_t tag, uint16_t groesse) {
  if (*sitzung) {
    return true;
  }
  system_task_watchdog_feed();
  Uuid system_uuid = UUID_SYSTEM;
  *sitzung = dls_create(tag, DATA_LOGGING_BYTE_ARRAY, groesse, false /*buffered*/,
                        false /*resume*/, &system_uuid);
  system_task_watchdog_feed();
  if (!*sitzung) {
    PBL_LOG_WRN("Nacht: keine Datalogging-Sitzung fuer Tag %d", (int)tag);
    return false;
  }
  return true;
}

// Einen Satz loggen. Eine geschlossene oder verschwundene Sitzung wird beim
// naechsten Versuch neu angelegt.
static bool prv_log(DataLoggingSession **sitzung, const void *satz) {
  const DataLoggingResult ergebnis = dls_log(*sitzung, satz, 1);
  if (ergebnis == DATA_LOGGING_SUCCESS) {
    return true;
  }
  PBL_LOG_WRN("Nacht: Datalogging-Fehler %d", (int)ergebnis);
  if (ergebnis == DATA_LOGGING_CLOSED || ergebnis == DATA_LOGGING_NOT_FOUND) {
    *sitzung = NULL;
  }
  return false;
}

// Die ganze Nacht an Boulder, in Teilen zu NACHT_DLS_ABSCHNITTE Abschnitten.
// Rueckgabe: alle Teile im Datalogging.
static bool prv_schicke(Rechnung *r, time_t beginn) {
  const SchlafErgebnis *e = &r->ergebnis;
  if (!prv_sitzung(&s_dls, BOULDER_DLS_TAG_NACHT, sizeof(NachtDlsRecord))) {
    return false;
  }
  const time_t eingeschlafen = beginn + e->einschlafen * SECONDS_PER_MINUTE;
  const uint8_t messstufe = prv_bei(s_messstufe, eingeschlafen);
  const int teile = (e->abschnitt_anzahl + NACHT_DLS_ABSCHNITTE - 1) / NACHT_DLS_ABSCHNITTE;
  for (int teil = 0; teil < (teile ? teile : 1); teil++) {
    NachtDlsRecord *s = &r->satz;
    memset(s, 0, sizeof(*s));
    *s = (NachtDlsRecord){
      .version = NACHT_DLS_VERSION,
      .size = sizeof(NachtDlsRecord),
      .beginn_utc = (uint32_t)beginn,
      .utc_to_local = time_utc_to_local(beginn) - beginn,
      .einschlafen = e->einschlafen,
      .aufwachen = e->aufwachen,
      .schlaf_minuten = e->schlaf_minuten,
      .ruhepuls = e->ruhepuls,
      .teil = teil,
      .teile = teile,
      .hrv = e->hrv,
      .akku_beginn = prv_bei(s_akku, eingeschlafen),
      .akku_ende = battery_get_charge_state().charge_percent,
      .flags = s_geladen ? NACHT_DLS_FLAG_GELADEN : 0,
      .messstufe = (messstufe != 0xFF) ? messstufe : boulder_messstufe(),
    };
    for (int k = teil * NACHT_DLS_ABSCHNITTE;
         k < e->abschnitt_anzahl && s->anzahl < NACHT_DLS_ABSCHNITTE; k++) {
      s->abschnitte[s->anzahl++] = (NachtDlsAbschnitt){
        .von = e->abschnitte[k].von,
        .bis = e->abschnitte[k].bis,
        .phase = e->abschnitte[k].phase,
      };
    }
    if (!prv_log(&s_dls, s)) {
      return false;
    }
  }
  return true;
}

// Die HRV-Fenster der Nacht an Boulder (eigener Tag), gleich hinter dem Satz der
// Nacht. Ohne Fenster kein Satz: die Nacht wurde ohne HRV gerechnet.
static bool prv_schicke_hrv(Rechnung *r, time_t beginn, int hrv_anzahl) {
  if (hrv_anzahl <= 0) {
    return true;
  }
  if (!prv_sitzung(&s_dls_hrv, BOULDER_DLS_TAG_NACHT_HRV, sizeof(NachtHrvDlsRecord))) {
    return false;
  }
  const int teile = (hrv_anzahl + NACHT_HRV_DLS_FENSTER - 1) / NACHT_HRV_DLS_FENSTER;
  for (int teil = 0; teil < teile; teil++) {
    NachtHrvDlsRecord *s = &r->hrv_satz;
    memset(s, 0, sizeof(*s));
    *s = (NachtHrvDlsRecord){
      .version = NACHT_HRV_DLS_VERSION,
      .size = sizeof(NachtHrvDlsRecord),
      .beginn_utc = (uint32_t)beginn,
      .teil = teil,
      .teile = teile,
    };
    for (int k = teil * NACHT_HRV_DLS_FENSTER;
         k < hrv_anzahl && s->anzahl < NACHT_HRV_DLS_FENSTER; k++) {
      s->fenster[s->anzahl++] = (NachtHrvDlsFenster){
        .minute = r->hrv[k].minute,
        .rmssd = r->hrv[k].rmssd,
        .puls = r->hrv[k].puls,
      };
    }
    if (!prv_log(&s_dls_hrv, s)) {
      return false;
    }
  }
  return true;
}

static void prv_schicken_faellig(void *daten) {
  health_sync_schicken();
}

// Gleich los, nicht erst mit dem Datalogging-Takt: die Nacht soll kurz nach
// dem Aufstehen auf dem Telefon sein. Aber NICHT IN DIESEM MINUTENLAUF: er hat
// gerechnet, geschrieben und geloggt, und in derselben Minute beginnt oft ein
// HRV-Fenster. Der Schub kommt mit eigenem Zeitgeber spaeter.
static void prv_schicken_spaeter(void) {
  if (s_schicken_takt == TIMER_INVALID_ID) {
    s_schicken_takt = new_timer_create();
  }
  if (s_schicken_takt == TIMER_INVALID_ID ||
      !new_timer_start(s_schicken_takt, SCHICKEN_NACH_MS, prv_schicken_faellig, NULL, 0)) {
    PBL_LOG_WRN("Nacht: kein Zeitgeber, der Schub kommt mit dem Datalogging-Takt");
  }
}

bool boulder_nacht_im_fenster(time_t utc) {
  time_t beginn;
  time_t ende;
  boulder_nacht_fenster(utc, &beginn, &ende);
  return utc >= beginn && utc < ende;
}

bool boulder_nachtruhe(time_t utc, uint16_t letzte_bewegung) {
  // Dieselbe Schwelle wie im Schlaf: was darueber liegt, ist wach.
  return boulder_nacht_im_fenster(utc) && letzte_bewegung <= SCHLAF_SCHWELLE_VMC;
}

// Reicht der Kernel-Heap fuer die Rechnung - und bleibt danach genug fuer den
// Rest der Uhr?
static bool prv_platz_fuer_rechnung(void) {
  unsigned int belegt = 0;
  unsigned int frei = 0;
  unsigned int am_stueck = 0;
  heap_calc_totals(kernel_heap_get(), &belegt, &frei, &am_stueck);
  if (am_stueck >= sizeof(Rechnung) && frei >= sizeof(Rechnung) + HEAP_RESERVE) {
    return true;
  }
  PBL_LOG_WRN("Nacht: Heap knapp (%u frei, %u am Stueck), in einer Minute wieder", frei,
              am_stueck);
  return false;
}

// ---------------------------------------------------------------------------

void boulder_nacht_minute(time_t utc_sec) {
  prv_training_merken(utc_sec);
  prv_fortschreiben();
  const bool nachsehen = s_nachsehen_um != 0 && utc_sec >= s_nachsehen_um;
  if ((utc_sec / SECONDS_PER_MINUTE) % TAKT_MIN != 0 && !nachsehen) {
    return;
  }
  time_t beginn;
  time_t ende;
  boulder_nacht_fenster(utc_sec, &beginn, &ende);
  prv_zustand_laden(beginn);
  if (s_fest_fenster != beginn) {
    s_fest_fenster = beginn;
    s_fest_im_fenster = 0;
  }

  // Das Protokoll laeuft ab Beginn des Fensters mit, auch in der ersten
  // Stunde, in der noch nicht gerechnet wird.
  if (s_protokoll_fenster != beginn) {
    s_protokoll_fenster = beginn;
    memset(s_akku, 0xFF, sizeof(s_akku));
    memset(s_messstufe, 0xFF, sizeof(s_messstufe));
    s_geladen = false;
  }
  const BatteryChargeState akku = battery_get_charge_state();
  const int takt = (utc_sec - beginn) / (TAKT_MIN * SECONDS_PER_MINUTE);
  if (takt >= 0 && takt < PROTOKOLL_TAKTE) {
    s_akku[takt] = akku.charge_percent;
    s_messstufe[takt] = boulder_messstufe();
  }

  if (s_erledigt_bis >= ende || utc_sec < beginn + MINUTES_PER_HOUR * SECONDS_PER_MINUTE ||
      utc_sec > ende + NACHLAUF_MIN * SECONDS_PER_MINUTE) {
    s_nachsehen_um = 0;
    if (s_gemeldet_anzahl > 0 && utc_sec > ende) {
      prv_festschreiben();
      prv_zustand_schreiben(beginn);
    }
    return;
  }

  if (!prv_platz_fuer_rechnung()) {
    s_nachsehen_um = utc_sec + SECONDS_PER_MINUTE;
    return;
  }
  Rechnung *r = kernel_malloc(sizeof(Rechnung));
  if (!r) {
    PBL_LOG_WRN("Nacht: kein Speicher fuer die Rechnung, in einer Minute wieder");
    s_nachsehen_um = utc_sec + SECONDS_PER_MINUTE;
    return;
  }
  s_nachsehen_um = 0;
  const time_t ab = (s_erledigt_bis > beginn) ? s_erledigt_bis : beginn;
  const time_t bis = (utc_sec < ende) ? utc_sec : ende;
  const int anzahl = prv_lies(r, ab, bis);
  prv_trainings_raus(r, ab, anzahl);
  const int hrv_anzahl = boulder_hrv_fenster(ab, bis, r->hrv, SCHLAF_HRV_MAX);
  // Liegt in diesem Fenster schon eine endgueltige Nacht, ist alles danach
  // Morgenschlaf - erst ab FOLGENACHT_MIN eine eigene Nacht.
  const bool folgenacht = s_erledigt_bis > beginn && s_erledigt_bis < ende;
  const SchlafEingabe eingabe = {
    .bewegung = r->bewegung,
    .puls = r->puls,
    .gueltig = r->gueltig,
    .anzahl = anzahl,
    .schwelle = SCHLAF_SCHWELLE_VMC,
    .hrv = r->hrv,
    .hrv_anzahl = hrv_anzahl,
    .min_schlaf = folgenacht ? FOLGENACHT_MIN : 0,
  };
  schlafanalyse_werte(&eingabe, &r->arbeit, &r->ergebnis);
  system_task_watchdog_feed();

  // Hing die Uhr, seit Schlaf da ist, am Kabel? Dann sagt der Verbrauch nichts.
  s_geladen = s_geladen || (r->ergebnis.gefunden && akku.is_plugged);

  // Fertig: nach einer Viertelstunde wach - an vorhandenen Minuten gemessen,
  // nicht an der Uhrzeit - oder am Ende des Fensters. Endgueltig erst nach
  // zwei Stunden wach; bis dahin kann die Nacht noch wachsen.
  const SchlafErgebnis *e = &r->ergebnis;
  const int wach = e->gefunden ? anzahl - e->aufwachen : 0;
  const bool fertig = (utc_sec >= ende) || wach >= WACH_FERTIG_MIN;
  const bool endgueltig = (utc_sec >= ende) || wach >= WACH_ENDGUELTIG_MIN;

  // ERST ALS GESCHICKT MERKEN, WENN ALLES IM DATALOGGING STEHT. Scheitert es,
  // versucht es der naechste Takt noch einmal, und die Nacht bleibt so lange
  // offen - als endgueltig gerechnet kaeme sie nie wieder dran.
  bool geschickt = true;
  if (fertig && e->gefunden &&
      (ab != s_gesendet_beginn || e->aufwachen != s_gesendet_aufwachen ||
       e->schlaf_minuten != s_gesendet_schlaf)) {
    PBL_LOG_INFO("Nacht fertig: %d min Schlaf, Ruhepuls %d", e->schlaf_minuten, e->ruhepuls);
    geschickt = prv_schicke(r, ab) && prv_schicke_hrv(r, ab, hrv_anzahl);
    if (geschickt) {
      // Die Minuten der Nacht gehen mit dem Schub mit, damit Boulder sie
      // zusammen hat. Einmal je geschriebenem Satz, nicht je Teil. Wird die
      // Nacht spaeter endgueltig, ohne sich zu aendern, gibt es absichtlich
      // keinen Schub: der Satz traegt kein Endgueltig-Merkmal, es gibt nichts
      // Neues; die Minuten danach kommen mit dem Takt.
      prv_schicken_spaeter();
      s_gesendet_beginn = ab;
      s_gesendet_aufwachen = e->aufwachen;
      s_gesendet_schlaf = e->schlaf_minuten;
    }
  }
  const bool abschliessen = endgueltig && geschickt;
  prv_melde(r, ab, anzahl, abschliessen);
  prv_fortschreiben();
  prv_bild_merken(r, ab, fertig);
  if (abschliessen) {
    s_erledigt_bis = (e->gefunden && utc_sec < ende)
        ? ab + e->aufwachen * SECONDS_PER_MINUTE : ende;
    s_geladen = false;
  } else if (!fertig && wach > 0) {
    // DAS AUFWACHEN IST SCHON ZU SEHEN: noch einmal rechnen, sobald die
    // Viertelstunde wach voll sein kann, nicht erst im naechsten Takt. So ist
    // die Nacht bis zu einem Takt frueher fertig und auf dem Telefon; der
    // Datalogging-Takt selbst bleibt, wie er ist.
    s_nachsehen_um = utc_sec + (WACH_FERTIG_MIN - wach) * SECONDS_PER_MINUTE;
  }
  prv_zustand_schreiben(beginn);
  kernel_free(r);
}

#ifdef UNITTEST
size_t boulder_nacht_test_rechnung_groesse(void) {
  return sizeof(Rechnung);
}

void boulder_nacht_test_reset(void) {
  s_gemeldet_anzahl = 0;
  s_block = -1;
  s_tief_am_ende = -1;
  s_schlaeft_am_ende = false;
  s_fest_im_fenster = 0;
  s_fest_fenster = 0;
  s_erledigt_bis = 0;
  s_gesendet_beginn = 0;
  s_gesendet_aufwachen = 0;
  s_gesendet_schlaf = 0;
  s_dls = NULL;
  s_dls_hrv = NULL;
  s_schicken_takt = TIMER_INVALID_ID;
  memset(s_akku, 0xFF, sizeof(s_akku));
  memset(s_messstufe, 0xFF, sizeof(s_messstufe));
  s_protokoll_fenster = 0;
  s_geladen = false;
  s_bild_da = false;
  s_bild_gelesen = false;
  s_bild_in_datei = false;
  memset(&s_bild, 0, sizeof(s_bild));
  memset(&s_zustand_datei, 0, sizeof(s_zustand_datei));
  s_zustand_geladen = false;
  memset(s_trainings, 0, sizeof(s_trainings));
  s_trainings_naechstes = 0;
  s_training_offen = false;
  s_zuletzt_gelaufen = 0;
  s_nachsehen_um = 0;
}
#endif
