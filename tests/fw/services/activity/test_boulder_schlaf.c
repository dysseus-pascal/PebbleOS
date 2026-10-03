/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "pbl/services/activity/boulder/schlafanalyse.h"
#include "pbl/util/size.h"

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

// Die Nacht vom 26./27.9.2026, festgehalten: die Spalte `phase` ist, was diese
// Rechnung ergibt. Aendern sich die Regeln, wird sie neu geschrieben - der
// Test faengt Aenderungen, die niemand wollte. Zuletzt neu am 3.10.2026: die
// Aufwach-Schuebe um 06:00 und 06:30 sind wach, und die HRV um 02:00 und 04:00
// (36 und 37 ms) verwirft den Tiefschlaf dort nicht mehr. Der Puls dieser
// Nacht ist duenn (alle zehn Minuten), deshalb bleibt das REM aus der Unruhe.
void test_boulder_schlaf__echte_nacht_26_wie_festgehalten(void) {
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

  // Zum Nachlesen (und zum Neuschreiben der Spalte): je Minute ein Zeichen.
  static char lauf[SCHLAF_MINUTEN_MAX + 1];
  for (int i = 0; i < n; i++) {
    const int p = (i >= s_ergebnis.einschlafen && i < s_ergebnis.aufwachen)
        ? (int)prv_phase_bei(&s_ergebnis, i) : -1;
    lauf[i] = p < 0 ? '.' : "wltrs"[p];
  }
  lauf[n] = 0;
  printf("\nNACHT26 ein=%d auf=%d leicht=%d tief=%d rem=%d wach=%d\nLAUF26 %s\n",
         s_ergebnis.einschlafen, s_ergebnis.aufwachen,
         prv_minuten(&s_ergebnis, SchlafPhaseLeicht), prv_minuten(&s_ergebnis, SchlafPhaseTief),
         prv_minuten(&s_ergebnis, SchlafPhaseRem), prv_minuten(&s_ergebnis, SchlafPhaseWach), lauf);

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

// ---------------------------------------------------------------------------
// Echte Naechte aus Boulders Nachtexport, neben einem Fitbit. Minute 0 ist
// 20:00; die Spalten minute,uhrzeit,schritte,vmc,puls,... (die 29.9. hat
// statt der Schritte das Bewegungsbyte - beides wird uebersprungen). Neuere
// Exporte haengen die HRV-Fenster an (hrv_minute,uhrzeit,rmssd,puls).

static int prv_lade_nacht(const char *datei, int *hrv_anzahl) {
  char pfad[512];
  snprintf(pfad, sizeof(pfad), "%s/activity/boulder/%s", CLAR_FIXTURE_PATH, datei);
  FILE *f = fopen(pfad, "r");
  cl_assert(f);
  char zeile[256];
  int n = 0;
  int hrv_n = 0;
  bool hrv_teil = false;
  while (fgets(zeile, sizeof(zeile), f)) {
    // Kopfzeilen (die Nacht, wie Boulder sie sah, und die Spalten) ueberspringen.
    if (zeile[0] == '#' || !strncmp(zeile, "minute,", 7)) continue;
    if (!strncmp(zeile, "hrv_minute,", 11)) {
      hrv_teil = true;
      continue;
    }
    int minute, wert, puls;
    char uhr[8];
    if (hrv_teil) {
      if (sscanf(zeile, "%d,%7[^,],%d,%d", &minute, uhr, &wert, &puls) != 4) continue;
      cl_assert(hrv_n < SCHLAF_HRV_MAX);
      s_hrv[hrv_n++] = (SchlafHrv){ .minute = minute, .rmssd = wert, .puls = puls };
      continue;
    }
    if (sscanf(zeile, "%d,%7[^,],%*d,%d,%d", &minute, uhr, &wert, &puls) != 4) continue;
    cl_assert(minute < SCHLAF_MINUTEN_MAX);
    s_gueltig[minute] = true;
    s_bewegung[minute] = (uint16_t)wert;
    s_puls[minute] = (uint8_t)puls;
    if (minute + 1 > n) n = minute + 1;
  }
  fclose(f);
  if (hrv_anzahl) {
    *hrv_anzahl = hrv_n;
  }
  return n;
}

// Fitbits Nacht, Minute fuer Minute (w, l, t, r; '.' ausserhalb). Die
// Abschnitte gelten in der gegebenen Reihenfolge - spaetere ueberschreiben.
typedef struct {
  const char *von;   //< "HH:MM"
  const char *bis;   //< erste Minute danach
  char phase;
} FitbitAbschnitt;

static int prv_minute_ab_20(const char *uhr) {
  const int m = atoi(uhr) * 60 + atoi(uhr + 3) - 20 * 60;
  return (m < 0) ? m + 24 * 60 : m;
}

static void prv_fitbit(char *ref, int n, const FitbitAbschnitt *a, int anzahl) {
  memset(ref, '.', n);
  for (int k = 0; k < anzahl; k++) {
    for (int i = prv_minute_ab_20(a[k].von); i < prv_minute_ab_20(a[k].bis) && i < n; i++) {
      ref[i] = a[k].phase;
    }
  }
}

// Die Rechnung als Zeichen je Minute, '.' ausserhalb der Nacht.
static void prv_lauf(char *lauf, int n) {
  for (int i = 0; i < n; i++) {
    const int p = (i >= s_ergebnis.einschlafen && i < s_ergebnis.aufwachen)
        ? (int)prv_phase_bei(&s_ergebnis, i) : -1;
    lauf[i] = p < 0 ? '.' : "wltrs"[p];
  }
  lauf[n] = 0;
}

// DIE MINUTEN-UEBEREINSTIMMUNG in Prozent: ueber alle Minuten, die bei einem
// der beiden zur Nacht gehoeren; ausserhalb der Nacht zaehlt als wach.
static int prv_uebereinstimmung(const char *lauf, const char *ref, int n) {
  int alle = 0;
  int gleich = 0;
  for (int i = 0; i < n; i++) {
    if (lauf[i] == '.' && ref[i] == '.') continue;
    const char a = (lauf[i] == '.') ? 'w' : lauf[i];
    const char b = (ref[i] == '.') ? 'w' : ref[i];
    alle++;
    gleich += (a == b) ? 1 : 0;
  }
  return alle ? (100 * gleich + alle / 2) / alle : 0;
}

static void prv_bericht(const char *name, const char *lauf, int uebereinstimmung) {
  printf("\nNACHT%s ein=%d auf=%d schlaf=%d leicht=%d tief=%d rem=%d wach=%d uebereinstimmung=%d%%\n"
         "LAUF%s %s\n", name, s_ergebnis.einschlafen, s_ergebnis.aufwachen,
         s_ergebnis.schlaf_minuten, prv_minuten(&s_ergebnis, SchlafPhaseLeicht),
         prv_minuten(&s_ergebnis, SchlafPhaseTief), prv_minuten(&s_ergebnis, SchlafPhaseRem),
         prv_minuten(&s_ergebnis, SchlafPhaseWach), uebereinstimmung, name, lauf);
}

static SchlafEingabe prv_echte_eingabe(int n, int hrv_anzahl) {
  return (SchlafEingabe){
    .bewegung = s_bewegung, .puls = s_puls, .gueltig = s_gueltig, .anzahl = n,
    .schwelle = SCHLAF_SCHWELLE_VMC, .hrv = s_hrv, .hrv_anzahl = hrv_anzahl,
  };
}

// Die Nacht vom 29./30.9.2026 (Pebble Time 2, Firmware b10, Messstufe 4) -
// dieselbe Nacht hat eine Fitbit Air mitgemessen: ins Bett 21:32, auf 05:55,
// 22 min wach, REM 2:00 h, leicht 4:51 h, tief 1:10 h. Fitbit ist selbst eine
// Schaetzung, aber eine gut gepruefte - naeher als 15 % an sie heran ist gut.
// Fitbits Phasen sind aus dem Bild abgelesen; wo seine 22 Minuten wach lagen,
// zeigt das Bild nicht - sie stehen hier als leicht.
static const FitbitAbschnitt s_fitbit_29[] = {
  { "21:32", "05:55", 'l' },
  { "22:07", "22:29", 't' }, { "22:47", "22:50", 't' }, { "00:06", "00:30", 't' },
  { "02:11", "02:29", 't' }, { "04:11", "04:30", 't' },
  { "23:02", "23:38", 'r' }, { "01:00", "01:04", 'r' }, { "01:08", "01:35", 'r' },
  { "02:59", "03:16", 'r' }, { "03:23", "03:41", 'r' }, { "04:36", "05:04", 'r' },
};

void test_boulder_schlaf__echte_nacht_neben_fitbit(void) {
  const int n = prv_lade_nacht("nacht-2026-09-29.csv", NULL);
  // 20:00 bis 06:23, mit Luecken (Minuten ohne Satz gelten als nicht getragen).
  cl_assert(n >= 600 && n <= 660);
  const SchlafEingabe e = prv_echte_eingabe(n, 0);
  cl_assert(schlafanalyse_werte(&e, &s_arbeit, &s_ergebnis));
  static char lauf[SCHLAF_MINUTEN_MAX + 1];
  static char ref[SCHLAF_MINUTEN_MAX];
  prv_lauf(lauf, n);
  prv_fitbit(ref, n, s_fitbit_29, ARRAY_LENGTH(s_fitbit_29));
  const int gleich = prv_uebereinstimmung(lauf, ref, n);
  prv_bericht("29", lauf, gleich);
  // Fitbit: ins Bett 21:32 (Minute 92), auf 05:55 (Minute 595).
  cl_assert(s_ergebnis.einschlafen >= 82 && s_ergebnis.einschlafen <= 102);
  cl_assert(s_ergebnis.aufwachen >= 585 && s_ergebnis.aufwachen <= 605);
  // Geschlafen rund 8:01 h, tief 1:10 h, REM 2:00 h - je mit Spiel.
  cl_assert(s_ergebnis.schlaf_minuten >= 450 && s_ergebnis.schlaf_minuten <= 520);
  cl_assert(prv_minuten(&s_ergebnis, SchlafPhaseTief) >= 50 && prv_minuten(&s_ergebnis, SchlafPhaseTief) <= 100);
  cl_assert(prv_minuten(&s_ergebnis, SchlafPhaseRem) >= 90 && prv_minuten(&s_ergebnis, SchlafPhaseRem) <= 150);
  // Je Minute stimmte bis zum 3.10. rund 53 %, seither 57 %.
  cl_assert(gleich >= 50);
}

// Die Nacht vom 30.9./1.10.2026, aus Boulders Nachtexport, neben einem Fitbit:
// eingeschlafen 21:58, aufgewacht 05:58. Davor zwei Stunden Wegnicken und
// Aufstehen - die alte Doesen-Regel liess die Nacht schon um 20:08 beginnen.
// Fitbits Phasen sind aus dem Bild geschaetzt (0,6 Minuten je Bildpunkt):
// ungefaehr, deshalb nur zum Nachlesen.
static const FitbitAbschnitt s_fitbit_30[] = {
  { "21:58", "05:58", 'l' },
  { "01:35", "02:16", 'w' }, { "02:19", "02:31", 'w' }, { "02:43", "02:58", 'w' },
  { "22:55", "23:15", 'r' }, { "00:31", "01:13", 'r' }, { "03:08", "03:27", 'r' },
  { "04:28", "04:46", 'r' },
  { "22:16", "22:24", 't' }, { "22:30", "22:58", 't' }, { "23:36", "00:07", 't' },
  { "02:29", "02:37", 't' }, { "03:46", "04:03", 't' }, { "05:25", "05:37", 't' },
};

void test_boulder_schlaf__echte_nacht_30_neben_fitbit(void) {
  const int n = prv_lade_nacht("nacht-2026-09-30.csv", NULL);
  cl_assert_equal_i(n, 668);
  const SchlafEingabe e = prv_echte_eingabe(n, 0);
  cl_assert(schlafanalyse_werte(&e, &s_arbeit, &s_ergebnis));
  static char lauf[SCHLAF_MINUTEN_MAX + 1];
  static char ref[SCHLAF_MINUTEN_MAX];
  prv_lauf(lauf, n);
  prv_fitbit(ref, n, s_fitbit_30, ARRAY_LENGTH(s_fitbit_30));
  prv_bericht("30", lauf, prv_uebereinstimmung(lauf, ref, n));
  // Fitbit: eingeschlafen 21:58 (Minute 118), aufgewacht 05:58 (598).
  cl_assert(s_ergebnis.einschlafen >= 108 && s_ergebnis.einschlafen <= 128);
  cl_assert(s_ergebnis.aufwachen >= 588 && s_ergebnis.aufwachen <= 615);
  // Das Aufstehen um 01:45 ist wach.
  cl_assert_equal_i(prv_phase_bei(&s_ergebnis, 350), SchlafPhaseWach);
  cl_assert(s_ergebnis.schlaf_minuten >= 400 && s_ergebnis.schlaf_minuten <= 480);
}

// Die Nacht vom 2./3.10.2026, aus Boulders Nachtexport (Messstufe 4), neben
// einem Fitbit: im Bett 22:29 bis 06:27, davon 2:11 h wach, tief 1:12 h, REM
// 1:16 h - Fitbits eigene Abschnitte, aus der App abgeschrieben. Die Uhr sah
// bis Firmware b19: 22:02 bis 06:30, 8:08 h Schlaf, 20 min wach, REM 2:15 h,
// je Minute 39 % gleich - die ruhigen Minuten zwischen den Bewegungen waren
// fuer sie Schlaf, und das Herumwaelzen vor 22:29 schon Einschlafen.
static const FitbitAbschnitt s_fitbit_0310[] = {
  { "22:29", "06:27", 'l' },
  { "22:29", "22:31", 'w' }, { "01:50", "02:29", 'w' }, { "02:46", "03:47", 'w' },
  { "04:30", "04:37", 'w' }, { "05:02", "05:30", 'w' },
  { "22:45", "23:30", 't' }, { "00:42", "01:01", 't' }, { "04:16", "04:32", 't' },
  { "23:37", "00:14", 'r' }, { "01:32", "01:41", 'r' }, { "04:48", "05:03", 'r' },
  { "05:59", "06:27", 'r' },
};

void test_boulder_schlaf__echte_nacht_3_10_neben_fitbit(void) {
  const int n = prv_lade_nacht("nacht-2026-10-03.csv", NULL);
  cl_assert_equal_i(n, 690);
  const SchlafEingabe e = prv_echte_eingabe(n, 0);
  cl_assert(schlafanalyse_werte(&e, &s_arbeit, &s_ergebnis));
  static char lauf[SCHLAF_MINUTEN_MAX + 1];
  static char ref[SCHLAF_MINUTEN_MAX];
  prv_lauf(lauf, n);
  prv_fitbit(ref, n, s_fitbit_0310, ARRAY_LENGTH(s_fitbit_0310));
  const int gleich = prv_uebereinstimmung(lauf, ref, n);
  prv_bericht("0310", lauf, gleich);
  // Eingeschlafen nach dem Herumwaelzen: Fitbit 22:29 (Minute 149).
  cl_assert(s_ergebnis.einschlafen >= 135 && s_ergebnis.einschlafen <= 160);
  cl_assert(s_ergebnis.aufwachen >= 615 && s_ergebnis.aufwachen <= 640);
  // Fitbit: 2:11 h wach, 5:47 h Schlaf.
  cl_assert(prv_minuten(&s_ergebnis, SchlafPhaseWach) >= 90 &&
            prv_minuten(&s_ergebnis, SchlafPhaseWach) <= 170);
  cl_assert(s_ergebnis.schlaf_minuten >= 300 && s_ergebnis.schlaf_minuten <= 400);
  // Die langen Wachphasen sind wach - auch in ihren ruhigen Minuten.
  cl_assert_equal_i(prv_phase_bei(&s_ergebnis, prv_minute_ab_20("02:00")), SchlafPhaseWach);
  cl_assert_equal_i(prv_phase_bei(&s_ergebnis, prv_minute_ab_20("03:20")), SchlafPhaseWach);
  cl_assert_equal_i(prv_phase_bei(&s_ergebnis, prv_minute_ab_20("05:10")), SchlafPhaseWach);
  cl_assert(prv_minuten(&s_ergebnis, SchlafPhaseTief) > 0);
  // Fitbit REM 1:16 h; vorher waren es 2:15 h, aus den Aufwach-Schueben.
  cl_assert(prv_minuten(&s_ergebnis, SchlafPhaseRem) >= 40 &&
            prv_minuten(&s_ergebnis, SchlafPhaseRem) <= 110);
  cl_assert(gleich >= 50);
}

// DIE HRV VERWIRFT DEN TIEFSCHLAF NICHT PAUSCHAL. Auf der Uhr hatte die Nacht
// 2./3.10. keine Minute Tiefschlaf: der Median ihrer Fenster lag bei 61 ms, und
// die Fenster am Tiefschlaf lagen unter drei Vierteln davon. Hier dieselbe Lage
// - ein Fenster alle 15 Minuten (Messstufe 4), im Wachen 85 ms (Bewegung), im
// Schlaf gestreut von 48 bis 72 ms, am Tiefschlaf 44 ms. GESTREUT, WIE FENSTER
// ES SIND: nur so heben die Fenster im Wachen den Median aller (64 ms) ueber
// den der ruhigen. Die alte Regel - drei Viertel des Medians aller Fenster -
// verwarf hier jeden Tiefschlaf, wie auf der Uhr.
void test_boulder_schlaf__hrv_verwirft_tiefschlaf_nicht_pauschal(void) {
  const int n = prv_lade_nacht("nacht-2026-10-03.csv", NULL);
  SchlafEingabe e = prv_echte_eingabe(n, 0);
  cl_assert(schlafanalyse_werte(&e, &s_arbeit, &s_ergebnis));
  const int tief_ohne = prv_minuten(&s_ergebnis, SchlafPhaseTief);
  cl_assert(tief_ohne > 0);

  static const uint16_t streu[] = { 48, 56, 64, 72 };
  int k = 0;
  for (int m = 0; m + 5 <= n && k < SCHLAF_HRV_MAX; m += 15) {
    uint16_t rmssd = streu[k % ARRAY_LENGTH(streu)];
    if (prv_phase_bei(&s_ergebnis, m + 2) == SchlafPhaseWach) {
      rmssd = 85;
    } else {
      // Am Tiefschlaf: die Mitte in Reichweite (15 min) einer Tiefschlafminute.
      for (int a = 0; a < s_ergebnis.abschnitt_anzahl; a++) {
        const SchlafAbschnitt *t = &s_ergebnis.abschnitte[a];
        if (t->phase == SchlafPhaseTief && m + 2 + 15 >= t->von && m + 2 - 15 < t->bis) {
          rmssd = 44;
        }
      }
    }
    s_hrv[k++] = (SchlafHrv){ .minute = m, .rmssd = rmssd };
  }
  e.hrv_anzahl = k;
  cl_assert(schlafanalyse_werte(&e, &s_arbeit, &s_ergebnis));
  cl_assert_equal_i(s_ergebnis.hrv, 64);
  cl_assert_equal_i(prv_minuten(&s_ergebnis, SchlafPhaseTief), tief_ohne);

  // Ein Fenster weit unter den anderen (unter der Haelfte) spricht weiter
  // gegen Tiefschlaf - aber nur in seiner Reichweite.
  int tief_minute = -1;
  for (int a = 0; a < s_ergebnis.abschnitt_anzahl && tief_minute < 0; a++) {
    if (s_ergebnis.abschnitte[a].phase == SchlafPhaseTief) {
      tief_minute = s_ergebnis.abschnitte[a].von;
    }
  }
  cl_assert(tief_minute >= 0);
  for (int j = 0; j < k; j++) {
    const int abstand = s_hrv[j].minute + 2 - tief_minute;
    if (abstand >= -7 && abstand <= 7) {
      s_hrv[j].rmssd = 15;
    }
  }
  cl_assert(schlafanalyse_werte(&e, &s_arbeit, &s_ergebnis));
  cl_assert(prv_minuten(&s_ergebnis, SchlafPhaseTief) < tief_ohne);
  cl_assert(prv_minuten(&s_ergebnis, SchlafPhaseTief) > 0);
}

// ---------------------------------------------------------------------------
// Die neuen Regeln einzeln, an erfundenen Naechten

// Bloecke mit Puls je Minute aus einer Folge (wiederholt), fuer ein Herz,
// das springt oder ruhig bleibt.
static void prv_puls_folge(int von, int bis, const uint8_t *folge, int laenge) {
  for (int i = von; i < bis; i++) {
    s_puls[i] = folge[(i - von) % laenge];
  }
}

void test_boulder_schlaf__aufwach_schub_ist_wach(void) {
  // Fuenf Stunden Schlaf, in der Mitte ein Schub: alle sechs Minuten eine
  // Regung, eine halbe Stunde lang. Mit Pulsstoss ist der ganze Schub wach,
  // auch die ruhigen Minuten dazwischen.
  const int b[][3] = { { 20, WACH, 80 }, { 300, RUHIG, 55 }, { 20, WACH, 80 } };
  const int n = prv_bloecke(b, 3);
  for (int i = 170; i <= 200; i += 6) {
    s_bewegung[i] = 100 * 100;   // Bewegungsbyte 100: ueber fuenf Sechzehnteln der Schwelle
  }
  s_puls[170] = 72;
  const SchlafEingabe e = prv_eingabe(n, 0);
  cl_assert(schlafanalyse_werte(&e, &s_arbeit, &s_ergebnis));
  cl_assert_equal_i(prv_phase_bei(&s_ergebnis, 185), SchlafPhaseWach);
  cl_assert_equal_i(prv_phase_bei(&s_ergebnis, 197), SchlafPhaseWach);
  cl_assert(prv_minuten(&s_ergebnis, SchlafPhaseWach) >= 30);

  // Ohne Pulsstoss sind es Zuckungen im Schlaf, kein Aufwachen.
  s_puls[170] = 56;
  cl_assert(schlafanalyse_werte(&e, &s_arbeit, &s_ergebnis));
  cl_assert(prv_phase_bei(&s_ergebnis, 185) != SchlafPhaseWach);
}

void test_boulder_schlaf__einschlafen_nach_dem_herumwaelzen(void) {
  // Ab Minute 24 ruhig (Webster haengt vier Minuten an das Wachsein), aber um
  // 28, 31 und 35 noch Regungen: eingeschlafen erst, wenn in den zwanzig
  // Minuten danach hoechstens eine kommt - nach der vorletzten.
  const int b[][3] = { { 20, WACH, 80 }, { 360, RUHIG, 55 }, { 20, WACH, 80 } };
  const int n = prv_bloecke(b, 3);
  s_bewegung[28] = 100 * 100;
  s_bewegung[31] = 100 * 100;
  s_bewegung[35] = 100 * 100;
  const SchlafEingabe e = prv_eingabe(n, 0);
  cl_assert(schlafanalyse_werte(&e, &s_arbeit, &s_ergebnis));
  cl_assert(s_ergebnis.einschlafen > 31);
  cl_assert(s_ergebnis.einschlafen <= 35);

  // Eine einzelne Regung schiebt nichts: man darf sich einmal umdrehen.
  s_bewegung[31] = RUHIG * RUHIG;
  s_bewegung[35] = RUHIG * RUHIG;
  cl_assert(schlafanalyse_werte(&e, &s_arbeit, &s_ergebnis));
  cl_assert(s_ergebnis.einschlafen < 28);
}

void test_boulder_schlaf__unruhiger_schlaf_schiebt_das_einschlafen_nicht_weit(void) {
  // Ein unruhiger Schlaefer: ab Minute 24 ruhig, aber zwei Stunden lang alle
  // acht Minuten eine Regung, bei ruhigem Herz. Das Herumwaelzen schiebt den
  // Beginn hoechstens eine halbe Stunde - sonst fiele die erste Stunde Schlaf
  // weg, Regung fuer Regung.
  const int b[][3] = { { 20, WACH, 80 }, { 360, RUHIG, 55 }, { 20, WACH, 80 } };
  const int n = prv_bloecke(b, 3);
  for (int i = 28; i < 150; i += 8) {
    s_bewegung[i] = 100 * 100;
  }
  const SchlafEingabe e = prv_eingabe(n, 0);
  cl_assert(schlafanalyse_werte(&e, &s_arbeit, &s_ergebnis));
  cl_assert(s_ergebnis.einschlafen > 24);
  cl_assert(s_ergebnis.einschlafen <= 24 + 30);
  cl_assert(s_ergebnis.schlaf_minuten >= 300);
}

void test_boulder_schlaf__rem_aus_dem_herz(void) {
  // Dichter Puls (jede Minute), der Koerper still. Wo das Herz springt, ist
  // REM; wo es gleichmaessig schlaegt, nicht - auch ohne jede Bewegung.
  const int b[][3] = { { 20, WACH, 80 }, { 360, RUHIG, 55 }, { 20, WACH, 80 } };
  const int n = prv_bloecke(b, 3);
  for (int i = 20; i < 380; i++) {
    s_bewegung[i] = 0;
  }
  static const uint8_t springt[] = { 55, 62, 54, 63, 56 };
  prv_puls_folge(200, 260, springt, ARRAY_LENGTH(springt));
  const SchlafEingabe e = prv_eingabe(n, 0);
  cl_assert(schlafanalyse_werte(&e, &s_arbeit, &s_ergebnis));
  cl_assert_equal_i(prv_phase_bei(&s_ergebnis, 230), SchlafPhaseRem);
  cl_assert(prv_phase_bei(&s_ergebnis, 150) != SchlafPhaseRem);
  cl_assert(prv_phase_bei(&s_ergebnis, 320) != SchlafPhaseRem);
}

void test_boulder_schlaf__morgenschlaf_braucht_mehr(void) {
  // 80 Minuten Schlaf sind eine Nacht - ausser der Aufrufer verlangt mehr,
  // wie nach einer schon fertigen Nacht (nacht.c).
  const int b[][3] = { { 20, WACH, 80 }, { 80, RUHIG, 55 }, { 20, WACH, 80 } };
  SchlafEingabe e = prv_eingabe(prv_bloecke(b, 3), 0);
  cl_assert(schlafanalyse_werte(&e, &s_arbeit, &s_ergebnis));
  e.min_schlaf = 90;
  cl_assert(!schlafanalyse_werte(&e, &s_arbeit, &s_ergebnis));
}
