/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2: der Wechsel von b25/b26 (umzug.h).

#include "pbl/services/activity/boulder/umzug.h"

#include "pbl/kernel/compiler.h"
#include "pbl/kernel/mutex.h"
#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/activity_private.h"
#include "pbl/services/activity/boulder/ausgabe.h"
#include "pbl/services/activity/boulder/nacht.h"
#include "pbl/services/activity/boulder/nacht_satz.h"
#include "pbl/services/activity/boulder/schlafanalyse.h"
#include "pbl/services/activity/boulder/takt.h"
#include "pbl/services/activity/boulder/zyklen.h"
#include "pbl/services/filesystem/pfs.h"
#include "pbl/services/settings/settings_file.h"
#include "pbl/services/time.h"
#include "pbl/util/units.h"
#include <pbl/drivers/rtc.h>
#include <pbl/logging/logging.h>

#include <string.h>

PBL_LOG_MODULE_DECLARE(service_activity, CONFIG_SERVICE_ACTIVITY_LOG_LEVEL);

// b25s Zustand (M: nacht.c:173-189), Byte fuer Byte; dieselbe Datei wie das NachtBild.
#define B25_DATEI "boulder_nacht"
#define B25_DATEI_GROESSE 2048
#define B25_ZUSTAND_FASSUNG 1
typedef struct PBL_PACKED {
  uint8_t fassung;
  uint32_t fenster;          //< fuer dieses Fenster gelten erledigt_bis und fest
  uint32_t erledigt_bis;     //< bis hier ist das Fenster endgueltig (UTC), 0 = nichts
  uint8_t fest;
  uint32_t gesendet_beginn;
  uint16_t gesendet_aufwachen;
  uint16_t gesendet_schlaf;
} B25Zustand;
_Static_assert(sizeof(B25Zustand) == 18, "b25-Zustand: 18 Byte (M: nacht.c:180-189)");

// b25s HRV-Fenster (M: hrv.c:32-39): je Platz im Ring ein Satz, Schluessel 1 Byte. Auf der Uhr
// ist time_t 32 Bit (_USE_LONG_TIME_T, cmake/modules/libc.cmake): der Satz hat 8 Byte.
#define B25_HRV_DATEI "boulder_hrv"
#define B25_HRV_DATEI_GROESSE 4096
typedef struct {
  uint32_t start;
  uint16_t rmssd;
  uint8_t puls;
} B25HrvFenster;
_Static_assert(sizeof(B25HrvFenster) == 8, "b25-HRV-Fenster: 8 Byte wie auf der Uhr");
// b25: die Mitte des 5-min-Fensters muss vor dem Ende liegen (M: hrv.c:253-255).
#define B25_HRV_HALB (2 * 60)

// Welche b25-Nacht dem Umzug gehoert, in boulder_v2 (einmal geschrieben).
#define UMZUG_SCHLUESSEL "umzug"
#define UMZUG_FASSUNG 1
typedef struct PBL_PACKED {
  uint8_t fassung;
  uint32_t beginn;  //< beginn_utc des NachtBilds, 0 = keine Nacht
} UmzugMerker;

static bool s_erster_start;
static time_t s_v2_ab;  //< ab hier rechnet V2 selbst (Laden), 0 = unbekannt
static void prv_umzug_cb(void *data);
static BoulderAufgabe s_aufgabe = BOULDER_AUFGABE("Boulder Umzug", prv_umzug_cb);

// ---------------------------------------------------------------------------------------
// Kopf aus dem Zustand (Callback "Laden")

static bool prv_zustand(B25Zustand *z) {
  SettingsFile datei;
  if (settings_file_open(&datei, B25_DATEI, B25_DATEI_GROESSE) != S_SUCCESS) {
    return false;
  }
  const char s[] = "zustand";
  const bool da = settings_file_get_len(&datei, s, sizeof(s)) == (int)sizeof(*z) &&
                  settings_file_get(&datei, s, sizeof(s), z, sizeof(*z)) == S_SUCCESS;
  settings_file_close(&datei);
  if (da && z->fassung != B25_ZUSTAND_FASSUNG) {
    PBL_LOG_WRN("Boulder: b25-Zustand Fassung %d nicht gelesen", (int)z->fassung);
    return false;
  }
  return da;
}

bool boulder_umzug_kopf(time_t fenster, BoulderKopf *kopf) {
  s_v2_ab = fenster;
  B25Zustand z;
  const bool zustand = prv_zustand(&z);
  // Das Bild geliehen, nicht auf dem Stapel: die Kette "Laden" lag sonst ueber 1 KB (T7.3).
  const NachtBild *bild = boulder_nacht_letzte_leihen();
  const bool bild_da = bild != NULL;
  if (!zustand && !(bild_da && bild->fertig)) {
    return false;
  }
  memset(kopf, 0, sizeof(*kopf));
  kopf->fassung = SPEICHER_FASSUNG;
  kopf->fenster = (uint32_t)fenster;
  kopf->stufe = NachtOffen;
  if (zustand) {
    kopf->gesendet_beginn = z.gesendet_beginn;
    kopf->gesendet_aufwachen = z.gesendet_aufwachen;
    kopf->gesendet_schlaf = z.gesendet_schlaf;
    // IM SELBEN FENSTER KEINE ZWEITE NACHT: Was b25 endgueltig gemacht hat, rechnet V2 nicht
    // noch einmal - es ginge sonst ab Fensterbeginn neben b25s Sitzungen, und Schlaf danach
    // ist wie bei b25 erst ab 90 Minuten eine eigene Nacht (Folgenacht).
    if ((time_t)z.fenster == fenster && (time_t)z.erledigt_bis > fenster) {
      const time_t ab = ((time_t)z.erledigt_bis - fenster) / 60;
      if (ab >= NACHT_MINUTEN) {
        kopf->stufe = NachtErledigt;
        s_v2_ab = fenster + (time_t)NACHT_MINUTEN * 60;
      } else {
        kopf->ab = (uint16_t)ab;
        s_v2_ab = fenster + ab * 60;
      }
    }
  } else {
    // Ohne Zustand (b24): zuletzt geschickt ist die fertige Nacht im Bild (wie b25, M:
    // nacht.c:657-663) - sonst ginge sie noch einmal ans Telefon.
    kopf->gesendet_beginn = bild->beginn_utc;
    kopf->gesendet_aufwachen = bild->aufwachen;
    kopf->gesendet_schlaf = bild->schlaf_minuten;
  }
  // Der Ruhepuls der letzten Nacht: ohne ihn erkennte V2 bis zur naechsten fertigen Nacht kein
  // Nickerchen (nacht.c).
  kopf->ruhepuls = bild_da ? bild->ruhepuls : 0;
  PBL_LOG_INFO("Boulder: Umzug von b25, Zustand %d, Stufe %d ab Minute %d", (int)zustand,
               (int)kopf->stufe, (int)kopf->ab);
  return true;
}

void boulder_umzug_anstossen(bool erster_start) {
  s_erster_start |= erster_start;
  boulder_aufgabe_einreihen(&s_aufgabe);
}

// ---------------------------------------------------------------------------------------
// HRV-Fenster der offenen Nacht

typedef struct {
  time_t fenster;
  time_t jetzt;
  int genommen;
  int verworfen;
} HrvKontext;

// IN DER REIHENFOLGE DER DATEI: settings_file haengt jeden Satz an, und b25 schrieb jedes
// Fenster, als es fertig war - so kommen sie zeitlich aufsteigend, wie der Eingang sie
// verlangt, ohne Puffer. Eines ausser der Reihe (Uhr gestellt) lehnt der Eingang ab.
static bool prv_hrv_satz(SettingsFile *datei, SettingsRecordInfo *info, void *kontext) {
  HrvKontext *k = kontext;
  B25HrvFenster f;
  if (info->key_len != 1 || info->val_len != (int)sizeof(f)) {
    return true;
  }
  info->get_val(datei, &f, sizeof(f));
  // Wie b25 (M: hrv.c:253-257): nur brauchbare, nur ab dem Beginn der Nacht, die V2 rechnet.
  if (f.rmssd == 0 || (time_t)f.start < s_v2_ab ||
      (time_t)f.start + B25_HRV_HALB >= k->jetzt) {
    return true;
  }
  const int minute = (int)(((time_t)f.start - k->fenster) / 60);
  if (boulder_nacht_hrv(k->fenster, minute, f.rmssd, f.puls)) {
    k->genommen++;
  } else {
    k->verworfen++;
  }
  return true;
}

// Gibt es die Datei? settings_file_open legte eine fehlende an - b25s Dateien bleiben, wie sie
// sind, und eine neue braucht niemand.
static bool prv_datei_da(const char *name) {
  const int fd = pfs_open(name, OP_FLAG_READ, FILE_TYPE_STATIC, 0);
  if (fd < 0) {
    return false;
  }
  pfs_close(fd);
  return true;
}

static void prv_hrv_uebernehmen(time_t jetzt) {
  const time_t fenster = boulder_nacht_fenster();
  if (!fenster || !s_v2_ab || boulder_nacht_stufe() == NachtErledigt ||
      !prv_datei_da(B25_HRV_DATEI)) {
    return;
  }
  SettingsFile datei;
  if (settings_file_open(&datei, B25_HRV_DATEI, B25_HRV_DATEI_GROESSE) != S_SUCCESS) {
    return;
  }
  HrvKontext k = { .fenster = fenster, .jetzt = jetzt };
  settings_file_each(&datei, prv_hrv_satz, &k);
  settings_file_close(&datei);
  PBL_LOG_INFO("Boulder: Umzug, %d HRV-Fenster uebernommen, %d abgelehnt", k.genommen,
               k.verworfen);
}

// ---------------------------------------------------------------------------------------
// Die b25-Nacht als Zyklen

typedef struct {
  uint16_t von;
  uint16_t bis;
} Lauf;

// Belegung von Pebbles Liste, unter der Activity-Sperre (der Aufrufer haelt keine).
static uint16_t prv_belegt(void) {
  ActivityState *state = activity_private_state();
  pbl_mutex_lock(&state->mutex, PBL_FOREVER);
  const uint16_t belegt = state->activity_sessions_count;
  pbl_mutex_unlock(&state->mutex);
  return belegt;
}

static bool prv_in_liste(time_t start) {
  ActivityState *state = activity_private_state();
  bool da = false;
  pbl_mutex_lock(&state->mutex, PBL_FOREVER);
  for (int i = 0; i < state->activity_sessions_count && !da; i++) {
    da = state->activity_sessions[i].start_utc == start &&
         activity_sessions_prv_is_sleep_activity(state->activity_sessions[i].type);
  }
  pbl_mutex_unlock(&state->mutex);
  return da;
}

static void prv_melden(time_t start, uint16_t laenge, ActivitySessionType typ) {
  // ABGESCHLOSSEN: b25 hat die Nacht schon geloggt, Pebbles Merker stehen auf ihren Enden;
  // was nicht spaeter endet, sendet DLS 84 nicht noch einmal (activity_sessions.c:541).
  ActivitySession s = {
    .start_utc = start, .length_min = laenge, .type = typ, .ongoing = false,
  };
  activity_sessions_prv_add_activity_session(&s);
}

static bool prv_schlaf(uint8_t phase) {
  return phase >= SchlafPhaseLeicht && phase <= SchlafPhaseSchlaf;
}

// Die Zyklen wie zyklen.c sie aus dem Nachtbuch macht: jeder Schlaflauf nach einem
// Wachabschnitt beginnt einen neuen, hoechstens ZYKLEN_C_MAX, und 4 Plaetze der Liste bleiben
// frei. Der erste Zyklus beginnt beim Einschlafen - mit demselben Start wie b25s Block, den er
// so ersetzt (Pebble erkennt Schlaf am Start, activity_sessions.c:132-176).
static int prv_zyklen(const NachtBild *b, Lauf *c, uint16_t *belegt) {
  int nc = 0;
  for (int k = 0; k < b->anzahl && k < NACHT_BILD_ABSCHNITTE; k++) {
    const NachtDlsAbschnitt *a = &b->abschnitte[k];
    if (a->bis <= a->von || !prv_schlaf(a->phase)) {
      continue;
    }
    if (nc > 0 && c[nc - 1].bis == a->von) {
      c[nc - 1].bis = a->bis;
      continue;
    }
    const bool da = prv_in_liste(b->beginn_utc + (time_t)a->von * 60);
    if (nc == 0 || (nc < ZYKLEN_C_MAX &&
                    (da || *belegt < ACTIVITY_MAX_ACTIVITY_SESSIONS_COUNT - ZYKLEN_FREI))) {
      *belegt += !da;
      c[nc++] = (Lauf){ .von = a->von, .bis = a->bis };
    } else {
      // Kein Platz: der vorige Zyklus ueberspannt den Wachabschnitt (wie zyklen.c).
      c[nc - 1].bis = a->bis;
    }
  }
  return nc;
}

static void prv_nacht_anlegen(const NachtBild *b, time_t jetzt) {
  const time_t beginn = b->beginn_utc;
  const time_t ende = beginn + (time_t)b->aufwachen * 60;
  // NUR IM LAUFENDEN SCHLAFTAG: Pebble raeumt Schlaf, der vor 21 Uhr des Vortags endet, beim
  // Boot und um Mitternacht weg (activity_sessions.c:24-31, :59), und eine Sitzung mit Ende in
  // der Zukunft ebenso.
  const time_t frueheste = time_util_get_midnight_of(jetzt) -
      (PBL_SEC_PER_DAY - ACTIVITY_LAST_SLEEP_MINUTE_OF_DAY * PBL_SEC_PER_MIN);
  if (ende < frueheste || ende > jetzt) {
    return;
  }
  uint16_t belegt = prv_belegt();
  Lauf c[ZYKLEN_C_MAX];
  const int nc = prv_zyklen(b, c, &belegt);
  int summe = 0;
  for (int i = 0; i < nc; i++) {
    prv_melden(beginn + (time_t)c[i].von * 60, c[i].bis - c[i].von, ActivitySessionType_Sleep);
    summe += c[i].bis - c[i].von;
  }
  // TIEFSCHLAF je Abschnitt des Bilds, nie in der Startminute eines Zyklus (sonst ersetzte er
  // ihn; b25 rueckte beim Einschlafen ebenso eine Minute weiter). So trifft jeder Abschnitt
  // b25s eigenen Tiefschlaf mit demselben Start und ersetzt ihn - auch einen, den b25 ueber eine
  // Luecke zusammengelegt hatte (mehr als 7, M: nacht.c:345-365): die Luecke zaehlte U: sonst
  // als Tiefschlaf (nicht_schlaf_min liest es nicht).
  int nr = 0;
  for (int k = 0; k < b->anzahl && k < NACHT_BILD_ABSCHNITTE; k++) {
    const NachtDlsAbschnitt *a = &b->abschnitte[k];
    if (a->phase != SchlafPhaseTief || a->bis <= a->von) {
      continue;
    }
    uint16_t von = a->von;
    uint16_t bis = a->bis;
    // Aufeinanderfolgende Tief-Abschnitte sind einer.
    while (k + 1 < b->anzahl && b->abschnitte[k + 1].phase == SchlafPhaseTief &&
           b->abschnitte[k + 1].von == bis) {
      bis = b->abschnitte[++k].bis;
    }
    for (int i = 0; i < nc; i++) {
      von += (c[i].von == von);
    }
    const time_t start = beginn + (time_t)von * 60;
    const bool da = prv_in_liste(start);
    // Neu hoechstens ZYKLEN_R_MAX und nur mit Platz; einen von b25 ersetzt er immer (derselbe
    // Platz) - sonst bliebe dort eine zusammengelegte Luecke als Tiefschlaf stehen.
    if (bis <= von || (!da && (nr >= ZYKLEN_R_MAX ||
                               belegt >= ACTIVITY_MAX_ACTIVITY_SESSIONS_COUNT - ZYKLEN_FREI))) {
      continue;
    }
    belegt += !da;
    nr++;
    prv_melden(start, bis - von, ActivitySessionType_RestfulSleep);
  }
  PBL_LOG_INFO("Boulder: Umzug, b25-Nacht als %d Zyklen (%d min) und %d Tief", nc, summe, nr);
}

// ---------------------------------------------------------------------------------------

static bool prv_merker(SettingsFile *datei, UmzugMerker *m) {
  return settings_file_get_len(datei, UMZUG_SCHLUESSEL, sizeof(UMZUG_SCHLUESSEL)) ==
             (int)sizeof(*m) &&
         settings_file_get(datei, UMZUG_SCHLUESSEL, sizeof(UMZUG_SCHLUESSEL), m, sizeof(*m)) ==
             S_SUCCESS &&
         m->fassung == UMZUG_FASSUNG;
}

static void prv_umzug_cb(void *data) {
  BOULDER_AUFGABE_LAEUFT(&s_aufgabe);
  const time_t jetzt = rtc_get_time();
  const NachtBild *bild = boulder_nacht_letzte_leihen();
  const bool bild_da = bild != NULL;
  SettingsFile datei;
  if (settings_file_open(&datei, SPEICHER_DATEI, SPEICHER_DATEI_GROESSE) != S_SUCCESS) {
    PBL_LOG_WRN("Boulder: Umzug, %s nicht geoeffnet", SPEICHER_DATEI);
    return;
  }
  UmzugMerker m;
  bool merker = prv_merker(&datei, &m);
  if (!merker && s_erster_start) {
    // EINMAL ENTSCHEIDEN, WELCHE NACHT b25 GEHOERT: die fertige im Bild, wenn V2 sie nicht
    // selbst rechnet (sie endet vor dem Beginn der Rechnung, Laden). Rechnet V2 sie, kommt sie
    // mit demselben beginn_utc als V2-Nacht (Boulder ersetzt sie, 0.50).
    const time_t ende = bild_da ? (time_t)bild->beginn_utc + (time_t)bild->aufwachen * 60 : 0;
    m = (UmzugMerker){
      .fassung = UMZUG_FASSUNG,
      .beginn = (bild_da && bild->fertig && s_v2_ab && ende <= s_v2_ab) ? bild->beginn_utc : 0,
    };
    merker = settings_file_set(&datei, UMZUG_SCHLUESSEL, sizeof(UMZUG_SCHLUESSEL), &m,
                               sizeof(m)) == S_SUCCESS;
    if (!merker) {
      PBL_LOG_WRN("Boulder: Umzug-Merker nicht geschrieben");
    }
  }
  settings_file_close(&datei);
  if (s_erster_start) {
    s_erster_start = false;
    prv_hrv_uebernehmen(jetzt);
  }
  // Bei jedem Boot dieselbe Nacht, solange das Bild noch ihres ist (V2 schreibt "letzte" erst
  // mit einer eigenen, spaeteren Nacht neu).
  if (merker && m.beginn && bild_da && bild->beginn_utc == m.beginn) {
    prv_nacht_anlegen(bild, jetzt);
  }
}

void boulder_umzug_test_neustart(void) {
  s_erster_start = false;
  s_v2_ab = 0;
  s_aufgabe.steht_an = false;
}
