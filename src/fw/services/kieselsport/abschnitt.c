/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "abschnitt.h"

static Abschnitt s_liste[KS_ABSCHNITT_MAX];
static int s_anzahl;        //< wie viele in der Liste stehen
static uint16_t s_gesamt;   //< wie viele es gab
static uint32_t s_summe;

void abschnitt_leeren(void) {
  s_anzahl = 0;
  s_gesamt = 0;
  s_summe = 0;
}

void abschnitt_hinzu(uint16_t beginn_s, uint16_t anzahl, uint16_t dauer_s) {
  s_gesamt++;
  s_summe += anzahl;
  // DIE ZAEHLER LAUFEN WEITER, auch wenn die Liste voll ist. Wer einundzwanzig
  // Bahnen schwimmt, hat einundzwanzig geschwommen - nur stehen die letzten
  // nicht mehr einzeln da.
  if (s_anzahl >= KS_ABSCHNITT_MAX) return;
  s_liste[s_anzahl].beginn_s = beginn_s;
  s_liste[s_anzahl].anzahl = anzahl;
  s_liste[s_anzahl].dauer_s = dauer_s;
  s_anzahl++;
}

int abschnitt_anzahl(void) { return s_anzahl; }
uint16_t abschnitt_gesamt(void) { return s_gesamt; }
uint32_t abschnitt_summe(void) { return s_summe; }

void abschnitt_als_text(char *aus, size_t platz) {
  if (platz == 0) return;
  aus[0] = '\0';
  size_t hier = 0;
  for (int i = 0; i < s_anzahl; i++) {
    char stueck[24];
    const int n = snprintf(stueck, sizeof(stueck), "%u:%u:%u;",
                           (unsigned)s_liste[i].beginn_s,
                           (unsigned)s_liste[i].anzahl,
                           (unsigned)s_liste[i].dauer_s);
    if (n <= 0) continue;
    // ABBRECHEN STATT ABSCHNEIDEN. Ein halb geschriebener Abschnitt am Ende
    // waere auf dem Telefon eine erfundene Zahl.
    if (hier + (size_t)n + 1 > platz) break;
    memcpy(aus + hier, stueck, (size_t)n);
    hier += (size_t)n;
    aus[hier] = '\0';
  }
}
