/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "plattform.h"
#include "art.h"

// Ein Training: was gerade läuft, und was davon übrig bleibt.
//
// DIE SUMMEN WERDEN NICHT MITGEZÄHLT, SONDERN AM ENDE ABGEZOGEN. Die Uhr
// führt Schritte, Distanz und Kalorien ohnehin über den ganzen Tag; ein
// Training ist die Differenz zwischen Anfang und Ende. Selbst zu zählen
// hiesse, dieselbe Messung ein zweites Mal zu machen - mit eigenen Fehlern.
//
// Ausnahme ist der Puls: der ist ein Augenblickswert und kein Zähler. Er wird
// aufsummiert, solange das Training läuft.
//
// Die Typen (Laufzustand, Trainingsstand) stehen in kieselsport.h, weil die
// App sie auch braucht.

void training_init(void);

Laufzustand training_zustand(void);
Sportart training_art(void);

void training_starte(Sportart art);
// Mit vorgegebenem Beginn (0 = jetzt) - dem, den die App schon ans Telefon
// gemeldet hat.
void training_starte_ab(Sportart art, uint32_t beginn);
void training_pause_umschalten(void);
// Beendet das Training und legt es ins Archiv. Gibt den Stand zurueck.
Trainingsstand training_stoppe(void);
// Beendet das Training, ohne es zu behalten - ein Fehlgriff, ein Abbruch.
void training_verwerfen(void);
uint32_t training_beginn(void);
// Wann die aktive Zeit endete (Unix-Sekunden): der Beginn der offenen Pause,
// sonst jetzt. Gilt auch nach dem Stoppen, bis das naechste Training beginnt.
uint32_t training_ende(void);

// Der laufende Stand, jederzeit abfragbar (auch in der Pause).
Trainingsstand training_stand(void);

// Sekunde fuer Sekunde vom Zeitgeber gerufen.
void training_tick(void);

// Aktueller Puls, 0 wenn keiner zu haben ist.
uint16_t training_puls(void);
// Ob der Puls frisch ist - oder nur der letzte gute Wert von vorhin.
bool training_puls_frisch(void);

// Die Sekunden seit dem Start - fuer alles, was Abschnitte zeitlich einordnet.
uint16_t training_sekunde(void);

// --- Archiv ---

int training_archiv_anzahl(void);
// index 0 ist das juengste Training.
bool training_archiv_lesen(int index, Trainingsstand *aus);
