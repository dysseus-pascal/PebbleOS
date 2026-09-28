/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/activity/boulder/nacht.h"

#include "applib/data_logging.h"
#include "kernel/pbl_malloc.h"
#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/activity_algorithm.h"
#include "pbl/services/activity/activity_private.h"
#include "pbl/services/activity/boulder/schlafanalyse.h"
#include "pbl/services/data_logging/data_logging_service.h"
#include "util/time/time.h"
#include <pbl/logging/logging.h>

#include <string.h>

// Das Nachtfenster in Minuten des Tages (Ortszeit): 20 bis 11 Uhr.
#define FENSTER_VON_MIN (20 * MINUTES_PER_HOUR)
#define FENSTER_DAUER_MIN (15 * MINUTES_PER_HOUR)
// So oft wird gerechnet. Jede Minute waere Strom fuer nichts: die Minutendatei
// haelt ohnehin die letzten rund zwanzig Minuten zurueck, bis sie sicher sind.
#define TAKT_MIN 15
// So lange wach nach dem letzten Schlaf, und die Nacht ist vorbei. Kurz, damit
// die Nacht bald nach dem Aufstehen da ist; ein laengeres Wachliegen mitten in
// der Nacht zerlegt sie trotzdem nicht - was danach kommt, wird eine eigene
// Nacht (siehe s_erledigt_bis), und ein Schlaf unter einer Stunde davor ist
// keine.
#define WACH_FERTIG_MIN 15
// Nach dem Fenster noch so lange, um eine offene Nacht abzuschliessen.
#define NACHLAUF_MIN 60
// Mehr Schlafbloecke und Tiefschlaf-Abschnitte traegt keine Nacht ein; die Uhr
// haelt je Tag hoechstens ACTIVITY_MAX_ACTIVITY_SESSIONS_COUNT Sitzungen.
#define SITZUNGEN_MAX 24
// So viele Minuten je Griff in die Minutendatei.
#define LESE_BLOCK 60

typedef struct {
  uint16_t bewegung[SCHLAF_MINUTEN_MAX];
  uint8_t puls[SCHLAF_MINUTEN_MAX];
  bool gueltig[SCHLAF_MINUTEN_MAX];
  HealthMinuteData block[LESE_BLOCK];
  SchlafArbeit arbeit;
  SchlafErgebnis ergebnis;
  NachtDlsRecord satz;
} Rechnung;

// Die laufenden Sitzungen dieser Nacht - sie werden bei jeder Rechnung ersetzt.
static ActivitySession s_gemeldet[SITZUNGEN_MAX];
static int s_gemeldet_anzahl;
// Bis hierher ist gerechnet: das Aufwachen der zuletzt fertigen Nacht, oder das
// Ende des Fensters. Die naechste Rechnung beginnt dort - schlaeft man nach
// einer fertigen Nacht noch einmal ein, wird das eine eigene.
static time_t s_erledigt_bis;
static DataLoggingSession *s_dls;

// ---------------------------------------------------------------------------

// Das Fenster, in dem `utc` liegt - oder das zuletzt vergangene.
static void prv_fenster(time_t utc, time_t *beginn, time_t *ende) {
  const time_t lokal = time_utc_to_local(utc);
  const int minute_des_tages = (lokal % SECONDS_PER_DAY) / SECONDS_PER_MINUTE;
  const time_t tagesbeginn_lokal = lokal - (lokal % SECONDS_PER_DAY);
  time_t beginn_lokal = tagesbeginn_lokal + FENSTER_VON_MIN * SECONDS_PER_MINUTE;
  if (minute_des_tages < FENSTER_VON_MIN) {
    beginn_lokal -= SECONDS_PER_DAY;
  }
  *beginn = utc - (lokal - beginn_lokal);
  *ende = *beginn + FENSTER_DAUER_MIN * SECONDS_PER_MINUTE;
}

// Die Minuten [beginn, bis) aus der Minutendatei. Rueckgabe: bis zur letzten
// vorhandenen Minute - DIE FEHLENDEN AM ENDE ZAEHLEN NICHT. Die Datei haelt die
// juengsten Minuten zurueck, bis sie sicher sind; als ungueltig gerechnet saehe
// jede Nacht so aus, als waere man eben aufgewacht.
static int prv_lies(Rechnung *r, time_t beginn, time_t bis) {
  int n = (bis - beginn) / SECONDS_PER_MINUTE;
  if (n > SCHLAF_MINUTEN_MAX) {
    n = SCHLAF_MINUTEN_MAX;
  }
  memset(r->gueltig, 0, sizeof(r->gueltig));
  memset(r->bewegung, 0, sizeof(r->bewegung));
  memset(r->puls, 0, sizeof(r->puls));
  int letzte = -1;
  time_t t = beginn;
  while (t < beginn + n * SECONDS_PER_MINUTE) {
    uint32_t anzahl = LESE_BLOCK;
    time_t start = t;
    if (!activity_algorithm_get_minute_history(r->block, &anzahl, &start) || anzahl == 0) {
      break;
    }
    for (uint32_t k = 0; k < anzahl; k++) {
      const int i = (start - beginn) / SECONDS_PER_MINUTE + (int)k;
      if (i < 0) {
        continue;
      }
      if (i >= n) {
        break;
      }
      const HealthMinuteData *m = &r->block[k];
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
    const time_t weiter = start + (time_t)anzahl * SECONDS_PER_MINUTE;
    if (weiter <= t) {
      break;
    }
    t = weiter;
  }
  return letzte + 1;
}

static void prv_neu(ActivitySessionType art, time_t start, int minuten, bool laufend) {
  if (minuten <= 0) {
    return;
  }
  ActivitySession s = {
    .start_utc = start,
    .length_min = minuten,
    .type = art,
    .ongoing = laufend,
  };
  activity_sessions_prv_add_activity_session(&s);
  if (laufend && s_gemeldet_anzahl < SITZUNGEN_MAX) {
    s_gemeldet[s_gemeldet_anzahl++] = s;
  }
}

// Die Sitzungen dieser Nacht ersetzen: die alten (laufenden) weg, die neuen hin.
static void prv_melde(const SchlafErgebnis *e, time_t beginn, bool fertig) {
  for (int k = 0; k < s_gemeldet_anzahl; k++) {
    activity_sessions_prv_delete_activity_session(&s_gemeldet[k]);
  }
  s_gemeldet_anzahl = 0;
  if (!e->gefunden) {
    return;
  }

  int gemeldet = 0;
  int k = 0;
  while (k < e->abschnitt_anzahl && gemeldet < SITZUNGEN_MAX) {
    if (e->abschnitte[k].phase == SchlafPhaseWach) {
      k++;
      continue;
    }
    // Ein Schlafblock: alles bis zur naechsten Wachphase.
    const int von = e->abschnitte[k].von;
    int j = k;
    while (j < e->abschnitt_anzahl && e->abschnitte[j].phase != SchlafPhaseWach) {
      j++;
    }
    const int bis = e->abschnitte[j - 1].bis;
    prv_neu(ActivitySessionType_Sleep, beginn + von * SECONDS_PER_MINUTE, bis - von, !fertig);
    gemeldet++;

    for (int m = k; m < j && gemeldet < SITZUNGEN_MAX; m++) {
      if (e->abschnitte[m].phase != SchlafPhaseTief) {
        continue;
      }
      int t_von = e->abschnitte[m].von;
      // DIE FIRMWARE HAELT ALLE SCHLAFARTEN MIT GLEICHEM BEGINN FUER DIESELBE
      // SITZUNG (activity_sessions.c, any_sleep): ein Tiefschlaf, der mit dem
      // Block beginnt, ueberschriebe ihn. Also eine Minute spaeter.
      if (t_von == von) {
        t_von++;
      }
      prv_neu(ActivitySessionType_RestfulSleep, beginn + t_von * SECONDS_PER_MINUTE,
              e->abschnitte[m].bis - t_von, !fertig);
      gemeldet++;
    }
    k = j;
  }
}

// Die ganze Nacht an Boulder, in Teilen zu NACHT_DLS_ABSCHNITTE Abschnitten.
static void prv_schicke(Rechnung *r, time_t beginn) {
  const SchlafErgebnis *e = &r->ergebnis;
  if (!s_dls) {
    Uuid system_uuid = UUID_SYSTEM;
    s_dls = dls_create(DlsSystemTagBoulderNacht, DATA_LOGGING_BYTE_ARRAY, sizeof(NachtDlsRecord),
                       false /*buffered*/, false /*resume*/, &system_uuid);
    if (!s_dls) {
      PBL_LOG_WRN("Nacht: keine Datalogging-Sitzung");
      return;
    }
  }
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
    };
    for (int k = teil * NACHT_DLS_ABSCHNITTE;
         k < e->abschnitt_anzahl && s->anzahl < NACHT_DLS_ABSCHNITTE; k++) {
      s->abschnitte[s->anzahl++] = (NachtDlsAbschnitt){
        .von = e->abschnitte[k].von,
        .bis = e->abschnitte[k].bis,
        .phase = e->abschnitte[k].phase,
      };
    }
    dls_log(s_dls, s, 1);
  }
}

// ---------------------------------------------------------------------------

void boulder_nacht_minute(time_t utc_sec) {
  if ((utc_sec / SECONDS_PER_MINUTE) % TAKT_MIN != 0) {
    return;
  }
  time_t beginn;
  time_t ende;
  prv_fenster(utc_sec, &beginn, &ende);
  if (s_erledigt_bis >= ende || utc_sec < beginn + MINUTES_PER_HOUR * SECONDS_PER_MINUTE ||
      utc_sec > ende + NACHLAUF_MIN * SECONDS_PER_MINUTE) {
    return;
  }

  Rechnung *r = kernel_malloc(sizeof(Rechnung));
  if (!r) {
    PBL_LOG_WRN("Nacht: kein Speicher fuer die Rechnung");
    return;
  }
  const time_t ab = (s_erledigt_bis > beginn) ? s_erledigt_bis : beginn;
  const time_t bis = (utc_sec < ende) ? utc_sec : ende;
  const int anzahl = prv_lies(r, ab, bis);
  const SchlafEingabe eingabe = {
    .bewegung = r->bewegung,
    .puls = r->puls,
    .gueltig = r->gueltig,
    .anzahl = anzahl,
    .schwelle = SCHLAF_SCHWELLE_VMC,
  };
  schlafanalyse_werte(&eingabe, &r->arbeit, &r->ergebnis);

  // Fertig: nach einer Stunde wach - an vorhandenen Minuten gemessen, nicht an
  // der Uhrzeit - oder am Ende des Fensters.
  const SchlafErgebnis *e = &r->ergebnis;
  const bool fertig = (utc_sec >= ende) ||
                      (e->gefunden && anzahl - e->aufwachen >= WACH_FERTIG_MIN);
  prv_melde(e, ab, fertig);
  if (fertig) {
    if (e->gefunden) {
      PBL_LOG_INFO("Nacht fertig: %d min Schlaf, Ruhepuls %d", e->schlaf_minuten, e->ruhepuls);
      prv_schicke(r, ab);
    }
    s_erledigt_bis = (e->gefunden && utc_sec < ende)
        ? ab + e->aufwachen * SECONDS_PER_MINUTE : ende;
    s_gemeldet_anzahl = 0;
  }
  kernel_free(r);
}
