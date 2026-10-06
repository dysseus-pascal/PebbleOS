/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Boulder: die Kraepelin-Kopie traegt eigene Namen. So lassen sich Original und
// Kopie in einem Programm binden - der Vergleichstest tut das, und ein Bau mit
// beiden bliebe moeglich. Die Kopie selbst bleibt Zeile fuer Zeile wie das
// Original; die Namen kommen hierher (D0). Die Liste sind alle nach aussen
// sichtbaren Funktionen des Kerns; der Glue legt nur activity_algorithm_* an,
// die Naht, die nur einer der beiden besetzen kann.
//
// Die Tests binden die Original-Tests gegen die Kopie: tests/fw/services/activity/
// boulder/CMakeLists.txt liest diese Datei und setzt dieselben Namen als -D.

#define kalg_activities_update boulder_kalg_activities_update
#define kalg_activity_hrm_is_active boulder_kalg_activity_hrm_is_active
#define kalg_activity_hrm_set_paused boulder_kalg_activity_hrm_set_paused
#define kalg_activity_last_processed_time boulder_kalg_activity_last_processed_time
#define kalg_analyze_finish_epoch boulder_kalg_analyze_finish_epoch
#define kalg_analyze_samples boulder_kalg_analyze_samples
#define kalg_deinit boulder_kalg_deinit
#define kalg_enable_activity_tracking boulder_kalg_enable_activity_tracking
#define kalg_get_sleep_stats boulder_kalg_get_sleep_stats
#define kalg_init boulder_kalg_init
#define kalg_minute_stats boulder_kalg_minute_stats
#define kalg_state_size boulder_kalg_state_size
