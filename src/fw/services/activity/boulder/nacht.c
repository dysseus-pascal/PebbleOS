/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/activity/boulder/nacht.h"

#include "applib/data_logging.h"
#include "kernel/pbl_malloc.h"
#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/activity_algorithm.h"
#include "pbl/services/activity/activity_private.h"
#include "pbl/services/activity/boulder/hrv.h"
#include "pbl/services/activity/boulder/messstufe.h"
#include "pbl/services/battery/battery_state.h"
#include "pbl/services/activity/boulder/schlafanalyse.h"
#include "pbl/services/data_logging/data_logging_service.h"
#include "pbl/services/settings/settings_file.h"
#include "pbl/kernel/mutex.h"
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
  SchlafHrv hrv[SCHLAF_HRV_MAX];
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
// DAS AKKU-PROTOKOLL: der Stand bei jedem Rechentakt im Fenster. Am Ende zaehlt
// der beim Einschlafen - nicht der, als die Rechnung den Schlaf zum ersten Mal
// sah; das ist erst nach einer Stunde Schlaf, und nach einem Wachliegen spaeter.
#define AKKU_TAKTE ((FENSTER_DAUER_MIN + NACHLAUF_MIN) / TAKT_MIN + 1)
static uint8_t s_akku[AKKU_TAKTE];
static time_t s_akku_fenster;
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

// Die Abschnitte fuer die Anzeige verdichten, bis sie passen: der kuerzeste
// geht im laengeren Nachbarn auf, und gleiche Nachbarn werden eins. So
// bleiben die langen Phasen stehen, und nur das Flackern verschwindet.
int boulder_nacht_verdichten(SchlafAbschnitt *a, int n, int max) {
  while (n > max && n > 1) {
    int k = 0;
    for (int i = 1; i < n; i++) {
      if (a[i].bis - a[i].von < a[k].bis - a[k].von) {
        k = i;
      }
    }
    int ziel;
    if (k == 0) {
      ziel = 1;
    } else if (k == n - 1) {
      ziel = n - 2;
    } else {
      ziel = (a[k - 1].bis - a[k - 1].von >= a[k + 1].bis - a[k + 1].von) ? k - 1 : k + 1;
    }
    if (ziel < k) {
      a[ziel].bis = a[k].bis;
    } else {
      a[ziel].von = a[k].von;
    }
    memmove(&a[k], &a[k + 1], (n - k - 1) * sizeof(*a));
    n--;
    // Stehen jetzt zwei gleiche nebeneinander, werden sie einer. Der
    // gewachsene Nachbar steht nach dem Entfernen bei `ziel` (links) oder
    // bei `k` (rechts); zu pruefen ist er mit seinem anderen Nachbarn.
    const int links = (ziel < k) ? ziel : k - 1;
    if (links >= 0 && links + 1 < n && a[links].phase == a[links + 1].phase) {
      a[links].bis = a[links + 1].bis;
      memmove(&a[links + 1], &a[links + 2], (n - links - 2) * sizeof(*a));
      n--;
    }
  }
  return n;
}

static void prv_bild_merken(Rechnung *r, time_t beginn, bool fertig) {
  const SchlafErgebnis *e = &r->ergebnis;
  if (!e->gefunden) {
    return;
  }
  // Die Arbeitsliste der Rechnung ist jetzt frei - sie dient als Ablage.
  SchlafAbschnitt *a = r->arbeit.liste;
  memcpy(a, e->abschnitte, e->abschnitt_anzahl * sizeof(*a));
  const int n = boulder_nacht_verdichten(a, e->abschnitt_anzahl, NACHT_BILD_ABSCHNITTE);

  pbl_mutex_lock(&s_bild_sperre, PBL_FOREVER);
  s_bild = (NachtBild){
    .beginn_utc = (uint32_t)beginn,
    .utc_to_local = time_utc_to_local(beginn) - beginn,
    .einschlafen = e->einschlafen,
    .aufwachen = e->aufwachen,
    .schlaf_minuten = e->schlaf_minuten,
    .hrv = e->hrv,
    .ruhepuls = e->ruhepuls,
    .fertig = fertig,
    .anzahl = (uint8_t)n,
  };
  // Die Minuten je Phase aus der ungekuerzten Liste - die verdichtete
  // verschoebe sie.
  for (int k = 0; k < e->abschnitt_anzahl; k++) {
    if (e->abschnitte[k].phase < NACHT_PHASEN) {
      s_bild.minuten[e->abschnitte[k].phase] += e->abschnitte[k].bis - e->abschnitte[k].von;
    }
  }
  for (int k = 0; k < n; k++) {
    s_bild.abschnitte[k] = (NachtDlsAbschnitt){
      .von = a[k].von, .bis = a[k].bis, .phase = a[k].phase,
    };
  }
  s_bild_da = true;
  if (fertig) {
    SettingsFile datei;
    if (settings_file_open(&datei, BILD_DATEI, BILD_DATEI_GROESSE) == S_SUCCESS) {
      settings_file_set(&datei, BILD_SCHLUESSEL, sizeof(BILD_SCHLUESSEL), &s_bild,
                        sizeof(s_bild));
      settings_file_close(&datei);
    }
  }
  pbl_mutex_unlock(&s_bild_sperre);
}

bool boulder_nacht_letzte(NachtBild *aus) {
  pbl_mutex_lock(&s_bild_sperre, PBL_FOREVER);
  if (!s_bild_da && !s_bild_gelesen) {
    // Nach einem Neustart: die zuletzt fertige Nacht aus der Datei.
    s_bild_gelesen = true;
    SettingsFile datei;
    if (settings_file_open(&datei, BILD_DATEI, BILD_DATEI_GROESSE) == S_SUCCESS) {
      if (settings_file_get_len(&datei, BILD_SCHLUESSEL, sizeof(BILD_SCHLUESSEL)) ==
              (int)sizeof(s_bild) &&
          settings_file_get(&datei, BILD_SCHLUESSEL, sizeof(BILD_SCHLUESSEL), &s_bild,
                            sizeof(s_bild)) == S_SUCCESS) {
        s_bild_da = true;
      }
      settings_file_close(&datei);
    }
  }
  const bool da = s_bild_da;
  if (da) {
    *aus = s_bild;
  }
  pbl_mutex_unlock(&s_bild_sperre);
  return da;
}

// Der Akkustand zum ersten Takt ab `utc`; 0xFF, wenn keiner da ist.
static uint8_t prv_akku_bei(time_t utc) {
  for (int i = (utc - s_akku_fenster + TAKT_MIN * SECONDS_PER_MINUTE - 1) /
               (TAKT_MIN * SECONDS_PER_MINUTE);
       i >= 0 && i < AKKU_TAKTE; i++) {
    if (s_akku[i] != 0xFF) {
      return s_akku[i];
    }
  }
  return 0xFF;
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
      .akku_beginn = prv_akku_bei(beginn + e->einschlafen * SECONDS_PER_MINUTE),
      .akku_ende = battery_get_charge_state().charge_percent,
      .flags = s_geladen ? NACHT_DLS_FLAG_GELADEN : 0,
      .messstufe = boulder_messstufe(),
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

bool boulder_nacht_im_fenster(time_t utc) {
  time_t beginn;
  time_t ende;
  boulder_nacht_fenster(utc, &beginn, &ende);
  return utc >= beginn && utc < ende;
}

// ---------------------------------------------------------------------------

void boulder_nacht_minute(time_t utc_sec) {
  if ((utc_sec / SECONDS_PER_MINUTE) % TAKT_MIN != 0) {
    return;
  }
  time_t beginn;
  time_t ende;
  boulder_nacht_fenster(utc_sec, &beginn, &ende);

  // Das Akku-Protokoll laeuft ab Beginn des Fensters mit, auch in der ersten
  // Stunde, in der noch nicht gerechnet wird.
  if (s_akku_fenster != beginn) {
    s_akku_fenster = beginn;
    memset(s_akku, 0xFF, sizeof(s_akku));
    s_geladen = false;
  }
  const BatteryChargeState akku = battery_get_charge_state();
  const int takt = (utc_sec - beginn) / (TAKT_MIN * SECONDS_PER_MINUTE);
  if (takt >= 0 && takt < AKKU_TAKTE) {
    s_akku[takt] = akku.charge_percent;
  }

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
  const int hrv_anzahl = boulder_hrv_fenster(ab, bis, r->hrv, SCHLAF_HRV_MAX);
  const SchlafEingabe eingabe = {
    .bewegung = r->bewegung,
    .puls = r->puls,
    .gueltig = r->gueltig,
    .anzahl = anzahl,
    .schwelle = SCHLAF_SCHWELLE_VMC,
    .hrv = r->hrv,
    .hrv_anzahl = hrv_anzahl,
  };
  schlafanalyse_werte(&eingabe, &r->arbeit, &r->ergebnis);

  // Hing die Uhr, seit Schlaf da ist, am Kabel? Dann sagt der Verbrauch nichts.
  s_geladen = s_geladen || (r->ergebnis.gefunden && akku.is_plugged);

  // Fertig: nach einer Stunde wach - an vorhandenen Minuten gemessen, nicht an
  // der Uhrzeit - oder am Ende des Fensters.
  const SchlafErgebnis *e = &r->ergebnis;
  const bool fertig = (utc_sec >= ende) ||
                      (e->gefunden && anzahl - e->aufwachen >= WACH_FERTIG_MIN);
  prv_melde(e, ab, fertig);
  prv_bild_merken(r, ab, fertig);
  if (fertig) {
    if (e->gefunden) {
      PBL_LOG_INFO("Nacht fertig: %d min Schlaf, Ruhepuls %d", e->schlaf_minuten, e->ruhepuls);
      prv_schicke(r, ab);
    }
    s_erledigt_bis = (e->gefunden && utc_sec < ende)
        ? ab + e->aufwachen * SECONDS_PER_MINUTE : ende;
    s_gemeldet_anzahl = 0;
    s_geladen = false;
  }
  kernel_free(r);
}
