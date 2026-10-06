/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2: die Szene des Puls-Algorithmus (D10, Spezifikation 3.7).
//
// WARUM EINE STELLE: Die Szene ist global (hrm_manager.c:778-783); Kraepelins Walk/Run
// und das Abo "Aktiv" der Variante wollen sie beide setzen, das Training auch
// (workout_service.c:490, :593). Kraepelin geht vor (Walk/Run sind gemessen, Aktiv nur
// vermutet). Zurueckgesetzt wird nur, was die Variante selbst gesetzt hat - sonst
// loeschte das Ende eines Spaziergangs die Szene eines Trainings.

#include "pbl/services/activity/boulder/variante.h"
#include "pbl/services/hrm/hrm_manager_private.h"

static HRMActivityScene s_kraepelin = HRMActivityScene_Default;
static bool s_aktiv;
static bool s_eigene_gesetzt;

static void prv_anwenden(void) {
  HRMActivityScene szene;
  if (s_kraepelin != HRMActivityScene_Default) {
    szene = s_kraepelin;
  } else if (s_aktiv) {
    szene = HRMActivityScene_HighIntensity;
  } else if (s_eigene_gesetzt) {
    szene = HRMActivityScene_Default;
  } else {
    return;
  }
  s_eigene_gesetzt = (szene != HRMActivityScene_Default);
  hrm_manager_set_activity_scene(szene);
}

void boulder_szene_kraepelin(HRMActivityScene szene) {
  s_kraepelin = szene;
  prv_anwenden();
}

void boulder_szene_aktiv(bool an) {
  s_aktiv = an;
  prv_anwenden();
}
