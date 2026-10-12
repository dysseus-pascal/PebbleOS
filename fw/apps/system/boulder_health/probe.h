/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Die Probe ohne Systemrechte (Spezifikation Schritt 3, AP1; T2.1, T2.5). Nur im QEMU-Pruefbau
// (CONFIG_BOULDER_PRUEFLAUF), nie in einem Bau fuer die Uhr.
//
// WIE SIE STARTET: Die Pruefbefehle "boulder probe" und "boulder fehlerprobe" (boulder/shell.c)
// oeffnen die App wie "Open App" aus einem Pin, also mit APP_LAUNCH_TIMELINE_ACTION und einem
// Startargument. Nur fuer diesen Grund reicht Pebble das Argument an die App durch
// (process_manager.c, sys_process_get_launch_args). Die Werte liegen weit ausserhalb der
// Kartentypen der Pins (HealthCardType 0 und 1), ein echter Pin trifft sie nie.
//
// WARUM UEBER DEN START UND NICHT IN JEDEM START DES PRUEFBAUS: Die Ladezeiten (L1, AP8) misst
// derselbe Pruefbau. Liefe die Probe bei jedem Start mit, mass L1 die Probe statt der App.

// Jeder Baustein aus 3.6 und jeder Helfer aus 3.7.5 einmal, dann bleibt die Probe offen, bis
// Zurueck sie schliesst. Danach liest "boulder stapel" den Tiefstand des Syscall-Stapels.
#define BH_PROBE_BAUSTEINE 0x42480001u

// Schreibt nach 300 ms in Kernel-RAM, an dieselbe Stelle wie Pebbles Demo "Kernel RAM W"
// (fw/apps/demo/mpu_violation_test/). Ohne Systemrechte muss die MPU das abfangen: Die App endet,
// die Uhr laeuft weiter.
#define BH_PROBE_FEHLER 0x42480002u

// L6 (AP8): Die App ruft jeden Syscall ihrer Daten 100-mal hintereinander und meldet die Ticks
// ("bh syscall <name> <ticks fuer 100>"). Ein einzelner Aufruf liegt unter der Aufloesung der Ticks
// (1/1024 s); hundert machen ihn messbar.
#define BH_PROBE_SYSCALLS   0x42480003u
#define BH_PROBE_SYSCALLS_N 100

// S4 aus der App ohne Systemrechte (AP2, T6.1 in QEMU): Die App setzt die Messstufe n (1-5) wie
// spaeter die Karte Settings und endet. Argument BH_PROBE_STUFE + n.
#define BH_PROBE_STUFE 0x42480010u

// Der Wert, den die Fehlerprobe schreibt; die Gegenprobe mit Systemrechten liest ihn zurueck.
#define BH_FEHLERPROBE_WERT 0xDEADBEEFu

#if defined(CONFIG_BOULDER_PRUEFLAUF)
#include <stdbool.h>
#include <stdint.h>

// true, wenn die App fuer eine Probe gestartet ist; dann ist die Probe schon gelaufen und die App
// endet. false: der gewoehnliche Start.
bool bh_probe(void);

// fehlerprobe.c: schreibt BH_FEHLERPROBE_WERT in Kernel-RAM und liest zurueck. Kehrt nur zurueck,
// wenn die App Systemrechte hat (Gegenprobe).
uint32_t bh_fehlerprobe_schreiben(void);
#else
#define bh_probe() false
#endif
