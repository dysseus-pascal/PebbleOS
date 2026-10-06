/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2: benannte Arbeit auf KernelBG (takt.h).

#include "pbl/services/activity/boulder/takt.h"

#include <pbl/logging/logging.h>

PBL_LOG_MODULE_DECLARE(service_activity, CONFIG_SERVICE_ACTIVITY_LOG_LEVEL);

bool boulder_aufgabe_einreihen(BoulderAufgabe *aufgabe) {
  if (aufgabe->steht_an) {
    return true;
  }
  // Knappe Schlange: nicht einreihen; der naechste Anlass versucht es wieder.
  if (system_task_get_available_space() < BOULDER_SCHLANGE_FREI) {
    PBL_LOG_DBG("Boulder: %s wartet, Schlange knapp", aufgabe->name);
    return false;
  }
  if (!system_task_add_callback_droppable(aufgabe->lauf, NULL)) {
    PBL_LOG_DBG("Boulder: %s verworfen, Schlange voll", aufgabe->name);
    return false;
  }
  aufgabe->steht_an = true;
  return true;
}

void boulder_aufgabe_laeuft(BoulderAufgabe *aufgabe) {
  aufgabe->steht_an = false;
}
