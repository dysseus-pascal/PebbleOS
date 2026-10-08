/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// NUR IM QEMU-PRUEFBAU (Spezifikation Schritt 3, AP1, T2.1). Ohne CONFIG_BOULDER_PRUEFLAUF bleibt
// die Datei leer.
#if defined(CONFIG_BOULDER_HEALTH_APP) && defined(CONFIG_BOULDER_PRUEFLAUF)

#include "probe.h"

#include "applib/app_logging.h"

// Die Fehlerprobe: ein Schreibzugriff in Kernel-RAM aus der App.
//
// WARUM IN EINER EIGENEN DATEI: Sie ist der einzige Code der App, der absichtlich verbotenen
// Speicher anfasst. tools/boulder/pruefe_ohne_rechte.py meldet sie im Pruefbau als rot, und zwar
// genau mit diesem Symbol; die anderen Objekte der App bleiben gruen.
//
// WARUM DIESE STELLE: Es ist dieselbe wie in Pebbles Demo "Kernel RAM W"
// (fw/apps/demo/mpu_violation_test/test_mpu_violation.c): der Boden des Kernel-Hauptstapels, mitten
// im Kernel-RAM und in keiner Region, die die App-Task sieht. So vergleicht T2.1 Gleiches mit
// Gleichem. Mit Systemrechten (Gegenprobe) landet der Wert dort, und das Skript liest ihn mit gdb.

extern const uint32_t __kernel_main_stack_start__[];

uint32_t bh_fehlerprobe_schreiben(void) {
  volatile uint32_t *ziel = (volatile uint32_t *)__kernel_main_stack_start__;
  APP_LOG(APP_LOG_LEVEL_INFO, "bh fehlerprobe schreibt %p", (void *)ziel);
  *ziel = BH_FEHLERPROBE_WERT;
  return *ziel;
}

#endif // CONFIG_BOULDER_HEALTH_APP && CONFIG_BOULDER_PRUEFLAUF
