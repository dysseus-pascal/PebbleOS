/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Was die Health-App zeigt, einmal je Start gelesen (Spezifikation Schritt 3, 2.6.2).
//
// NUR KLEINE KOPIEN: Alles kommt aus den Syscalls S1-S7 der Variante und wenigen Metriken von
// Pebble. Die App liest keinen Minutenverlauf, keine Datei und nichts vom Telefon (L5); daran war
// b25 langsam (daten.c auf main, 24 Lesungen je Tag beim Start und jede Minute).
//
// WANN GELESEN WIRD: einmal vor dem ersten Bild (Phase 1), fehlendes bis zu fuenfmal im
// Sekundenabstand nach (Phase 2), danach nur im Minutentakt (Auffrischen). Eine Taste liest nie
// (L3): Karten und Details zeichnen aus dieser Struktur.

#include "rechnung.h"

#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/boulder/nacht_satz.h"
#include "pbl/services/activity/boulder/schnittstelle.h"
#include "pbl/services/activity/boulder/spo2.h"

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

// Quellen, die nach dem Boot erst kommen, wenn KernelBG sie geladen hat (Phase 2).
typedef enum {
  BhQuelleNacht = 1 << 0,   //!< S1 sagte "spaeter"
  BhQuelleNaechte = 1 << 1, //!< S5 false
  BhQuelleHerz = 1 << 2,    //!< S6 false
  BhQuelleWoche = 1 << 3,   //!< S7 false
  BhQuelleLage = 1 << 4,    //!< S2 false
} BhQuelle;

// Wie oft Phase 2 nachfragt, im Abstand von BH_NACHLADEN_MS.
#define BH_NACHFRAGEN   5
#define BH_NACHLADEN_MS 1000

typedef struct {
  // Uhr und Einstellungen
  time_t jetzt;
  int32_t utc_zu_ort; //!< Sekunden, Sommerzeit inbegriffen (tm_gmtoff)
  uint16_t heute;     //!< Tage seit 1.1.2000, Ortszeit
  uint16_t minute;    //!< Minute des Tages, Ortszeit
  uint8_t wochentag;  //!< 0 = Montag
  bool h24;
  bool meilen;

  // Variante (S1-S7)
  bool lage_da;
  BoulderLage lage;
  BoulderLesen nacht_lesen;
  NachtBild nacht;
  bool naechte_da;
  BoulderNaechte naechte;
  bool herz_da;
  BoulderHerz herz;
  bool woche_da;
  BoulderWoche woche;
  uint32_t spo2_anzahl;
  Spo2Wert spo2[SPO2_WERTE_MAX]; //!< die letzten 24 h, aelteste zuerst

  // Pebble
  int32_t schritte;
  int32_t meter;
  int32_t aktiv_s;
  int32_t kcal; //!< Ruhe- und Aktivkalorien (V12)
  int32_t puls; //!< bpm, 0 = keiner
  time_t puls_zeit;
  uint32_t sitzungen_anzahl;
  ActivitySession sitzungen[ACTIVITY_MAX_ACTIVITY_SESSIONS_COUNT];

  // Laden
  uint8_t offen;      //!< BhQuelle-Bits, die Phase 2 noch holt
  uint8_t nachfragen; //!< Phase 2 bisher
} BhDaten;

// Phase 1: alles, vor dem ersten Bild.
void bh_daten_laden(BhDaten *d);

// Phase 2: nur, was in `offen` steht. true, solange noch etwas fehlt und eine Nachfrage bleibt.
bool bh_daten_nachladen(BhDaten *d);

// Im Minutentakt: Uhrzeit, Lage, Herz, Woche, SpO2, Metriken und Sitzungen neu; Nacht (S1) und
// Naechte (S5) nur, wenn die Lage eine neue Nacht meldet.
void bh_daten_auffrischen(BhDaten *d);

// Nur der Puls jetzt (Health-Ereignis, Puls live auf Heart).
void bh_daten_puls(BhDaten *d);

// Die Naechte als BhNachtTag fuer rechnung.c, in der Reihenfolge des Rings. Rueckgabe: wie viele.
uint8_t bh_daten_nachttage(const BhDaten *d, BhNachtTag aus[NAECHTE_MAX]);
