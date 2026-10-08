/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Zeitstempel fuer die Ladeziele L1-L6 (Spezifikation Schritt 3, 2.6.3).
//
// WARUM IM PROTOKOLL UND NICHT IN DER APP GERECHNET: Gemessen wird in QEMU, und ein Skript liest
// die Zeilen von aussen. So bleibt die App frei von Mess-Zustand, und ohne den Schalter steht im
// Bau kein einziger Aufruf davon (BOULDER_HEALTH_ZEITEN gibt es nur im QEMU-Pruefbau).
// sys_get_ticks zaehlt 1024 je Sekunde.
//
// BH_PROTOKOLL schreibt dazu, was das Skript pruefen muss, etwa wie die App gestartet ist und
// wohin eine Taste blaettert (AP7): "bh t=<ticks> <text>".
#ifdef CONFIG_BOULDER_HEALTH_ZEITEN
#include "applib/app_logging.h"
#include "syscall/syscall.h"
#define BH_PROTOKOLL(fmt, ...) \
  APP_LOG(APP_LOG_LEVEL_INFO, "bh t=%lu " fmt, (unsigned long)sys_get_ticks(), ##__VA_ARGS__)
#else
#define BH_PROTOKOLL(fmt, ...) do { } while (0)
#endif
#define BH_ZEIT(stelle) BH_PROTOKOLL("%s", (stelle))
