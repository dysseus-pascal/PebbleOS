/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Boulder V2: das eine Puls-Abo der Variante - dichterer Nachtpuls und HRV-Fenster nach
// der Messstufe (Spezifikation 4.9, 5.3, 5.4).
//
// EIN ABO, DAUERHAFT. Es wird einmal angemeldet und nie abgemeldet; der Takt schaltet nur
// Merkmale und Intervall um (Setter). Im HRM-Rueckruf wird weder an- noch abgemeldet: der
// Manager laeuft dort ueber seine Abo-Liste (Use-after-free, Gutachten Stabilitaet 1).
//   ruhend       Merkmale 0           - der Manager uebergeht das Abo, kein Sensor
//   Nachtpuls    BPM, Takt der Stufe  - der Manager schaltet den Sensor selbst, sobald
//                                       das Abo seit seinem letzten guten Wert faellig ist
//   HRV-Fenster  BPM|HRV, 1 s, 5 min  - der Sensor bleibt an, die Abstaende gehen O(1) in
//                                       die Rechnung (hrv.h)
// WARUM DER MANAGER DIE FENSTER MACHT: Er gibt jeden guten BPM-Wert an alle Abos mit BPM
// und zaehlt ihn jedem als "bedient" (hrm_manager.c, hrm_manager_new_data_cb). Ein
// Fenster von Pebbles eigener Messung bedient so auch dieses Abo - es gibt kein
// Doppelfenster. Und Pebbles Rueckruf nimmt die Werte dieses Abos in den Minutenmedian
// auf (activity.c:207-210): der dichtere Puls landet ohne Pebble-Zeile im Minutensatz.
//
// WARUM DIE MERKMALE MITWECHSELN: Mit HRV dauerhaft im Abo liefe HRV bei jeder
// Nachtpuls-Messung mit, und ein faellig werdendes Abo startete Pebbles laufendes
// BPM-Fenster neu (hrm_manager.c, prv_should_reconfigure).
//
// Alles auf KernelBG. Setter und das Anmelden reihen einen nicht verwerfbaren
// KernelBG-Callback ein; sie laufen nur bei freiem Platz (2.4 Regel 2), sonst im
// naechsten Takt.

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

// Dauer eines HRV-Fensters.
#define HRV_FENSTER_MIN 5
// So lange ohne Abstand, und dazwischen fehlen Schlaege: der Sensor liefert bis zu vier
// Abstaende auf einmal, sechs Sekunden Pause sind also noch normal (b25).
#define HRV_LUECKE_MS 6000

// Minutenteil (O(1), ohne Heap, Datei, Pebble-Setter): `jetzt_utc` ist der Beginn der
// laufenden Minute, `nachtruhe` gilt fuer sie (Nachtfenster, letzte Minute ruhig). Steht
// etwas an, reiht es den Callback "Boulder Puls" ein.
void boulder_puls_minute(time_t jetzt_utc, bool nachtruhe);

// Die Messstufe hat sich geaendert (messstufe.c, KernelBG).
void boulder_puls_stufe_neu(void);

// deinit: Das Tracking endet. Ein laufendes Fenster zaehlt nicht, das Abo ruht.
void boulder_puls_tracking_aus(void);

// An den Dauerteil: ein fertiges HRV-Fenster ab `beginn_utc` (KernelBG, Callback).
void boulder_dauerteil_hrv_fenster(time_t beginn_utc, uint16_t rmssd, uint8_t puls);

// Diagnose und Tests
bool boulder_puls_hrv_laeuft(void);
uint32_t boulder_puls_abo(void);

#ifdef UNITTEST
void boulder_puls_test_zuruecksetzen(void);
#endif
