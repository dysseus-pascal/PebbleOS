/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Boulder V2: der Smart Alarm ueber einen Neustart (Entscheid 07.10.2026, Kriterium 4).
//
// WARUM: Pebble haelt den Lauf eines Smart Alarms (Fenster T-30 min bis T, jede Minute die
// Weckregel) nur im RAM. Schon beim Fensterbeginn plant alarm.c den naechsten Termin und
// speichert ihn als "scharf" (prv_timer_kernel_bg_callback -> prv_reload_alarms ->
// prv_persist_armed_alarm). Nach einem Neustart im Fenster weiss alarm.c deshalb nichts mehr
// vom laufenden Fenster. Einen Fensterbeginn, den die Uhr aus war, holt es nur bis 5 min
// nach (ALARM_MISSED_MAX_DELAY_S in alarm_init). Startet die Uhr spaeter neu, klingelt der
// Wecker an diesem Tag nicht. Diese Sicherheit will der Nutzer haben.
//
// DIE REGEL nach einem Boot, fuer einen Smart Alarm mit Weckzeit T:
// - Boot vor T-30: nichts, Pebble plant ihn wie immer.
// - Boot in [T-30, T): Er laeuft weiter, mit der strengen Regel (wecker.h): nie im
//   Tiefschlaf, spaetestens zur Weckzeit.
// - Boot in [T, T + WECKER_NACHHOLEN_S), wenn er noch nicht geklungen hat: sofort. Gibt
//   alarm.c die Wecker erst nach dem Nachholfenster frei, klingelt er nicht mehr.
// - Hat er fuer diese Weckzeit schon geklungen (dann weggedrueckt oder geschlummert): nach
//   einem Neustart nie noch einmal. Ein Schlummern, das der Neustart abbricht, ist verloren -
//   wie bei Pebble. Ohne Neustart bleibt es bei Pebble: Geht die Uhr nach dem Klingeln vor
//   das Fenster zurueck, plant alarm.c das Fenster neu, und er klingelt noch einmal.
// - Endete Pebbles Lauf vor dem Neustart ohne Klingeln (ausgeschaltet, geloescht, von einem
//   anderen Wecker abgeloest), klingelt er an diesem Morgen nicht mehr, auch wenn er gleich
//   wieder an ist: Pebble plant ihn dann fuer morgen.
// - Seither ausgeschaltet, geloescht, auf eine andere Zeit gestellt oder nicht mehr "smart":
//   nichts. Ein Einmalwecker ist ab dem Fensterbeginn aus (alarm.c), er zaehlt trotzdem.
// Holt Pebble selbst einen verpassten Wecker nach (hoechstens 5 min her), bleibt es dabei:
// alarm.c startet nur einen.
//
// WOHER DAS WISSEN KOMMT:
// - Lief sein Fenster vor dem Neustart, steht der Lauf in der Datei "boulder_wecker": E4
//   schreibt ihn bei der ersten Frage des Fensters, sein Ende (geklungen, oder Pebbles Lauf ist
//   ohne Klingeln vorbei) ist ein Byte an seinem Platz. Scheitert ein Schreiben, holt die
//   naechste Frage oder Minute es nach.
// - War die Uhr ueber den Fensterbeginn aus, ist es Pebbles "scharfer" Termin von vor dem
//   Boot: Er ist der frueheste geplante. Liegt er nicht in der Zukunft, lief alarm.c seither
//   nicht mehr (jedes Neuplanen schreibt ihn weiter) - jeder Smart Alarm, dessen Fensterbeginn
//   zwischen ihm und jetzt liegt, hat sein Fenster verpasst. Von mehreren der mit der
//   fruehesten Weckzeit.
// - Ob Pebbles Lauf noch lebt, fragt die Minute der Variante (alarm_get_most_recent_id). Ohne
//   Aktivitaetsaufzeichnung gibt es diese Minute nicht: Dann holt ein Neustart einen Lauf, den
//   der Nutzer im Fenster aus- und wieder einschaltete, doch nach.
// Die Weckzeit eines Laufs steht ab seiner ersten Frage in UTC fest, wie Pebbles Zaehler:
// Ein Zeitzonen- oder Uhrzeitsprung verschiebt sie nicht, auch nicht ueber einen Neustart.
// Was ein Sprung sonst ausloest, bleibt bei alarm.c (alarm_handle_clock_change).
//
// Dateizugriff nur unter der eigenen Sperre: E4 laeuft auf KernelBG, das Klingeln nach einem
// Uhrzeitsprung auf KernelMain. Reihenfolge immer erst alarm.c, dann diese Sperre.

#include "pbl/services/alarms/alarm.h"
#include "pbl/util/units.h"

#include <stdbool.h>
#include <time.h>

// Pebble startet einen verpassten Wecker nur, wenn die Wecker hoechstens so lange nach seiner
// Zeit freigegeben werden: ALARM_MISSED_MAX_DELAY_S, privat in alarm.c. test_boulder_wecker
// haelt beide gleich.
#define WECKLAUF_PEBBLE_VERPASST_S (5 * PBL_SEC_PER_MIN)

// alarm_init, vor dem Neuplanen (Datei "alarms" offen): Pebbles scharfer Termin von vor dem
// Boot. Liest den Lauf aus "boulder_wecker" und ersetzt allen RAM-Stand, wie ein Boot.
void boulder_wecklauf_boot(AlarmId scharf_id, time_t scharf_utc);

// Ende von alarm_init (Datei "alarms" zu): Setzt den nachzuholenden Smart Alarm in *id und
// *zeit, wenn Pebble keinen verpassten Wecker hat. alarm_service_enable_alarms startet ihn
// dann wie einen verpassten: Pebbles Lauf mit Zaehler, die Weckregel fragt E4. *zeit liegt so,
// dass Pebble ihn nach dem Nachholfenster nicht mehr startet.
void boulder_wecklauf_nachholen(AlarmId *id, time_t *zeit);

// E4 (prv_should_smart_alarm_trigger, KernelBG): Merkt sich den Lauf (Datei nur bei der
// ersten Frage, `zaehler` = Pebbles Schlummerzaehler, 0 am Anfang des Laufs) und fragt die
// Weckregel mit seiner Weckzeit.
bool boulder_wecklauf_fragen(AlarmId id, int zaehler, int stunde, int minute);

// prv_record_alarm_op: Der Wecker klingelt zum ersten Mal fuer diese Weckzeit, auf welchem
// Weg auch immer (Weckregel, Ende des Zaehlers, Uhrzeitsprung).
void boulder_wecklauf_geklungen(AlarmId id);

// Minutenteil der Variante (KernelBG): ohne Datei und ohne Sperre. Laeuft ein Lauf, schaut eine
// Aufgabe auf KernelBG nach, ob Pebbles Lauf noch lebt, und holt ein gescheitertes Schreiben
// nach.
void boulder_wecklauf_minute(void);
