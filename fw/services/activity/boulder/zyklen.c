/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/activity/boulder/zyklen.h"

#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/activity_private.h"
#include "pbl/services/activity/boulder/nachlauf.h"
#include "pbl/services/activity/boulder/schlafanalyse.h"
#include "pbl/kernel/mutex.h"
#include <pbl/logging/logging.h>

#include <string.h>

PBL_LOG_MODULE_DECLARE(service_activity, CONFIG_SERVICE_ACTIVITY_LOG_LEVEL);

typedef enum {
  ArtLeer = 0,
  ArtP,
  ArtNeutral,  //< P0: RestfulSleep, Laenge 0, laufend - zaehlt nicht, wird nie geloggt
  ArtC,
  ArtR,
} Art;

typedef struct {
  time_t start;
  uint16_t laenge;     //< Minuten
  uint8_t art;         //< Art
  uint8_t nacht;       //< zu welcher Nacht (laufende Nummer)
  bool zu;             //< abgeschlossen: Pebble loggt sie, danach nie mehr aendern
  bool schliessen;     //< im gestaffelten Abschluss
  bool bestaetigt;     //< im laufenden Abgleich gewuenscht
} Eintrag;

static Eintrag s_eintraege[ZYKLEN_EINTRAEGE];
static uint8_t s_nacht;
static time_t s_beginn;      //< Minute 0 der laufenden Nacht, 0 = keine
static uint16_t s_ab;
static bool s_ein_fest;      //< das Einschlafen steht im Nachtbuch: ab jetzt C statt P
static bool s_fertig;
static uint16_t s_fertig_auf;  //< Aufwachen bei "fertig": davor wird nichts mehr verlaengert
static uint16_t s_schlaf_bis;  //< bis hier hat der Automat geschlafen (Minute, exklusiv)
static uint16_t s_fest_bis;
static int8_t s_offen = -1;    //< Index der offenen Sitzung (P oder C)
static BoulderZyklenDiagnose s_diagnose;
static bool s_stumm;           //< AP4: Nachspielen nach dem Boot, nichts an Pebble

// ---------------------------------------------------------------------------------------
// Pebble

static void prv_melden(const Eintrag *e) {
  if (s_stumm) {
    return;
  }
  ActivitySession s = {
    .start_utc = e->start,
    .length_min = (e->art == ArtNeutral) ? 0 : e->laenge,
    .type = (e->art == ArtP || e->art == ArtC) ? ActivitySessionType_Sleep
                                                : ActivitySessionType_RestfulSleep,
    .ongoing = !e->zu,
  };
  activity_sessions_prv_add_activity_session(&s);
}

// Belegung der Liste, unter der Activity-Sperre (der Aufrufer haelt keine).
static uint16_t prv_belegt(void) {
  ActivityState *state = activity_private_state();
  pbl_mutex_lock(&state->mutex, PBL_FOREVER);
  const uint16_t belegt = state->activity_sessions_count;
  pbl_mutex_unlock(&state->mutex);
  return belegt;
}

static bool prv_pebble_wach(void) {
  ActivityState *state = activity_private_state();
  pbl_mutex_lock(&state->mutex, PBL_FOREVER);
  const bool wach = (state->sleep_data.cur_state == ActivitySleepStateAwake);
  pbl_mutex_unlock(&state->mutex);
  return wach;
}

// ---------------------------------------------------------------------------------------
// Tabelle

static time_t prv_utc(uint16_t minute) {
  return s_beginn + (time_t)minute * 60;
}

static time_t prv_ende(const Eintrag *e) {
  return e->start + (time_t)e->laenge * 60;
}

static Eintrag *prv_suchen(time_t start) {
  for (int i = 0; i < ZYKLEN_EINTRAEGE; i++) {
    if (s_eintraege[i].art != ArtLeer && s_eintraege[i].start == start) {
      return &s_eintraege[i];
    }
  }
  return NULL;
}

static Eintrag *prv_frei(void) {
  for (int i = 0; i < ZYKLEN_EINTRAEGE; i++) {
    if (s_eintraege[i].art == ArtLeer) {
      return &s_eintraege[i];
    }
  }
  s_diagnose.tabelle_voll++;
  PBL_LOG_WRN("Boulder: Zyklen-Tabelle voll");
  return NULL;
}

// Setzt den Eintrag mit diesem Start (vorhanden oder neu) und meldet ihn, wenn er sich
// aendert. Rueckgabe: der Eintrag, NULL ohne Platz.
static Eintrag *prv_setzen(time_t start, Art art, uint16_t laenge) {
  Eintrag *e = prv_suchen(start);
  if (e && e->nacht != s_nacht) {
    // Ein Start einer frueheren Nacht: das waere dieselbe Sitzung in Pebbles Liste.
    PBL_LOG_ERR("Boulder: Zyklus-Start einer frueheren Nacht (%d)", (int)e->art);
    return NULL;
  }
  if (!e) {
    e = prv_frei();
    if (!e) {
      return NULL;
    }
    *e = (Eintrag){ .start = start, .nacht = s_nacht };
  } else if (e->zu) {
    if (e->art != art || e->laenge != laenge) {
      s_diagnose.zu_geaendert++;
      PBL_LOG_WRN("Boulder: abgeschlossener Zyklus haette sich geaendert");
    }
    e->bestaetigt = true;
    return e;
  } else if (e->art == art && e->laenge == laenge) {
    e->bestaetigt = true;
    return e;
  }
  e->art = art;
  e->laenge = laenge;
  e->bestaetigt = true;
  prv_melden(e);
  return e;
}

static int prv_anzahl(Art art) {
  int n = 0;
  for (int i = 0; i < ZYKLEN_EINTRAEGE; i++) {
    n += (s_eintraege[i].nacht == s_nacht && s_eintraege[i].art == art);
  }
  return n;
}

// ---------------------------------------------------------------------------------------

void boulder_zyklen_zuruecksetzen(void) {
  memset(s_eintraege, 0, sizeof(s_eintraege));
  memset(&s_diagnose, 0, sizeof(s_diagnose));
  s_nacht = 0;
  s_beginn = 0;
  s_ab = 0;
  s_ein_fest = false;
  s_fertig = false;
  s_fertig_auf = 0;
  s_schlaf_bis = 0;
  s_fest_bis = 0;
  s_offen = -1;
  s_stumm = false;
}

void boulder_zyklen_nacht(time_t beginn_utc, uint16_t ab) {
  // Fertig abgeschlossene und neutrale Eintraege frueherer Naechte braucht niemand mehr:
  // Pebble hat sie geloggt oder raeumt sie selbst weg (activity_sessions.c:59, :632).
  for (int i = 0; i < ZYKLEN_EINTRAEGE; i++) {
    Eintrag *e = &s_eintraege[i];
    if (e->art != ArtLeer && (e->zu || e->art == ArtNeutral || e->art == ArtP)) {
      *e = (Eintrag){ 0 };
    }
  }
  s_nacht++;
  s_beginn = beginn_utc;
  s_ab = ab;
  s_ein_fest = false;
  s_fertig = false;
  s_fertig_auf = 0;
  s_schlaf_bis = 0;
  s_fest_bis = ab;
  s_offen = -1;
}

void boulder_zyklen_live(bool schlaeft, int lauf_beginn, uint16_t ende) {
  if (!schlaeft || !s_beginn) {
    return;
  }
  s_schlaf_bis = ende;
  Eintrag *offen = (s_offen >= 0) ? &s_eintraege[s_offen] : NULL;
  if (!offen || offen->art == ArtP) {
    // KEIN OFFENER ZYKLUS (vor dem Einschlafen oder nach "fertig"): ein P traegt den
    // Live-Zustand, bis das Nachtbuch den Zyklus festschreibt. Nach "fertig" beginnt es
    // fruehestens beim Aufwachen - sonst zaehlte es die Nacht ein zweites Mal.
    uint16_t beginn = (lauf_beginn > s_ab) ? (uint16_t)lauf_beginn : s_ab;
    beginn = (s_fertig && beginn < s_fertig_auf) ? s_fertig_auf : beginn;
    const time_t start = prv_utc(beginn);
    const bool neues_p = !offen || start >= prv_ende(offen) + ZYKLEN_P_STEHT * 60;
    if (neues_p && beginn < ende) {
      Eintrag *vorhanden = prv_suchen(start);
      const bool platz = (vorhanden && vorhanden->nacht == s_nacht) ||
                         prv_belegt() < ACTIVITY_MAX_ACTIVITY_SESSIONS_COUNT - ZYKLEN_FREI;
      if (prv_anzahl(ArtP) < ZYKLEN_P_MAX && platz) {
        Eintrag *p = prv_setzen(start, ArtP, (uint16_t)(ende - beginn));
        if (p) {
          s_offen = (int8_t)(p - s_eintraege);
          return;
        }
      }
      s_diagnose.p_entfallen++;
    }
  }
  if (!offen || offen->zu || (offen->art != ArtP && offen->art != ArtC)) {
    return;
  }
  // Das offene P (auch ueber eine kurze Wachzeit) oder der letzte Zyklus waechst bis zur
  // Minutenwende - nur nach vorn, nie in die Zukunft.
  const time_t bis = prv_utc(ende);
  if (bis > prv_ende(offen)) {
    offen->laenge = (uint16_t)((bis - offen->start) / 60);
    prv_melden(offen);
  }
}

static bool prv_schlaf(uint16_t minute) {
  const uint8_t phase = boulder_nachlauf_phase(minute);
  return phase >= SchlafPhaseLeicht && phase <= SchlafPhaseSchlaf;
}

typedef struct {
  uint16_t von;
  uint16_t bis;
} Lauf;

// DER ABGLEICH: Aus dem festen Teil des Nachtbuchs folgen die gewuenschten C (Schlaf
// zwischen geschlossenen Wachabschnitten) und R (geschlossene Tief-Abschnitte). Jeder
// Wunsch setzt den Eintrag mit seinem Start; was nicht mehr gewuenscht ist, wird neutral.
// Idempotent: derselbe Stand ergibt dieselben Sitzungen, ein ausgefallener Lauf schadet
// nicht.
static void prv_abgleich(void) {
  int ein = -1;
  for (uint16_t m = s_ab; m < s_fest_bis; m++) {
    if (prv_schlaf(m)) {
      ein = m;
      break;
    }
  }
  if (ein < 0) {
    return;
  }
  s_ein_fest = true;
  uint16_t belegt = prv_belegt();
  s_diagnose.c_ueberspannt = 0;
  s_diagnose.r_entfallen = 0;

  // Zyklen: jeder Schlaflauf nach einem festen Wachabschnitt beginnt einen neuen.
  Lauf c[ZYKLEN_C_MAX];
  int nc = 0;
  bool im_schlaf = false;
  for (uint16_t m = (uint16_t)ein; m < s_fest_bis; m++) {
    if (!prv_schlaf(m)) {
      im_schlaf = false;
      continue;
    }
    if (!im_schlaf) {
      const Eintrag *e = prv_suchen(prv_utc(m));
      const bool platz = (e && e->nacht == s_nacht) ||
                         belegt < ACTIVITY_MAX_ACTIVITY_SESSIONS_COUNT;
      if (nc == 0 || (nc < ZYKLEN_C_MAX && platz)) {
        belegt += !e;
        c[nc++].von = m;
      } else {
        // Kein Platz: der vorige Zyklus ueberspannt den Wachabschnitt.
        s_diagnose.c_ueberspannt++;
      }
    }
    c[nc - 1].bis = m + 1;
    im_schlaf = true;
  }
  // Der letzte Zyklus ist offen - ausser er endet beim Aufwachen einer fertigen Nacht
  // (ein Zyklus danach, wenn die Nacht waechst, ist wieder offen): er reicht, so weit der
  // Automat schon geschlafen hat.
  const bool letzter_offen = !s_fertig || c[nc - 1].von >= s_fertig_auf;
  const uint16_t fest_ende = c[nc - 1].bis;
  if (letzter_offen && s_schlaf_bis > c[nc - 1].bis) {
    c[nc - 1].bis = s_schlaf_bis;
  }

  // Tiefschlaf: nur geschlossene Abschnitte, nie in der Startminute eines Zyklus.
  Lauf r[ZYKLEN_R_MAX];
  int nr = 0;
  for (uint16_t m = (uint16_t)ein; m < s_fest_bis;) {
    if (boulder_nachlauf_phase(m) != SchlafPhaseTief) {
      m++;
      continue;
    }
    uint16_t von = m;
    while (m < s_fest_bis && boulder_nachlauf_phase(m) == SchlafPhaseTief) {
      m++;
    }
    if (m >= s_fest_bis) {
      break;  // noch offen: sein Ende kann sich aendern
    }
    for (int k = 0; k < nc; k++) {
      von += (c[k].von == von);
    }
    const Eintrag *e = prv_suchen(prv_utc(von));
    const bool platz = (e && e->nacht == s_nacht) ||
                       belegt < ACTIVITY_MAX_ACTIVITY_SESSIONS_COUNT - ZYKLEN_FREI;
    if (von < m && nr < ZYKLEN_R_MAX && platz) {
      belegt += !e;
      r[nr++] = (Lauf){ .von = von, .bis = m };
    } else if (von < m) {
      s_diagnose.r_entfallen++;
    }
  }

  for (int i = 0; i < ZYKLEN_EINTRAEGE; i++) {
    s_eintraege[i].bestaetigt = false;
  }
  const int8_t offenes_p = (s_offen >= 0 && s_eintraege[s_offen].art == ArtP) ? s_offen : -1;
  s_offen = -1;
  for (int k = 0; k < nc; k++) {
    Eintrag *e = prv_setzen(prv_utc(c[k].von), ArtC, c[k].bis - c[k].von);
    if (e && letzter_offen && k == nc - 1) {
      s_offen = (int8_t)(e - s_eintraege);
    }
  }
  for (int k = 0; k < nr; k++) {
    prv_setzen(prv_utc(r[k].von), ArtR, r[k].bis - r[k].von);
  }
  // Nicht mehr gewuenscht (ein R oder C, das die Endfassung anders sieht; ein P, dessen
  // Schlaf jetzt ein Zyklus traegt): neutral statt geloescht. Nur ohne offenen Zyklus traegt
  // ein P, das hinter dem festen Schlaf beginnt, weiter den Live-Zustand.
  const time_t fest_schlaf_bis = prv_utc(fest_ende);
  for (int i = 0; i < ZYKLEN_EINTRAEGE; i++) {
    Eintrag *e = &s_eintraege[i];
    if (e->nacht != s_nacht || e->art == ArtLeer || e->art == ArtNeutral || e->bestaetigt ||
        e->zu) {
      continue;
    }
    if (!letzter_offen && e->art == ArtP && e->start >= fest_schlaf_bis) {
      if (i == offenes_p && s_offen < 0) {
        s_offen = (int8_t)i;
      }
      continue;
    }
    e->art = ArtNeutral;
    e->laenge = 0;
    prv_melden(e);
  }
}

void boulder_zyklen_fest(uint16_t fest_bis) {
  if (!s_beginn) {
    return;
  }
  s_fest_bis = fest_bis;
  prv_abgleich();
}

void boulder_zyklen_fertig(uint16_t aufwachen) {
  s_fertig = true;
  s_fertig_auf = aufwachen;
  s_offen = -1;
}

void boulder_zyklen_endgueltig(void) {
  for (int i = 0; i < ZYKLEN_EINTRAEGE; i++) {
    Eintrag *e = &s_eintraege[i];
    if (e->nacht != s_nacht || e->zu) {
      continue;
    }
    if (e->art == ArtP) {
      // Ohne festes Einschlafen (Fensterende): das P war keine Nacht.
      e->art = ArtNeutral;
      e->laenge = 0;
      prv_melden(e);
    } else if (e->art == ArtC || e->art == ArtR) {
      e->schliessen = true;
    }
  }
  s_fertig = true;
  s_offen = -1;
}

void boulder_zyklen_abschluss_schritt(bool automat_wach) {
  if (!automat_wach) {
    return;
  }
  Eintrag *frueheste[2] = { NULL, NULL };  //< Schlaf, Tiefschlaf
  for (int i = 0; i < ZYKLEN_EINTRAEGE; i++) {
    Eintrag *e = &s_eintraege[i];
    if (!e->schliessen || e->zu || (e->art != ArtC && e->art != ArtR)) {
      continue;
    }
    Eintrag **f = &frueheste[e->art == ArtR];
    if (!*f || prv_ende(e) < prv_ende(*f)) {
      *f = e;
    }
  }
  if ((!frueheste[0] && !frueheste[1]) || !prv_pebble_wach()) {
    return;
  }
  for (int k = 0; k < 2; k++) {
    if (frueheste[k]) {
      frueheste[k]->zu = true;
      prv_melden(frueheste[k]);
    }
  }
}

time_t boulder_zyklen_offen_ende(void) {
  if (s_offen < 0) {
    return 0;
  }
  return prv_ende(&s_eintraege[s_offen]);
}

void boulder_zyklen_stumm(bool stumm) {
  s_stumm = stumm;
}

// Hat Pebble eine abgeschlossene Schlafsitzung mit diesem Start? (unter der Activity-Sperre)
static bool prv_pebble_zu(time_t start) {
  ActivityState *state = activity_private_state();
  bool zu = false;
  pbl_mutex_lock(&state->mutex, PBL_FOREVER);
  for (uint16_t i = 0; i < state->activity_sessions_count; i++) {
    const ActivitySession *s = &state->activity_sessions[i];
    if (s->start_utc == start && !s->ongoing &&
        (s->type == ActivitySessionType_Sleep || s->type == ActivitySessionType_RestfulSleep)) {
      zu = true;
      break;
    }
  }
  pbl_mutex_unlock(&state->mutex);
  return zu;
}

void boulder_zyklen_nach_boot(void) {
  s_stumm = false;
  for (int i = 0; i < ZYKLEN_EINTRAEGE; i++) {
    Eintrag *e = &s_eintraege[i];
    if (e->art == ArtLeer || e->art == ArtNeutral) {
      // P0 legt die Variante nach einem Neustart nicht neu an (Regel 6); der Platz wird frei.
      if (e->art == ArtNeutral && s_offen == i) {
        s_offen = -1;
      }
      *e = (Eintrag){ 0 };
      continue;
    }
    if (prv_pebble_zu(e->start)) {
      e->zu = true;
      continue;
    }
    prv_melden(e);
  }
}

bool boulder_zyklen_abschluss_offen(void) {
  for (int i = 0; i < ZYKLEN_EINTRAEGE; i++) {
    if (s_eintraege[i].schliessen && !s_eintraege[i].zu) {
      return true;
    }
  }
  return false;
}

bool boulder_zyklen_nickerchen(time_t start_utc, uint16_t laenge_min) {
  if (prv_suchen(start_utc) ||
      prv_belegt() >= ACTIVITY_MAX_ACTIVITY_SESSIONS_COUNT - ZYKLEN_FREI) {
    PBL_LOG_WRN("Boulder: Nickerchen nicht gemeldet (Platz)");
    return false;
  }
  ActivitySession s = {
    .start_utc = start_utc,
    .length_min = laenge_min,
    .type = ActivitySessionType_Sleep,
    .ongoing = false,
  };
  activity_sessions_prv_add_activity_session(&s);
  return true;
}

const BoulderZyklenDiagnose *boulder_zyklen_diagnose(void) {
  return &s_diagnose;
}
