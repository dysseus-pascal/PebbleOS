/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "applib/data_logging.h"
#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/activity_algorithm.h"
#include "pbl/services/activity/boulder/nacht.h"
#include "pbl/services/activity/boulder/schlafanalyse.h"
#include "pbl/services/data_logging/data_logging_service.h"
#include "pbl/services/battery/battery_state.h"

#include "stubs_logging.h"
#include "stubs_pbl_malloc.h"
#include "stubs_mutex.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Die Nachtrechnung der Boulder-Firmware im Takt der Uhr: jede Minute ein
// Aufruf, die Minutendatei haelt die letzten 19 Minuten zurueck, und am Morgen
// muss genau eine fertige Nacht dastehen - als Sitzungen und als Satz fuer
// Boulder.
//
// DIE NACHT IST ECHT (26./27.9.2026, 22:00 bis 06:59). Davor, ab Beginn des
// Fensters um 20 Uhr, fehlen Daten; danach ist man wach und geht umher.

#define ZURUECKGEHALTEN_MIN 19
#define OFFSET_LOKAL (2 * 60 * 60)   // Sommerzeit in der Schweiz
#define MINUTEN (16 * 60)

static time_t s_beginn;              // 20:00 Ortszeit, Minute 0
static time_t s_jetzt;
static HealthMinuteData s_minuten[MINUTEN];

// --- Was die Uhr sonst liefert -------------------------------------------------

// HRV-Fenster kommen hier keine: die Nacht wird wie in der Firmware ohne sie
// gerechnet, wenn der Sensor keine liefert.
int boulder_hrv_fenster(time_t von, time_t bis, SchlafHrv *aus, int max) {
  return 0;
}

// Der Akku faellt im Test um ein Prozent je Stunde, ab 80 um 20 Uhr.
BatteryChargeState battery_get_charge_state(void) {
  return (BatteryChargeState){ .charge_percent = 80 - (s_jetzt - s_beginn) / 3600 };
}
uint8_t boulder_messstufe(void) { return 3; }

time_t time_utc_to_local(time_t utc) {
  return utc + OFFSET_LOKAL;
}

bool activity_algorithm_get_minute_history(HealthMinuteData *minute_data, uint32_t *num_records,
                                           time_t *utc_start) {
  memset(minute_data, 0xFF, *num_records * sizeof(HealthMinuteData));
  const time_t vorhanden_bis = s_jetzt - ZURUECKGEHALTEN_MIN * SECONDS_PER_MINUTE;
  int32_t anzahl = (vorhanden_bis - *utc_start) / SECONDS_PER_MINUTE;
  if (anzahl < 0) {
    anzahl = 0;
  }
  if ((uint32_t)anzahl > *num_records) {
    anzahl = *num_records;
  }
  for (int32_t k = 0; k < anzahl; k++) {
    const int32_t i = (*utc_start - s_beginn) / SECONDS_PER_MINUTE + k;
    if (i >= 0 && i < MINUTEN) {
      minute_data[k] = s_minuten[i];
    }
  }
  *num_records = anzahl;
  return true;
}

#define SITZUNGEN 64
static ActivitySession s_sitzungen[SITZUNGEN];
static int s_sitzungen_anzahl;

void activity_sessions_prv_add_activity_session(ActivitySession *session) {
  for (int k = 0; k < s_sitzungen_anzahl; k++) {
    if (s_sitzungen[k].type == session->type && s_sitzungen[k].start_utc == session->start_utc) {
      s_sitzungen[k] = *session;
      return;
    }
  }
  cl_assert(s_sitzungen_anzahl < SITZUNGEN);
  s_sitzungen[s_sitzungen_anzahl++] = *session;
}

void activity_sessions_prv_delete_activity_session(ActivitySession *session) {
  for (int k = 0; k < s_sitzungen_anzahl; k++) {
    if (s_sitzungen[k].type == session->type && s_sitzungen[k].start_utc == session->start_utc) {
      // Wie in der Firmware: nur laufende Sitzungen duerfen weg.
      cl_assert(s_sitzungen[k].ongoing);
      memmove(&s_sitzungen[k], &s_sitzungen[k + 1],
              (s_sitzungen_anzahl - k - 1) * sizeof(ActivitySession));
      s_sitzungen_anzahl--;
      return;
    }
  }
  cl_fail("Sitzung zum Loeschen nicht gefunden");
}

#define SAETZE 8
static NachtDlsRecord s_saetze[SAETZE];
static int s_saetze_anzahl;
static time_t s_gesendet_um;

DataLoggingSession *dls_create(uint32_t tag, DataLoggingItemType item_type, uint16_t item_size,
                               bool buffered, bool resume, const Uuid *uuid) {
  cl_assert_equal_i(tag, DlsSystemTagBoulderNacht);
  cl_assert_equal_i(item_size, sizeof(NachtDlsRecord));
  return (DataLoggingSession *)1;
}

DataLoggingResult dls_log(DataLoggingSession *session, const void *data, uint32_t num_items) {
  cl_assert(s_saetze_anzahl < SAETZE);
  memcpy(&s_saetze[s_saetze_anzahl++], data, sizeof(NachtDlsRecord));
  s_gesendet_um = s_jetzt;
  return DATA_LOGGING_SUCCESS;
}

// --- Die Nacht ------------------------------------------------------------------

static void prv_lade_nacht(void) {
  // Vor 22 Uhr: keine Daten (so liefert die Datei fehlende Minuten).
  memset(s_minuten, 0xFF, sizeof(s_minuten));
  char pfad[512];
  snprintf(pfad, sizeof(pfad), "%s/activity/boulder/nacht-2026-09-26.csv", CLAR_FIXTURE_PATH);
  FILE *f = fopen(pfad, "r");
  cl_assert(f);
  char zeile[256];
  int n = 0;
  while (fgets(zeile, sizeof(zeile), f)) {
    if (zeile[0] == '\n' || zeile[0] == '\r') {
      break;  // danach kommen die HRV-Fenster; die kennt die Firmware noch nicht
    }
    if (!strncmp(zeile, "minute,", 7)) {
      continue;
    }
    char *feld[8] = { 0 };
    int k = 0;
    char *p = zeile;
    feld[k++] = p;
    while (*p && k < 8) {
      if (*p == ',') {
        *p = 0;
        feld[k++] = p + 1;
      }
      p++;
    }
    const int i = 120 + n++;
    if (feld[2][0]) {
      // Der Export traegt das Bewegungsbyte b; die Datei der Uhr das VMC = b*b/16.
      const int b = atoi(feld[2]);
      s_minuten[i] = (HealthMinuteData){
        .vmc = (uint16_t)((b * b + 8) / 16),
        .heart_rate_bpm = feld[4][0] ? atoi(feld[4]) : 0,
      };
    }
  }
  fclose(f);
  cl_assert_equal_i(n, 540);
  // Ab 07:00 wach und unterwegs.
  for (int i = 660; i < MINUTEN; i++) {
    s_minuten[i] = (HealthMinuteData){ .vmc = 3000, .steps = 60, .heart_rate_bpm = 82 };
  }
}

void test_boulder_nacht__initialize(void) {
  // 26.9.2026, 20:00 Ortszeit = 18:00 UTC
  s_beginn = 1790445600;
  s_sitzungen_anzahl = 0;
  s_saetze_anzahl = 0;
  s_gesendet_um = 0;
  prv_lade_nacht();
}

void test_boulder_nacht__eine_fertige_nacht_am_morgen(void) {
  // Die Uhr ruft jede Minute auf, von 20 Uhr bis zum naechsten Mittag.
  for (s_jetzt = s_beginn; s_jetzt <= s_beginn + MINUTEN * SECONDS_PER_MINUTE;
       s_jetzt += SECONDS_PER_MINUTE) {
    boulder_nacht_minute(s_jetzt);
  }

  // Genau eine Nacht - das Wachliegen um 23 Uhr zerlegt sie nicht, der Schlaf
  // davor ist kuerzer als eine Stunde. Fertig nach einer Viertelstunde wach
  // (07:00 + 15 min, dazu die zurueckgehaltenen Minuten und der Takt): 07:45.
  cl_assert(s_saetze_anzahl >= 1);
  const NachtDlsRecord *s = &s_saetze[0];
  cl_assert_equal_i(s->teil, 0);
  cl_assert_equal_i(s_saetze_anzahl, s->teile);
  cl_assert_equal_i(s->beginn_utc, s_beginn);
  cl_assert_equal_i(s->utc_to_local, OFFSET_LOKAL);
  cl_assert_equal_i(s_gesendet_um, s_beginn + (11 * 60 + 45) * SECONDS_PER_MINUTE);
  // Das Akku-Protokoll: Schlaf zuerst gesehen um 22:45 (78 %), fertig 07:45 (69 %).
  cl_assert_equal_i(s->version, NACHT_DLS_VERSION);
  cl_assert_equal_i(s->akku_beginn, 78);
  cl_assert_equal_i(s->akku_ende, 69);
  cl_assert_equal_i(s->flags, 0);
  cl_assert_equal_i(s->messstufe, 3);
  // Eingeschlafen 22:17, aufgewacht 07:00.
  cl_assert_equal_i(s->einschlafen, 120 + 17);
  cl_assert_equal_i(s->aufwachen, 660);

  // Die Sitzungen: nichts mehr laufend, die Schlafbloecke ergeben die
  // Schlafminuten, jeder Tiefschlaf liegt in einem Block.
  int schlaf = 0;
  for (int k = 0; k < s_sitzungen_anzahl; k++) {
    const ActivitySession *a = &s_sitzungen[k];
    cl_assert(!a->ongoing);
    if (a->type == ActivitySessionType_Sleep) {
      schlaf += a->length_min;
    } else {
      cl_assert_equal_i(a->type, ActivitySessionType_RestfulSleep);
      bool drin = false;
      for (int j = 0; j < s_sitzungen_anzahl; j++) {
        const ActivitySession *b = &s_sitzungen[j];
        drin = drin || (b->type == ActivitySessionType_Sleep && a->start_utc > b->start_utc &&
                        a->start_utc + a->length_min * 60 <= b->start_utc + b->length_min * 60);
      }
      cl_assert(drin);
    }
  }
  cl_assert_equal_i(schlaf, s->schlaf_minuten);

  // Zum Nachlesen im Testprotokoll: die Nacht, wie die Uhr sie sieht.
  int je_phase[5] = { 0 };
  for (int t = 0; t < s_saetze_anzahl; t++) {
    for (int k = 0; k < s_saetze[t].anzahl; k++) {
      je_phase[s_saetze[t].abschnitte[k].phase] +=
          s_saetze[t].abschnitte[k].bis - s_saetze[t].abschnitte[k].von;
    }
  }
  printf("\nNacht: Schlaf %d min (leicht %d, tief %d, REM %d), wach %d min, Ruhepuls %d, "
         "%d Sitzungen\n", s->schlaf_minuten, je_phase[SchlafPhaseLeicht],
         je_phase[SchlafPhaseTief], je_phase[SchlafPhaseRem], je_phase[SchlafPhaseWach],
         s->ruhepuls, s_sitzungen_anzahl);

  // Die Abschnitte des Satzes: lueckenlos von Einschlafen bis Aufwachen.
  cl_assert_equal_i(s->abschnitte[0].von, s->einschlafen);
  int bis = s->einschlafen;
  for (int t = 0; t < s_saetze_anzahl; t++) {
    for (int k = 0; k < s_saetze[t].anzahl; k++) {
      cl_assert_equal_i(s_saetze[t].abschnitte[k].von, bis);
      bis = s_saetze[t].abschnitte[k].bis;
    }
  }
  cl_assert_equal_i(bis, s->aufwachen);

  // DIE UHR SELBST KENNT DIE NACHT (Health-App): dieselben Zahlen, die
  // Minuten je Phase wie im Satz, die Abschnitte lueckenlos und hoechstens
  // NACHT_BILD_ABSCHNITTE.
  NachtBild b;
  cl_assert(boulder_nacht_letzte(&b));
  cl_assert(b.fertig);
  cl_assert_equal_i(b.beginn_utc, s->beginn_utc);
  cl_assert_equal_i(b.einschlafen, s->einschlafen);
  cl_assert_equal_i(b.aufwachen, s->aufwachen);
  cl_assert_equal_i(b.schlaf_minuten, s->schlaf_minuten);
  cl_assert_equal_i(b.ruhepuls, s->ruhepuls);
  for (int p = 0; p < NACHT_PHASEN; p++) {
    cl_assert_equal_i(b.minuten[p], je_phase[p]);
  }
  cl_assert(b.anzahl > 0 && b.anzahl <= NACHT_BILD_ABSCHNITTE);
  cl_assert_equal_i(b.abschnitte[0].von, b.einschlafen);
  for (int k = 1; k < b.anzahl; k++) {
    cl_assert_equal_i(b.abschnitte[k].von, b.abschnitte[k - 1].bis);
    cl_assert(b.abschnitte[k].phase != b.abschnitte[k - 1].phase);
  }
  cl_assert_equal_i(b.abschnitte[b.anzahl - 1].bis, b.aufwachen);
}

void test_boulder_nacht__ausserhalb_des_fensters_ruhe(void) {
  // Tagsueber (12 bis 19 Uhr) wird nichts gerechnet, auch wenn die Uhr still liegt.
  memset(s_minuten, 0, sizeof(s_minuten));
  for (s_jetzt = s_beginn - 8 * 60 * SECONDS_PER_MINUTE; s_jetzt < s_beginn - 60;
       s_jetzt += SECONDS_PER_MINUTE) {
    boulder_nacht_minute(s_jetzt);
  }
  cl_assert_equal_i(s_sitzungen_anzahl, 0);
  cl_assert_equal_i(s_saetze_anzahl, 0);
}

void test_boulder_nacht__langes_wachliegen_bleibt_eine_nacht(void) {
  // 22:00 bis 01:00 Schlaf, 40 Minuten wach und unterwegs, dann Schlaf bis 06:30.
  // Nach einer Viertelstunde wach geht die Nacht vorlaeufig ans Telefon; kommt
  // der zweite Schlaf dazu, geht dieselbe Nacht - gleicher Beginn - noch einmal,
  // jetzt bis 06:30. Frueher wurden es zwei Naechte, und das Telefon zeigte nur
  // den Rest.
  // Einen Tag spaeter: die Nachtrechnung merkt sich, was schon erledigt ist.
  s_beginn += SECONDS_PER_DAY;
  memset(s_minuten, 0xFF, sizeof(s_minuten));
  for (int i = 120; i < MINUTEN; i++) {
    const bool wach = (i >= 300 && i < 340) || i >= 630;
    s_minuten[i] = (HealthMinuteData){
      .vmc = wach ? 3000 : 2,
      .steps = wach ? 60 : 0,
      .heart_rate_bpm = wach ? 85 : (i % 4 == 0 ? 55 : 0),
    };
  }
  for (s_jetzt = s_beginn; s_jetzt <= s_beginn + MINUTEN * SECONDS_PER_MINUTE;
       s_jetzt += SECONDS_PER_MINUTE) {
    boulder_nacht_minute(s_jetzt);
  }
  cl_assert(s_saetze_anzahl >= 2);
  const NachtDlsRecord *erste = &s_saetze[0];
  const NachtDlsRecord *letzte = &s_saetze[s_saetze_anzahl - 1];
  cl_assert_equal_i(erste->beginn_utc, s_beginn);
  cl_assert_equal_i(erste->aufwachen, 300);
  cl_assert(erste->schlaf_minuten >= 170);
  // Dieselbe Nacht, gewachsen: gleicher Beginn, Aufwachen 06:30 = Minute 630,
  // beide Schlafbloecke zusammen.
  cl_assert_equal_i(letzte->beginn_utc, s_beginn);
  cl_assert_equal_i(letzte->aufwachen, 630);
  cl_assert(letzte->schlaf_minuten >= 440);
  // Nichts laeuft mehr, und dieselbe Nacht geht nicht ein drittes Mal, ohne
  // dass sich etwas aendert.
  for (int k = 0; k < s_sitzungen_anzahl; k++) {
    cl_assert(!s_sitzungen[k].ongoing);
  }
}

void test_boulder_nacht__verdichten_laesst_die_langen_phasen_stehen(void) {
  // Leicht 60, Tief 2, Leicht 30, REM 40, Wach 1, REM 20. Auf vier verdichtet
  // verschwindet das Wachflackern, und die beiden REM werden einer.
  SchlafAbschnitt a[] = {
    { 0, 60, SchlafPhaseLeicht }, { 60, 62, SchlafPhaseTief }, { 62, 92, SchlafPhaseLeicht },
    { 92, 132, SchlafPhaseRem }, { 132, 133, SchlafPhaseWach }, { 133, 153, SchlafPhaseRem },
  };
  cl_assert_equal_i(boulder_nacht_verdichten(a, 6, 4), 4);
  cl_assert_equal_i(a[3].phase, SchlafPhaseRem);
  cl_assert_equal_i(a[3].von, 92);
  cl_assert_equal_i(a[3].bis, 153);
  // Auf drei: der kurze Tiefschlaf geht, und die beiden Leicht werden einer.
  const int n = boulder_nacht_verdichten(a, 4, 3);
  cl_assert_equal_i(n, 2);
  cl_assert_equal_i(a[0].phase, SchlafPhaseLeicht);
  cl_assert_equal_i(a[0].von, 0);
  cl_assert_equal_i(a[0].bis, 92);
  cl_assert_equal_i(a[1].phase, SchlafPhaseRem);
  cl_assert_equal_i(a[1].bis, 153);
}
