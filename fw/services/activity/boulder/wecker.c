/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2: die strenge Weckregel (Spezifikation 6). Warum so: wecker.h.

#include "pbl/services/activity/boulder/wecker.h"

#include "pbl/services/activity/boulder/live.h"
#include "pbl/services/time.h"
#include "pbl/util/units.h"
#include <pbl/drivers/rtc.h>

// Beginn der letzten eigenen Minute; 0 = keine seit dem Boot oder seit "Tracking aus".
// Statisch und nur im RAM: nach einem Neustart ist die Regel erst mit der ersten neuen
// Minute bereit. Bis dahin klingelt der Wecker erst zur Weckzeit - lieber spaet als im Tief.
static time_t s_letzte_utc;
static uint32_t s_bewegt;
// Eigene Minuten in Folge bis zur letzten, ohne Luecke seit Boot oder "Tracking aus".
static uint8_t s_in_folge;

void boulder_wecker_minute(time_t minute_utc, uint16_t vmc) {
  if (s_letzte_utc == 0 || minute_utc < s_letzte_utc) {
    // Erste Minute oder die Uhr ging zurueck: aeltere Bits liessen sich nicht mehr einordnen.
    s_bewegt = 0;
    s_in_folge = 0;
  } else {
    // Fehlende Minuten (Luecke, Zeitsprung vor) zaehlen als ohne Bewegung.
    const time_t schritt = (minute_utc - s_letzte_utc) / PBL_SEC_PER_MIN;
    s_bewegt = (schritt >= 32) ? 0 : (s_bewegt << schritt);
    s_in_folge = (schritt > 1) ? 0 : s_in_folge;
  }
  s_bewegt |= (vmc > 0) ? WECKER_S1 : 0;
  s_in_folge += (s_in_folge < UINT8_MAX) ? 1 : 0;
  s_letzte_utc = minute_utc;
}

void boulder_wecker_vergessen(void) {
  s_letzte_utc = 0;
  s_bewegt = 0;
  s_in_folge = 0;
}

time_t boulder_wecker_weckzeit(time_t jetzt, int stunde, int minute) {
  const int seit_weckzeit =
      (time_util_get_minute_of_day(jetzt) - (stunde * PBL_MIN_PER_HOUR + minute) +
       PBL_MIN_PER_DAY) % PBL_MIN_PER_DAY;
  const time_t minute_beginn = jetzt - (jetzt % PBL_SEC_PER_MIN);
  if (seit_weckzeit < WECKER_NACHHOLEN_S / PBL_SEC_PER_MIN) {
    return minute_beginn - (time_t)seit_weckzeit * PBL_SEC_PER_MIN;
  }
  return minute_beginn + (time_t)(PBL_MIN_PER_DAY - seit_weckzeit) * PBL_SEC_PER_MIN;
}

bool boulder_wecker_erlaubt(time_t weckzeit) {
  const time_t jetzt = rtc_get_time();
  // Spaetestens zur Weckzeit: ab der Weckzeit immer, bis zum Ende des Nachholfensters. Pebbles
  // Zaehler endet nur zur Weckzeit, wenn er am Fensterbeginn anfing; nach einem nachgeholten
  // Wecker (Neustart im Fenster oder danach) oder einem Zeitsprung vor endete er spaeter.
  if (jetzt >= weckzeit && jetzt < weckzeit + WECKER_NACHHOLEN_S) {
    return true;
  }
  // Nicht bereit: keine eigene Minute seit Boot/Tracking aus, die Uhr ging zurueck, oder die
  // letzte Minute ist zu alt. Dann weiss die Variante nicht, ob gerade Tief ist.
  if (s_letzte_utc == 0 || jetzt < s_letzte_utc ||
      jetzt - (s_letzte_utc + PBL_SEC_PER_MIN) > WECKER_FRISCH_S) {
    return false;
  }
  // S1: Bewegung in der letzten Minute.
  if (s_bewegt & WECKER_S1) {
    return true;
  }
  // W: wach im Bett, kurz nach Bewegung (der Automat entscheidet zwei Minuten nach). Erst wenn
  // der Automat nach einem Boot oder einer Luecke wieder einschlafen konnte (WECKER_W_AB).
  return s_in_folge >= WECKER_W_AB && !boulder_live_schlaeft() && (s_bewegt & WECKER_W) != 0;
}
