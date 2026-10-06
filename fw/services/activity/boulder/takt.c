/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2: benannte Arbeit auf KernelBG (takt.h).

#include "pbl/services/activity/boulder/takt.h"

#include <stdint.h>

#include <pbl/logging/logging.h>

PBL_LOG_MODULE_DECLARE(service_activity, CONFIG_SERVICE_ACTIVITY_LOG_LEVEL);

// Die gemessenen Arbeiten. Eingetragen wird beim ersten Lauf: KernelBG und KernelMain (Dialog)
// koennen gleichzeitig eintragen, deshalb holt sich jede ihren Platz mit einem atomaren
// Zaehler. Jede Arbeit laeuft nur auf einer Task, ihre Werte schreibt also nur diese.
static BoulderAufgabe *s_gemessen[BOULDER_MESSUNG_MAX];
static int s_gemessen_anzahl;
static uint32_t s_schlange_min = UINT32_MAX;
static BoulderAufgabe s_minutenteil = BOULDER_AUFGABE("Boulder Minutenteil", NULL);

bool boulder_aufgabe_einreihen(BoulderAufgabe *aufgabe) {
  if (aufgabe->steht_an) {
    return true;
  }
  // Knappe Schlange: nicht einreihen; der naechste Anlass versucht es wieder.
  const uint32_t frei = system_task_get_available_space();
  if (frei < s_schlange_min) {
    s_schlange_min = frei;
  }
  if (frei < BOULDER_SCHLANGE_FREI) {
    PBL_LOG_DBG("Boulder: %s wartet, Schlange knapp", aufgabe->name);
    return false;
  }
  // DER MERKER VOR DEM EINREIHEN: Reiht eine andere Task ein (Menue auf der App-Task,
  // BlobDB-Abo auf KernelMain), kann KernelBG den Callback laufen lassen, bevor der
  // Merker danach gesetzt waere - er bliebe dann fuer immer stehen und nichts kaeme mehr.
  aufgabe->steht_an = true;
  if (!system_task_add_callback_droppable(aufgabe->lauf, NULL)) {
    aufgabe->steht_an = false;
    PBL_LOG_DBG("Boulder: %s verworfen, Schlange voll", aufgabe->name);
    return false;
  }
  return true;
}

BoulderLauf boulder_aufgabe_beginnt(BoulderAufgabe *aufgabe) {
  aufgabe->steht_an = false;
  return (BoulderLauf){.aufgabe = aufgabe, .beginn = rtc_get_ticks()};
}

void boulder_aufgabe_endet(BoulderLauf *lauf) {
  const RtcTicks dauer = rtc_get_ticks() - lauf->beginn;
  const uint64_t ms = (dauer * 1000 + RTC_TICKS_HZ / 2) / RTC_TICKS_HZ;  // gerundet
  const uint16_t ms16 = (ms > UINT16_MAX) ? UINT16_MAX : (uint16_t)ms;
  BoulderAufgabe *a = lauf->aufgabe;
  a->laeufe++;
  a->summe_ticks += (uint32_t)dauer;
  if (ms16 > a->max_ms) {
    a->max_ms = ms16;
  }
  // Der Tageswechsel (KernelBG) setzt dies zurueck, waehrend der Dialog (KernelMain) es
  // schreiben kann: schlimmstenfalls faellt ein Wert genau an der Tagesgrenze weg.
  if (ms16 > a->tag_max_ms) {
    a->tag_max_ms = ms16;
  }
  if (!a->gemeldet) {
    a->gemeldet = true;
    const int platz = __atomic_fetch_add(&s_gemessen_anzahl, 1, __ATOMIC_RELAXED);
    if (platz < BOULDER_MESSUNG_MAX) {
      s_gemessen[platz] = a;
    } else {
      PBL_LOG_WRN("Boulder: %s nicht in der Diagnose, Liste voll", a->name);
    }
  }
}

int boulder_messung_liste(BoulderAufgabe *const **liste) {
  *liste = s_gemessen;
  const int n = __atomic_load_n(&s_gemessen_anzahl, __ATOMIC_RELAXED);
  return (n < BOULDER_MESSUNG_MAX) ? n : BOULDER_MESSUNG_MAX;
}

BoulderAufgabe *boulder_messung_minutenteil(void) {
  return &s_minutenteil;
}

uint32_t boulder_messung_schlange_min(void) {
  return s_schlange_min;
}

void boulder_messung_tag_neu(void) {
  BoulderAufgabe *const *liste;
  const int n = boulder_messung_liste(&liste);
  for (int i = 0; i < n; i++) {
    if (liste[i]) {
      liste[i]->tag_max_ms = 0;
    }
  }
  s_schlange_min = UINT32_MAX;
}
