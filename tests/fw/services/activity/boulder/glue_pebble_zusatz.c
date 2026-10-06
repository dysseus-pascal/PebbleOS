/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder: Zusatz fuer Pebbles Glue-Test (test_activity_algorithm_kraepelin.c) gegen die
// Kopie (Spezifikation 9, T2.2).
//
// ANGEPASST UND BEGRUENDET: Die Kopie schreibt Minutendatei und DLS 81 nicht mehr im
// Minutenlauf, sondern im Callback "Minutendatei" (D12); die DLS-Sitzung legt der
// Callback "Anlage" an (D9). Auf der Uhr laufen beide auf KernelBG gleich nach dem
// Minutenlauf. Pebbles Test ruft den Minutenlauf direkt; --wrap haengt deshalb das
// Abarbeiten der KernelBG-Schlange an jeden Aufruf - genau das, was die Uhr tut. Der Test
// selbst bleibt unveraendert. Ohne diese Anpassung scheitern sechs der acht Tests (alle,
// die Datei, DLS oder den Schlafabzug beim Schreiben pruefen; geprueft 06.10.2026),
// mit ihr laufen alle acht.

#include "pbl/services/activity/activity_algorithm.h"

void fake_system_task_callbacks_invoke_pending(void);
void __real_activity_algorithm_minute_handler(time_t utc_sec, AlgMinuteRecord *record_out);

void __wrap_activity_algorithm_minute_handler(time_t utc_sec, AlgMinuteRecord *record_out) {
  __real_activity_algorithm_minute_handler(utc_sec, record_out);
  fake_system_task_callbacks_invoke_pending();
}

// Pebbles Test ersetzt den Kern durch Attrappen (kalg_*); die neue Lesefunktion der Kopie
// (D7) fehlt dort. Die Teilminute prueft der Test nur auf ihre Schritte.
typedef struct KAlgState KAlgState;
void boulder_kalg_minute_stats_peek(KAlgState *state, uint16_t *vmc, uint8_t *orientation) {
  *vmc = 0;
  *orientation = 0;
}
