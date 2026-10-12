/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2, Schritt 3, AP4: Herzband und Zonen von heute fuer die Health-App (Spezifikation
// Schritt 3, 3.8.2, Entscheid F8 = A; S6; 9.1: T4.4, dazu T2.3 und T2.4 fuer S6). Am Ende die Woche
// im Umfeld der ganzen Variante (S7, Ablage und Laden in speicher.c); was eine Saeule gegen Pebbles
// Tageswert zeigt, prueft test_boulder_woche mit Pebbles echtem activity.c.
//
// ECHT wie in test_boulder_schnittstelle (schnittstelle_umfeld.h): die ganze Variante, S6 mit
// Pebbles Pruefung der Rechte. Die Minuten laufen DURCH DIE GLUE-KOPIE (activity_algorithm_minute_
// handler): Sie schreibt sie in ihren Puffer und mit dem Callback "Minutendatei" in Pebbles echte
// Minutendatei (settings_file.c) - genau das liest "Herz nachrechnen" (D18). Nur Bewegung und Puls
// je Minute kommen aus dem Test (--wrap boulder_kalg_*, Pebbles Puls-Median im Umfeld).
// ORAKEL (orakel_herz.h, erzeugt aus Boulder 34fc13cb): Grenzen und Zonen mit Boulders pulsZonen und
// pulsZone; Band und Zonen ueber genau die Minutenreihen, die hier durch die Uhr laufen.
// GEZAEHLT: Dateien und Heap im Minutenteil und im Durchlauf, settings_file_each und die Lesezugriffe
// auf den Flash (--wrap).

#include "schnittstelle_umfeld.h"

#include "orakel_herz.h"
#include "pbl/services/activity/boulder/kraepelin/kraepelin_algorithm.h"
#include "pbl/services/settings/settings_raw_iter.h"

// =============================================================================================
// Bewegung je Minute statt Messung: gehend, damit weder Schlaf noch eine Velofahrt entsteht.

static uint8_t s_vorgabe_schritte;
static uint16_t s_vorgabe_vmc;
static uint8_t s_vorgabe_lage;

uint32_t __wrap_boulder_kalg_analyze_samples(KAlgState *state, AccelRawData *data,
                                             uint32_t num_samples, uint32_t *consumed_samples) {
  *consumed_samples = num_samples;
  const uint32_t schritte = s_vorgabe_schritte;
  s_vorgabe_schritte = 0;
  return schritte;
}

void __wrap_boulder_kalg_minute_stats(KAlgState *state, uint16_t *vmc, uint8_t *orientation,
                                      bool *still) {
  *vmc = s_vorgabe_vmc;
  *orientation = s_vorgabe_lage;
  *still = false;
}

// =============================================================================================
// Gezaehlt: der Durchlauf von "Herz nachrechnen" (D18) und was er liest

static int s_durchlaeufe;   //< boulder_kraepelin_minuten
static int s_each;          //< settings_file_each im Durchlauf
static int s_flash_lesen;   //< flash_read_bytes, solange s_flash_zaehlen
static uint32_t s_flash_bytes;
static bool s_flash_zaehlen;

bool __real_boulder_kraepelin_minuten(time_t von_utc, time_t bis_utc, BoulderJeMinute je_minute,
                                      void *kontext);
bool __wrap_boulder_kraepelin_minuten(time_t von_utc, time_t bis_utc, BoulderJeMinute je_minute,
                                      void *kontext) {
  s_durchlaeufe++;
  const bool vorher = s_zaehlen;
  s_zaehlen = true;
  s_flash_zaehlen = true;
  const bool r = __real_boulder_kraepelin_minuten(von_utc, bis_utc, je_minute, kontext);
  s_flash_zaehlen = false;
  s_zaehlen = vorher;
  return r;
}

status_t __real_settings_file_each(SettingsFile *file, SettingsFileEachCallback cb, void *context);
status_t __wrap_settings_file_each(SettingsFile *file, SettingsFileEachCallback cb, void *context) {
  s_each += s_zaehlen;
  return __real_settings_file_each(file, cb, context);
}

void __real_flash_read_bytes(uint8_t *buffer, uint32_t start_addr, uint32_t buffer_size);
void __wrap_flash_read_bytes(uint8_t *buffer, uint32_t start_addr, uint32_t buffer_size) {
  if (s_flash_zaehlen) {
    s_flash_lesen++;
    s_flash_bytes += buffer_size;
  }
  __real_flash_read_bytes(buffer, start_addr, buffer_size);
}

// Was "woche" in boulder_v2 schreibt.
static int s_woche_saetze;
status_t __real_settings_file_set(SettingsFile *file, const void *key, size_t key_len,
                                  const void *val, size_t val_len);
status_t __wrap_settings_file_set(SettingsFile *file, const void *key, size_t key_len,
                                  const void *val, size_t val_len) {
  if (file->iter.file_name && !strcmp(file->iter.file_name, SPEICHER_DATEI) && key_len == 5 &&
      !memcmp(key, "woche", 5)) {
    // Nur Woche und Spiegel, sonst nichts.
    cl_assert_equal_i(val_len, sizeof(BoulderWoche) + sizeof(BoulderSchrittSpiegel));
    s_woche_saetze++;
  }
  return __real_settings_file_set(file, key, key_len, val, val_len);
}

// =============================================================================================
// Hilfen

#define TAG_NULL 946684800  // 1.1.2000 0:00, Tag 0

// Europe/Zurich 2026 wie die Uhr sie kennt: MEZ, Sommerzeit vom 29.3. 1:00 bis 25.10. 1:00 UTC.
static void prv_zone_zuerich(void) {
  TimezoneInfo tz = { .tm_zone = "CE*T", .tm_gmtoff = PBL_SEC_PER_HOUR,
                      .dst_start = 1774746000, .dst_end = 1792890000 };
  time_util_update_timezone(&tz);
}

// Ein Neustart: Pebble beendet den Tracking-Teil (D13 schreibt dabei nur volle Gruppen, wie das
// Original). Die Dateien bleiben, alles Statische beginnt neu. Einen Absturz ohne dieses Ende kann
// das Umfeld nicht stellen (init verlangt einen beendeten Tracking-Teil); hier schreibt D13 nichts,
// was nicht schon in der Datei steht, denn weder Schlaf noch eine Fahrt halten Minuten zurueck.
static void prv_neustart(void) {
  prv_kernel_bg();
  activity_algorithm_deinit();
  prv_boot();
}

// Eine Minute ab `beginn` wie auf der Uhr: Beschleunigung, Pebbles Minutenlauf mit der Kopie (sie
// schreibt den Satz in ihren Puffer und gibt ihn an den Minutenteil der Variante), KernelBG, der
// eigene Takt zur Sekunde 10, KernelBG.
static void prv_kopie_minute(time_t beginn, uint8_t puls) {
  s_median_puls = puls;
  s_vorgabe_schritte = 95;
  s_vorgabe_vmc = 2600;
  s_vorgabe_lage = (uint8_t)(0x41 + (beginn / 60) % 4);
  rtc_set_time(beginn + 30);
  AccelRawData leer[KALG_SAMPLE_HZ] = {};
  activity_algorithm_handle_accel(leer, ARRAY_LENGTH(leer), 0);
  rtc_set_time(beginn + PBL_SEC_PER_MIN);
  AlgMinuteRecord satz = {};
  s_zaehlen = true;
  activity_algorithm_minute_handler(beginn + PBL_SEC_PER_MIN, &satz);
  s_zaehlen = false;
  activity_sessions_prv_minute_handler(beginn + PBL_SEC_PER_MIN);
  prv_kernel_bg();
  rtc_set_time(beginn + PBL_SEC_PER_MIN + 10);
  boulder_dauerteil_takt(beginn + PBL_SEC_PER_MIN + 10);
  prv_kernel_bg();
}

static uint16_t prv_tag(time_t utc) {
  return (uint16_t)((time_utc_to_local(utc) - TAG_NULL) / PBL_SEC_PER_DAY);
}

// Band und Zonen, wie die App sie ueber S6 sieht, gegen das Orakel. Jeder Platz mit Messung muss
// eine Halbstunde des Orakels sein und umgekehrt.
static void prv_wie_orakel(const char *name, const OrakelHerzErgebnis *e) {
  cl_assert(prv_s6());
  const BoulderHerz *h = s_app_herz;
  int belegt = 0;
  for (int k = 0; k < HERZ_HALBSTUNDEN; k++) {
    belegt += (h->n[k] != 0);
  }
  printf("\n%s (Maximalpuls %d): neueste %lu/%lu, Grenzen %d/%d/%d, Tag %d/%d, Zonen %d %d %d %d "
         "(Orakel %d %d %d %d), Halbstunden %d/%d",
         name, e->maxpuls, (unsigned long)h->neueste, (unsigned long)e->neueste, h->grenzen[0],
         h->grenzen[1], h->grenzen[2], h->zonen_tag, e->zonen_tag, h->zonen_min[0],
         h->zonen_min[1], h->zonen_min[2], h->zonen_min[3], e->zonen_min[0], e->zonen_min[1],
         e->zonen_min[2], e->zonen_min[3], belegt, e->halbstunden_zahl);
  cl_assert_equal_i(h->neueste, e->neueste);
  cl_assert_equal_m(h->grenzen, e->grenzen, sizeof(h->grenzen));
  cl_assert_equal_i(h->zonen_tag, e->zonen_tag);
  for (int z = 0; z < HERZ_ZONEN; z++) {
    cl_assert_equal_i(h->zonen_min[z], e->zonen_min[z]);
  }
  cl_assert_equal_i(belegt, e->halbstunden_zahl);
  for (int i = 0; i < e->halbstunden_zahl; i++) {
    const OrakelHalbstunde *o = &e->halbstunden[i];
    const int platz = (int)(o->halbstunde % HERZ_HALBSTUNDEN);
    if (h->min[platz] != o->min || h->max[platz] != o->max || h->mittel[platz] != o->mittel ||
        h->n[platz] != o->n) {
      printf("\n  Halbstunde %lu: %d/%d/%d/%d, Orakel %d/%d/%d/%d", (unsigned long)o->halbstunde,
             h->min[platz], h->max[platz], h->mittel[platz], h->n[platz], o->min, o->max,
             o->mittel, o->n);
    }
    cl_assert_equal_i(h->n[platz], o->n);
    cl_assert_equal_i(h->min[platz], o->min);
    cl_assert_equal_i(h->max[platz], o->max);
    cl_assert_equal_i(h->mittel[platz], o->mittel);
  }
  cl_assert_equal_m(h->reserve, ((uint8_t[3]){ 0 }), sizeof(h->reserve));
}

static const OrakelHerzReihe *prv_reihe(const char *name) {
  for (size_t i = 0; i < ARRAY_LENGTH(ORAKEL_HERZ_REIHEN); i++) {
    if (!strcmp(ORAKEL_HERZ_REIHEN[i].name, name)) {
      return &ORAKEL_HERZ_REIHEN[i];
    }
  }
  cl_fail("Reihe fehlt im Orakel");
  return NULL;
}

// Uhr kurz vor der Reihe, ein sauberer Neustart dort (der Tracking-Teil beginnt mit leerem Puffer),
// "Laden" und das erste Nachrechnen (leer). Maximalpuls fuer die Grenzen; ohne Nacht ist der
// Ruhepuls-Median 0 (wie das Orakel: 60).
static void prv_vor_der_reihe(const OrakelHerzReihe *r, uint16_t maxpuls) {
  prv_zone_zuerich();
  s_kieselsport_maxpuls = maxpuls;
  rtc_set_time(r->beginn_utc - 2 * PBL_SEC_PER_MIN);
  prv_neustart();
  prv_kernel_bg();
  boulder_dauerteil_takt(rtc_get_time());
  prv_kernel_bg();
  cl_assert(boulder_herz_bereit());
}

// Die Reihe Minute fuer Minute durch die Kopie. Prueft dabei, dass der Minutenteil weder eine Datei
// oeffnet noch Heap holt (S1-T7.1), und dass die Zonen mit dem ersten Puls eines neuen Ortstags neu
// beginnen.
static void prv_reihe_durch_die_kopie(const OrakelHerzReihe *r) {
  s_gezaehlt_dateien = 0;
  s_gezaehlt_heap = 0;
  uint16_t tag_vorher = 0;
  int mitternaechte = 0;
  for (int i = 0; i < r->minuten; i++) {
    const time_t utc = (time_t)r->beginn_utc + (time_t)r->versatz_min[i] * PBL_SEC_PER_MIN;
    cl_assert(i == 0 || r->versatz_min[i] == r->versatz_min[i - 1] + 1);
    prv_kopie_minute(utc, r->puls[i]);
    const uint16_t tag = prv_tag(utc);
    if (tag_vorher && tag != tag_vorher) {
      // Um Mitternacht (Ortszeit) gehen die Zonen auf 0: Nach der ersten Minute des Tags zaehlt nur
      // sie.
      cl_assert(prv_s6());
      cl_assert_equal_i(s_app_herz->zonen_tag, tag);
      const int summe = s_app_herz->zonen_min[0] + s_app_herz->zonen_min[1] +
                        s_app_herz->zonen_min[2] + s_app_herz->zonen_min[3];
      cl_assert_equal_i(summe, r->puls[i] ? 1 : 0);
      mitternaechte++;
    }
    tag_vorher = tag;
  }
  cl_assert_equal_i(s_gezaehlt_dateien, 0);
  cl_assert_equal_i(s_gezaehlt_heap, 0);
  printf("\n%s: %d Minuten durch die Kopie, %d Mitternacht(e)", r->name, r->minuten,
         mitternaechte);
}

// =============================================================================================

void test_boulder_herz__initialize(void) {
  prv_umfeld_beginn();
  s_durchlaeufe = 0;
  s_each = 0;
  s_flash_lesen = 0;
  s_flash_bytes = 0;
  s_flash_zaehlen = false;
  s_woche_saetze = 0;
}

void test_boulder_herz__cleanup(void) {
  prv_umfeld_ende();
}

// Grenzen und Zonen wie Boulders pulsZonen und pulsZone, ohne Ruhepuls-Median mit 60.
void test_boulder_herz__grenzen_und_zonen_wie_boulder(void) {
  int proben = 0;
  for (size_t i = 0; i < ARRAY_LENGTH(ORAKEL_HERZ_GRENZEN); i++) {
    const OrakelHerzGrenzen *o = &ORAKEL_HERZ_GRENZEN[i];
    uint8_t g[HERZ_ZONEN - 1];
    boulder_herz_grenzen(o->median, o->max, g);
    cl_assert_equal_m(g, o->grenzen, sizeof(g));
    for (int k = 0; k < o->proben_zahl; k++) {
      cl_assert_equal_i(boulder_herz_zone(o->proben[k].bpm, g), o->proben[k].zone);
      proben++;
    }
  }
  printf("\nGrenzen wie Boulder: %d Faelle, %d Proben, 0 Abweichungen",
         (int)ARRAY_LENGTH(ORAKEL_HERZ_GRENZEN), proben);
}

// Vor dem ersten Nachrechnen ist das Band nicht da (S6 false, leer). Eine Minute, die vor "Laden"
// kommt, zaehlt der laufende Betrieb nicht - das Nachrechnen findet sie im Puffer der Kopie.
void test_boulder_herz__vor_dem_nachrechnen_nicht_da(void) {
  memset(s_app_herz, 0xAB, sizeof(*s_app_herz));
  cl_assert(!prv_s6());
  const BoulderHerz leer = { 0 };
  cl_assert_equal_m(s_app_herz, &leer, sizeof(leer));
  // Die erste Minute: Minutenteil vor "Laden", danach KernelBG mit "Laden" und dem Nachrechnen.
  const time_t minute = s_jetzt - (s_jetzt % PBL_SEC_PER_MIN);
  const uint32_t laeufe = prv_laeufe("Boulder Herz nachrechnen");
  prv_kopie_minute(minute, 77);
  cl_assert_equal_i(prv_laeufe("Boulder Herz nachrechnen"), laeufe + 1);
  cl_assert(prv_s6());
  const int platz = (int)((minute / HERZ_HALBSTUNDE_S) % HERZ_HALBSTUNDEN);
  cl_assert_equal_i(s_app_herz->neueste, minute / HERZ_HALBSTUNDE_S);
  cl_assert_equal_i(s_app_herz->n[platz], 1);
  cl_assert_equal_i(s_app_herz->min[platz], 77);
  cl_assert_equal_i(s_app_herz->mittel[platz], 77);
  // Ohne Nacht: Ruhepuls 60, Maximalpuls 179 aus dem Umfeld -> 108/131/155; 77 ist leicht.
  cl_assert_equal_i(s_app_herz->zonen_min[0], 1);
  cl_assert_equal_i(s_app_herz->grenzen[0], 108);
}

// T4.4: Minuten mit Puls ueber Mitternacht, mit Luecken (Minuten ohne Messung, zwei Stunden ohne)
// und ueber beide Zeitumstellungen. Der laufende Betrieb gibt Band und Zonen wie das Orakel ueber
// dieselben Minuten. Dann aendern sich die Grenzen (anderer Maximalpuls): Der Takt stoesst das
// Nachrechnen an, und es zaehlt dieselben Minuten mit den neuen Grenzen - das Band bleibt Byte
// fuer Byte, was der laufende Betrieb hatte.
void test_boulder_herz__band_und_zonen_wie_boulder(void) {
  static const char *const s_reihen[] = { "mitternacht", "herbst", "fruehling", "dreissig" };
  for (size_t i = 0; i < ARRAY_LENGTH(s_reihen); i++) {
    const OrakelHerzReihe *r = prv_reihe(s_reihen[i]);
    prv_vor_der_reihe(r, r->ergebnis.maxpuls);
    const uint32_t nachgerechnet = prv_laeufe("Boulder Herz nachrechnen");
    prv_reihe_durch_die_kopie(r);
    // Kein Nachrechnen unterwegs: Die Grenzen blieben, das Band ist der laufende Betrieb.
    cl_assert_equal_i(prv_laeufe("Boulder Herz nachrechnen"), nachgerechnet);
    prv_wie_orakel(r->name, &r->ergebnis);
    BoulderHerz laufend;
    memcpy(&laufend, s_app_herz, sizeof(laufend));

    s_kieselsport_maxpuls = r->ergebnis_neu.maxpuls;
    boulder_dauerteil_takt(rtc_get_time());
    prv_kernel_bg();
    cl_assert_equal_i(prv_laeufe("Boulder Herz nachrechnen"), nachgerechnet + 1);
    prv_wie_orakel(r->name, &r->ergebnis_neu);
    cl_assert_equal_m(s_app_herz->min, laufend.min, sizeof(laufend.min) * 4);
    cl_assert_equal_i(s_app_herz->neueste, laufend.neueste);
  }
}

// Spruenge im laufenden Betrieb (Uhr aus, eine Luecke ueber einen Tag): Die uebersprungenen
// Halbstunden werden leer, nach mehr als 24 h das ganze Band. Die Minuten kommen hier direkt in den
// Minutenteil - Pebbles Minutendatei legt die Minuten einer Gruppe lueckenlos ab, solche Spruenge
// kennt sie nur ueber einen Neustart des Tracking-Teils.
void test_boulder_herz__spruenge_im_laufenden_betrieb(void) {
  const OrakelHerzReihe *r = prv_reihe("spruenge");
  prv_vor_der_reihe(r, r->ergebnis.maxpuls);
  for (int i = 0; i < r->minuten; i++) {
    prv_minute((time_t)r->beginn_utc, (int)r->versatz_min[i], 2600, r->puls[i], true);
  }
  prv_wie_orakel(r->name, &r->ergebnis);
}

// Gegenpruefung K5: Geht die Uhr mehr als 24 h zurueck (eine falsche Zukunftszeit wird richtig
// gestellt), liegen Band und Zonen in der Zukunft, und bis die Uhr sie einholt, zaehlte keine
// Minute. Der Takt merkt es und rechnet nach; danach zaehlt die richtige Zeit wieder.
void test_boulder_herz__uhr_weit_zurueck_rechnet_nach(void) {
  prv_kernel_bg();
  cl_assert(boulder_herz_bereit());
  const time_t zukunft = s_jetzt + 3 * PBL_SEC_PER_DAY;
  const time_t beginn_z = zukunft - (zukunft % HERZ_HALBSTUNDE_S);
  for (int m = 0; m < 30; m++) {
    prv_minute(beginn_z, m, 2600, 80, true);
  }
  // Die Uhr wieder richtig: zwei Stunden mit Puls 120.
  const time_t jetzt = s_jetzt - (s_jetzt % HERZ_HALBSTUNDE_S);
  for (int m = 0; m < 120; m++) {
    prv_minute(jetzt, m, 2600, 120, true);
  }
  cl_assert(prv_s6());
  cl_assert_equal_i(s_app_herz->neueste, (uint32_t)((jetzt + 119 * 60) / HERZ_HALBSTUNDE_S));
  cl_assert_equal_i(s_app_herz->zonen_tag, prv_tag(jetzt));
  // Hoechstens die erste Minute fehlt: Sie kam vor dem Takt, der das Nachrechnen ausloeste, und
  // liegt hier (ohne die Kopie) in keiner Datei.
  const int summe = s_app_herz->zonen_min[0] + s_app_herz->zonen_min[1] + s_app_herz->zonen_min[2] +
                    s_app_herz->zonen_min[3];
  cl_assert(summe >= 119);
}

// Halbstunden ohne Puls (Uhr nicht am Arm, Sensor ohne Wert) zaehlen fuer das Band trotzdem: Die
// juengste Halbstunde ist die der juengsten Minute. Nach 24 h ohne Puls ist das Band leer, nicht
// mit Werten von gestern - im laufenden Betrieb wie nachgerechnet.
void test_boulder_herz__halbstunden_ohne_puls_verdraengen(void) {
  prv_kernel_bg();
  const time_t beginn = s_jetzt - (s_jetzt % HERZ_HALBSTUNDE_S);
  for (int m = 0; m < 30; m++) {
    prv_kopie_minute(beginn + m * PBL_SEC_PER_MIN, 80);
  }
  cl_assert(prv_s6());
  cl_assert_equal_i(s_app_herz->n[(beginn / HERZ_HALBSTUNDE_S) % HERZ_HALBSTUNDEN], 30);
  const int ohne = 24 * 60 + 30;
  for (int m = 30; m < 30 + ohne; m++) {
    prv_kopie_minute(beginn + m * PBL_SEC_PER_MIN, 0);
  }
  const uint32_t juengste = (uint32_t)((beginn + (30 + ohne - 1) * PBL_SEC_PER_MIN) /
                                       HERZ_HALBSTUNDE_S);
  for (int nachgerechnet = 0; nachgerechnet <= 1; nachgerechnet++) {
    cl_assert(prv_s6());
    int belegt = 0;
    for (int k = 0; k < HERZ_HALBSTUNDEN; k++) {
      belegt += (s_app_herz->n[k] != 0);
    }
    printf("\nohne Puls %s: neueste %lu (soll %lu), %d Halbstunden mit Puls",
           nachgerechnet ? "nachgerechnet" : "laufend", (unsigned long)s_app_herz->neueste,
           (unsigned long)juengste, belegt);
    cl_assert_equal_i(s_app_herz->neueste, juengste);
    cl_assert_equal_i(belegt, 0);
    s_kieselsport_maxpuls = 150;
    boulder_dauerteil_takt(rtc_get_time());
    prv_kernel_bg();
  }
}

// T4.4: Nachrechnen nach einem Neustart ("sys reset"). Was in Pebbles Minutendatei steht, kommt
// zurueck. Die Minuten nach der letzten vollen Gruppe zu 15 stehen beim Neustart nur im Puffer der
// Kopie; Pebble schreibt sie auch beim sauberen Ende nicht (D13 wie das Original), also fehlen sie
// danach auch im Band (Reihe "rest": 7 Minuten).
void test_boulder_herz__nachrechnen_nach_neustart(void) {
  static const char *const s_reihen[] = { "mitternacht", "rest" };
  for (size_t i = 0; i < ARRAY_LENGTH(s_reihen); i++) {
    const OrakelHerzReihe *r = prv_reihe(s_reihen[i]);
    prv_vor_der_reihe(r, r->ergebnis.maxpuls);
    prv_reihe_durch_die_kopie(r);
    prv_wie_orakel(r->name, &r->ergebnis);
    // Neustart nach der letzten Minute, dann "Laden" und das Nachrechnen.
    prv_neustart();
    cl_assert(!prv_s6());
    prv_kernel_bg();
    printf("\nnach dem Neustart:");
    prv_wie_orakel(r->name, &r->ergebnis_datei);
  }
}

// Nachrechnen: ein Durchlauf durch Pebbles Minutendatei, ohne Heap ausser dem Oeffnen der Datei
// (der Name, settings_file.c). Mit dem Ballast des Pruefbaus (activity_algorithm_test_fill_minute_
// file, wie in QEMU) gezaehlt: Lesezugriffe auf den Flash gegen Pebbles Minutenverlauf ueber 24 h
// (Spezifikation 2.6.1 und 13: "so viel wie ein Aufruf ueber 24 h").
void test_boulder_herz__nachrechnen_ein_durchlauf_ohne_heap(void) {
  prv_kernel_bg();
  cl_assert(activity_algorithm_test_fill_minute_file());
  uint32_t saetze = 0, bytes = 0, minuten = 0;
  cl_assert(activity_algorithm_minute_file_info(false, &saetze, &bytes, &minuten));

  s_each = 0;
  s_gezaehlt_dateien = 0;
  s_gezaehlt_heap = 0;
  s_gezaehlt_heap_ausser_oeffnen = 0;
  s_flash_lesen = 0;
  s_flash_bytes = 0;
  const int durchlaeufe = s_durchlaeufe;
  boulder_herz_nachrechnen();
  prv_kernel_bg();
  cl_assert_equal_i(s_durchlaeufe, durchlaeufe + 1);
  cl_assert_equal_i(s_each, 1);
  cl_assert_equal_i(s_gezaehlt_dateien, 1);
  cl_assert_equal_i(s_gezaehlt_heap_ausser_oeffnen, 0);
  const int lesen_herz = s_flash_lesen;
  const uint32_t bytes_herz = s_flash_bytes;

  // Pebbles Minutenverlauf ueber 24 h, derselbe Lesecode der Kopie.
  static HealthMinuteData s_verlauf[24 * 60];
  uint32_t anzahl = ARRAY_LENGTH(s_verlauf);
  time_t von = rtc_get_time() - 24 * PBL_SEC_PER_HOUR;
  s_flash_lesen = 0;
  s_flash_bytes = 0;
  s_flash_zaehlen = true;
  cl_assert(activity_algorithm_get_minute_history(s_verlauf, &anzahl, &von));
  s_flash_zaehlen = false;

  cl_assert(prv_s6());
  int belegt = 0;
  for (int k = 0; k < HERZ_HALBSTUNDEN; k++) {
    belegt += (s_app_herz->n[k] != 0);
  }
  printf("\nBallast: %lu Saetze; Nachrechnen %d Lesezugriffe / %lu B (Heap beim Oeffnen %d), "
         "Minutenverlauf 24 h %d / %lu B; Band %d Halbstunden",
         (unsigned long)saetze, lesen_herz, (unsigned long)bytes_herz, s_gezaehlt_heap,
         s_flash_lesen, (unsigned long)s_flash_bytes, belegt);
  cl_assert(belegt > 0);
}

// Das Nachrechnen haengt nicht von der Reihenfolge der Saetze in der Minutendatei ab. Pebble legt
// sie gewoehnlich in der Zeitfolge ab; eine gestellte Uhr oder ein neu geschriebener Schluessel
// legt aeltere hinter juengere. Hier stehen die Gruppen einer Reihe rueckwaerts und gemischt in der
// Datei, im Format der Kopie (activity_algorithm.h).
void test_boulder_herz__nachrechnen_unabhaengig_von_der_reihenfolge(void) {
  const OrakelHerzReihe *r = prv_reihe("dreissig");
  prv_vor_der_reihe(r, r->ergebnis.maxpuls);
  const int gruppen = r->minuten / ALG_MINUTES_PER_FILE_RECORD;
  SettingsFile f;
  cl_must_pass(settings_file_open(&f, "activity_sleep", ALG_MINUTE_DATA_FILE_LEN));
  for (int k = 0; k < gruppen; k++) {
    // rueckwaerts, dabei je zwei Nachbarn vertauscht
    const int j = ((k ^ 1) < gruppen) ? (k ^ 1) : k;
    const int g = gruppen - 1 - j;
    const time_t beginn = (time_t)r->beginn_utc + (time_t)g * ALG_MINUTES_PER_FILE_RECORD * 60;
    AlgMinuteFileRecord satz = {
      .hdr = { .version = ALG_MINUTE_FILE_RECORD_VERSION, .time_utc = (uint32_t)beginn,
               .time_local_offset_15_min = (int16_t)((time_utc_to_local(beginn) - beginn) / 900),
               .sample_size = sizeof(AlgMinuteFileSample),
               .num_samples = ALG_MINUTES_PER_FILE_RECORD },
    };
    for (int i = 0; i < ALG_MINUTES_PER_FILE_RECORD; i++) {
      satz.samples[i].v5_fields.vmc = 2600;
      satz.samples[i].heart_rate_bpm = r->puls[g * ALG_MINUTES_PER_FILE_RECORD + i];
    }
    const uint32_t schluessel = (uint32_t)(beginn / (ALG_MINUTES_PER_FILE_RECORD * 60));
    cl_must_pass(settings_file_set(&f, &schluessel, sizeof(schluessel), &satz, sizeof(satz)));
  }
  settings_file_close(&f);
  rtc_set_time((time_t)r->beginn_utc + (time_t)r->minuten * 60 + 10);
  boulder_herz_nachrechnen();
  prv_kernel_bg();
  printf("\nDatei in gemischter Reihenfolge (%d Gruppen):", gruppen);
  prv_wie_orakel(r->name, &r->ergebnis);
}

// Ohne Tracking-Teil (am Kabel, Tracking aus) kann das Nachrechnen nicht lesen: Es bleibt faellig,
// der Takt versucht es jede Minute wieder, und sobald der Tracking-Teil da ist, laeuft es.
void test_boulder_herz__ohne_tracking_teil_spaeter(void) {
  activity_algorithm_deinit();
  prv_kernel_bg();
  cl_assert(!prv_s6());
  const uint32_t laeufe = prv_laeufe("Boulder Herz nachrechnen");
  cl_assert(laeufe >= 1);
  boulder_dauerteil_takt(rtc_get_time());
  prv_kernel_bg();
  cl_assert_equal_i(prv_laeufe("Boulder Herz nachrechnen"), laeufe + 1);
  cl_assert(!prv_s6());
  cl_assert(activity_algorithm_init(&s_rate));
  boulder_dauerteil_takt(rtc_get_time());
  prv_kernel_bg();
  cl_assert(prv_s6());
  // Danach ruht es, solange die Grenzen bleiben.
  boulder_dauerteil_takt(rtc_get_time());
  prv_kernel_bg();
  cl_assert_equal_i(prv_laeufe("Boulder Herz nachrechnen"), laeufe + 2);
}

// T2.3: S6 prueft den Puffer der App (ganz im App-RAM), mit Systemrechten nicht. T2.4 zaehlt das
// Umfeld (eine Sperre je Syscall, keine Datei, kein Heap).
void test_boulder_herz__puffer_ausserhalb_der_app_beenden_sie(void) {
  prv_kernel_bg();
  static uint8_t kern[512];
  prv_app_beginn();
  cl_assert_passert(sys_boulder_herz((BoulderHerz *)kern));
  prv_app_abbruch();
  prv_app_beginn();
  cl_assert_passert(
      sys_boulder_herz((BoulderHerz *)&s_app_ram[sizeof(s_app_ram) - sizeof(BoulderHerz) + 1]));
  prv_app_abbruch();
  prv_app_beginn();
  cl_assert(sys_boulder_herz((BoulderHerz *)&s_app_ram[sizeof(s_app_ram) - sizeof(BoulderHerz)]));
  prv_app_ende();
  const int vorher = s_puffer_geprueft;
  BoulderHerz h;
  cl_assert(sys_boulder_herz(&h));
  cl_assert_equal_i(s_puffer_geprueft, vorher);
  cl_assert(prv_s6());
  cl_assert_equal_i(s_puffer_geprueft, vorher + 1);
  // S7 ebenso.
  prv_app_beginn();
  cl_assert_passert(sys_boulder_woche((BoulderWoche *)kern));
  prv_app_abbruch();
  prv_app_beginn();
  cl_assert_passert(
      sys_boulder_woche((BoulderWoche *)&s_app_ram[sizeof(s_app_ram) - sizeof(BoulderWoche) + 1]));
  prv_app_abbruch();
  // Gezaehlt sind S6, die zwei abgebrochenen S7 und dieses.
  cl_assert(prv_s7());
  cl_assert_equal_i(s_puffer_geprueft, vorher + 4);
}

// =============================================================================================
// Die Woche im Umfeld der Variante (die Saeulen gegen Pebbles echten Tageswert: test_boulder_woche)

static void prv_schritte_minute(time_t beginn, int32_t schritte) {
  s_pebble_schritte = schritte;
  prv_minute(beginn, 0, 2600, 70, true);
}

// S7 vor "Laden" leer und false, danach leer und true. Der Minutenteil spiegelt Pebbles Schritte;
// die erste Minute eines neuen Ortsdatums schiebt den Spiegel als Vortag in die Woche, und die
// Ablage schreibt "woche" gleich. Nach einem Neustart bringt "Laden" Woche und Spiegel zurueck.
void test_boulder_herz__woche_im_umfeld(void) {
  memset(s_app_woche, 0xAB, sizeof(*s_app_woche));
  cl_assert(!prv_s7());
  cl_assert_equal_i(s_app_woche->tag, 0);
  for (int i = 0; i < WOCHE_VORTAGE; i++) {
    cl_assert_equal_i(s_app_woche->schritte[i], WOCHE_KEIN_WERT);
  }
  // 30.9.2026, 23:55 Ortszeit (CEST im Umfeld) bis 0:06.
  const time_t abend = 1790805300;  // 30.9.2026 21:55 UTC
  prv_kernel_bg();
  cl_assert(prv_s7());
  cl_assert_equal_i(s_app_woche->tag, 0);
  const uint16_t tag = (uint16_t)((abend + 2 * PBL_SEC_PER_HOUR - TAG_NULL) / PBL_SEC_PER_DAY);
  for (int m = 0; m < 5; m++) {
    prv_schritte_minute(abend + m * PBL_SEC_PER_MIN, 6000 + m * 50);
  }
  cl_assert_equal_i(boulder_woche_spiegel()->tag, tag);
  cl_assert_equal_i(boulder_woche_spiegel()->schritte, 6200);
  const int saetze_vorher = s_woche_saetze;
  // 0:00: Pebble hat auf 0 gestellt.
  prv_schritte_minute(abend + 5 * PBL_SEC_PER_MIN, 0);
  cl_assert(prv_s7());
  cl_assert_equal_i(s_app_woche->tag, tag);
  cl_assert_equal_i(s_app_woche->schritte[0], 6200);
  for (int i = 1; i < WOCHE_VORTAGE; i++) {
    cl_assert_equal_i(s_app_woche->schritte[i], WOCHE_KEIN_WERT);
  }
  cl_assert_equal_i(s_woche_saetze, saetze_vorher + 1);
  prv_schritte_minute(abend + 6 * PBL_SEC_PER_MIN, 40);
  // Neustart: Woche und Spiegel aus boulder_v2.
  prv_neustart();
  cl_assert(!prv_s7());
  prv_kernel_bg();
  cl_assert(prv_s7());
  cl_assert_equal_i(s_app_woche->tag, tag);
  cl_assert_equal_i(s_app_woche->schritte[0], 6200);
  cl_assert_equal_i(boulder_woche_spiegel()->tag, tag + 1);
  printf("\nWoche im Umfeld: Vortag %d mit %lu Schritten, %d Saetze \"woche\"", tag,
         (unsigned long)s_app_woche->schritte[0], s_woche_saetze);
}
