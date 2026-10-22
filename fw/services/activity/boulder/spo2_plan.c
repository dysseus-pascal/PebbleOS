/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2: wann Pebbles SpO2-Plan messen darf (spo2_plan.h).

#include "pbl/services/activity/boulder/spo2_plan.h"

#include "pbl/services/activity/boulder/messstufe.h"
#include "pbl/services/activity/boulder/puls.h"

bool boulder_spo2_plan(bool intervall, bool schlaf, bool nachtruhe) {
  return intervall || (schlaf && nachtruhe);
}

bool boulder_spo2_plan_offen(bool intervall) {
  return boulder_spo2_plan(intervall, boulder_spo2_schlaf_geladen(), boulder_puls_nachtruhe());
}
