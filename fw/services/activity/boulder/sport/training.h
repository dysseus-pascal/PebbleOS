/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "plattform.h"
#include "art.h"

// Ein Training: was gerade laeuft, und was davon uebrig bleibt.
//
// PEBBLE ZAEHLT, KIESELSPORT RECHNET NICHT NACH (Weg 2). Dauer, Schritte und Strecke fuehrt
// Pebbles Workout-Dienst, die kcal Boulders Pulsrechnung in demselben Dienst. Der Dienst reicht
// sie hier herein (training_werte); selbst zu zaehlen hiesse, dieselbe Messung ein zweites Mal
// zu machen - mit eigenen Fehlern, und Sitzung und Zusammenfassung stimmten nicht ueberein.
//
// EINE UHR FUER DIE AKTIVE ZEIT (Spezifikation 2.5, Regel 3): Die Sekunde des Trainings ist
// Pebbles duration_s. Pebbles Timer laesst bei belegter Sperre einen Tick aus; der Dienst ruft
// training_tick deshalb so oft, wie Pebbles Dauer seit dem letzten Mal gewachsen ist, und nicht
// einmal je Takt.
//
// Ausnahme ist der Puls: der ist ein Augenblickswert und kein Zaehler. Mittel und Maximum
// (10006/10007) rechnet Kieselsport wie bisher aus seinem geglaetteten Puls.
//
// Die Typen (Laufzustand, Trainingsstand) stehen in kieselsport.h, weil die App sie auch
// braucht.

void training_init(void);

Laufzustand training_zustand(void);
Sportart training_art(void);

// Mit vorgegebenem Beginn (0 = jetzt) - dem, den die App schon ans Telefon gemeldet hat.
void training_starte_ab(Sportart art, uint32_t beginn);
void training_pause_umschalten(void);
// Beendet das Training. Gibt den Stand zurueck.
Trainingsstand training_stoppe(void);
// Beendet das Training, ohne es zu behalten - ein Fehlgriff, ein Abbruch.
void training_verwerfen(void);
uint32_t training_beginn(void);
// Wann die aktive Zeit endete (Unix-Sekunden): der Beginn der offenen Pause, sonst jetzt. Gilt
// auch nach dem Stoppen, bis das naechste Training beginnt.
uint32_t training_ende(void);

// Der laufende Stand, jederzeit abfragbar (auch in der Pause).
Trainingsstand training_stand(void);

// Was Pebble zaehlt: Schritte, Strecke und kcal (aus dem Puls), wie zuletzt gelesen.
void training_werte(const KsWorkout *w);

// Eine aktive Sekunde. Der Dienst ruft es, bis training_sekunde() Pebbles Dauer erreicht.
void training_tick(void);
// Ein Zeitsprung: weiter bei [sekunde], ohne die Sekunden dazwischen einzeln zu zaehlen.
void training_springe(uint32_t sekunde);

// Aktueller Puls, 0 wenn keiner zu haben ist.
uint16_t training_puls(void);
// Ob der Puls frisch ist - oder nur der letzte gute Wert von vorhin.
bool training_puls_frisch(void);

// Die aktiven Sekunden seit dem Start (Pebbles duration_s, soweit verarbeitet).
uint32_t training_sekunde(void);
