/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "pbl/services/activity/boulder/eingang.h"
#include "pbl/services/activity/boulder/nachlauf.h"
#include "pbl/services/activity/boulder/nacht_satz.h"
#include "pbl/services/activity/boulder/schlafanalyse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

// DER SCHLAFKERN VON V2 RECHNET WIE b26 (AP2, Spezifikation 4.3, T3.1).
//
// Jede Referenznacht (tests/fixtures/activity/boulder/referenz-b26, AP-G) laeuft
// Minute fuer Minute durch Eingang und Nachlauf (Takt: Minute = 2 modulo 5, ab
// der ersten Stunde, wie b26 rechnet). Zu jeder Minute, in der b26 einen Satz
// 0x4001 schrieb, setzt V2 die Endfassung zusammen; der Satz daraus muss Byte fuer
// Byte dem von b26 gleichen. Die Protokollbytes (Akku, Messstufe) kommen aus dem
// Modell der Referenz (80 % um 20:00, -1 je Stunde), nicht aus dem Referenzsatz.

#define MINUTEN SCHLAF_MINUTEN_MAX
#define SAETZE_MAX 8

typedef struct {
  uint32_t beginn_utc;
  uint8_t stufe;
  bool gueltig[MINUTEN];
  uint16_t vmc[MINUTEN];
  uint8_t puls[MINUTEN];
  int hrv_anzahl;
  SchlafHrv hrv[SCHLAF_HRV_MAX];
  int saetze;
  int satz_um[SAETZE_MAX];
  NachtDlsRecord satz[SAETZE_MAX];
  char phasen[MINUTEN + 1];
} Referenz;

static Referenz s_ref;

static void prv_hex(const char *hex, void *aus, size_t n) {
  cl_assert_equal_i(strlen(hex), 2 * n);
  uint8_t *b = aus;
  for (size_t k = 0; k < n; k++) {
    const char z[3] = { hex[2 * k], hex[2 * k + 1], 0 };
    b[k] = (uint8_t)strtoul(z, NULL, 16);
  }
}

static void prv_lade(const char *name) {
  static char zeile[4096];
  memset(&s_ref, 0, sizeof(s_ref));
  char pfad[512];
  snprintf(pfad, sizeof(pfad), "%s/activity/boulder/referenz-b26/%s.txt", CLAR_FIXTURE_PATH,
           name);
  FILE *f = fopen(pfad, "r");
  cl_assert(f);
  while (fgets(zeile, sizeof(zeile), f)) {
    zeile[strcspn(zeile, "\r\n")] = 0;
    char *wert = strchr(zeile, ' ');
    if (zeile[0] == '#' || !wert) {
      continue;
    }
    *wert++ = 0;
    int a, b, c;
    if (!strcmp(zeile, "beginn_utc")) {
      s_ref.beginn_utc = (uint32_t)atol(wert);
    } else if (!strcmp(zeile, "messstufe")) {
      s_ref.stufe = (uint8_t)atoi(wert);
    } else if (!strcmp(zeile, "eingabe") && sscanf(wert, "%d %d %d", &a, &b, &c) == 3) {
      cl_assert(a >= 0 && a < MINUTEN);
      s_ref.gueltig[a] = true;
      s_ref.vmc[a] = (uint16_t)b;
      s_ref.puls[a] = (uint8_t)c;
    } else if (!strcmp(zeile, "hrv") && sscanf(wert, "%d %d %d", &a, &b, &c) == 3) {
      cl_assert(s_ref.hrv_anzahl < SCHLAF_HRV_MAX);
      s_ref.hrv[s_ref.hrv_anzahl++] =
          (SchlafHrv){ .minute = (uint16_t)a, .rmssd = (uint16_t)b, .puls = (uint8_t)c };
    } else if (!strcmp(zeile, "satz4001")) {
      cl_assert(s_ref.saetze < SAETZE_MAX);
      s_ref.satz_um[s_ref.saetze] = atoi(wert);
      prv_hex(strchr(wert, ' ') + 1, &s_ref.satz[s_ref.saetze], sizeof(NachtDlsRecord));
      // Eine Nacht dieser Referenzen passt in einen Teil.
      cl_assert_equal_i(s_ref.satz[s_ref.saetze].teile, 1);
      s_ref.saetze++;
    } else if (!strcmp(zeile, "phasen")) {
      cl_assert_equal_i(strlen(wert), MINUTEN);
      strcpy(s_ref.phasen, wert);
    }
  }
  fclose(f);
  cl_assert(s_ref.saetze > 0 && s_ref.beginn_utc && s_ref.phasen[0]);
}

// Der Satz wie b26 ihn schreibt (M: nacht.c prv_schicke), Teil 0. Akku und
// Messstufe nach dem Modell der Referenz: das Protokoll alle 15 Minuten ab
// 20:00, gelesen beim Einschlafen; das Ende zur Minute des Sendens.
static void prv_satz(const SchlafErgebnis *e, int um, NachtDlsRecord *s) {
  memset(s, 0, sizeof(*s));
  const int takt = (e->einschlafen + 14) / 15;
  *s = (NachtDlsRecord){
    .version = NACHT_DLS_VERSION,
    .size = sizeof(NachtDlsRecord),
    .beginn_utc = s_ref.beginn_utc,
    .utc_to_local = 2 * 60 * 60,
    .einschlafen = e->einschlafen,
    .aufwachen = e->aufwachen,
    .schlaf_minuten = e->schlaf_minuten,
    .ruhepuls = e->ruhepuls,
    .teil = 0,
    .teile = (e->abschnitt_anzahl + NACHT_DLS_ABSCHNITTE - 1) / NACHT_DLS_ABSCHNITTE,
    .hrv = e->hrv,
    .akku_beginn = 80 - (takt * 15) / 60,
    .akku_ende = 80 - um / 60,
    .flags = 0,
    .messstufe = s_ref.stufe,
  };
  for (int k = 0; k < e->abschnitt_anzahl && s->anzahl < NACHT_DLS_ABSCHNITTE; k++) {
    s->abschnitte[s->anzahl++] = (NachtDlsAbschnitt){
      .von = e->abschnitte[k].von,
      .bis = e->abschnitte[k].bis,
      .phase = e->abschnitte[k].phase,
    };
  }
}

// Die Phase je Minute: aus einem Satz (wie die Referenz) oder aus dem Nachtbuch
// (ausserhalb mitten in der Nacht ist wach, wie in der Endfassung).
static void prv_phasen_satz(const NachtDlsRecord *s, char *p) {
  memset(p, '.', MINUTEN);
  p[MINUTEN] = 0;
  for (int a = 0; a < s->anzahl; a++) {
    for (int m = s->abschnitte[a].von; m < s->abschnitte[a].bis && m < MINUTEN; m++) {
      p[m] = '0' + s->abschnitte[a].phase;
    }
  }
}

static void prv_phasen_buch(char *p) {
  int ein = -1, auf = -1;
  for (int m = 0; m < MINUTEN; m++) {
    const uint8_t b = boulder_nachlauf_phase(m);
    p[m] = (b == NACHLAUF_AUSSERHALB) ? '.' : '0' + b;
    if (b != NACHLAUF_AUSSERHALB) {
      ein = (ein < 0) ? m : ein;
      auf = m + 1;
    }
  }
  p[MINUTEN] = 0;
  for (int m = ein; m >= 0 && m < auf; m++) {
    p[m] = (p[m] == '.') ? '0' : p[m];
  }
}

// Wie viele Minuten anders; jede einzeln ausgegeben.
static int prv_anders(const char *name, const char *art, const char *ist, const char *soll) {
  int anders = 0;
  for (int m = 0; m < MINUTEN; m++) {
    if (ist[m] != soll[m]) {
      printf("\n  %s %s: %02d:%02d (Minute %d) %c statt %c", name, art, (20 + m / 60) % 24,
             m % 60, m, ist[m], soll[m]);
      anders++;
    }
  }
  return anders;
}

static bool prv_gleich(const char *name, const char *art, int um, const NachtDlsRecord *ist,
                       const NachtDlsRecord *soll) {
  if (!memcmp(ist, soll, sizeof(*ist))) {
    return true;
  }
  printf("\n%s %s um %d ANDERS: ein %d/%d auf %d/%d schlaf %d/%d ruhepuls %d/%d hrv %d/%d "
         "abschnitte %d/%d akku %d/%d %d/%d", name, art, um, ist->einschlafen, soll->einschlafen,
         ist->aufwachen, soll->aufwachen, ist->schlaf_minuten, soll->schlaf_minuten,
         ist->ruhepuls, soll->ruhepuls, ist->hrv, soll->hrv, ist->anzahl, soll->anzahl,
         ist->akku_beginn, soll->akku_beginn, ist->akku_ende, soll->akku_ende);
  return false;
}

// Die Ganznacht mit der kopierten b26-Rechnung: [0, um), wie nacht.c sie liest.
static SchlafArbeit s_arbeit;
static SchlafErgebnis s_ganz;
static const SchlafErgebnis *prv_ganznacht(int um) {
  int n = 0;
  for (int i = 0; i < um; i++) {
    n = s_ref.gueltig[i] ? i + 1 : n;
  }
  SchlafHrv hrv[SCHLAF_HRV_MAX];
  int h = 0;
  for (int k = 0; k < s_ref.hrv_anzahl; k++) {
    if (s_ref.hrv[k].minute + 2 < um) {
      hrv[h++] = s_ref.hrv[k];
    }
  }
  const SchlafEingabe e = {
    .bewegung = s_ref.vmc, .puls = s_ref.puls, .gueltig = s_ref.gueltig, .anzahl = n,
    .schwelle = SCHLAF_SCHWELLE_VMC, .hrv = hrv, .hrv_anzahl = h,
  };
  schlafanalyse_werte(&e, &s_arbeit, &s_ganz);
  return &s_ganz;
}

typedef struct {
  int saetze_gleich;      //< Endfassung = b26, Byte fuer Byte
  int ganz_gleich;        //< die kopierte Rechnung allein (Ganznacht im Test) = b26
  int nachlauf_anders;    //< Minuten, die der Nachlauf vor der Endfassung anders hatte
  int buch_anders;        //< Minuten im Nachtbuch nach der Endfassung anders als im Satz
  int phasen_anders;      //< Minuten des letzten Satzes anders als die Referenzphasen
  double max_lauf_us;
  double max_ende_us;
} Ergebnis;

static double prv_us(struct timeval *t0) {
  struct timeval t1;
  gettimeofday(&t1, NULL);
  return 1e6 * (double)(t1.tv_sec - t0->tv_sec) + (double)(t1.tv_usec - t0->tv_usec);
}

// Eine Nacht durch V2. `extra`: zu jeder Minute eines b26-Satzes zusaetzlich
// rechnen (die Endfassung zur selben Minute wie b26); sonst nur im Takt, und die
// Endfassung zum ersten Takt danach (wann V2 die Nacht selbst schicken wuerde).
// Ohne `endfassung` nur der Nachlauf: das Nachtbuch zu jeder Satzminute gegen b26
// (die Endfassung schriebe es zurueck und verdeckte seine Fehler).
static Ergebnis prv_nacht(const char *name, bool extra, bool endfassung) {
  Ergebnis r = { 0 };
  boulder_eingang_neu();
  boulder_nachlauf_neu(0, 0);
  int satz = 0;
  int hrv = 0;
  for (int t = 1; t <= MINUTEN && satz < s_ref.saetze; t++) {
    const int m = t - 1;
    boulder_eingang_minute(m, s_ref.vmc[m], s_ref.puls[m], s_ref.gueltig[m]);
    // Die Referenz gibt ein HRV-Fenster, sobald seine Mitte vorbei ist.
    while (hrv < s_ref.hrv_anzahl && s_ref.hrv[hrv].minute + 2 < t) {
      cl_assert(boulder_eingang_hrv(s_ref.hrv[hrv].minute, s_ref.hrv[hrv].rmssd,
                                    s_ref.hrv[hrv].puls));
      hrv++;
    }
    const bool takt = (t % NACHLAUF_TAKT == 2) && t >= 60;
    const int um = s_ref.satz_um[satz];
    const bool faellig = extra ? (t == um) : (takt && t >= um);
    struct timeval t0;
    if (takt || faellig) {
      gettimeofday(&t0, NULL);
      boulder_nachlauf_rechnen(t);
      const double us = prv_us(&t0);
      r.max_lauf_us = (us > r.max_lauf_us) ? us : r.max_lauf_us;
    }
    if (!faellig) {
      continue;
    }
    const NachtDlsRecord *soll = &s_ref.satz[satz];
    char p_soll[MINUTEN + 1], p_ist[MINUTEN + 1];
    prv_phasen_satz(soll, p_soll);
    if (!endfassung) {
      prv_phasen_buch(p_ist);
      r.nachlauf_anders += prv_anders(name, "Nachlauf", p_ist, p_soll);
      satz++;
      continue;
    }
    gettimeofday(&t0, NULL);
    const SchlafErgebnis *e = boulder_nachlauf_endfassung();
    const double us = prv_us(&t0);
    r.max_ende_us = (us > r.max_ende_us) ? us : r.max_ende_us;
    NachtDlsRecord ist;
    prv_satz(e, t, &ist);
    NachtDlsRecord soll_jetzt = *soll;
    soll_jetzt.akku_ende = 80 - t / 60;
    r.saetze_gleich += prv_gleich(name, extra ? "Endfassung" : "Takt", t, &ist, &soll_jetzt);
    prv_phasen_buch(p_ist);
    r.buch_anders += prv_anders(name, "Nachtbuch", p_ist, p_soll);
    if (extra) {
      NachtDlsRecord ganz;
      prv_satz(prv_ganznacht(t), t, &ganz);
      r.ganz_gleich += prv_gleich(name, "Ganznacht", t, &ganz, soll);
    }
    if (satz == s_ref.saetze - 1) {
      // Der letzte Satz traegt die Phasen der Referenz.
      prv_phasen_satz(&ist, p_ist);
      r.phasen_anders = prv_anders(name, "Satz", p_ist, s_ref.phasen);
    }
    satz++;
  }
  cl_assert_equal_i(satz, s_ref.saetze);
  return r;
}

// `nachlauf_anders`: so viele Minuten hatte der Nachlauf vor der Endfassung
// anders als b26 (nur in HRV-Naechten erlaubt, siehe nachlauf.c).
static void prv_pruefe(const char *name, int nachlauf_anders) {
  prv_lade(name);
  const Ergebnis a = prv_nacht(name, true, true);
  const Ergebnis b = prv_nacht(name, false, true);
  const Ergebnis c = prv_nacht(name, true, false);
  printf("\n%s: Saetze %d gleich %d (Ganznacht-Kopie %d, im Takt %d), Satz-Minuten anders %d, "
         "Nachtbuch anders %d, Nachlauf allein anders %d; Lauf max %.0f us, Endfassung max %.0f us",
         name, s_ref.saetze, a.saetze_gleich, a.ganz_gleich, b.saetze_gleich, a.phasen_anders,
         a.buch_anders + b.buch_anders, c.nachlauf_anders,
         (a.max_lauf_us > b.max_lauf_us) ? a.max_lauf_us : b.max_lauf_us,
         (a.max_ende_us > b.max_ende_us) ? a.max_ende_us : b.max_ende_us);
  cl_assert_equal_i(a.ganz_gleich, s_ref.saetze);
  cl_assert_equal_i(a.saetze_gleich, s_ref.saetze);
  cl_assert_equal_i(b.saetze_gleich, s_ref.saetze);
  cl_assert_equal_i(a.phasen_anders, 0);
  cl_assert_equal_i(a.buch_anders + b.buch_anders, 0);
  cl_assert_equal_i(c.nachlauf_anders, nachlauf_anders);
}

void test_boulder_schlafkern__initialize(void) {
}

void test_boulder_schlafkern__nacht_2026_09_26_mit_hrv(void) {
  prv_pruefe("nacht-2026-09-26", 0);
}

void test_boulder_schlafkern__nacht_2026_09_29(void) {
  prv_pruefe("nacht-2026-09-29", 0);
}

void test_boulder_schlafkern__nacht_2026_09_30(void) {
  prv_pruefe("nacht-2026-09-30", 0);
}

void test_boulder_schlafkern__nacht_2026_10_03(void) {
  prv_pruefe("nacht-2026-10-03", 0);
}

// Der Nachlauf allein hat 22:14-22:19 Tief statt Leicht: die HRV-Regel kennt beim
// Festschreiben die spaeteren Fenster nicht (nachlauf.c, Endfassung).
void test_boulder_schlafkern__nacht_2026_10_05(void) {
  prv_pruefe("nacht-2026-10-05", 6);
}

void test_boulder_schlafkern__nacht_2026_10_05_morgen(void) {
  prv_pruefe("nacht-2026-10-05-morgen", 6);
}

// Nur der Nachlauf, gegen die Ganznacht mit derselben Rechnung (T3.1) - zu
// jeder Minute, zu der b26 die Nacht schicken wuerde (fertig: 15 Minuten wach
// nach dem Aufwachen, Aufwachen oder Schlaf neu). Rueckgabe: Minuten anders.
static int prv_nachlauf_gegen_ganznacht(const char *name, int *vergleiche) {
  boulder_eingang_neu();
  boulder_nachlauf_neu(0, 0);
  int hrv = 0;
  int auf = -1, schlaf = -1;
  int anders = 0;
  *vergleiche = 0;
  for (int t = 1; t <= MINUTEN; t++) {
    const int m = t - 1;
    boulder_eingang_minute(m, s_ref.vmc[m], s_ref.puls[m], s_ref.gueltig[m]);
    while (hrv < s_ref.hrv_anzahl && s_ref.hrv[hrv].minute + 2 < t) {
      boulder_eingang_hrv(s_ref.hrv[hrv].minute, s_ref.hrv[hrv].rmssd, s_ref.hrv[hrv].puls);
      hrv++;
    }
    if (t % NACHLAUF_TAKT != 2 || t < 60) {
      continue;
    }
    boulder_nachlauf_rechnen(t);
    const SchlafErgebnis *g = prv_ganznacht(t);
    int n = 0;
    for (int i = 0; i < t; i++) {
      n = s_ref.gueltig[i] ? i + 1 : n;
    }
    if (!g->gefunden || n - g->aufwachen < 15 ||
        (g->aufwachen == auf && g->schlaf_minuten == schlaf)) {
      continue;
    }
    auf = g->aufwachen;
    schlaf = g->schlaf_minuten;
    NachtDlsRecord ganz;
    prv_satz(g, t, &ganz);
    char p_soll[MINUTEN + 1], p_ist[MINUTEN + 1];
    prv_phasen_satz(&ganz, p_soll);
    prv_phasen_buch(p_ist);
    anders += prv_anders(name, "Nachlauf", p_ist, p_soll);
    (*vergleiche)++;
  }
  return anders;
}

// LANGES WACHLIEGEN MITTEN IN DER NACHT (Spezifikation 4.3, strom P2/P7): in die
// Nacht 29.9. ab 00:30 alle acht Minuten eine Regung mit Puls 72, 60 bis 150
// Minuten lang (gestellt). Der Nachlauf muss die Nacht trotzdem Minute fuer
// Minute wie die Ganznacht haben, wann immer b26 sie schicken wuerde (mit einem
// Fenster von 300 Minuten bei 150 Minuten nicht mehr, mit 240 ab 90).
void test_boulder_schlafkern__wachliegen_60_bis_150_min(void) {
  const int laengen[] = { 0, 60, 90, 120, 150 };
  for (size_t k = 0; k < sizeof(laengen) / sizeof(laengen[0]); k++) {
    prv_lade("nacht-2026-09-29");
    for (int i = 270; i < 270 + laengen[k]; i += 8) {
      s_ref.vmc[i] = 3000;
      s_ref.puls[i] = 72;
    }
    char name[64];
    snprintf(name, sizeof(name), "29.9. + %d min wach", laengen[k]);
    int vergleiche;
    const int anders = prv_nachlauf_gegen_ganznacht(name, &vergleiche);
    printf("\n%s: %d Vergleiche, Nachlauf allein anders %d", name, vergleiche, anders);
    cl_assert(vergleiche > 0);
    cl_assert_equal_i(anders, 0);
  }
}

// Der Eingang: der erste Satz einer Minute gilt, die Uhr zurueck aendert nichts,
// eine fehlende Minute bleibt ungueltig, VMC 0xFFFF ist keine Minute.
void test_boulder_schlafkern__eingang_doppelt_und_luecken(void) {
  boulder_eingang_neu();
  cl_assert(boulder_eingang_minute(10, 100, 60, true));
  cl_assert(!boulder_eingang_minute(10, 999, 99, true));
  cl_assert(!boulder_eingang_minute(9, 999, 99, true));
  cl_assert(boulder_eingang_minute(12, 0xFFFF, 60, true));
  cl_assert(boulder_eingang_minute(13, 50, 0xFF, true));
  cl_assert(!boulder_eingang_minute(MINUTEN, 50, 60, true));
  const BoulderEingang *e = boulder_eingang();
  cl_assert(e->gueltig[10] && e->bewegung[10] == 100 && e->puls[10] == 60);
  cl_assert(!e->gueltig[11] && !e->gueltig[12]);
  cl_assert(e->gueltig[13] && e->puls[13] == 0);
  cl_assert_equal_i(e->ende, 14);
  cl_assert(boulder_eingang_hrv(20, 40, 60));
  cl_assert(!boulder_eingang_hrv(20, 41, 60));
  cl_assert_equal_i(e->hrv_anzahl, 1);
}
