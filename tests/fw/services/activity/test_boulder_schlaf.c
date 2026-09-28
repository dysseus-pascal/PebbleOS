/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "pbl/services/activity/boulder/schlafanalyse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Die Schlafanalyse der Boulder-Firmware, gegen die Kotlin-Fassung aus
// Kiesel-Helper.
//
// DIE ECHTE NACHT ZUERST. Der CSV-Export aus Kiesel-Helper traegt je Minute die
// Rohwerte UND die Phase, die die Kotlin-Fassung daraus gerechnet hat. Die
// C-Fassung muss jede Minute genauso sehen - sonst ist die Uebertragung falsch,
// nicht die Nacht.
//
// DIE EINHEITEN WIE IM EXPORT. Dort steht das Bewegungsbyte b der Uhr; Kotlin
// rechnet mit b*b/16 und der Schwelle 800. Hier geht b*b hinein und die
// Schwelle 800*16 - derselbe Vergleich, ganzzahlig und ohne Rundung.

#define SCHWELLE_EXPORT (SCHLAF_SCHWELLE_VMC * 16)

static uint16_t s_bewegung[SCHLAF_MINUTEN_MAX];
static uint8_t s_puls[SCHLAF_MINUTEN_MAX];
static bool s_gueltig[SCHLAF_MINUTEN_MAX];
static SchlafHrv s_hrv[SCHLAF_HRV_MAX];
static SchlafArbeit s_arbeit;
static SchlafErgebnis s_ergebnis;

void test_boulder_schlaf__initialize(void) {
  memset(s_bewegung, 0, sizeof(s_bewegung));
  memset(s_puls, 0, sizeof(s_puls));
  memset(s_gueltig, 0, sizeof(s_gueltig));
  memset(s_hrv, 0, sizeof(s_hrv));
}

static SchlafEingabe prv_eingabe(int anzahl, int hrv_anzahl) {
  return (SchlafEingabe){
    .bewegung = s_bewegung,
    .puls = s_puls,
    .gueltig = s_gueltig,
    .anzahl = anzahl,
    .schwelle = SCHWELLE_EXPORT,
    .hrv = s_hrv,
    .hrv_anzahl = hrv_anzahl,
  };
}

static SchlafPhase prv_phase_bei(const SchlafErgebnis *e, int minute) {
  for (int k = 0; k < e->abschnitt_anzahl; k++) {
    if (minute >= e->abschnitte[k].von && minute < e->abschnitte[k].bis) {
      return e->abschnitte[k].phase;
    }
  }
  return (SchlafPhase)-1;
}

static int prv_minuten(const SchlafErgebnis *e, SchlafPhase p) {
  int summe = 0;
  for (int k = 0; k < e->abschnitt_anzahl; k++) {
    if (e->abschnitte[k].phase == p) {
      summe += e->abschnitte[k].bis - e->abschnitte[k].von;
    }
  }
  return summe;
}

// Bloecke aus (Minuten, Bewegungsbyte, Puls); Bewegungsbyte 255 = ungueltig.
static int prv_bloecke(const int (*bloecke)[3], int anzahl) {
  int n = 0;
  for (int b = 0; b < anzahl; b++) {
    for (int k = 0; k < bloecke[b][0]; k++, n++) {
      s_gueltig[n] = bloecke[b][1] != 255;
      s_bewegung[n] = s_gueltig[n] ? bloecke[b][1] * bloecke[b][1] : 0;
      s_puls[n] = bloecke[b][2];
    }
  }
  return n;
}

#define WACH 160
#define RUHIG 4

// ---------------------------------------------------------------------------
// Die Nacht vom 26./27.9.2026 (Pebble Time 2, Firmware 4.38.1)

static SchlafPhase prv_phase_aus_text(const char *t) {
  if (!strcmp(t, "wach")) return SchlafPhaseWach;
  if (!strcmp(t, "leicht")) return SchlafPhaseLeicht;
  if (!strcmp(t, "tief")) return SchlafPhaseTief;
  if (!strcmp(t, "rem")) return SchlafPhaseRem;
  if (!strcmp(t, "schlaf")) return SchlafPhaseSchlaf;
  return (SchlafPhase)-1;  // leer: ausserhalb der Nacht
}

void test_boulder_schlaf__echte_nacht_wie_kotlin(void) {
  char pfad[512];
  snprintf(pfad, sizeof(pfad), "%s/activity/boulder/nacht-2026-09-26.csv", CLAR_FIXTURE_PATH);
  FILE *f = fopen(pfad, "r");
  cl_assert(f);

  static int s_erwartet[SCHLAF_MINUTEN_MAX];
  char zeile[256];
  int n = 0;
  int hrv_n = 0;
  bool hrv_teil = false;
  while (fgets(zeile, sizeof(zeile), f)) {
    zeile[strcspn(zeile, "\r\n")] = 0;
    if (zeile[0] == 0) {
      continue;
    }
    if (!strncmp(zeile, "minute,", 7)) {
      continue;
    }
    if (!strncmp(zeile, "hrv_minute,", 11)) {
      hrv_teil = true;
      continue;
    }
    // Felder von Hand trennen: leere Felder sind hier Bedeutung, strtok
    // verschluckte sie.
    char *feld[8] = { 0 };
    int k = 0;
    char *p = zeile;
    feld[k++] = p;
    while (*p && k < 8) {
      if (*p == ',') {
        *p = 0;
        feld[k++] = p + 1;
      }
      p++;
    }
    if (hrv_teil) {
      cl_assert(hrv_n < SCHLAF_HRV_MAX);
      s_hrv[hrv_n++] = (SchlafHrv){ .minute = atoi(feld[0]), .rmssd = atoi(feld[2]) };
    } else {
      cl_assert(n < SCHLAF_MINUTEN_MAX);
      const int b = feld[2][0] ? atoi(feld[2]) : -1;
      s_gueltig[n] = b >= 0;
      s_bewegung[n] = b >= 0 ? b * b : 0;
      s_puls[n] = feld[4][0] ? atoi(feld[4]) : 0;
      s_erwartet[n] = prv_phase_aus_text(feld[7] ? feld[7] : "");
      n++;
    }
  }
  fclose(f);
  cl_assert_equal_i(n, 540);
  cl_assert_equal_i(hrv_n, 18);

  const SchlafEingabe e = prv_eingabe(n, hrv_n);
  cl_assert(schlafanalyse_werte(&e, &s_arbeit, &s_ergebnis));

  int abweichungen = 0;
  for (int i = 0; i < n; i++) {
    const int ist = (i >= s_ergebnis.einschlafen && i < s_ergebnis.aufwachen)
        ? (int)prv_phase_bei(&s_ergebnis, i) : -1;
    if (ist != s_erwartet[i]) {
      if (abweichungen < 10) {
        printf("Minute %d: erwartet %d, gerechnet %d\n", i, s_erwartet[i], ist);
      }
      abweichungen++;
    }
  }
  cl_assert_equal_i(abweichungen, 0);
  // Eingeschlafen 22:17, geweckt vom Ende der Daten.
  cl_assert_equal_i(s_ergebnis.einschlafen, 17);
  cl_assert_equal_i(s_ergebnis.aufwachen, 540);
}

// ---------------------------------------------------------------------------
// Erfundene Naechte, deren Antwort man kennt (aus SchlafanalyseTest.kt)

void test_boulder_schlaf__einschlaflatenz(void) {
  // Eine Stunde unruhig im Bett, dann sechs Stunden Schlaf.
  const int b[][3] = { { 60, 120, 70 }, { 360, RUHIG, 55 }, { 20, WACH, 80 } };
  const SchlafEingabe e = prv_eingabe(prv_bloecke(b, 3), 0);
  cl_assert(schlafanalyse_werte(&e, &s_arbeit, &s_ergebnis));
  cl_assert(s_ergebnis.einschlafen >= 60 && s_ergebnis.einschlafen <= 66);
  cl_assert(s_ergebnis.schlaf_minuten >= 340 && s_ergebnis.schlaf_minuten <= 360);
}

void test_boulder_schlaf__still_wach_mit_hohem_puls(void) {
  const int b[][3] = { { 200, RUHIG, 52 }, { 30, RUHIG, 85 }, { 200, RUHIG, 52 } };
  const SchlafEingabe e = prv_eingabe(prv_bloecke(b, 3), 0);
  cl_assert(schlafanalyse_werte(&e, &s_arbeit, &s_ergebnis));
  cl_assert_equal_i(prv_phase_bei(&s_ergebnis, 215), SchlafPhaseWach);
}

void test_boulder_schlaf__fehlende_daten(void) {
  // Anderthalb Stunden ohne Uhr mitten in der Nacht, und am Anfang.
  const int b[][3] = { { 20, 255, 0 }, { 180, RUHIG, 54 }, { 90, 255, 0 }, { 180, RUHIG, 54 } };
  const SchlafEingabe e = prv_eingabe(prv_bloecke(b, 4), 0);
  cl_assert(schlafanalyse_werte(&e, &s_arbeit, &s_ergebnis));
  cl_assert(s_ergebnis.einschlafen >= 20);
  // Was nicht gemessen wurde, ist kein Schlaf.
  cl_assert_equal_i(prv_phase_bei(&s_ergebnis, 245), SchlafPhaseWach);
  cl_assert(s_ergebnis.schlaf_minuten >= 340 && s_ergebnis.schlaf_minuten <= 362);
}

void test_boulder_schlaf__ohne_puls_nur_schlaf_und_wach(void) {
  const int b[][3] = { { 20, WACH, 0 }, { 200, RUHIG, 0 }, { 20, WACH, 0 }, { 200, RUHIG, 0 } };
  const SchlafEingabe e = prv_eingabe(prv_bloecke(b, 4), 0);
  cl_assert(schlafanalyse_werte(&e, &s_arbeit, &s_ergebnis));
  for (int k = 0; k < s_ergebnis.abschnitt_anzahl; k++) {
    const int p = s_ergebnis.abschnitte[k].phase;
    cl_assert(p == SchlafPhaseSchlaf || p == SchlafPhaseWach);
  }
  cl_assert(prv_minuten(&s_ergebnis, SchlafPhaseSchlaf) > 0);
  cl_assert_equal_i(s_ergebnis.ruhepuls, 0);
  cl_assert_equal_i(s_ergebnis.hrv, 0);
}

void test_boulder_schlaf__zu_kurz_ist_keine_nacht(void) {
  const int b[][3] = { { 30, WACH, 80 }, { 50, RUHIG, 55 }, { 30, WACH, 80 } };
  SchlafEingabe e = prv_eingabe(prv_bloecke(b, 3), 0);
  cl_assert(!schlafanalyse_werte(&e, &s_arbeit, &s_ergebnis));
  e.anzahl = 0;
  cl_assert(!schlafanalyse_werte(&e, &s_arbeit, &s_ergebnis));
  const int nur_wach[][3] = { { 300, WACH, 80 } };
  e = prv_eingabe(prv_bloecke(nur_wach, 1), 0);
  cl_assert(!schlafanalyse_werte(&e, &s_arbeit, &s_ergebnis));
}

void test_boulder_schlaf__abschnitte_lueckenlos(void) {
  // Nach dem Glaetten: kein Abschnitt unter fuenf Minuten, keine Luecke.
  const int b[][3] = { { 30, WACH, 85 }, { 170, RUHIG, 56 }, { 20, WACH, 88 }, { 230, RUHIG, 55 } };
  const SchlafEingabe e = prv_eingabe(prv_bloecke(b, 4), 0);
  cl_assert(schlafanalyse_werte(&e, &s_arbeit, &s_ergebnis));
  cl_assert_equal_i(s_ergebnis.abschnitte[0].von, s_ergebnis.einschlafen);
  cl_assert_equal_i(s_ergebnis.abschnitte[s_ergebnis.abschnitt_anzahl - 1].bis,
                    s_ergebnis.aufwachen);
  for (int k = 0; k < s_ergebnis.abschnitt_anzahl; k++) {
    cl_assert(s_ergebnis.abschnitte[k].bis - s_ergebnis.abschnitte[k].von >= 5);
    if (k > 0) {
      cl_assert_equal_i(s_ergebnis.abschnitte[k].von, s_ergebnis.abschnitte[k - 1].bis);
    }
  }
}
