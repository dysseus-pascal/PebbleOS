/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/activity/boulder/schlafanalyse.h"

#include <string.h>

// Wach oder nicht kommt aus Kiesel-Helper (Schlafanalyse.kt); die Phasen sind
// an den Naechten 29./30.9., 30.9./1.10. und 2./3.10.2026 neben einem Fitbit
// eingestellt, das Einschlafen zuletzt an der Nacht 5./6.10., das Aufstehen an
// ihrem zweiten Export bis 07:24 (der Host-Test rechnet alle Naechte nach).
// Der Puls relativ zu seiner Umgebung traf Fitbits
// Tiefschlaf nur zufaellig. Was traf, war der Koerper: im Tiefschlaf liegt er
// lange ganz still. Beim REM traf das Herz: es schlaegt unregelmaessig,
// waehrend der Koerper ruht.

// Um so viel ueber dem Puls der ruhigen Minuten gilt man als wach, auch ohne
// Bewegung: wer nachts gruebelt, liegt still, aber sein Herz nicht.
#define PULS_WACH_UEBER 20
// Kuerzer ist kein Einschlafen: ein Nickerchen vor dem Fernseher zaehlt nicht.
#define MIN_SCHLAFLAUF 10
// Eingeschlafen ist, wessen Schlaf haelt: von der Stunde ab dem ersten Schlaf
// muessen drei Viertel Schlaf sein. Sonst war es Doesen vor dem Fernseher, und
// die Nacht beginnt spaeter (Nacht 30.9.: zwei Stunden Wegnicken und Aufstehen).
#define HALTEN_FENSTER 60
#define HALTEN_SCHLAF 45
// Eine Regung: fuenf Sechzehntel der Wachschwelle in einer Minute (250 beim
// VMC der Uhr). Darunter zuckt nur ein Finger. MITTEN IM BEREICH, nicht an
// seinem Rand: von 150 bis 300 aendert sich keine der vier Naechte bis 3.10. um
// mehr als zwei Minuten, ab 350 zerfallen die Wachphasen der 3.10. Die Nacht
// 5.10. kam spaeter dazu: bei 150 und 200 hat sie einen Aufwach-Schub mehr
// (15 Minuten wach, Fitbit sah keine), ab 250 nicht.
#define REGUNG_SECHZEHNTEL 5
// EINGESCHLAFEN IST, WER ZUR RUHE KOMMT: zwei Waelz-Regungen in den vierzig
// Minuten ab dem Einschlafen sind noch das Herumwaelzen davor (Nacht 3.10.:
// 22:11, 22:19, 22:25 - Fitbit sah den Schlaf erst um 22:29).
// VIERZIG MINUTEN, NICHT MEHR ZWANZIG (b26): am 5.10. lag der Nutzer ab 21:46
// still, waelzte sich aber noch um 21:54, 22:10 und 22:13 - Fitbit sah den
// Schlaf erst um 22:15. In zwanzig Minuten ab 21:46 liegt davon nur eine
// Regung, und die Nacht begann 29 Minuten zu frueh. Ab 25 Minuten traegt das
// Fenster die Regung um 22:10; von 25 bis 60 rechnen alle vier Naechte
// gleich, 40 liegt mitten darin. EINGESTELLT ALLEIN AM 5.10. - unabhaengig
// prueft es erst die naechste Fitbit-Nacht.
#define EINSCHLAF_RUHE 40
#define EINSCHLAF_REGUNGEN 2
// ALS WAELZEN ZAEHLT NUR EINE KRAEFTIGE REGUNG: neun Sechzehntel der
// Wachschwelle (450 beim VMC der Uhr), nicht fuenf wie eine Regung sonst.
// Schwache Regungen kommen auch im beginnenden Schlaf: am 30.9. (Fitbit 21:58)
// schoben die um 22:03 (VMC 402) und 22:26 (256) den Beginn mit fuenf
// Sechzehnteln auf 22:04, mit dem langen Fenster sogar auf 22:12; mit neun
// beginnt er um 22:00. Die 9/16 standen schon vor der Nacht 5.10. als Vorschlag
// im Test; der 5.10. haelt sie, ist dazu aber kein Beleg: von 6/16 bis 12/16
// bleiben alle vier Naechte innerhalb von zehn Minuten um Fitbit, ab 13/16
// zaehlen die Regungen vom 5.10. um 22:10 und 22:13 nicht mehr (21:46).
// Den Aufwach-Schueben bleibt die gewoehnliche Regung: dort zaehlt jedes
// Regen eines Wachliegenden (REGUNG_SECHZEHNTEL).
#define EINSCHLAF_REGUNG_SECHZEHNTEL 9
// Hoechstens so weit schiebt das Herumwaelzen das Einschlafen. Zwei Waelz-
// Regungen in vierzig Minuten kommen auch in festem Schlaf vor: ab einer
// Stunde nach dem Einschlafen in drei Vierteln bis neun Zehnteln der Fenster
// (mit zwanzig Minuten und 5/16 bis b25: zwei Fuenftel bis knapp zwei
// Drittel). Ohne Grenze wanderte der Beginn dort um mehr als eine Stunde - die
// Grenze traegt die Regel mit. Die 3.10. brauchte 25 Minuten, die 5.10. 28,
// KNAPP UNTER DER GRENZE. Hoeher setzen hiesse aber, unruhigen Schlaf weiter
// wegzuschieben (Test unruhiger_schlaf_schiebt_das_einschlafen_nicht_weit).
#define EINSCHLAF_SCHIEBEN_MAX 30
// WER AUFGESTANDEN IST, SCHLAEFT NUR NOCH WEITER, WENN ES HAELT (b26). Nach
// dem Aufwachen bleibt die Nacht in nacht.c noch zwei Stunden offen, damit sie
// nach dem Toilettengang weiterwaechst. Am 5.10. wuchs sie so auch nach dem
// Aufstehen: um 06:00 auf, Kaffee gemacht, von 06:26 bis 06:43 still mit ihm
// gesessen, beim Ruhepuls - b25 sah darin Schlaf und das Aufwachen um 06:44.
// Nach dem Gehen verlaengert deshalb erst eine halbe Stunde Schlaf am Stueck
// die Nacht; kuerzere Ruhe (Kaffee, Zeitung, Sofa) nicht. Nach dem
// Toilettengang schlaeft man laenger weiter (30.9.: um 01:45 auf, dann 51
// Minuten am Stueck), die Nacht waechst dort wie bisher.
// GEHEN SIEHT DIE RECHNUNG AM VMC, NICHT AN DEN SCHRITTEN: die Eingabe hat
// keine, nacht.c liest sie nicht aus der Minutendatei. Eine Gehminute traegt
// das Vierfache der Wachschwelle (3200 beim VMC der Uhr). Cole-Kripke macht
// sie in jedem Fall wach (allein reichen knapp drei Schwellen): sie
// unterbricht den Schlaf am Stueck.
// SCHRITTE ALLEIN WAEREN KEIN BELEG: in den vier Fitbit-Naechten zaehlte die
// Uhr im Bett sieben einzelne Minuten mit 8 bis 20 Schritten, drei davon
// mitten im Schlaf bei einem VMC unter 1000 (Arm, Kratzen). Und beim Gehen am Morgen
// trugen nur 2 bis 8 von 10 bis 14 Gehminuten einer Viertelstunde Schritte.
#define GEHEN_FACH 4
// Aufgestanden: sieben Gehminuten in einer Viertelstunde, irgendwo zwischen
// dem Aufwachen und dem naechsten Schlaf (wer erst liegen bleibt und dann
// aufsteht, ist auch aufgestanden). Das Aufstehen am Morgen hatte in allen
// Naechten 10 bis 14 davon, Wachliegen im Bett hoechstens 3 (3.10. um 02:10),
// der Toilettengang am 30.9. sechs. MITTEN IM BEREICH: von 7 bis 10 Minuten
// und vom 3,5- bis 6-Fachen der Schwelle gilt in allen Naechten dasselbe als
// Aufstehen; von 4 bis 6 zaehlte der Toilettengang mit, was dort nichts
// aendert (51 Minuten Schlaf danach).
#define AUFSTEHEN_FENSTER 15
#define AUFSTEHEN_GEHMINUTEN 7
#define SCHLAF_NACH_DEM_AUFSTEHEN 30
// AUFWACHEN MITTEN IN DER NACHT: mindestens drei Regungen, keine mehr als zehn
// Minuten nach der vorigen, und das Herz schiesst dabei um dreizehn Schlaege
// ueber den Schlafpuls ringsum. Dann ist der ganze Schub wach, auch die
// ruhigen Minuten zwischen den Regungen - wer wach liegt, bewegt sich nicht
// jede Minute (Nacht 3.10.: 01:50-02:29, 02:46-03:47, 05:02-05:30). Ohne den
// Pulsstoss sind es die Zuckungen im REM (Nacht 29.9.: 03:00-03:41).
// DIE ZEHN MINUTEN SIND KEIN ZUFALL: wer am 3.10. wach lag, regte sich alle
// sieben bis neun Minuten; unter neun zerfallen diese Schuebe. Von neun bis
// zwoelf Minuten und von zwoelf bis vierzehn Schlaegen rechnen alle vier
// Naechte gleich - beide Werte liegen mitten darin, nicht an einem Rand.
#define AUFWACH_LUECKE 10
#define AUFWACH_REGUNGEN 3
#define AUFWACH_PULS 13
// So weit um die Minute herum zaehlt die Unruhe, in Minuten nach jeder Seite.
#define UNRUHE_FENSTER 7
// REM fruehestens eine Stunde nach dem Einschlafen.
#define REM_LATENZ 60
// REM MIT DICHTEM PULS: die bis zu fuenf naechsten Pulswerte (hoechstens drei
// Minuten weg) liegen fuenf Schlaege oder mehr auseinander. Fuenf Werte, nicht
// alle: sonst saehe eine Messstufe mit Minutenpuls mehr REM als eine mit
// Zwei-Minuten-Puls, nur weil mehr Werte mehr Spreizung haben.
#define REM_NAH 3
#define REM_WERTE 5
#define REM_WERTE_MIN 3
#define REM_SPANNE 5
// OHNE DICHTEN PULS (Messstufe 1 bis 3, alte Naechte) zeigt nur der Koerper
// das REM: ab so vielen unruhigen Minuten im Fenster zuckt er, ohne
// aufzuwachen.
#define REM_UNRUHE 4
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
// Unter so vielen ruhigen HRV-Fenstern gibt es keinen Vergleich.
#define HRV_RUHIG_MIN 3

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

// Eine Minute, in der sich der Koerper wirklich bewegt hat. Die Schwelle steht
// in den Einheiten der Eingabe, deshalb der Anteil an ihr.
static bool prv_regung(const SchlafEingabe *e, int i) {
  return e->gueltig[i] && 16 * (uint32_t)e->bewegung[i] >= REGUNG_SECHZEHNTEL * e->schwelle;
}

// Eine Regung, kraeftig genug fuer das Herumwaelzen vor dem Einschlafen
// (EINSCHLAF_REGUNG_SECHZEHNTEL).
static bool prv_waelzen(const SchlafEingabe *e, int i) {
  return e->gueltig[i] &&
      16 * (uint32_t)e->bewegung[i] >= EINSCHLAF_REGUNG_SECHZEHNTEL * e->schwelle;
}

// Eine Minute mit Bewegung wie beim Gehen (GEHEN_FACH).
static bool prv_gehen(const SchlafEingabe *e, int i) {
  return e->gueltig[i] && e->bewegung[i] >= GEHEN_FACH * e->schwelle;
}

// Ist man zwischen `von` und `bis` gegangen - AUFSTEHEN_GEHMINUTEN in
// AUFSTEHEN_FENSTER Minuten?
static bool prv_aufgestanden(const SchlafEingabe *e, int von, int bis) {
  for (int s = von; s < bis; s++) {
    if (!prv_gehen(e, s)) {
      continue;
    }
    int zahl = 0;
    for (int j = s; j < s + AUFSTEHEN_FENSTER && j < bis; j++) {
      zahl += prv_gehen(e, j) ? 1 : 0;
    }
    if (zahl >= AUFSTEHEN_GEHMINUTEN) {
      return true;
    }
  }
  return false;
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

// Ob der Schlaf ab `von` haelt. Am Ende der Daten zaehlt, was da ist: wer
// kurz vor dem Ende einschlaeft, hat trotzdem geschlafen.
static bool prv_haelt(const bool *wach, int n, int von) {
  const int bis = (von + HALTEN_FENSTER < n) ? von + HALTEN_FENSTER : n;
  int schlaf = 0;
  for (int i = von; i < bis; i++) {
    schlaf += wach[i] ? 0 : 1;
  }
  return HALTEN_FENSTER * schlaf >= HALTEN_SCHLAF * (bis - von);
}

// Das Einschlafen hinter das Herumwaelzen schieben: solange in den
// EINSCHLAF_RUHE Minuten ab `ein` zwei Waelz-Regungen liegen, beginnt der
// Schlaf erst nach der ersten - hoechstens EINSCHLAF_SCHIEBEN_MAX nach `ein`.
// Dann bis zur naechsten Schlafminute.
static int prv_zur_ruhe(const SchlafEingabe *e, const bool *wach, int ein, int auf) {
  int s = ein;
  while (s < auf) {
    int erste = -1;
    int zahl = 0;
    for (int j = s; j < s + EINSCHLAF_RUHE && j < e->anzahl; j++) {
      if (prv_waelzen(e, j)) {
        erste = (erste < 0) ? j : erste;
        zahl++;
      }
    }
    // Weiter weg ist es kein Herumwaelzen mehr, sondern Schlaf mit Zuckungen.
    if (zahl < EINSCHLAF_REGUNGEN || erste + 1 - ein > EINSCHLAF_SCHIEBEN_MAX) {
      break;
    }
    s = erste + 1;
  }
  while (s < auf && wach[s]) {
    s++;
  }
  return s;
}

// Schiesst das Herz waehrend des Schubs [von, bis) ueber den Schlafpuls
// ringsum (eine Stunde davor und danach)? Ohne Puls ringsum: kein Beleg.
// EIN WERT REICHT, auch wenn er ein Messfehler sein koennte: am 3.10. trug um
// 02:21 ein einzelner (82 Schlaege) ein Aufwachen, das Fitbit bestaetigt. Mit
// einem zweiten hohen Wert in der Naehe als Bedingung fiel es weg, und die
// Minuten stimmten seltener (54 statt 58 %).
static bool prv_puls_schiesst(const SchlafEingabe *e, const bool *wach, int ein, int auf,
                              int von, int bis, int16_t *puffer) {
  int k = 0;
  for (int j = (von - UMGEBUNG > ein) ? von - UMGEBUNG : ein; j < von; j++) {
    if (!wach[j] && e->puls[j] > 0) {
      puffer[k++] = e->puls[j];
    }
  }
  for (int j = bis; j < bis + UMGEBUNG && j < auf; j++) {
    if (!wach[j] && e->puls[j] > 0) {
      puffer[k++] = e->puls[j];
    }
  }
  if (k == 0) {
    return false;
  }
  const int32_t grund2 = prv_median2(puffer, k);
  int hoch = 0;
  for (int j = (von - 2 > 0) ? von - 2 : 0; j < bis + 2 && j < e->anzahl; j++) {
    hoch = (e->puls[j] > hoch) ? e->puls[j] : hoch;
  }
  return hoch > 0 && 2 * hoch - grund2 >= 2 * AUFWACH_PULS;
}

// Die Aufwach-Schuebe zwischen Einschlafen und Aufwachen als wach markieren.
// NUR MITTEN IN DER NACHT: ein Schub, der bis ans Aufwachen reicht, ist das
// Aufstehen - dort setzt Cole-Kripke das Ende schon richtig. Erst sammeln,
// dann markieren: der Grundpuls eines Schubs haengt nicht davon ab, ob der
// vorige schon wach ist.
// BEKANNTE GRENZE: der Pulsstoss gilt fuer den ganzen Schub. Ein spaeter
// Stoss macht so auch vereinzelte Zuckungen davor wach (3.10. 04:39-05:03,
// bei Fitbit leicht und REM). Ruhig wach liegen (03:19-03:47) sieht in den
// Minuten aber genauso aus: Regungen alle sieben bis neun Minuten, das Herz
// beim Schlafpuls. Wer den Schub dort trennt, verliert beides.
static void prv_aufwach_schuebe(const SchlafEingabe *e, SchlafArbeit *w, int ein, int auf) {
  int anzahl = 0;
  int von = -1;
  int letzte = -1;
  int zahl = 0;
  for (int i = ein; i <= auf; i++) {
    const bool regung = (i < auf) && prv_regung(e, i);
    if (regung && von >= 0 && i - letzte <= AUFWACH_LUECKE) {
      letzte = i;
      zahl++;
      continue;
    }
    if (!regung && i < auf) {
      continue;
    }
    // Ein Schub ist zu Ende: eine Regung zu weit weg, oder das Aufwachen.
    if (von > ein && zahl >= AUFWACH_REGUNGEN && letzte + 1 + MIN_SCHLAFLAUF <= auf &&
        prv_puls_schiesst(e, w->wach, ein, auf, von, letzte + 1, w->sortiert2)) {
      w->liste[anzahl++] = (SchlafAbschnitt){ .von = von, .bis = letzte + 1 };
    }
    von = i;
    letzte = i;
    zahl = 1;
  }
  for (int k = 0; k < anzahl; k++) {
    for (int j = w->liste[k].von; j < w->liste[k].bis; j++) {
      w->wach[j] = true;
    }
  }
}

static bool prv_ruhig(const bool *wach, int ein, int auf, int j) {
  return j >= ein && j < auf && !wach[j];
}

static int prv_mitte(const SchlafHrv *f) {
  return f->minute + 2;
}

// Ein HRV-Fenster, das ganz im Schlaf lag. Fenster im Wachen tragen die
// Stoerungen der Bewegung und machen die HRV scheinbar hoch.
static bool prv_hrv_ruhig(const SchlafEingabe *e, const bool *wach, int ein, int auf,
                          const SchlafHrv *f) {
  const int m = prv_mitte(f);
  if (f->rmssd == 0 || m < ein || m >= auf) {
    return false;
  }
  for (int j = f->minute; j < f->minute + 5; j++) {
    if (j >= e->anzahl || wach[j]) {
      return false;
    }
  }
  return true;
}

// Der Median der ruhigen HRV-Fenster, mal zwei; KEIN bei zu wenigen.
static int32_t prv_hrv_ruhig_median2(const SchlafEingabe *e, const bool *wach, int ein, int auf,
                                     int16_t *puffer) {
  int n = 0;
  for (int k = 0; k < e->hrv_anzahl; k++) {
    if (prv_hrv_ruhig(e, wach, ein, auf, &e->hrv[k])) {
      puffer[n++] = (int16_t)e->hrv[k].rmssd;
    }
  }
  return (n >= HRV_RUHIG_MIN) ? prv_median2(puffer, n) : KEIN;
}

// NUR EINE HRV WEIT UNTER DEN ANDEREN SPRICHT GEGEN TIEFSCHLAF - unter der
// Haelfte des Medians der ruhigen Fenster. Frueher reichten drei Viertel des
// Medians aller Fenster: die Fenster im Wachliegen trieben den Median hoch, und
// in der Nacht 2./3.10. fiel aller Tiefschlaf weg, obwohl er zu drei Vierteln
// auf Fitbits Tiefschlaf lag. Ohne ruhiges Fenster in Reichweite passt es.
// DAMIT FAENGT DIE HRV NUR NOCH AUSREISSER: in der Nacht 26./27.9. (18 echte
// Fenster, 36 bis 82 ms) aendert sie keine Minute.
static bool prv_hrv_passt(const SchlafEingabe *e, const bool *wach, int ein, int auf,
                          int32_t median2, int i) {
  if (median2 == KEIN) {
    return true;
  }
  const SchlafHrv *naechstes = NULL;
  int abstand = 0;
  for (int k = 0; k < e->hrv_anzahl; k++) {
    const SchlafHrv *f = &e->hrv[k];
    if (!prv_hrv_ruhig(e, wach, ein, auf, f)) {
      continue;
    }
    const int m = prv_mitte(f);
    const int a = (m > i) ? m - i : i - m;
    if (!naechstes || a < abstand) {
      naechstes = f;
      abstand = a;
    }
  }
  if (!naechstes || abstand > HRV_REICHWEITE) {
    return true;
  }
  return 4 * (int32_t)naechstes->rmssd >= median2;
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
// Tiefschlaf.
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

// Wie unregelmaessig das Herz um `i` schlaegt: die Spreizung der bis zu fuenf
// naechsten Pulswerte, von innen nach aussen gesammelt. -1 bei zu wenigen.
static int prv_herz_unruhe(const SchlafEingabe *e, int i) {
  int zahl = 0;
  int hoch = 0;
  int tief = 0;
  for (int d = 0; d <= REM_NAH && zahl < REM_WERTE; d++) {
    for (int s = 0; s < (d ? 2 : 1) && zahl < REM_WERTE; s++) {
      const int j = s ? i + d : i - d;
      if (j < 0 || j >= e->anzahl || e->puls[j] == 0) {
        continue;
      }
      const int p = e->puls[j];
      hoch = (zahl == 0 || p > hoch) ? p : hoch;
      tief = (zahl == 0 || p < tief) ? p : tief;
      zahl++;
    }
  }
  return (zahl >= REM_WERTE_MIN) ? hoch - tief : -1;
}

// Dicht ist der Puls, wenn um mindestens die Haelfte der Minuten drei Werte in
// sieben Minuten liegen (ab Messstufe 4). Nur dann traegt er das REM.
static bool prv_puls_dicht(const SchlafEingabe *e, int ein, int auf) {
  int dicht = 0;
  for (int i = ein; i < auf; i++) {
    int zahl = 0;
    for (int j = (i - REM_NAH > 0) ? i - REM_NAH : 0; j <= i + REM_NAH && j < e->anzahl; j++) {
      zahl += (e->puls[j] > 0) ? 1 : 0;
    }
    dicht += (zahl >= REM_WERTE_MIN) ? 1 : 0;
  }
  return 2 * dicht >= auf - ein;
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

  const int32_t hrv_median2 = prv_hrv_ruhig_median2(e, w->wach, ein, auf, fenster);
  const bool dicht = prv_puls_dicht(e, ein, auf);

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
      const bool rem = (i >= ein + REM_LATENZ) &&
          (dicht ? prv_herz_unruhe(e, i) >= REM_SPANNE : unruhe >= REM_UNRUHE);
      if (unruhe == 0 && ringsum_ruhig && r != KEIN && r <= TIEF_PULS_UEBER2 &&
          prv_spanne(e, i) <= TIEF_SPANNE &&
          prv_hrv_passt(e, w->wach, ein, auf, hrv_median2, i)) {
        // TIEF: eine Viertelstunde ohne jede Regung, das Herz ruhig und
        // gleichmaessig. Ohne Puls kein Tiefschlaf - nur Stillliegen reicht nicht.
        p = SchlafPhaseTief;
      } else if (rem) {
        // REM: das Herz springt, der Koerper bleibt liegen. Die Aufwach-Schuebe
        // sind schon wach und kommen hier nicht mehr an.
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

// Verdichten, bis die Liste passt: der kuerzeste Abschnitt geht im laengeren
// Nachbarn auf, und gleiche Nachbarn werden eins. So bleiben die langen Phasen
// stehen, nur das Flackern verschwindet - und die Liste deckt weiter alles vom
// ersten bis zum letzten Abschnitt ab, ohne Luecke.
int schlafanalyse_verdichten(SchlafAbschnitt *a, int n, int max) {
  while (n > max && n > 1) {
    int k = 0;
    for (int i = 1; i < n; i++) {
      if (a[i].bis - a[i].von < a[k].bis - a[k].von) {
        k = i;
      }
    }
    int ziel;
    if (k == 0) {
      ziel = 1;
    } else if (k == n - 1) {
      ziel = n - 2;
    } else {
      ziel = (a[k - 1].bis - a[k - 1].von >= a[k + 1].bis - a[k + 1].von) ? k - 1 : k + 1;
    }
    if (ziel < k) {
      a[ziel].bis = a[k].bis;
    } else {
      a[ziel].von = a[k].von;
    }
    memmove(&a[k], &a[k + 1], (n - k - 1) * sizeof(*a));
    n--;
    // Stehen jetzt zwei gleiche nebeneinander, werden sie einer. Der
    // gewachsene Nachbar steht nach dem Entfernen bei `ziel` (links) oder
    // bei `k` (rechts); zu pruefen ist er mit seinem anderen Nachbarn.
    const int links = (ziel < k) ? ziel : k - 1;
    if (links >= 0 && links + 1 < n && a[links].phase == a[links + 1].phase) {
      a[links].bis = a[links + 1].bis;
      memmove(&a[links + 1], &a[links + 2], (n - links - 2) * sizeof(*a));
      n--;
    }
  }
  return n;
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
  // genug ist, um einer zu sein. DOESEN ZAEHLT NICHT: ein Schlaf, der nicht
  // haelt, ist das Wegnicken vor dem Einschlafen.
  const int laeufe = prv_laeufe(w->wach, n, w->liste);
  int ein = -1;
  int auf = -1;
  for (int k = 0; k < laeufe; k++) {
    const SchlafAbschnitt *l = &w->liste[k];
    if (l->phase || (l->bis - l->von) < MIN_SCHLAFLAUF) {
      continue;
    }
    if (ein < 0) {
      if (!prv_haelt(w->wach, n, l->von)) {
        continue;
      }
      ein = l->von;
    } else if ((l->bis - l->von) < SCHLAF_NACH_DEM_AUFSTEHEN &&
               prv_aufgestanden(e, auf, l->von)) {
      // RUHE NACH DEM AUFSTEHEN verlaengert die Nacht nicht. Sie bleibt aber
      // Schlaf, falls spaeter ein langer Schlaf die Nacht doch noch verlaengert:
      // nachts nach einem Gang ist der kurze Schlaf davor echter Schlaf (5.10.
      // mit zehn Minuten Gang um 03:00: 22 Minuten, kurz wach, dann 58). Folgt
      // ein langer Schlaf, rechnet die Nacht so Minute fuer Minute wie ohne die
      // Regel. Der Preis: legt man sich nach dem Kaffee noch einmal lange hin,
      // zaehlt die Ruhe dazwischen als Schlaf, wie bis b25.
      continue;
    }
    auf = l->bis;
  }
  if (ein < 0) {
    return false;
  }
  ein = prv_zur_ruhe(e, w->wach, ein, auf);
  if (ein >= auf) {
    return false;
  }
  prv_aufwach_schuebe(e, w, ein, auf);

  int schlaf = 0;
  bool hat_puls = false;
  for (int i = ein; i < auf; i++) {
    if (!w->wach[i]) {
      schlaf++;
      hat_puls = hat_puls || e->puls[i] > 0;
    }
  }
  if (schlaf < (e->min_schlaf ? e->min_schlaf : MIN_NACHT)) {
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
  // MEHR ABSCHNITTE, ALS DAS ERGEBNIS FASST, WERDEN VERDICHTET, NICHT
  // ABGESCHNITTEN: abgeschnitten endete die Liste vor dem Aufwachen, und die
  // Schlafminuten (aus der Liste) passten nicht mehr zu ihm.
  anzahl = schlafanalyse_verdichten(w->liste, anzahl, SCHLAF_ABSCHNITTE_MAX);

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
