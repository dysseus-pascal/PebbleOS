/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2: die strenge Weckregel (Spezifikation 6). Warum so: wecker.h.

#include "pbl/services/activity/boulder/wecker.h"

#include "pbl/services/activity/boulder/live.h"
#include "pbl/services/alarms/alarm.h"
#include "pbl/services/time.h"
#include "pbl/util/units.h"
#include <pbl/drivers/rtc.h>

// Beginn der letzten eigenen Minute; 0 = keine seit dem Boot oder seit "Tracking aus".
// Statisch und nur im RAM: nach einem Neustart ist die Regel erst mit der ersten neuen
// Minute bereit. Bis dahin klingelt der Wecker erst zur Weckzeit - lieber spaet als im Tief.
static time_t s_letzte_utc;
static uint32_t s_bewegt;

void boulder_wecker_minute(time_t minute_utc, uint16_t vmc) {
  if (s_letzte_utc == 0 || minute_utc < s_letzte_utc) {
    // Erste Minute oder die Uhr ging zurueck: aeltere Bits liessen sich nicht mehr einordnen.
    s_bewegt = 0;
  } else {
    // Fehlende Minuten (Luecke, Zeitsprung vor) zaehlen als ohne Bewegung.
    const time_t schritt = (minute_utc - s_letzte_utc) / PBL_SEC_PER_MIN;
    s_bewegt = (schritt >= 32) ? 0 : (s_bewegt << schritt);
  }
  s_bewegt |= (vmc > 0) ? WECKER_S1 : 0;
  s_letzte_utc = minute_utc;
}

void boulder_wecker_vergessen(void) {
  s_letzte_utc = 0;
  s_bewegt = 0;
}

bool boulder_kraepelin_wecken_erlaubt(int stunde, int minute) {
  const time_t jetzt = rtc_get_time();
  // Spaetestens zur Weckzeit: in den 30 min ab der Weckzeit immer. Pebbles Zaehler endet nur
  // zur Weckzeit, wenn er am Fensterbeginn anfing; nach einem nachgeholten Wecker (Neustart
  // ueber den Fensterbeginn) oder einem Zeitsprung vor endete er spaeter.
  const int seit_weckzeit =
      (time_util_get_minute_of_day(jetzt) - (stunde * PBL_MIN_PER_HOUR + minute) +
       PBL_MIN_PER_DAY) % PBL_MIN_PER_DAY;
  if (seit_weckzeit < SMART_ALARM_RANGE_S / PBL_SEC_PER_MIN) {
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
  // W: wach im Bett, kurz nach Bewegung (der Automat entscheidet zwei Minuten nach).
  return !boulder_live_schlaeft() && (s_bewegt & WECKER_W) != 0;
}
