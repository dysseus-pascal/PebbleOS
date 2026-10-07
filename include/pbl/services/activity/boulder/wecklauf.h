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
// - Boot in [T, T + WECKER_NACHHOLEN_S), wenn er noch nicht geklungen hat: sofort.
// - Hat er fuer diese Weckzeit schon geklungen (dann weggedrueckt oder geschlummert): nie
//   noch einmal. Ein Schlummern, das der Neustart abbricht, ist verloren - wie bei Pebble.
// - Seither ausgeschaltet, geloescht, auf eine andere Zeit gestellt oder nicht mehr "smart":
//   nichts. Ein Einmalwecker ist ab dem Fensterbeginn aus (alarm.c), er zaehlt trotzdem.
// Holt Pebble selbst nach (Fensterbeginn hoechstens 5 min her), bleibt es bei Pebble.
//
// WOHER DAS WISSEN KOMMT:
// - Lief sein Fenster vor dem Neustart, steht der Lauf in der Datei "boulder_wecker": E4
//   schreibt ihn bei der ersten Frage des Fensters, das erste Klingeln schreibt "geklungen".
//   Das sind zwei kleine Schreibvorgaenge je Wecker und Morgen.
// - War die Uhr ueber den Fensterbeginn aus, ist es Pebbles "scharfer" Wecker von vor dem
//   Boot (Cron-Zeit = T-30).
// Die Weckzeit eines Laufs steht ab seiner ersten Frage in UTC fest, wie Pebbles Zaehler:
// Ein Zeitzonen- oder Uhrzeitsprung verschiebt sie nicht, auch nicht ueber einen Neustart.
// Was ein Sprung sonst ausloest, bleibt bei alarm.c (alarm_handle_clock_change).
//
// Dateizugriff nur unter der eigenen Sperre: E4 laeuft auf KernelBG, das Klingeln nach einem
// Uhrzeitsprung auf KernelMain. Reihenfolge immer erst alarm.c, dann diese Sperre.

#include "pbl/services/alarms/alarm.h"

#include <stdbool.h>
#include <time.h>

// alarm_init, vor dem Neuplanen (Datei "alarms" offen): Pebbles scharfer Wecker von vor dem
// Boot. Liest den Lauf aus "boulder_wecker" und ersetzt allen RAM-Stand, wie ein Boot.
void boulder_wecklauf_boot(AlarmId scharf_id, time_t scharf_utc);

// Ende von alarm_init (Datei "alarms" zu): Setzt den nachzuholenden Smart Alarm in *id und
// *zeit, wenn Pebble keinen verpassten Wecker hat. alarm_service_enable_alarms startet ihn
// dann wie einen verpassten: Pebbles Lauf mit Zaehler, die Weckregel fragt E4.
void boulder_wecklauf_nachholen(AlarmId *id, time_t *zeit);

// E4 (prv_should_smart_alarm_trigger, KernelBG): Merkt sich den Lauf (Datei nur bei der
// ersten Frage, `zaehler` = Pebbles Schlummerzaehler, 0 am Anfang des Laufs) und fragt die
// Weckregel mit seiner Weckzeit.
bool boulder_wecklauf_fragen(AlarmId id, int zaehler, int stunde, int minute);

// prv_record_alarm_op: Der Wecker klingelt zum ersten Mal fuer diese Weckzeit, auf welchem
// Weg auch immer (Weckregel, Ende des Zaehlers, Uhrzeitsprung).
void boulder_wecklauf_geklungen(AlarmId id);
