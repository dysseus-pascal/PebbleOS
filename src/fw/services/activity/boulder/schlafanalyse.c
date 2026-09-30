/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/activity/boulder/schlafanalyse.h"

#include <string.h>

// Wach oder nicht kommt aus Kiesel-Helper (Schlafanalyse.kt); die Phasen sind
// an der Nacht 29./30.9.2026 neben einem Fitbit eingestellt (der Host-Test
// rechnet sie nach). Der Puls allein trug die Phasen nicht: relativ zu seiner
// Umgebung traf er Fitbits Tief- und REM-Minuten nur zufaellig. Was traf, war
// der Koerper: im Tiefschlaf liegt er lange ganz still, im REM zuckt er.

// Um so viel ueber dem Puls der ruhigen Minuten gilt man als wach, auch ohne
// Bewegung: wer nachts gruebelt, liegt still, aber sein Herz nicht.
#define PULS_WACH_UEBER 20
// Kuerzer ist kein Einschlafen: ein Nickerchen vor dem Fernseher zaehlt nicht.
#define MIN_SCHLAFLAUF 10
// Ein Schlaflauf unter zwanzig Minuten, dem eine Viertelstunde Wachsein folgt,
// ist Doesen vor dem Einschlafen - die Nacht beginnt danach.
#define DOESEN_MAX 20
#define DOESEN_WACH 15
// So weit um die Minute herum zaehlt die Unruhe, in Minuten nach jeder Seite.
#define UNRUHE_FENSTER 7
// Ab so vielen unruhigen Minuten im Fenster ist es REM: der Koerper zuckt,
// ohne aufzuwachen. Fruehestens eine Stunde nach dem Einschlafen.
#define REM_UNRUHE 4
#define REM_LATENZ 60
// Tiefschlaf: keine Regung im Fenster, der Puls hoechstens zwei Schlaege ueber
// seiner Umgebung (mal zwei) und ueber fuenf Minuten um hoechstens drei
// gespreizt - ein ruhiges, gleichmaessiges Herz.
#define TIEF_PULS_UEBER2 4
#define TIEF_SPANNE 3
// Weniger Schlaf als das ist keine Nacht - eher eine Uhr auf dem Tisch.
#define MIN_NACHT 60
// Kuerzere Phasen gehen im Nachbarn auf: fuenf Minuten REM sind Rauschen.
#define MIN_ABSCHNITT 5
// So weit wird der Puls mit seiner Umgebung verglichen, in Minuten.
#define UMGEBUNG 60
// Ein HRV-Fenster, das weiter weg liegt, sagt ueber die Minute nichts.
#define HRV_REICHWEITE 15

// Cole-Kripke: die Gewichte fuer die Minuten i-4 .. i+2.
#define VOR 4
static const uint16_t s_gewichte[7] = { 106, 54, 58, 76, 230, 74, 67 };

#define KEIN INT16_MIN

// ---------------------------------------------------------------------------
// Kleinzeug

// Einfuegesortierung: die Felder sind klein (hoechstens eine Nacht), und die
// Firmware soll dafuer keine Bibliothek brauchen.
static void prv_sortiere(int16_t *a, int n) {
  for (int i = 1; i < n; i++) {
    const int16_t x = a[i];
    int j = i - 1;
    while (j >= 0 && a[j] > x) {
      a[j + 1] = a[j];
      j--;
    }
    a[j + 1] = x;
  }
}

// Der Median, mal zwei - so bleibt der halbe Schlag einer geraden Anzahl ganz.
// Sortiert das Feld.
static int32_t prv_median2(int16_t *a, int n) {
  prv_sortiere(a, n);
  const int m = n / 2;
  return (n % 2 == 1) ? 2 * (int32_t)a[m] : (int32_t)a[m - 1] + a[m];
}

// Laeufe gleicher Wach/Schlaf-Werte, in `liste` (phase = wach ja/nein).
static int prv_laeufe(const bool *wach, int n, SchlafAbschnitt *liste) {
  int anzahl = 0;
  int s = 0;
  for (int i = 1; i <= n; i++) {
    if (i == n || wach[i] != wach[s]) {
      liste[anzahl++] = (SchlafAbschnitt){ .von = s, .bis = i, .phase = wach[s] };
      s = i;
    }
  }
  return anzahl;
}

// ---------------------------------------------------------------------------
// Die Schritte der Rechnung

// Cole-Kripke: wach, wenn das gewichtete Mittel der Bewegung die Schwelle
// uebersteigt. AM RAND UND UM LUECKEN NEU GEWICHTET: fehlt eine Nachbarminute,
// zaehlen die vorhandenen mit ihrem Anteil.
static bool prv_wach_nach_bewegung(const SchlafEingabe *e, int i) {
  if (!e->gueltig[i]) {
    return true;
  }
  uint32_t summe = 0;
  uint32_t gewicht = 0;
  for (int k = 0; k < 7; k++) {
    const int j = i + k - VOR;
    if (j < 0 || j >= e->anzahl || !e->gueltig[j]) {
      continue;
    }
    summe += (uint32_t)s_gewichte[k] * e->bewegung[j];
    gewicht += s_gewichte[k];
  }
  // Ohne einen einzigen gueltigen Nachbarn: wach, wie in Kotlin (MAX_VALUE).
  return gewicht == 0 || summe > e->schwelle * gewicht;
}

// Die Regeln von Webster, beide auf dem Stand VOR der Regel: sonst loeste jede
// verlaengerte Wachphase die naechste Verlaengerung aus.
static void prv_webster(bool *wach, int n, SchlafAbschnitt *liste) {
  int anzahl = prv_laeufe(wach, n, liste);
  for (int k = 0; k < anzahl; k++) {
    const SchlafAbschnitt *l = &liste[k];
    if (!l->phase) {
      continue;
    }
    const int laenge = l->bis - l->von;
    const int dazu = (laenge >= 15) ? 4 : (laenge >= 10) ? 3 : (laenge >= 4) ? 1 : 0;
    for (int j = l->bis; j < l->bis + dazu && j < n; j++) {
      wach[j] = true;
    }
  }
  anzahl = prv_laeufe(wach, n, liste);
  for (int k = 1; k < anzahl - 1; k++) {
    const SchlafAbschnitt *l = &liste[k];
    if (!l->phase && (l->bis - l->von) <= 6 &&
        (liste[k - 1].bis - liste[k - 1].von) >= 10 &&
        (liste[k + 1].bis - liste[k + 1].von) >= 10) {
      for (int j = l->von; j < l->bis; j++) {
        wach[j] = true;
      }
    }
  }
}

static bool prv_ruhig(const bool *wach, int ein, int auf, int j) {
  return j >= ein && j < auf && !wach[j];
}

static int prv_mitte(const SchlafHrv *f) {
  return f->minute + 2;
}

// Eine nahe HRV-Messung klar unter dem Median spricht gegen Tiefschlaf: dort
// ueberwiegt der Parasympathikus. Nur klar darunter (drei Viertel): ein
// Fenster alle 30 Minuten sagt ueber eine einzelne Minute wenig, und die
// Haelfte liegt immer unter dem Median.
static bool prv_hrv_passt(const SchlafEingabe *e, int ein, int auf, int32_t median2, int i) {
  if (median2 == KEIN) {
    return true;
  }
  const SchlafHrv *naechstes = NULL;
  int abstand = 0;
  for (int k = 0; k < e->hrv_anzahl; k++) {
    const SchlafHrv *f = &e->hrv[k];
    const int m = prv_mitte(f);
    if (f->rmssd == 0 || m < ein || m >= auf) {
      continue;
    }
    const int a = (m > i) ? m - i : i - m;
    if (!naechstes || a < abstand) {
      naechstes = f;
      abstand = a;
    }
  }
  if (!naechstes || abstand > HRV_REICHWEITE) {
    return true;
  }
  return 8 * (int32_t)naechstes->rmssd >= 3 * median2;
}

// Der Median der HRV-Fenster im Schlaf, mal zwei; KEIN ohne Messung.
static int32_t prv_hrv_median2(const SchlafEingabe *e, int ein, int auf, int16_t *puffer) {
  int n = 0;
  for (int k = 0; k < e->hrv_anzahl; k++) {
    const int m = prv_mitte(&e->hrv[k]);
    if (e->hrv[k].rmssd > 0 && m >= ein && m < auf) {
      puffer[n++] = (int16_t)e->hrv[k].rmssd;
    }
  }
  return n ? prv_median2(puffer, n) : KEIN;
}

// Wie viele Minuten um `i` herum sich der Koerper geregt hat, auch ohne
// aufzuwachen. Bewegung unter der Wachschwelle ist nicht nichts: sie fehlt im
// Tiefschlaf und haeuft sich im REM.
static int prv_unruhe(const SchlafEingabe *e, int ein, int auf, int i) {
  int n = 0;
  for (int j = i - UNRUHE_FENSTER; j <= i + UNRUHE_FENSTER; j++) {
    if (j >= ein && j < auf && e->gueltig[j] && e->bewegung[j] > 0) {
      n++;
    }
  }
  return n;
}

// Die Spreizung des Pulses ueber die fuenf Minuten um `i`; -1 ohne Puls.
static int prv_spanne(const SchlafEingabe *e, int i) {
  int hoch = -1;
  int tief = -1;
  for (int j = (i - 2 > 0 ? i - 2 : 0); j <= i + 2 && j < e->anzahl; j++) {
    const int p = e->puls[j];
    if (p > 0) {
      hoch = (hoch < 0 || p > hoch) ? p : hoch;
      tief = (tief < 0 || p < tief) ? p : tief;
    }
  }
  return hoch < 0 ? -1 : hoch - tief;
}

// Die Phasen Minute fuer Minute. Der Puls wird relativ zu seiner Umgebung
// (eine Stunde davor und danach) gesehen: er faellt ueber die Nacht, und gegen
// die ganze Nacht gemessen laege der Tiefschlaf nur am Morgen.
static void prv_phasen_mit_puls(const SchlafEingabe *e, SchlafArbeit *w, int ein, int auf) {
  const int n = e->anzahl;
  int16_t fenster[2 * UMGEBUNG + 1];

  for (int i = ein; i < auf; i++) {
    // Geglaettet ueber fuenf Minuten: ein Ausreisser macht keinen Tiefschlaf.
    int k = 0;
    for (int j = (i - 2 > 0 ? i - 2 : 0); j <= i + 2 && j < n; j++) {
      if (e->puls[j] > 0) {
        fenster[k++] = e->puls[j];
      }
    }
    w->glatt2[i] = k ? (int16_t)prv_median2(fenster, k) : KEIN;
  }

  for (int i = ein; i < auf; i++) {
    w->relativ2[i] = KEIN;
    if (w->glatt2[i] == KEIN) {
      continue;
    }
    int k = 0;
    const int von = (i - UMGEBUNG > ein) ? i - UMGEBUNG : ein;
    const int bis = (i + UMGEBUNG < auf - 1) ? i + UMGEBUNG : auf - 1;
    for (int j = von; j <= bis; j++) {
      if (!w->wach[j] && e->puls[j] > 0) {
        fenster[k++] = e->puls[j];
      }
    }
    if (k >= 5) {
      w->relativ2[i] = (int16_t)(w->glatt2[i] - prv_median2(fenster, k));
    }
  }

  const int32_t hrv_median2 = prv_hrv_median2(e, ein, auf, fenster);

  for (int i = ein; i < auf; i++) {
    const int16_t r = w->relativ2[i];
    SchlafPhase p;
    if (w->wach[i]) {
      p = SchlafPhaseWach;
    } else {
      const int unruhe = prv_unruhe(e, ein, auf, i);
      bool ringsum_ruhig = true;
      for (int j = i - 3; j <= i + 3; j++) {
        ringsum_ruhig = ringsum_ruhig && prv_ruhig(w->wach, ein, auf, j);
      }
      if (unruhe == 0 && ringsum_ruhig && r != KEIN && r <= TIEF_PULS_UEBER2 &&
          prv_spanne(e, i) <= TIEF_SPANNE && prv_hrv_passt(e, ein, auf, hrv_median2, i)) {
        // TIEF: eine Viertelstunde ohne jede Regung, das Herz ruhig und
        // gleichmaessig. Ohne Puls kein Tiefschlaf - nur Stillliegen reicht nicht.
        p = SchlafPhaseTief;
      } else if (i >= ein + REM_LATENZ && unruhe >= REM_UNRUHE) {
        // REM: der Koerper zuckt, ohne dass die Minute wach wuerde.
        p = SchlafPhaseRem;
      } else {
        p = SchlafPhaseLeicht;
      }
    }
    w->phase[i] = p;
  }
}

// Abschnitte unter fuenf Minuten dem laengeren Nachbarn zuschlagen, den
// kuerzesten zuerst - damit ein Flackern nicht einen echten kurzen Abschnitt
// schluckt.
static int prv_glaette(SchlafAbschnitt *l, int n) {
  while (n > 1) {
    int k = -1;
    for (int i = 0; i < n; i++) {
      const int laenge = l[i].bis - l[i].von;
      if (laenge < MIN_ABSCHNITT && (k < 0 || laenge < l[k].bis - l[k].von)) {
        k = i;
      }
    }
    if (k < 0) {
      break;
    }
    int ziel;
    if (k == 0) {
      ziel = 1;
    } else if (k == n - 1) {
      ziel = k - 1;
    } else {
      ziel = ((l[k + 1].bis - l[k + 1].von) > (l[k - 1].bis - l[k - 1].von)) ? k + 1 : k - 1;
    }
    SchlafAbschnitt *z = &l[ziel];
    const SchlafAbschnitt kurz = l[k];
    *z = (SchlafAbschnitt){
      .von = (z->von < kurz.von) ? z->von : kurz.von,
      .bis = (z->bis > kurz.bis) ? z->bis : kurz.bis,
      .phase = z->phase,
    };
    memmove(&l[k], &l[k + 1], (n - k - 1) * sizeof(*l));
    n--;
    // Gleiche Nachbarn wieder zusammenlegen.
    int i = 1;
    while (i < n) {
      if (l[i].phase == l[i - 1].phase) {
        l[i - 1].bis = l[i].bis;
        memmove(&l[i], &l[i + 1], (n - i - 1) * sizeof(*l));
        n--;
      } else {
        i++;
      }
    }
  }
  return n;
}

// Der Ruhepuls: das niedrigste Mittel ueber 30 Minuten Schlaf mit mindestens
// sechs Pulswerten - nicht der tiefste Einzelwert, der ist oft ein Messfehler.
static uint8_t prv_ruhepuls(const SchlafEingabe *e, const bool *wach, int ein, int auf) {
  int32_t beste_summe = 0;
  int32_t beste_zahl = 0;
  for (int s = ein; s <= auf - 30; s++) {
    int32_t summe = 0;
    int32_t zahl = 0;
    for (int j = s; j < s + 30; j++) {
      if (!wach[j] && e->puls[j] > 0) {
        summe += e->puls[j];
        zahl++;
      }
    }
    if (zahl < 6) {
      continue;
    }
    if (beste_zahl == 0 || summe * beste_zahl < beste_summe * zahl) {
      beste_summe = summe;
      beste_zahl = zahl;
    }
  }
  // Aufgerundet ab einem halben Schlag, wie roundToInt.
  return beste_zahl ? (uint8_t)((2 * beste_summe + beste_zahl) / (2 * beste_zahl)) : 0;
}

// ---------------------------------------------------------------------------

bool schlafanalyse_werte(const SchlafEingabe *e, SchlafArbeit *w, SchlafErgebnis *ergebnis) {
  memset(ergebnis, 0, sizeof(*ergebnis));
  const int n = e->anzahl;
  if (n == 0 || n > SCHLAF_MINUTEN_MAX) {
    return false;
  }

  for (int i = 0; i < n; i++) {
    w->wach[i] = prv_wach_nach_bewegung(e, i);
  }

  // STILL LIEGEN UND WACH SEIN GEHT: ein Puls deutlich ueber dem der ruhigen
  // Minuten ist wach, auch ohne Bewegung.
  int m = 0;
  for (int i = 0; i < n; i++) {
    if (!w->wach[i] && e->puls[i] > 0) {
      w->sortiert2[m++] = e->puls[i];
    }
  }
  if (m > 0) {
    const int32_t median2 = prv_median2(w->sortiert2, m);
    for (int i = 0; i < n; i++) {
      if (e->puls[i] > 0 && 2 * (int32_t)e->puls[i] > median2 + 2 * PULS_WACH_UEBER) {
        w->wach[i] = true;
      }
    }
  }

  prv_webster(w->wach, n, w->liste);

  // Einschlafen und Aufwachen: der erste und der letzte Schlaf, der lang
  // genug ist, um einer zu sein. DOESEN ZAEHLT NICHT: ein kurzer Schlaf, auf
  // den langes Wachsein folgt, ist das Wegnicken vor dem Einschlafen.
  const int laeufe = prv_laeufe(w->wach, n, w->liste);
  int ein = -1;
  int auf = -1;
  for (int k = 0; k < laeufe; k++) {
    const SchlafAbschnitt *l = &w->liste[k];
    if (l->phase || (l->bis - l->von) < MIN_SCHLAFLAUF) {
      continue;
    }
    if (ein < 0) {
      const bool doesen = (l->bis - l->von) < DOESEN_MAX && k + 1 < laeufe &&
          (w->liste[k + 1].bis - w->liste[k + 1].von) >= DOESEN_WACH;
      if (doesen) {
        continue;
      }
      ein = l->von;
    }
    auf = l->bis;
  }
  if (ein < 0) {
    return false;
  }
  int schlaf = 0;
  bool hat_puls = false;
  for (int i = ein; i < auf; i++) {
    if (!w->wach[i]) {
      schlaf++;
      hat_puls = hat_puls || e->puls[i] > 0;
    }
  }
  if (schlaf < MIN_NACHT) {
    return false;
  }

  if (hat_puls) {
    prv_phasen_mit_puls(e, w, ein, auf);
  } else {
    for (int i = ein; i < auf; i++) {
      w->phase[i] = w->wach[i] ? SchlafPhaseWach : SchlafPhaseSchlaf;
    }
  }

  // Die Minuten zu Abschnitten, dann glaetten.
  int anzahl = 0;
  int s = ein;
  for (int i = ein + 1; i <= auf; i++) {
    if (i == auf || w->phase[i] != w->phase[s]) {
      w->liste[anzahl++] = (SchlafAbschnitt){ .von = s, .bis = i, .phase = w->phase[s] };
      s = i;
    }
  }
  anzahl = prv_glaette(w->liste, anzahl);
  if (anzahl > SCHLAF_ABSCHNITTE_MAX) {
    anzahl = SCHLAF_ABSCHNITTE_MAX;
  }

  ergebnis->gefunden = true;
  ergebnis->einschlafen = ein;
  ergebnis->aufwachen = auf;
  ergebnis->abschnitt_anzahl = anzahl;
  memcpy(ergebnis->abschnitte, w->liste, anzahl * sizeof(SchlafAbschnitt));
  for (int k = 0; k < anzahl; k++) {
    if (w->liste[k].phase != SchlafPhaseWach) {
      ergebnis->schlaf_minuten += w->liste[k].bis - w->liste[k].von;
    }
  }
  ergebnis->ruhepuls = prv_ruhepuls(e, w->wach, ein, auf);
  const int32_t hrv2 = prv_hrv_median2(e, ein, auf, w->sortiert2);
  ergebnis->hrv = (hrv2 == KEIN) ? 0 : (uint16_t)((hrv2 + 1) / 2);
  return true;
}
