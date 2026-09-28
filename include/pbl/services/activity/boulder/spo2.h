/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <time.h>

// Die SpO2-Messungen der letzten Zeit, fuer die Health-App der Uhr.
//
// WARUM EIN EIGENER SPEICHER: die Firmware legt SpO2 nur in den Minutensatz
// fuer das Telefon (Datalogging). Die Minutenhistorie, die Apps lesen koennen,
// hat dafuer kein Feld. Also merkt sich die Uhr die Werte hier, im Ring.
//
// 96 WERTE reichen fuer einen Tag und eine Nacht bei der dichtesten Messstufe
// (alle zehn Minuten sind es 144 am Tag, aber gemessen wird nur, wenn die Uhr
// ruhig am Arm liegt). Nach einem Neustart ist der Ring leer - die Werte
// selbst hat das Telefon.

#define SPO2_WERTE_MAX 96

typedef struct {
  time_t zeit;
  uint8_t prozent;
} Spo2Wert;

void boulder_spo2_merken(time_t utc, uint8_t prozent);

// Die Werte in [von, bis), aelteste zuerst. Rueckgabe: wie viele.
int boulder_spo2_werte(time_t von, time_t bis, Spo2Wert *aus, int max);
