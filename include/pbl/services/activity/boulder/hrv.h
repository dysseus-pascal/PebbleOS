/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/activity/boulder/schlafanalyse.h"

#include <stdint.h>
#include <time.h>

// HRV in der Nacht: alle paar Minuten (Messstufe) fuenf Minuten lang die
// Schlagabstaende, daraus der RMSSD.
//
// VORHER TAT ES DER KIESELSPORT-WORKER, alle 30 Minuten. In der Firmware braucht
// es keinen Worker, und die Fenster gehen gleich in die Schlafanalyse: eine
// niedrige HRV spricht gegen Tiefschlaf. Die Rechnung ist Kieselsports, damit
// alte und neue Werte vergleichbar bleiben.

#define HRV_FENSTER_MIN 5
#define HRV_FENSTER_MAX SCHLAF_HRV_MAX

// Jede Minute aus dem Aktivitaetsdienst (KernelBG). `letzte_bewegung`: das VMC
// der letzten Minute - gemessen wird nur in Ruhe.
void boulder_hrv_minute(time_t utc, uint16_t letzte_bewegung);

// Die Fenster, deren Mitte in [von, bis) liegt, als Minuten ab `von`.
// Rueckgabe: wie viele.
int boulder_hrv_fenster(time_t von, time_t bis, SchlafHrv *aus, int max);

// Fuer die Tests: ein Intervall so zaehlen, als kaeme es vom Sensor.
void boulder_hrv_intervall(uint16_t ppi_ms);
// Fuer die Tests: den laufenden RMSSD, 0 unter 30 Intervallen.
uint16_t boulder_hrv_rmssd(void);
// Der Puls aus denselben Intervallen, in bpm; 0 unter 30 Intervallen.
uint8_t boulder_hrv_puls(void);
