/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Boulder V2: der QEMU-Pruefbau (Spezifikation 7.5, AP9) - nur mit CONFIG_BOULDER_PRUEFLAUF.
//
// `boulder prueflauf <n> [tage]` spielt eine Nacht im Zeitraffer durch den echten Minutenteil der
// Variante, den Takt und alle Callbacks, die sie einreiht (Nachlauf, Ablage, Ausgabe,
// Anlage), mit der echten activity_sessions.c und dem echten Datalogging des QEMU-Abbilds:
//   0-3  die Referenznaechte 26.9., 29.9., 30.9., 3.10. (prueflauf_daten.c)
//   4    die schlimmste Nacht: Puls jede Minute, zehn Zyklen, 84 HRV-Fenster (zwei Teile)
//   5    eine Velofahrt (braucht den Ruhepuls einer Nacht davor): Nachfrage-Dialog
//
// WIE AUF DER UHR: Je Glied genau eine Minute; das naechste erst, wenn die KernelBG-Schlange
// wieder leer ist - alles, was die Minute einreiht, laeuft vorher ganz durch wie zwischen zwei
// echten Minuten.
// Die Minuten liegen im Nachtfenster, das zur QEMU-Uhrzeit gehoert (20 Uhr Ortszeit plus
// Minute), damit Pebbles Liste sie als juengste Nacht fuehrt. Mit `tage` (Schritt 3, AP3) liegt
// die Nacht so viele Fenster frueher: So bekommen Pruefnaechte verschiedene Wachtage, und die
// Liste der Naechte der Health-App (naechte.h) waechst wie an aufeinanderfolgenden Tagen. Waehrend
// des Laufs nimmt der Dauerteil keine echten Minuten und keinen Cron-Takt an (sonst mischten sich
// zwei Zeiten). Vor einer Nacht (0-4) beginnt der Dauerteil neu (boulder_dauerteil_test_neustart).
//
// BALLAST: Beim ersten Lauf holt der Pruefbau einen Block, der den Kernel-Heap auf die Groesse
// von obelix bringt (CONFIG_BOULDER_PRUEFLAUF_HEAP, Spezifikation 7.2), und fuellt die
// Minutendatei (activity_algorithm_test_fill_minute_file), damit Verdichtungen vorkommen.

#include "pbl/services/activity/boulder/variante.h"

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#define PRUEFLAUF_NAECHTE 4

typedef struct {
  uint16_t minute;
  uint16_t rmssd;
  uint8_t puls;
} PrueflaufHrv;

typedef struct {
  const char *name;
  const uint16_t *vmc;    //< SCHLAF_MINUTEN_MAX Minuten, 0xFFFF = ungueltig
  const uint8_t *puls;
  const PrueflaufHrv *hrv;
  uint8_t hrv_anzahl;
} PrueflaufNacht;

extern const PrueflaufNacht g_boulder_prueflauf_naechte[PRUEFLAUF_NAECHTE];

// Shell (KernelBG): Rueckgabe false, wenn `nr` unbekannt ist, `tage` nicht 0..30 oder ein Lauf
// laeuft.
bool boulder_prueflauf_starten(int nr, int tage);

// Den Ballast holen, einmal je Boot (KernelBG). Ihn brauchen die Pruefnaechte und das lange
// Training von Kieselsport (Schritt 2, T7.5). false: Er fehlt, der Heap ist groesser als auf obelix.
bool boulder_prueflauf_ballast(void);

// Naht im Dauerteil (dauerteil.c, nur im Pruefbau): echte Minuten und Cron-Takt sperren,
// eine Pruefminute durchreichen, das Nachtfenster einer Zeit.
void boulder_dauerteil_prueflauf(bool laeuft);
void boulder_dauerteil_prueflauf_minute(const BoulderMinute *minute);
time_t boulder_dauerteil_fenster(time_t utc);
