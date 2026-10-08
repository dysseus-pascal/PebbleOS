/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2, Schritt 3, AP3: HRV und Ruhepuls je Wachtag fuer die Health-App (Spezifikation
// Schritt 3, 3.8.1, Entscheid F2; S5; 9.1: T4.3, dazu T2.3 und T2.4 fuer S5).
//
// ECHT wie in test_boulder_schnittstelle (schnittstelle_umfeld.h): die ganze Variante mit nacht.c,
// speicher.c, umzug.c und naechte.c auf der echten settings_file.c, S5 mit Pebbles Pruefung der
// Rechte. Die Naechte laufen Minute fuer Minute aus den Referenznaechten von b26 durch den
// Minutenteil; fuer die Regeln laeuft Boulders eigener Code als Orakel (orakel_naechte.h, erzeugt
// aus Boulder 34fc13cb: nachtTag, jeTag, jeTagMitHrv).
// GEZAEHLT: jedes settings_file_set (--wrap) - so oft und so viel schreibt die Liste in boulder_v2.

#include "schnittstelle_umfeld.h"

#include "pbl/services/settings/settings_raw_iter.h"

typedef struct {
  uint32_t beginn_utc;
  uint16_t aufwachen;
  uint16_t schlaf;
  uint16_t hrv;
  uint8_t ruhepuls;
} OrakelNacht;

typedef struct {
  uint16_t tag;
  uint16_t hrv;
  uint8_t ruhepuls;
} OrakelTag;

typedef struct {
  const char *name;
  int erste_nacht;
  int naechte;
  int erster_tag;
  int tage;
  uint16_t min_ruhe;
  uint16_t min_hrv;
} OrakelFall;

#include "orakel_naechte.h"

// =============================================================================================
// Schreiblast: was in boulder_v2 geschrieben wird

static int s_naechte_saetze;  //< Saetze "naechte"
static int s_naechte_bytes;   //< samt Satzkopf von settings_file
static int s_v2_saetze;       //< alle Saetze in boulder_v2
static int s_v2_bytes;

status_t __real_settings_file_set(SettingsFile *file, const void *key, size_t key_len,
                                  const void *val, size_t val_len);
status_t __wrap_settings_file_set(SettingsFile *file, const void *key, size_t key_len,
                                  const void *val, size_t val_len) {
  if (file->iter.file_name && !strcmp(file->iter.file_name, SPEICHER_DATEI)) {
    const int satz = (int)(sizeof(SettingsRecordHeader) + key_len + val_len);
    s_v2_saetze++;
    s_v2_bytes += satz;
    if (key_len == 7 && !memcmp(key, "naechte", 7)) {
      // Nur die Liste, sonst nichts (F2): genau BoulderNaechte, ohne Schlafdauer je Nacht.
      cl_assert_equal_i(val_len, sizeof(BoulderNaechte));
      s_naechte_saetze++;
      s_naechte_bytes += satz;
    }
  }
  return __real_settings_file_set(file, key, key_len, val, val_len);
}

// =============================================================================================
// Hilfen

#define TAG_NULL 946684800  // 1.1.2000 0:00, Tag 0 der Wachtage
#define CEST (2 * PBL_SEC_PER_HOUR)

// Der Wachtag zu `utc` bei festem Versatz, von Hand (das Umfeld laeuft in CEST ohne Umstellung).
static uint16_t prv_wachtag(time_t utc, int versatz) {
  return (uint16_t)((utc + versatz - TAG_NULL) / PBL_SEC_PER_DAY);
}

// S5 aus der App: der Puffer im App-RAM hinter dem der SpO2-Werte.
static BoulderNaechte *const s_app_naechte = (BoulderNaechte *)&s_app_ram[3072];

static bool prv_s5(void) {
  prv_app_beginn();
  const bool r = sys_boulder_naechte(s_app_naechte);
  prv_app_ende();
  return r;
}

// Der Ring, wie schnittstelle.h ihn beschreibt (tage[neu] der juengste, rueckwaerts aelter), als
// aufsteigende Folge. Prueft dabei, was fuer jede Liste gilt (T4.3): hoechstens 37, jeder Tag
// einmal und aufsteigend, kein Feld ausser Datum, HRV und Ruhepuls belegt.
static int prv_folge(const BoulderNaechte *n, BoulderNachtEintrag *aus) {
  cl_assert_equal_i(n->fassung, BOULDER_NAECHTE_FASSUNG);
  cl_assert(n->anzahl <= NAECHTE_MAX);
  cl_assert(n->neu < NAECHTE_MAX);
  cl_assert_equal_i(n->reserve, 0);
  for (int i = 0; i < n->anzahl; i++) {
    aus[n->anzahl - 1 - i] = n->tage[(n->neu + NAECHTE_MAX - i) % NAECHTE_MAX];
  }
  for (int i = 0; i < n->anzahl; i++) {
    cl_assert_equal_i(aus[i].reserve, 0);
    if (i > 0) {
      cl_assert(aus[i].tag > aus[i - 1].tag);
    }
  }
  return n->anzahl;
}

// Die Liste, wie die App sie ueber S5 sieht, aufsteigend.
static BoulderNachtEintrag s_folge[NAECHTE_MAX];
static int prv_liste(void) {
  cl_assert(prv_s5());
  return prv_folge(s_app_naechte, s_folge);
}

static void prv_zeige(const char *titel) {
  const int n = prv_liste();
  printf("\n%s: %d Wachtage", titel, n);
  for (int i = 0; i < n; i++) {
    printf(" %d/%d/%d", s_folge[i].tag, s_folge[i].hrv, s_folge[i].ruhepuls);
  }
}

// Neu starten um `jetzt`: Pebbles Bereinigung, init, "Laden" (Dateien bleiben).
static void prv_neustart_um(time_t jetzt) {
  prv_kernel_bg();
  activity_algorithm_deinit();
  prv_kernel_bg();
  pbl_cron_deinit();
  rtc_set_time(jetzt);
  activity_sessions_prv_remove_out_of_range_activity_sessions(jetzt, true /*remove_ongoing*/);
  pbl_cron_init();
  prv_boot();
  prv_kernel_bg();  // "Laden"
}

// Die Referenznacht `name` im Fenster ab `beginn`, bis Minute `bis` (danach wach und unterwegs;
// das Fensterende um 11 Uhr macht die Nacht spaetestens endgueltig). Vorher raeumt Pebbles
// Tageswechsel die Sitzungen der Vortage weg (activity.c), wie auf der Uhr.
static void prv_fenster(const char *name, time_t beginn, int bis) {
  prv_lade(name);
  s_ref.beginn_utc = (uint32_t)beginn;
  activity_sessions_prv_remove_out_of_range_activity_sessions(beginn, false);
  prv_nacht(0, bis);
}

// Der letzte Satz 0x4001 mit diesem Beginn: was Boulder von der Nacht hat (NULL: keiner).
static const NachtDlsRecord *prv_satz(time_t beginn) {
  for (int i = s_saetze_anzahl - 1; i >= 0; i--) {
    if ((time_t)s_saetze[i].beginn_utc == beginn) {
      return &s_saetze[i];
    }
  }
  return NULL;
}

static const char *const s_referenzen[] = {
  "nacht-2026-09-26", "nacht-2026-09-29", "nacht-2026-09-30",
  "nacht-2026-10-03", "nacht-2026-10-05", "nacht-2026-10-05-morgen",
};
#define REFERENZEN ((int)ARRAY_LENGTH(s_referenzen))
// Das Fenster der ersten Referenz (25.9.2026, 20 Uhr CEST); die Folge rueckt je einen Tag weiter.
#define FENSTER_0 ((time_t)1790445600)

// Wie Pebbles Uhr nach einem Neustart: vor dem Fenster `beginn` hochfahren.
static void prv_beginne_vor(time_t beginn) {
  prv_neustart_um(beginn - PBL_SEC_PER_MIN);
}

// =============================================================================================
// T2.3, T2.4: S5 wie S1-S4

void test_boulder_naechte__initialize(void) {
  prv_umfeld_beginn();
  s_naechte_saetze = 0;
  s_naechte_bytes = 0;
  s_v2_saetze = 0;
  s_v2_bytes = 0;
}

void test_boulder_naechte__cleanup(void) {
  prv_umfeld_ende();
}

void test_boulder_naechte__puffer_ausserhalb_der_app_beenden_sie(void) {
  prv_kernel_bg();
  static uint8_t kern[512];
  prv_app_beginn();
  cl_assert_passert(sys_boulder_naechte((BoulderNaechte *)kern));
  prv_app_abbruch();
  // Ein Byte zu wenig am Ende des App-RAMs.
  prv_app_beginn();
  cl_assert_passert(sys_boulder_naechte(
      (BoulderNaechte *)&s_app_ram[sizeof(s_app_ram) - sizeof(BoulderNaechte) + 1]));
  prv_app_abbruch();
  // Gerade passend geht.
  prv_app_beginn();
  cl_assert(sys_boulder_naechte(
      (BoulderNaechte *)&s_app_ram[sizeof(s_app_ram) - sizeof(BoulderNaechte)]));
  prv_app_ende();
  // Mit Systemrechten (Kern) keine Pruefung, aus der App jede.
  const int vorher = s_puffer_geprueft;
  BoulderNaechte n;
  cl_assert(sys_boulder_naechte(&n));
  cl_assert_equal_i(s_puffer_geprueft, vorher);
  cl_assert(prv_s5());
  cl_assert_equal_i(s_puffer_geprueft, vorher + 1);
}

// Vor "Laden": false und eine leere Liste. Nach "Laden" die Liste aus der Datei.
void test_boulder_naechte__vor_dem_laden_leer(void) {
  memset(s_app_naechte, 0xAB, sizeof(*s_app_naechte));
  cl_assert(!prv_s5());
  cl_assert_equal_i(s_app_naechte->fassung, BOULDER_NAECHTE_FASSUNG);
  cl_assert_equal_i(s_app_naechte->anzahl, 0);
  cl_assert_equal_i(s_app_naechte->tage[NAECHTE_MAX - 1].tag, 0);
  prv_kernel_bg();  // "Laden"
  cl_assert(prv_s5());
  cl_assert_equal_i(s_app_naechte->anzahl, 0);
  cl_assert_equal_i(s_naechte_saetze, 0);
}

// =============================================================================================
// Die Regeln gegen Boulder (Orakel, T4.3): je Wachtag der Ruhepuls der laengsten Nacht, die HRV
// der laengsten Nacht mit HRV, die juengsten 37. Die Naechte gehen ueber die Funktion hinein, die
// nacht.c ruft, wenn eine Nacht endgueltig wird; gelesen wird ueber S5 wie in der App.

void test_boulder_naechte__regeln_wie_boulder(void) {
  // Die Zone des Orakels: Europe/Zurich 2026 (CET, Sommerzeit 29.3. bis 25.10.).
  TimezoneInfo tz = { .tm_zone = "CE*T", .tm_gmtoff = PBL_SEC_PER_HOUR,
                      .dst_start = 1774746000, .dst_end = 1792890000 };
  time_util_update_timezone(&tz);
  prv_kernel_bg();
  int naechte = 0, tage = 0;
  for (int f = 0; f < (int)ARRAY_LENGTH(ORAKEL_FALL); f++) {
    const OrakelFall *fall = &ORAKEL_FALL[f];
    boulder_naechte_test_neustart();
    boulder_naechte_laden(NULL);
    for (int i = 0; i < fall->naechte; i++) {
      const OrakelNacht *n = &ORAKEL_NACHT[fall->erste_nacht + i];
      boulder_naechte_eintragen((time_t)n->beginn_utc + n->aufwachen * PBL_SEC_PER_MIN, n->schlaf,
                                n->hrv, n->ruhepuls);
    }
    const int anzahl = prv_liste();
    if (anzahl != fall->tage) {
      printf("\nFall %s: %d Tage statt %d", fall->name, anzahl, fall->tage);
    }
    cl_assert_equal_i(anzahl, fall->tage);
    for (int i = 0; i < anzahl; i++) {
      const OrakelTag *soll = &ORAKEL_TAG[fall->erster_tag + i];
      if (s_folge[i].tag != soll->tag || s_folge[i].hrv != soll->hrv ||
          s_folge[i].ruhepuls != soll->ruhepuls) {
        printf("\nFall %s, Tag %d: Uhr %d/%d/%d, Boulder %d/%d/%d", fall->name, i, s_folge[i].tag,
               s_folge[i].hrv, s_folge[i].ruhepuls, soll->tag, soll->hrv, soll->ruhepuls);
      }
      cl_assert_equal_i(s_folge[i].tag, soll->tag);
      cl_assert_equal_i(s_folge[i].hrv, soll->hrv);
      cl_assert_equal_i(s_folge[i].ruhepuls, soll->ruhepuls);
    }
    cl_assert_equal_i(s_app_naechte->min_ruhe, fall->min_ruhe);
    cl_assert_equal_i(s_app_naechte->min_hrv, fall->min_hrv);
    naechte += fall->naechte;
    tage += anzahl;
  }
  printf("\nRegeln wie Boulder: %d Faelle, %d Naechte, %d Wachtage, 0 Abweichungen\n",
         (int)ARRAY_LENGTH(ORAKEL_FALL), naechte, tage);
}

// UHR ZURUECK (naechte.h): Ein aelterer Wachtag, der fehlt, wird einsortiert; einer, den die Liste
// hat, bleibt, auch wenn die neue Nacht laenger ist (ihre Laenge kennt die Uhr nicht, F2). Ist die
// Liste voll, faellt der aelteste, und was aelter als alle 37 ist, kommt nicht hinein. Hier weicht
// die Uhr bewusst von Boulder ab, das die laengere naehme.
static time_t prv_morgen(int tag) {
  return TAG_NULL + (time_t)tag * PBL_SEC_PER_DAY + 7 * PBL_SEC_PER_HOUR - CEST;
}

void test_boulder_naechte__uhr_zurueck_sortiert_ein(void) {
  prv_kernel_bg();
  const int t0 = 9770;
  for (int k = 0; k < 5; k += 2) {  // Tage t0, t0+2, t0+4
    boulder_naechte_eintragen(prv_morgen(t0 + k), 300, 40 + k, 50 + k);
  }
  // Fehlt: einsortiert, mit HRV und Ruhepuls dieser Nacht.
  boulder_naechte_eintragen(prv_morgen(t0 + 1), 200, 71, 61);
  boulder_naechte_eintragen(prv_morgen(t0 - 3), 200, 72, 62);
  // Da: bleibt, auch laenger.
  boulder_naechte_eintragen(prv_morgen(t0 + 2), 600, 99, 99);
  cl_assert_equal_i(prv_liste(), 5);
  const uint16_t tage[] = { t0 - 3, t0, t0 + 1, t0 + 2, t0 + 4 };
  const uint16_t hrv[] = { 72, 40, 71, 42, 44 };
  const uint8_t ruhe[] = { 62, 50, 61, 52, 54 };
  for (int i = 0; i < 5; i++) {
    cl_assert_equal_i(s_folge[i].tag, tage[i]);
    cl_assert_equal_i(s_folge[i].hrv, hrv[i]);
    cl_assert_equal_i(s_folge[i].ruhepuls, ruhe[i]);
  }
  // Der juengste Tag und seine Auswahlhilfe bleiben (t0 + 4, 300 min).
  cl_assert_equal_i(s_app_naechte->min_ruhe, 300);

  // Voll: 37 Tage ab t0 + 10 mit Luecke bei t0 + 30.
  boulder_naechte_test_neustart();
  boulder_naechte_laden(NULL);
  for (int k = 0; k <= NAECHTE_MAX; k++) {
    if (k != 20) {
      boulder_naechte_eintragen(prv_morgen(t0 + 10 + k), 300, 30 + k, 50);
    }
  }
  cl_assert_equal_i(prv_liste(), NAECHTE_MAX);
  cl_assert_equal_i(s_folge[0].tag, t0 + 10);
  // Aelter als alle: nichts.
  boulder_naechte_eintragen(prv_morgen(t0 + 9), 300, 1, 1);
  cl_assert_equal_i(prv_liste(), NAECHTE_MAX);
  cl_assert_equal_i(s_folge[0].tag, t0 + 10);
  // In die Luecke: der aelteste faellt, der neue steht an seinem Platz.
  boulder_naechte_eintragen(prv_morgen(t0 + 30), 300, 2, 2);
  cl_assert_equal_i(prv_liste(), NAECHTE_MAX);
  cl_assert_equal_i(s_folge[0].tag, t0 + 11);
  cl_assert_equal_i(s_folge[19].tag, t0 + 30);
  cl_assert_equal_i(s_folge[19].hrv, 2);
  cl_assert_equal_i(s_folge[NAECHTE_MAX - 1].tag, t0 + 10 + NAECHTE_MAX);
}

// Eine Nacht ohne gueltige Uhrzeit (vor 2000) kommt nicht in die Liste.
void test_boulder_naechte__ohne_uhrzeit_nichts(void) {
  prv_kernel_bg();
  boulder_naechte_eintragen(100000, 400, 50, 55);
  cl_assert_equal_i(prv_liste(), 0);
}

// "Laden": Nur ein Satz mit genau 230 B, Fassung 1 und einem gueltigen Ring gilt - sonst beginnt
// die Liste leer, statt Eintraege ueber das Feld hinaus zu schieben.
static void prv_satz_in_datei(const void *wert, size_t laenge) {
  SettingsFile f;
  cl_must_pass(settings_file_open(&f, SPEICHER_DATEI, SPEICHER_DATEI_GROESSE));
  // Am Zaehler vorbei: Das schreibt der Test, nicht die Uhr.
  cl_must_pass(__real_settings_file_set(&f, "naechte", 7, wert, laenge));
  settings_file_close(&f);
}

void test_boulder_naechte__laden_prueft_den_satz(void) {
  prv_kernel_bg();
  BoulderNaechte gut = { .fassung = 1, .anzahl = 3, .neu = 1, .min_ruhe = 410, .min_hrv = 0 };
  gut.tage[NAECHTE_MAX - 1] = (BoulderNachtEintrag){ .tag = 9000, .hrv = 41, .ruhepuls = 51 };
  gut.tage[0] = (BoulderNachtEintrag){ .tag = 9002, .hrv = 42, .ruhepuls = 52 };
  gut.tage[1] = (BoulderNachtEintrag){ .tag = 9005, .hrv = 0, .ruhepuls = 53 };
  prv_satz_in_datei(&gut, sizeof(gut));
  prv_neustart_um(rtc_get_time());
  cl_assert_equal_i(prv_liste(), 3);
  cl_assert(!memcmp(s_app_naechte, &gut, sizeof(gut)));

  BoulderNaechte schlecht[5];
  for (int i = 0; i < 5; i++) {
    schlecht[i] = gut;
  }
  schlecht[0].fassung = 2;
  schlecht[1].anzahl = NAECHTE_MAX + 1;
  schlecht[2].neu = NAECHTE_MAX;
  schlecht[3].tage[0].tag = 8999;  // nicht aufsteigend
  schlecht[4].tage[1].tag = 9002;  // doppelt
  for (int i = 0; i < 5; i++) {
    prv_satz_in_datei(&schlecht[i], sizeof(schlecht[i]));
    prv_neustart_um(rtc_get_time());
    cl_assert(prv_s5());
    cl_assert_equal_i(s_app_naechte->anzahl, 0);
  }
  prv_satz_in_datei(&gut, sizeof(gut) - 1);
  prv_neustart_um(rtc_get_time());
  cl_assert(prv_s5());
  cl_assert_equal_i(s_app_naechte->anzahl, 0);
}

// =============================================================================================
// Mit der echten Nacht: Minute fuer Minute durch nacht.c, Ablage und Laden in speicher.c

// T4.3: Zehn Referenznaechte in Folge, je Fenster ein Tag weiter. Jede steht danach einmal in der
// Liste, mit HRV und Ruhepuls wie im NachtBild von b26 und wie im letzten Satz 0x4001, den Boulder
// bekommt - eingetragen erst, wenn die Nacht endgueltig ist, danach unveraendert. Nach der fuenften
// ein Neustart: nichts geht verloren. Dazu Schreiblast und Platz in boulder_v2.
void test_boulder_naechte__zehn_referenznaechte_in_folge(void) {
  prv_beginne_vor(FENSTER_0);
  int saetze_davor = s_naechte_saetze;
  for (int k = 0; k < 10; k++) {
    const time_t beginn = FENSTER_0 + (time_t)k * PBL_SEC_PER_DAY;
    prv_lade(s_referenzen[k % REFERENZEN]);
    s_ref.beginn_utc = (uint32_t)beginn;
    activity_sessions_prv_remove_out_of_range_activity_sessions(beginn, false);
    int fertig_ab = -1, eintrag_ab = -1;
    BoulderNachtEintrag eintrag = { 0 };
    for (int m = 0; m < 20 * 60; m++) {
      prv_nacht(m, m + 1);
      if (fertig_ab < 0 && boulder_nacht_stufe() == NachtFertig) {
        fertig_ab = m;
      }
      const int n = prv_liste();
      if (eintrag_ab < 0 && n == k + 1) {
        eintrag_ab = m;
        eintrag = s_folge[n - 1];
      } else if (eintrag_ab >= 0) {
        // Danach bleibt er, wie er ist.
        cl_assert(!memcmp(&s_folge[n - 1], &eintrag, sizeof(eintrag)));
      } else {
        cl_assert_equal_i(n, k);
      }
    }
    const NachtBild *b = &s_ref.bild;
    const NachtDlsRecord *satz = prv_satz(beginn);
    cl_assert(satz);
    printf("\n%s am %d: fertig ab Minute %d, eingetragen ab %d, Wachtag %d, HRV %d, Ruhepuls %d",
           s_referenzen[k % REFERENZEN], k, fertig_ab, eintrag_ab, eintrag.tag, eintrag.hrv,
           eintrag.ruhepuls);
    // Erst endgueltig: zwei Stunden wach nach dem Aufwachen oder das Fensterende (11 Uhr).
    cl_assert(fertig_ab >= 0 && eintrag_ab > fertig_ab);
    cl_assert(eintrag_ab >= b->aufwachen + NACHT_WACH_ENDGUELTIG || eintrag_ab >= NACHT_MINUTEN);
    // Wachtag = Ortsdatum des Aufwachens; Werte wie b26 und wie Boulders letzter Satz.
    cl_assert_equal_i(eintrag.tag, prv_wachtag(beginn + b->aufwachen * PBL_SEC_PER_MIN, CEST));
    cl_assert_equal_i(eintrag.hrv, b->hrv);
    cl_assert_equal_i(eintrag.ruhepuls, b->ruhepuls);
    cl_assert_equal_i(satz->aufwachen, b->aufwachen);
    cl_assert_equal_i(eintrag.hrv, satz->hrv);
    cl_assert_equal_i(eintrag.ruhepuls, satz->ruhepuls);
    // Genau einmal geschrieben je Nacht.
    cl_assert_equal_i(s_naechte_saetze, saetze_davor + 1);
    saetze_davor = s_naechte_saetze;
    if (k == 4) {
      BoulderNaechte vorher;
      cl_assert(prv_s5());
      vorher = *s_app_naechte;
      prv_neustart_um(rtc_get_time() + 30 * PBL_SEC_PER_MIN);
      cl_assert(prv_s5());
      cl_assert(!memcmp(s_app_naechte, &vorher, sizeof(vorher)));
      saetze_davor = s_naechte_saetze;
    }
  }
  prv_zeige("Zehn Naechte");
  cl_assert_equal_i(prv_liste(), 10);
  // Platz und Schreiblast in boulder_v2 (Spezifikation 3.5): die Liste je Nacht gegen alles.
  SettingsFile f;
  cl_must_pass(settings_file_open(&f, SPEICHER_DATEI, SPEICHER_DATEI_GROESSE));
  const int belegt = f.used_space, tot = f.dead_space;
  settings_file_close(&f);
  printf("\nboulder_v2 nach 10 Naechten: belegt %d B von %d (davon \"naechte\" %d B), tot %d B; "
         "geschrieben %d Saetze / %d B, davon \"naechte\" %d / %d B (%d B je Nacht)\n",
         belegt, SPEICHER_DATEI_GROESSE, (int)(sizeof(SettingsRecordHeader) + 7 +
                                               sizeof(BoulderNaechte)),
         tot, s_v2_saetze, s_v2_bytes, s_naechte_saetze, s_naechte_bytes,
         s_naechte_bytes / s_naechte_saetze);
  cl_assert_equal_i(s_naechte_bytes / s_naechte_saetze, 8 + 7 + 230);
  cl_assert(belegt < SPEICHER_DATEI_GROESSE);
}

// Nickerchen zaehlen nicht (V5): Ein Schlaf am Nachmittag - auch einer ueber drei Stunden, den
// Pebble als Sleep fuehrt - aendert die Liste nicht. Einmal am Tag der Nacht, einmal am Tag danach
// ohne Nacht: Dort haette die Liste noch keinen Eintrag, ein Nickerchen fiele also auf.
static int s_quelle, s_bester;

// Ein Fenster ab `beginn`, von Minute `von` an wach und unterwegs, ab 14:00 (Minute 1080) `laenge`
// Minuten Schlaf: die Bewegung aus dem laengsten Schlaflauf der Nacht, wiederholt, der Puls ruhig.
static void prv_nachmittag_spielen(time_t beginn, int von, int laenge, uint8_t ruhepuls) {
  for (int m = von; m < NACHT_FENSTER_MINUTEN; m++) {
    const int i = m - 18 * 60;
    if (i >= 0 && i < laenge) {
      const int q = s_quelle + 10 + i % (s_bester - 20);
      prv_minute(beginn, m, s_ref.vmc[q], ruhepuls, true);
    } else {
      prv_minute(beginn, m, 3000, 82, true);
    }
  }
}

static int prv_nachmittag(int laenge) {
  prv_beginne_vor(FENSTER_0);
  prv_fenster("nacht-2026-09-29", FENSTER_0, NACHT_MINUTEN + 30);
  cl_assert_equal_i(prv_liste(), 1);
  const BoulderNachtEintrag nacht = s_folge[0];
  const uint16_t gemeldet = boulder_nacht_nickerchen_gemeldet();
  // Der laengste Schlaflauf der Nacht: aufeinander folgende Abschnitte ohne Wach.
  const NachtBild *b = &s_ref.bild;
  s_quelle = 0;
  s_bester = 0;
  for (int i = 0; i < b->anzahl;) {
    int j = i;
    while (j < b->anzahl && b->abschnitte[j].phase != SchlafPhaseWach) {
      j++;
    }
    if (j > i && b->abschnitte[j - 1].bis - b->abschnitte[i].von > s_bester) {
      s_quelle = b->abschnitte[i].von;
      s_bester = b->abschnitte[j - 1].bis - b->abschnitte[i].von;
    }
    i = j + 1;
  }
  cl_assert(s_bester > 60);
  prv_nachmittag_spielen(FENSTER_0, NACHT_MINUTEN + 30, laenge, b->ruhepuls);
  cl_assert_equal_i(prv_liste(), 1);
  cl_assert(!memcmp(&s_folge[0], &nacht, sizeof(nacht)));
  // Der Tag danach: die ganze Nacht wach, am Nachmittag dasselbe Nickerchen.
  const time_t beginn = FENSTER_0 + PBL_SEC_PER_DAY;
  activity_sessions_prv_remove_out_of_range_activity_sessions(beginn, false);
  prv_nachmittag_spielen(beginn, 0, laenge, b->ruhepuls);
  cl_assert_equal_i(prv_liste(), 1);
  cl_assert(!memcmp(&s_folge[0], &nacht, sizeof(nacht)));
  return boulder_nacht_nickerchen_gemeldet() - gemeldet;
}

void test_boulder_naechte__nickerchen_zaehlen_nicht(void) {
  const int kurz = prv_nachmittag(75);
  test_boulder_naechte__cleanup();
  test_boulder_naechte__initialize();
  const int lang = prv_nachmittag(200);
  printf("\nNachmittag an zwei Tagen: 75 min -> %d Nickerchen, 200 min -> %d; Liste unveraendert\n",
         kurz, lang);
  // Die kurzen sind gemeldete Nickerchen, an beiden Tagen: die Probe ist echt, nicht leer.
  cl_assert_equal_i(kurz, 2);
}

// Eine Eingabe aus Stuecken der Referenznaechte: [von, bis) des Fensters aus `name` ab deren
// Minute `ab`, mit den HRV-Fenstern, die ganz im Stueck liegen. Dazwischen wach und unterwegs.
static Referenz s_mix;

static void prv_mix_neu(void) {
  memset(&s_mix, 0, sizeof(s_mix));
  for (int m = 0; m < MINUTEN; m++) {
    s_mix.gueltig[m] = true;
    s_mix.vmc[m] = 3000;
    s_mix.puls[m] = 82;
  }
}

static void prv_mix_stueck(const char *name, int ab, int von, int bis, bool mit_hrv) {
  prv_lade(name);
  for (int m = von; m < bis; m++) {
    s_mix.gueltig[m] = s_ref.gueltig[ab + m - von];
    s_mix.vmc[m] = s_ref.vmc[ab + m - von];
    s_mix.puls[m] = s_ref.puls[ab + m - von];
  }
  for (int i = 0; mit_hrv && i < s_ref.hrv_anzahl; i++) {
    const int m = s_ref.hrv[i].minute - ab + von;
    if (s_ref.hrv[i].minute >= ab && m + 5 <= bis) {
      s_mix.hrv[s_mix.hrv_anzahl] = s_ref.hrv[i];
      s_mix.hrv[s_mix.hrv_anzahl++].minute = (uint16_t)m;
    }
  }
}

static void prv_mix_spielen(time_t beginn, int bis) {
  s_ref = s_mix;
  s_ref.beginn_utc = (uint32_t)beginn;
  s_hrv_naechstes = 0;
  activity_sessions_prv_remove_out_of_range_activity_sessions(beginn, false);
  prv_nacht(0, bis);
}

// Die Saetze 0x4001 eines Fensters, je Nacht der letzte (Boulder ersetzt eine Nacht mit gleichem
// Beginn): Rueckgabe wie viele Naechte.
static int prv_naechte_im_fenster(time_t beginn, NachtDlsRecord *aus, int max) {
  int n = 0;
  for (int i = 0; i < s_saetze_anzahl; i++) {
    const time_t b = s_saetze[i].beginn_utc;
    if (b < beginn || b >= beginn + NACHT_FENSTER_MINUTEN * PBL_SEC_PER_MIN) {
      continue;
    }
    int k = 0;
    while (k < n && (time_t)aus[k].beginn_utc != b) {
      k++;
    }
    if (k == n) {
      cl_assert(n < max);
      n++;
    }
    aus[k] = s_saetze[i];
  }
  return n;
}

// Folgenacht (T4.3): Eine kurze Nacht mit HRV bis 01:30, zwei Stunden auf, dann eine laengere ohne
// HRV. Beide wachen am selben Ortstag auf: ein Eintrag, der Ruhepuls von der laengeren (jeTag),
// die HRV von der einzigen mit HRV (jeTagMitHrv) - nicht von der laengsten.
void test_boulder_naechte__folgenacht(void) {
  prv_beginne_vor(FENSTER_0);
  prv_mix_neu();
  prv_mix_stueck("nacht-2026-09-26", 0, 0, 330, true);    // 26.9.: HRV, schlaeft ab 22:17
  prv_mix_stueck("nacht-2026-09-29", 100, 480, 800, false);  // 29.9.: ohne HRV
  prv_mix_spielen(FENSTER_0, 20 * 60);
  NachtDlsRecord n[4];
  const int naechte = prv_naechte_im_fenster(FENSTER_0, n, 4);
  cl_assert_equal_i(naechte, 2);
  printf("\nFolgenacht: 1. Nacht %d min HRV %d Ruhepuls %d auf %d, 2. Nacht %d min HRV %d "
         "Ruhepuls %d auf %d", n[0].schlaf_minuten, n[0].hrv, n[0].ruhepuls,
         (int)(n[0].beginn_utc - FENSTER_0) / 60 + n[0].aufwachen, n[1].schlaf_minuten, n[1].hrv,
         n[1].ruhepuls, (int)(n[1].beginn_utc - FENSTER_0) / 60 + n[1].aufwachen);
  // Die Lage, die die Regeln trennt: die erste kuerzer und mit HRV, die zweite laenger ohne.
  cl_assert(n[0].hrv > 0 && n[1].hrv == 0);
  cl_assert(n[1].schlaf_minuten > n[0].schlaf_minuten);
  cl_assert(n[1].ruhepuls != n[0].ruhepuls);
  const uint16_t tag0 = prv_wachtag(n[0].beginn_utc + n[0].aufwachen * PBL_SEC_PER_MIN, CEST);
  const uint16_t tag1 = prv_wachtag(n[1].beginn_utc + n[1].aufwachen * PBL_SEC_PER_MIN, CEST);
  cl_assert_equal_i(tag0, tag1);
  cl_assert_equal_i(prv_liste(), 1);
  cl_assert_equal_i(s_folge[0].tag, tag0);
  cl_assert_equal_i(s_folge[0].ruhepuls, n[1].ruhepuls);
  cl_assert_equal_i(s_folge[0].hrv, n[0].hrv);
  cl_assert_equal_i(s_app_naechte->min_ruhe, n[1].schlaf_minuten);
  cl_assert_equal_i(s_app_naechte->min_hrv, n[0].schlaf_minuten);
  // Zwei Naechte, zwei Aenderungen der Liste, zwei Saetze in der Datei.
  cl_assert_equal_i(s_naechte_saetze, 2);
}

// Nach Mitternacht Ortszeit, vor Mitternacht UTC aufgewacht: der Wachtag ist der Ortstag. Danach
// stellt das Telefon eine andere Zone ein (UTC-4): Die naechste Nacht zaehlt in der neuen Zone, die
// Liste bleibt aufsteigend, der fruehere Eintrag unveraendert.
void test_boulder_naechte__zeitzone(void) {
  prv_beginne_vor(FENSTER_0);
  prv_mix_neu();
  prv_mix_stueck("nacht-2026-09-26", 0, 0, 290, true);  // schlaeft 22:17 bis etwa 00:50
  prv_mix_spielen(FENSTER_0, 20 * 60);
  NachtDlsRecord n[2];
  cl_assert_equal_i(prv_naechte_im_fenster(FENSTER_0, n, 2), 1);
  const time_t auf = n[0].beginn_utc + n[0].aufwachen * PBL_SEC_PER_MIN;
  printf("\nZeitzone: aufgewacht %d:%02d Ortszeit (UTC %d:%02d)", (int)((auf + CEST) / 3600 % 24),
         (int)((auf + CEST) / 60 % 60), (int)(auf / 3600 % 24), (int)(auf / 60 % 60));
  // Die Probe trennt nur, wenn Orts- und UTC-Datum verschieden sind.
  cl_assert(prv_wachtag(auf, CEST) != prv_wachtag(auf, 0));
  cl_assert_equal_i(prv_liste(), 1);
  cl_assert_equal_i(s_folge[0].tag, prv_wachtag(auf, CEST));
  const BoulderNachtEintrag erster = s_folge[0];

  // Neue Zone (EDT, UTC-4): das naechste Fenster beginnt um 20 Uhr dort.
  TimezoneInfo tz = { .tm_zone = "EDT", .tm_gmtoff = -4 * PBL_SEC_PER_HOUR };
  time_util_update_timezone(&tz);
  const time_t fenster = FENSTER_0 + PBL_SEC_PER_DAY + 6 * PBL_SEC_PER_HOUR;
  prv_fenster("nacht-2026-09-30", fenster, 20 * 60);
  const time_t auf2 = fenster + s_ref.bild.aufwachen * PBL_SEC_PER_MIN;
  cl_assert_equal_i(prv_liste(), 2);
  cl_assert(!memcmp(&s_folge[0], &erster, sizeof(erster)));
  cl_assert_equal_i(s_folge[1].tag, prv_wachtag(auf2, -4 * PBL_SEC_PER_HOUR));
  cl_assert_equal_i(s_folge[1].ruhepuls, s_ref.bild.ruhepuls);
  prv_zeige("Zeitzone");
  printf("\n");
}

// Neustarts (T4.3: "ein Neustart verliert Eintraege"). Die Nacht steht nach jedem Neustart genau
// einmal in der Liste, mit den Werten ihres letzten NachtBilds und Boulders letztem Satz.
// - Nach "fertig", vor "endgueltig" (30 min nach dem Aufwachen): dieselben Werte wie ohne Neustart.
// - Absturz zwischen dem Eintrag und der Ablage: Die Datei hat die Nacht noch nicht, der Kopf nennt
//   sie noch nicht endgueltig - nach dem Neustart wird sie es noch einmal und steht wieder genau
//   einmal da, mit denselben Werten.
// - Mitten im Schlaf: Die Nacht rechnet nach dem Neustart weiter; ihr Schlaf kann von dem ohne
//   Neustart abweichen (Schritt 1, T8.3: verschluckte Minuten). Die Liste folgt der Nacht.
static void prv_neustart_in_der_nacht(int m) {
  const time_t b = s_ref.beginn_utc;
  prv_kernel_bg();
  activity_algorithm_deinit();
  prv_kernel_bg();
  pbl_cron_deinit();
  const time_t jetzt = b + (time_t)(m + 2) * PBL_SEC_PER_MIN;
  rtc_set_time(jetzt);
  activity_sessions_prv_remove_out_of_range_activity_sessions(jetzt, true /*remove_ongoing*/);
  pbl_cron_init();
  prv_boot();
  // D14: Pebbles Minutendatei bringt die Minuten bis zur letzten vollen Viertelstunde davor (wie
  // test_boulder_zyklen, prv_hochfahren).
  const int datei_bis = (m + 1) - (m + 1) % 15 - 15;
  for (int i = 0; i < datei_bis && i < NACHT_MINUTEN; i++) {
    const BoulderMinute minute = {
      .utc = b + (time_t)i * PBL_SEC_PER_MIN, .vmc = s_ref.gueltig[i] ? s_ref.vmc[i] : 0,
      .puls = s_ref.gueltig[i] ? s_ref.puls[i] : 0, .nicht_getragen = !s_ref.gueltig[i],
    };
    boulder_dauerteil_ring_minute(&minute);
  }
  prv_kernel_bg();  // "Laden"
}

static bool prv_eintrag_da(void) {
  return boulder_naechte_kern()->anzahl > 0;
}

// Die Liste hat genau die eine Nacht, mit den Werten des NachtBilds und des letzten Satzes.
static BoulderNachtEintrag prv_eine_wie_die_nacht(void) {
  cl_assert_equal_i(prv_liste(), 1);
  NachtBild bild;
  cl_assert(boulder_nacht_letzte(&bild));
  const NachtDlsRecord *satz = prv_satz(bild.beginn_utc);
  cl_assert(satz);
  cl_assert_equal_i(s_folge[0].tag,
                    prv_wachtag(bild.beginn_utc + bild.aufwachen * PBL_SEC_PER_MIN, CEST));
  cl_assert_equal_i(s_folge[0].hrv, bild.hrv);
  cl_assert_equal_i(s_folge[0].ruhepuls, bild.ruhepuls);
  cl_assert_equal_i(satz->hrv, bild.hrv);
  cl_assert_equal_i(satz->ruhepuls, bild.ruhepuls);
  cl_assert_equal_i(s_app_naechte->min_ruhe, bild.schlaf_minuten);
  return s_folge[0];
}

static void prv_neu_mit(const char *name) {
  test_boulder_naechte__cleanup();
  test_boulder_naechte__initialize();
  prv_lade(name);
  prv_beginne_vor(s_ref.beginn_utc);
}

void test_boulder_naechte__neustarts(void) {
  for (int r = 0; r < REFERENZEN; r++) {
    const char *name = s_referenzen[r];
    // Ohne Neustart.
    prv_neu_mit(name);
    prv_nacht(0, 20 * 60);
    const BoulderNachtEintrag ohne = prv_eine_wie_die_nacht();
    const int auf = s_ref.bild.aufwachen, schlaf = s_ref.bild.schlaf_minuten;

    // Nach "fertig", vor "endgueltig".
    prv_neu_mit(name);
    prv_nacht(0, auf + 31);
    cl_assert_equal_i(boulder_nacht_stufe(), NachtFertig);
    cl_assert_equal_i(prv_liste(), 0);
    prv_neustart_in_der_nacht(auf + 30);
    prv_nacht(auf + 32, 20 * 60);
    BoulderNachtEintrag mit = prv_eine_wie_die_nacht();
    cl_assert(!memcmp(&mit, &ohne, sizeof(ohne)));

    // Absturz gleich nach dem Eintrag, bevor die Ablage lief.
    prv_neu_mit(name);
    int m = 0;
    s_absturz_nach = prv_eintrag_da;
    while (!s_abgestuerzt && m < 20 * 60) {
      prv_nacht(m, m + 1);
      m++;
    }
    cl_assert(s_abgestuerzt);
    cl_assert_equal_i(s_naechte_saetze, 0);
    prv_neustart_in_der_nacht(m - 1);
    cl_assert_equal_i(prv_liste(), 0);  // die Datei hat sie nicht
    prv_nacht(m + 1, 20 * 60);
    mit = prv_eine_wie_die_nacht();
    cl_assert(!memcmp(&mit, &ohne, sizeof(ohne)));
    cl_assert_equal_i(s_naechte_saetze, 1);
    prv_neustart_um(rtc_get_time() + PBL_SEC_PER_HOUR);
    cl_assert_equal_i(prv_liste(), 1);
    cl_assert(!memcmp(&s_folge[0], &ohne, sizeof(ohne)));
    const int absturz = m - 1;

    // Mitten im Schlaf (zwei Stunden vor dem Aufwachen).
    prv_neu_mit(name);
    prv_nacht(0, auf - 119);
    prv_neustart_in_der_nacht(auf - 120);
    prv_nacht(auf - 118, 20 * 60);
    prv_eine_wie_die_nacht();
    NachtBild bild;
    cl_assert(boulder_nacht_letzte(&bild));
    printf("\n%s: ohne Neustart %d min, Absturz in Minute %d: gleich; Neustart in Minute %d: "
           "%d min, Ruhepuls %d statt %d, HRV %d statt %d", name, schlaf, absturz, auf - 120,
           bild.schlaf_minuten, bild.ruhepuls, ohne.ruhepuls, bild.hrv, ohne.hrv);
  }
  printf("\n");
}

// Uhr zurueck (nach einem Neustart mit falscher Zeit): drei Naechte, dann drei Tage zurueck ein
// fehlender Wachtag (einsortiert), dann ein vorhandener (bleibt), dann wieder vorwaerts.
void test_boulder_naechte__uhr_zurueck(void) {
  prv_beginne_vor(FENSTER_0);
  for (int k = 0; k < 3; k++) {
    prv_fenster(s_referenzen[k], FENSTER_0 + (time_t)k * PBL_SEC_PER_DAY, 20 * 60);
  }
  cl_assert_equal_i(prv_liste(), 3);
  BoulderNachtEintrag vorher[3];
  memcpy(vorher, s_folge, sizeof(vorher));

  const time_t zurueck = FENSTER_0 - 3 * PBL_SEC_PER_DAY;
  prv_beginne_vor(zurueck);
  prv_fenster(s_referenzen[3], zurueck, 20 * 60);
  const uint16_t alt = prv_wachtag(zurueck + s_ref.bild.aufwachen * PBL_SEC_PER_MIN, CEST);
  cl_assert_equal_i(prv_liste(), 4);
  cl_assert_equal_i(s_folge[0].tag, alt);
  cl_assert_equal_i(s_folge[0].ruhepuls, s_ref.bild.ruhepuls);
  cl_assert(!memcmp(&s_folge[1], vorher, sizeof(vorher)));

  // Derselbe Tag wie der zweite: bleibt (hier mit einer anderen Nacht).
  prv_beginne_vor(FENSTER_0 + PBL_SEC_PER_DAY);
  prv_fenster("nacht-2026-09-26", FENSTER_0 + PBL_SEC_PER_DAY, 20 * 60);
  cl_assert(s_ref.bild.schlaf_minuten != 0);
  cl_assert_equal_i(prv_liste(), 4);
  cl_assert(!memcmp(&s_folge[1], vorher, sizeof(vorher)));

  // Wieder vorwaerts: angehaengt.
  prv_beginne_vor(FENSTER_0 + 3 * PBL_SEC_PER_DAY);
  prv_fenster(s_referenzen[4], FENSTER_0 + 3 * PBL_SEC_PER_DAY, 20 * 60);
  cl_assert_equal_i(prv_liste(), 5);
  cl_assert(!memcmp(&s_folge[1], vorher, sizeof(vorher)));
  cl_assert(s_folge[4].tag > vorher[2].tag);
  prv_zeige("Uhr zurueck");
  printf("\n");
}

// 40 Naechte: Die Liste laeuft ueber und haelt die juengsten 37, auch nach einem Neustart.
void test_boulder_naechte__vierzig_naechte(void) {
  prv_beginne_vor(FENSTER_0);
  uint16_t tage[40];
  for (int k = 0; k < 40; k++) {
    const time_t beginn = FENSTER_0 + (time_t)k * PBL_SEC_PER_DAY;
    prv_fenster(s_referenzen[k % REFERENZEN], beginn, NACHT_MINUTEN + 30);
    tage[k] = prv_wachtag(beginn + s_ref.bild.aufwachen * PBL_SEC_PER_MIN, CEST);
    cl_assert_equal_i(prv_liste(), (k + 1 < NAECHTE_MAX) ? k + 1 : NAECHTE_MAX);
  }
  for (int i = 0; i < NAECHTE_MAX; i++) {
    cl_assert_equal_i(s_folge[i].tag, tage[40 - NAECHTE_MAX + i]);
  }
  BoulderNaechte vorher = *s_app_naechte;
  prv_neustart_um(rtc_get_time() + PBL_SEC_PER_HOUR);
  cl_assert(prv_s5());
  cl_assert(!memcmp(s_app_naechte, &vorher, sizeof(vorher)));
  printf("\nVierzig Naechte: %d in der Liste, Wachtage %d bis %d; %d Saetze \"naechte\"\n",
         prv_liste(), s_folge[0].tag, s_folge[NAECHTE_MAX - 1].tag, s_naechte_saetze);
  cl_assert_equal_i(s_naechte_saetze, 40);
}

// =============================================================================================
// Saat beim Wechsel b26 -> V2 (3.8.1): Die Liste beginnt mit der Nacht, die b26 abgeschlossen hat.

// boulder_nacht/"zustand" (M: nacht.c:180-189) wie test_boulder_zyklen: Fassung, Fenster,
// erledigt_bis, fest, gesendet_beginn, gesendet_aufwachen, gesendet_schlaf.
static void prv_b26(const NachtBild *bild, uint32_t fenster, uint32_t erledigt_bis) {
  uint8_t z[18] = { 1 };
  memcpy(&z[1], &fenster, 4);
  memcpy(&z[5], &erledigt_bis, 4);
  z[9] = 2;
  memcpy(&z[10], &bild->beginn_utc, 4);
  memcpy(&z[14], &bild->aufwachen, 2);
  memcpy(&z[16], &bild->schlaf_minuten, 2);
  SettingsFile f;
  cl_must_pass(settings_file_open(&f, "boulder_nacht", 2048));
  cl_must_pass(settings_file_set(&f, "zustand", sizeof("zustand"), z, sizeof(z)));
  cl_must_pass(settings_file_set(&f, "letzte", sizeof("letzte"), bild, sizeof(*bild)));
  settings_file_close(&f);
}

void test_boulder_naechte__saat_beim_wechsel(void) {
  // b26 hat die Nacht des Fensters davor abgeschlossen; das Update kommt am Abend.
  prv_lade("nacht-2026-09-26");
  NachtBild bild = s_ref.bild;
  const time_t fenster = (time_t)bild.beginn_utc;
  prv_b26(&bild, fenster, fenster + bild.aufwachen * PBL_SEC_PER_MIN);
  prv_beginne_vor(fenster + PBL_SEC_PER_DAY);
  cl_assert_equal_i(prv_liste(), 1);
  cl_assert_equal_i(s_folge[0].tag, prv_wachtag(fenster + bild.aufwachen * PBL_SEC_PER_MIN, CEST));
  cl_assert_equal_i(s_folge[0].hrv, bild.hrv);
  cl_assert_equal_i(s_folge[0].ruhepuls, bild.ruhepuls);
  cl_assert_equal_i(s_app_naechte->min_ruhe, bild.schlaf_minuten);
  // Die erste Nacht von V2 kommt dazu, und beides steht nach einem Neustart noch da.
  prv_fenster("nacht-2026-09-29", fenster + PBL_SEC_PER_DAY, 20 * 60);
  cl_assert_equal_i(prv_liste(), 2);
  prv_neustart_um(rtc_get_time() + PBL_SEC_PER_HOUR);
  cl_assert_equal_i(prv_liste(), 2);
  cl_assert_equal_i(s_folge[0].ruhepuls, bild.ruhepuls);
  // Ein zweiter Start (boulder_v2 hat jetzt einen Kopf) saet nicht noch einmal.
  cl_assert_equal_i(s_naechte_saetze, 2);

  // Gegenprobe 1: b26s Bild nicht fertig - keine Saat. Das Update kommt ein Fenster spaeter, so
  // liegt die Nacht vor dem, was V2 rechnet; mit fertigem Bild saet es dort (Kontrolle).
  for (int fertig = 1; fertig >= 0; fertig--) {
    test_boulder_naechte__cleanup();
    test_boulder_naechte__initialize();
    bild.fertig = fertig;
    prv_b26(&bild, fenster, 0);
    prv_beginne_vor(fenster + 2 * PBL_SEC_PER_DAY);
    cl_assert_equal_i(prv_liste(), fertig);
  }

  // Gegenprobe 2: Update im selben Fenster, b26 hatte die Nacht noch nicht endgueltig (fertig,
  // 40 min nach dem Aufwachen). V2 rechnet sie selbst (D14) und traegt sie ein, wenn sie
  // endgueltig wird - mit V2s Werten, genau einmal; vorher steht nichts in der Liste.
  test_boulder_naechte__cleanup();
  test_boulder_naechte__initialize();
  bild.fertig = true;
  prv_b26(&bild, fenster, 0);
  prv_lade("nacht-2026-09-26");
  const int u = bild.aufwachen + 40;
  prv_neustart_in_der_nacht(u);
  cl_assert_equal_i(prv_liste(), 0);
  s_hrv_naechstes = s_ref.hrv_anzahl;  // b26s HRV-Fenster liegen hinter dem Update
  prv_nacht(u + 2, 20 * 60);
  cl_assert_equal_i(prv_liste(), 1);
  const NachtDlsRecord *satz = prv_satz(fenster);
  NachtBild v2;
  cl_assert(boulder_nacht_letzte(&v2));
  printf("\nSaat: b26 %d min Ruhepuls %d HRV %d; V2 rechnet dieselbe Nacht %d min Ruhepuls %d "
         "HRV %d (Satz %s)\n", bild.schlaf_minuten, bild.ruhepuls, bild.hrv, v2.schlaf_minuten,
         v2.ruhepuls, v2.hrv, satz ? "ja" : "nein");
  cl_assert_equal_i(s_folge[0].ruhepuls, v2.ruhepuls);
  cl_assert_equal_i(s_folge[0].hrv, v2.hrv);
  cl_assert_equal_i(s_folge[0].tag, prv_wachtag(fenster + v2.aufwachen * PBL_SEC_PER_MIN, CEST));
}
