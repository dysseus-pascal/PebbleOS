/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "plattform.h"

// Puls und Pulszonen.
//
// EINE ZONE IST EIN ANTEIL DES MAXIMALPULSES, und der ist persoenlich. Eine
// Formel nach dem Alter streut um gut zehn Schlaege; deshalb kann der Wert
// in den Einstellungen stehen - ohne ihn gilt Tanaka.
//
// DER SENSOR GEHOERT HIER DEM DIENST (F2 = A). In Kieselsport bat die App das System
// um einen schnelleren Takt, und die Bitte blieb bestehen, wenn die App
// endete - die teuerste Falle der App. Hier meldet sich der Dienst selbst
// beim Sensor an und wieder ab, und die Anmeldung laeuft von allein aus,
// wenn niemand sie erneuert (puls_auffrischen). Pebbles Workout-Dienst legt kein
// eigenes Abo an; er bekommt dieselben Werte ueber Pebbles Health-Rundruf.
//
// DAS ABO STELLT DER DIENST NACH DER SPERRE (puls_sensor_faellig): Der HRM-Manager
// ruft den Pulsrueckruf unter seiner Sperre, und der nimmt die des Dienstes. Stellte
// der Dienst das Abo unter seiner Sperre, naehme er die beiden andersherum.

void puls_init(void);

// Der eingestellte Wert, sonst Tanaka nach dem Alter der Uhr - so meldet ihn die
// App dem Telefon (siehe kieselsport_maxpuls in kieselsport.h).
uint16_t puls_maximum(void);
// DER WERT, MIT DEM GERECHNET WIRD (V2, Boulders Herzgrenzen.kt:37-47): der
// eingestellte nur, wenn er ueber Ruhepuls + 20 und hoechstens Tanaka + 25 liegt,
// sonst Tanaka. Zonen, Brummen und die Pulskalorien nehmen diesen.
uint16_t puls_maximum_wirksam(void);
// Der Ruhepuls fuer diese Pruefung, 0 = keiner (dann 60 wie in Boulder). Der Dienst
// liest ihn ohne Sperre aus der Variante und reicht ihn hier herein.
void puls_setze_ruhepuls(uint8_t bpm);
bool puls_maximum_gesetzt(void);
void puls_setze_maximum(uint16_t schlaege);
void puls_maximum_nicht_setzen(void);
// Eine Zahl ohne Angabe, ob sie gesetzt ist (kieselsport_maxpuls_alt).
void puls_maximum_alt(uint16_t schlaege);

// In welcher Zone ein Wert liegt: 0 = unter Zone 1, 1..5 = Zone.
int puls_zone(uint16_t bpm);
// Untergrenze einer Zone in Schlaegen, fuer die Anzeige.
uint16_t puls_zonengrenze(int zone);

// Dichte Messung fuer die Dauer eines Trainings, normale in der Pause.
void puls_dicht_messen(void);
void puls_normal_messen(void);
// Jede Minute, solange beobachtet wird: erneuert die Anmeldung am Sensor.
void puls_auffrischen(void);
// Unter der Sperre: Ist ein Stellen des Abos faellig, true mit Takt (0 = ab) und HRV;
// der Dienst ruft danach ohne Sperre ks_puls_sensor.
bool puls_sensor_faellig(uint16_t *takt_s, bool *hrv);

// SPARSAM BEI SCHWACHEM AKKU. Unter KS_AKKU_SPARSAM Prozent misst die Uhr
// im Training alle fuenf Sekunden statt jede.
#define KS_AKKU_SPARSAM 20
void puls_setze_sparsam(bool sparsam);
bool puls_sparsam(void);

// --- HRV, fuer Yoga ---
//
// BEI YOGA IST DER PULS NICHT DIE AUSSAGE, die Herzratenvariabilitaet ist
// es. Die Uhr misst dann die Abstaende zwischen zwei Schlaegen und rechnet
// daraus den RMSSD. Nur solange ein Training laeuft.
void puls_hrv_start(void);
void puls_hrv_stop(void);
uint16_t puls_hrv_rmssd(void);   //< 0, wenn zu wenig Schlaege
uint16_t puls_hrv_anzahl(void);

// Frisch heisst: der Sensor hat sich in den letzten KS_PULS_ALT_S Sekunden
// mit einem Wert gemeldet. Am Lenker, bei kalter Haut oder lockerem Band
// kommt keiner - dann soll die Anzeige das sagen, statt eine halbe Stunde
// lang die »75« von vorhin zu zeigen.
#define KS_PULS_ALT_S 90

void puls_beobachten(void);
void puls_ignorieren(void);

uint16_t puls_lesen(void);   //< 0, wenn keiner zu haben ist
bool puls_frisch(void);

// --- Was der Sensor meldet (plattform.c, unter der Sperre)
// guete: 0 schlecht bis 4 ausgezeichnet, negativ = nicht am Handgelenk.
void puls_ereignis_bpm(uint8_t bpm, int8_t guete);
void puls_ereignis_ppi(uint16_t ppi_ms);
