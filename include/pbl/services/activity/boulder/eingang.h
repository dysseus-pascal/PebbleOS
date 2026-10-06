/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Boulder V2: die Minuten der Nacht fuer den Schlafkern (Spezifikation 4.1).
//
// FESTE FELDER, KEIN HEAP, KEINE DATEI: Minute 0 ist der Beginn des
// Nachtfensters (beginn_utc), hoechstens SCHLAF_MINUTEN_MAX Minuten. Jede
// Minute kommt einmal aus dem Minutenteil der Variante; die Rechnung
// (nachlauf.c) zeigt mit Zeigern hinein, ohne Kopie.
//
// KEIN BETRIEBSSYSTEM HIER. Sperren und Takt liegen beim Aufrufer.

#include "pbl/services/activity/boulder/schlafanalyse.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  uint16_t bewegung[SCHLAF_MINUTEN_MAX];  //< VMC
  uint8_t puls[SCHLAF_MINUTEN_MAX];       //< Minutenmedian, 0 = keiner
  bool gueltig[SCHLAF_MINUTEN_MAX];       //< getragen, Satz da, kein Training
  SchlafHrv hrv[SCHLAF_HRV_MAX];          //< `minute` ab Minute 0 der Nacht
  uint8_t hrv_anzahl;
  int16_t letzte;   //< die zuletzt angenommene Minute, -1 = keine
  int16_t ende;     //< letzte gueltige Minute + 1, 0 = keine
} BoulderEingang;

// Neue Nacht: alle Minuten ungueltig, keine HRV.
void boulder_eingang_neu(void);

// Die Minute `minute` (ab Minute 0). DER ERSTE SATZ GILT: eine Minute, die
// nicht nach der zuletzt angenommenen liegt (doppelt, Uhr zurueck), wird
// verworfen. Eine uebersprungene Minute bleibt ungueltig. Rueckgabe: angenommen.
bool boulder_eingang_minute(int minute, uint16_t vmc, uint8_t puls, bool gueltig);

// Ein HRV-Fenster, das bei `minute` beginnt (fuenf Minuten), in zeitlicher
// Folge. Rueckgabe: angenommen (falsch, wenn voll oder nicht nach dem letzten).
bool boulder_eingang_hrv(int minute, uint16_t rmssd, uint8_t puls);

const BoulderEingang *boulder_eingang(void);
