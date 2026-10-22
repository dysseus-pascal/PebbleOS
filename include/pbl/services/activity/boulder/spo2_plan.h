/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Boulder V2: wann Pebbles SpO2-Plan messen darf (Entscheid vom 22.10.2026).
//
// Pebbles Plan (activity.c, prv_spo2_subscription_update) misst im Takt der Messstufe, wenn
// "Blood Oxygen" an ist (Schalter "Intervall"). Mit Boulders Schalter "Schlaf" (messstufe.h)
// darf er auch ohne ihn messen, aber nur in Nachtruhe - dieselbe wie fuer den Nachtpuls
// (puls.h): im Nachtfenster und die eben fertige Minute ruhig. Alles andere bleibt Pebbles Plan:
// Takt, Vortritt fuer Puls und Training, flach oder nicht am Arm. Endet die Nachtruhe mitten in
// einer Messung, endet sie wie bei "Blood Oxygen" aus.

#include <stdbool.h>

// Die Regel: `intervall` ist Pebbles "Blood Oxygen", `schlaf` Boulders Schalter, `nachtruhe` die
// der laufenden Minute.
bool boulder_spo2_plan(bool intervall, bool schlaf, bool nachtruhe);

// Fuer Pebbles Plan (KernelBG): die Regel mit dem geladenen Schalter und der Nachtruhe des
// Puls-Abos. O(1), ohne Datei.
bool boulder_spo2_plan_offen(bool intervall);
