/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2, Schritt 3, AP2: die Schnittstelle zur Health-App (Spezifikation Schritt 3, 3.1-3.4,
// 3.8.4; 9.1: T2.3, T2.4, T6.2, T6.3). Was echt ist und was Attrappe, steht im Umfeld
// (schnittstelle_umfeld.h); die Naechte (S5, AP3) prueft test_boulder_naechte.

#include "schnittstelle_umfeld.h"

void test_boulder_schnittstelle__initialize(void) {
  prv_umfeld_beginn();
}

void test_boulder_schnittstelle__cleanup(void) {
  prv_umfeld_ende();
}

// =============================================================================================
// T2.3: Puffer und Ueberlauf

void test_boulder_schnittstelle__puffer_ausserhalb_der_app_beenden_sie(void) {
  prv_kernel_bg();
  static uint8_t kern[1024];  // Kernel-RAM aus Sicht der App
  // Ganz ausserhalb.
  prv_app_beginn();
  cl_assert_passert(sys_boulder_nacht((NachtBild *)kern));
  prv_app_abbruch();
  prv_app_beginn();
  cl_assert_passert(sys_boulder_lage((BoulderLage *)kern));
  prv_app_abbruch();
  prv_app_beginn();
  cl_assert_passert(sys_boulder_spo2(0, INT32_MAX, (Spo2Wert *)kern, 1));
  prv_app_abbruch();
  // Ueber das Ende des App-RAMs hinaus: ein Byte zu wenig.
  prv_app_beginn();
  cl_assert_passert(sys_boulder_nacht(
      (NachtBild *)&s_app_ram[sizeof(s_app_ram) - sizeof(NachtBild) + 1]));
  prv_app_abbruch();
  prv_app_beginn();
  cl_assert_passert(sys_boulder_lage(
      (BoulderLage *)&s_app_ram[sizeof(s_app_ram) - sizeof(BoulderLage) + 1]));
  prv_app_abbruch();
  // SpO2: genug Platz fuer 10 Werte, verlangt 11.
  Spo2Wert *zehn = (Spo2Wert *)&s_app_ram[sizeof(s_app_ram) - 10 * sizeof(Spo2Wert)];
  prv_app_beginn();
  cl_assert_passert(sys_boulder_spo2(0, INT32_MAX, zehn, 11));
  prv_app_abbruch();
  // Gerade passend geht.
  cl_assert_equal_i(prv_s3(0, INT32_MAX, 10), 0);
  prv_app_beginn();
  cl_assert_equal_i(sys_boulder_spo2(0, INT32_MAX, zehn, 10), 0);
  prv_app_ende();
}

// Gegenpruefung K3: Ist die Schlange von KernelBG beim ersten init knapp, wartet "Laden" (takt.c).
// Den Takt plant erst "Laden" selbst; holte der Minutenteil es nicht nach, bliebe die Variante bis
// zum naechsten Neustart ohne Nacht, Naechte, Herz und Woche.
void test_boulder_schnittstelle__laden_bei_knapper_schlange_holt_die_minute_nach(void) {
  // Der Boot aus dem Umfeld geht verloren, bevor KernelBG laeuft; der naechste findet die Schlange
  // fast voll.
  activity_algorithm_deinit();
  s_kern_n = 0;
  boulder_dauerteil_test_neustart();
  boulder_messstufe_test_zuruecksetzen();
  boulder_naechte_test_neustart();
  boulder_herz_test_neustart();
  boulder_woche_test_neustart();
  while (KERN_SCHLANGE - s_kern_n >= BOULDER_SCHLANGE_FREI) {
    s_kern_schlange[s_kern_n++] = (Eintrag){prv_fremd_cb, NULL};
  }
  const uint32_t vorher = prv_laeufe("Boulder Laden");
  cl_assert(activity_algorithm_init(&s_rate));
  prv_kernel_bg();
  cl_assert_equal_i(prv_laeufe("Boulder Laden"), vorher);
  cl_assert(!prv_s7());
  // Die naechste Minute reiht es ein.
  const time_t minute = s_jetzt - (s_jetzt % PBL_SEC_PER_MIN);
  rtc_set_time(minute + PBL_SEC_PER_MIN);
  boulder_dauerteil_minute(&(BoulderMinute){.utc = minute, .vmc = 1000, .puls = 70});
  prv_kernel_bg();
  cl_assert_equal_i(prv_laeufe("Boulder Laden"), vorher + 1);
  cl_assert(prv_s7());
}

// Grosse Zahlen (Gegenpruefung, Befund K1): Die Uhr rechnet das letzte Byte der Pruefung in 32 Bit
// (buf + Bytes - 1). Mit max = UINT32_MAX / 8 waeren es 0xFFFFFFF8 Bytes, das letzte Byte laege
// bei buf - 9 im App-RAM, und kopiert wuerden bis zu 96 Werte hinter sein Ende. Geprueft wird darum
// genau, was kopiert wird: hoechstens SPO2_WERTE_MAX Werte.
void test_boulder_schnittstelle__spo2_grosse_zahl_prueft_was_kopiert_wird(void) {
  // Drei Werte in einem eigenen Fenster vor fuenf Tagen.
  const time_t von = s_jetzt - 5 * PBL_SEC_PER_DAY;
  for (int i = 0; i < 3; i++) {
    boulder_spo2_merken(von + i * 600, (uint8_t)(95 + i));
  }
  Spo2Wert *einer = (Spo2Wert *)&s_app_ram[sizeof(s_app_ram) - sizeof(Spo2Wert)];
  const uint32_t grenzen[] = {
    (uint32_t)(UINT32_MAX / sizeof(Spo2Wert)),     // letztes Byte bei buf - 9
    (uint32_t)(UINT32_MAX / sizeof(Spo2Wert)) + 1, // 0 Bytes
    UINT32_MAX,
  };
  for (size_t i = 0; i < ARRAY_LENGTH(grenzen); i++) {
    prv_app_beginn();
    cl_assert_passert(sys_boulder_spo2(von, von + 1800, einer, grenzen[i]));
    prv_app_abbruch();
  }
  // Mit Platz fuer alle 96 geht jede Zahl, und es kommen nur die Werte im Fenster.
  for (size_t i = 0; i < ARRAY_LENGTH(grenzen); i++) {
    cl_assert_equal_i(prv_s3(von, von + 1800, grenzen[i]), 3);
  }
  // Ohne Wunsch keine Pruefung und keine Kopie.
  cl_assert_equal_i(prv_s3(0, INT32_MAX, 0), 0);
}

// Mit Systemrechten (Kern) prueft der Syscall keinen Puffer - wie Pebbles eigene.
void test_boulder_schnittstelle__kern_mit_systemrechten_ohne_pruefung(void) {
  prv_kernel_bg();
  BoulderLage lage;
  cl_assert(sys_boulder_lage(&lage));
  NachtBild bild;
  sys_boulder_nacht(&bild);
  Spo2Wert w[2];
  sys_boulder_spo2(0, INT32_MAX, w, 2);
  cl_assert_equal_i(s_puffer_geprueft, 0);
  // Aus der App wird jeder Puffer geprueft.
  prv_s1();
  prv_s2();
  prv_s3(0, INT32_MAX, 2);
  cl_assert_equal_i(s_puffer_geprueft, 3);
}

// =============================================================================================
// S1: die Nacht ohne Datei

// Nach dem Boot, bevor "Laden" lief: Spaeter, der Auftrag laedt die Datei auf KernelBG (nicht im
// Syscall, nicht unter der Sperre), danach Da mit den Bytes aus der Datei.
void test_boulder_schnittstelle__nacht_nach_dem_boot_erst_spaeter(void) {
  prv_lade("nacht-2026-09-29");
  NachtBild datei = s_ref.bild;
  SettingsFile f;
  cl_must_pass(settings_file_open(&f, "boulder_nacht", 2048));
  cl_must_pass(settings_file_set(&f, "letzte", sizeof("letzte"), &datei, sizeof(datei)));
  settings_file_close(&f);
  s_bild_datei_geoeffnet = 0;
  const uint32_t laeufe = prv_laeufe("Boulder Bild laden");

  // Zehnmal "Spaeter" (2.6.2: nach 1 s erneut, hoechstens 10-mal), ein einziger Auftrag.
  for (int i = 0; i < 10; i++) {
    cl_assert_equal_i(prv_s1(), BoulderLesenSpaeter);
  }
  cl_assert_equal_i(s_app_n, 1);
  cl_assert_equal_i(s_bild_datei_geoeffnet, 0);
  prv_kernel_bg();
  cl_assert(s_bild_datei_geoeffnet >= 1);
  cl_assert_equal_i(prv_laeufe("Boulder Bild laden"), laeufe + 1);
  cl_assert_equal_i(prv_s1(), BoulderLesenDa);
  cl_assert(memcmp(s_app_bild, &datei, sizeof(datei)) == 0);
  // Jetzt ohne Auftrag.
  cl_assert_equal_i(prv_s1(), BoulderLesenDa);
  cl_assert_equal_i(s_app_n, 0);
}

void test_boulder_schnittstelle__nacht_ohne_datei_keine(void) {
  cl_assert_equal_i(prv_s1(), BoulderLesenSpaeter);
  prv_kernel_bg();
  cl_assert_equal_i(prv_s1(), BoulderLesenKeine);
  cl_assert_equal_i(prv_s1(), BoulderLesenKeine);
  cl_assert_equal_i(s_app_n, 0);
}

// Ein Bild von b24 (Fassung im Fuellbyte) kommt wie bisher als Fassung 1 heraus (Wechsel, AP8 von
// Schritt 1) - auch ueber den neuen Weg.
void test_boulder_schnittstelle__nacht_von_b24(void) {
  prv_lade("nacht-2026-09-29");
  NachtBild b24 = s_ref.bild;
  b24.version = NACHT_BILD_VERSION_B24;
  SettingsFile f;
  cl_must_pass(settings_file_open(&f, "boulder_nacht", 2048));
  cl_must_pass(settings_file_set(&f, "letzte", sizeof("letzte"), &b24, sizeof(b24)));
  settings_file_close(&f);
  cl_assert_equal_i(prv_s1(), BoulderLesenSpaeter);
  prv_kernel_bg();
  cl_assert_equal_i(prv_s1(), BoulderLesenDa);
  cl_assert(memcmp(s_app_bild, &s_ref.bild, sizeof(s_ref.bild)) == 0);
}

// =============================================================================================
// Die Lage nach Boot, Nacht, Messstufe und Zielen

// Vor der ersten Veroeffentlichung: false, die Vorgaben, Pebbles Schalter frisch. "Laden"
// veroeffentlicht; die Ziele und die Stufe kommen nach dem ersten Minutenlauf.
void test_boulder_schnittstelle__lage_nach_dem_boot(void) {
  cl_assert_equal_i(prv_ziele_schicken(1, 7500, 390, 8), S_SUCCESS);
  prv_shellpref_schreiben(4);
  s_tracking = false;
  cl_assert(!prv_s2());
  cl_assert_equal_i(s_app_lage->fassung, BOULDER_LAGE_FASSUNG);
  cl_assert_equal_i(s_app_lage->stand_utc, 0);
  cl_assert_equal_i(s_app_lage->ziel_schritte, 10000);
  cl_assert_equal_i(s_app_lage->ziel_schlaf_min, 480);
  cl_assert_equal_i(s_app_lage->messstufe, 0);
  cl_assert(!s_app_lage->tracking);
  s_tracking = true;
  prv_kernel_bg();  // "Laden"
  BoulderLage l = prv_lage();
  cl_assert_equal_i(l.stand_utc, rtc_get_time());
  cl_assert(l.tracking);
  cl_assert_equal_i(l.maxpuls, 179);
  cl_assert_equal_i(l.fenster_utc, boulder_nacht_fenster());
  cl_assert_equal_i(l.ziel_schritte, 10000);  // noch nicht gelesen
  prv_eine_minute();
  l = prv_lage();
  cl_assert(l.fenster_utc != 0);
  cl_assert_equal_i(l.fenster_utc, boulder_nacht_fenster());
  cl_assert_equal_i(l.ziel_schritte, 7500);
  cl_assert_equal_i(l.ziel_schlaf_min, 390);
  cl_assert_equal_i(l.messstufe, 4);
  // Stufe 4: SpO2 alle 10 min, sobald der Regler Pebbles Schalter gesetzt hat.
  cl_assert(s_spo2_an);
  cl_assert_equal_i(l.spo2_takt_min, 10);
  cl_assert_equal_i(l.nickerchen, 0);
  // Tracking aus: die App sieht es sofort, auch ohne neue Veroeffentlichung.
  s_tracking = false;
  cl_assert(prv_s2());
  cl_assert(!s_app_lage->tracking);
}

// Durch eine Referenznacht: Fenster, Nachtstufe, Live-Zustand und Ruhepuls stehen in der Lage, S1
// liefert das Bild der Nacht Byte fuer Byte wie b26.
void test_boulder_schnittstelle__lage_und_bild_durch_die_nacht(void) {
  prv_lade("nacht-2026-09-29");
  activity_algorithm_deinit();
  prv_kernel_bg();
  rtc_set_time(s_ref.beginn_utc - PBL_SEC_PER_MIN);
  prv_boot();
  prv_kernel_bg();
  int schlaeft = 0;
  int offen = 0;
  for (int m = 0; m < 20 * 60; m++) {
    prv_nacht(m, m + 1);
    const BoulderLage l = prv_lage();
    cl_assert_equal_i(l.stand_utc, rtc_get_time());
    cl_assert_equal_i(l.fenster_utc, boulder_nacht_fenster());
    cl_assert_equal_i(l.nacht_stufe, boulder_nacht_stufe());
    cl_assert_equal_b(l.schlaeft, boulder_live_schlaeft());
    cl_assert_equal_i(l.ruhepuls_median, boulder_nacht_ruhepuls_median());
    schlaeft += l.schlaeft;
    offen += (l.nacht_stufe == NachtOffen);
    if (m == 3 * 60) {
      cl_assert(l.schlaeft);
    }
  }
  const BoulderLage l = prv_lage();
  cl_assert_equal_i(l.fenster_utc, s_ref.beginn_utc);
  cl_assert_equal_i(l.nacht_stufe, NachtErledigt);
  cl_assert(l.ruhepuls_median > 0);
  cl_assert(schlaeft > 5 * 60);
  cl_assert(offen > 0);
  printf("\nnacht-2026-09-29: %d min schlaeft, %d min offen, Ruhepuls-Median %d", schlaeft, offen,
         l.ruhepuls_median);
  cl_assert_equal_i(prv_s1(), BoulderLesenDa);
  cl_assert(memcmp(s_app_bild, &s_ref.bild, sizeof(s_ref.bild)) == 0);
  NachtBild kern;
  cl_assert(boulder_nacht_letzte(&kern));
  cl_assert(memcmp(s_app_bild, &kern, sizeof(kern)) == 0);
}

// Am Kabel und mit Tracking aus laeuft kein Minutenteil; der Takt (Sekunde 10) haelt die Lage
// trotzdem frisch, denn der Lebenslauf der Nacht laeuft weiter.
void test_boulder_schnittstelle__takt_ohne_minutenteil_veroeffentlicht(void) {
  prv_kernel_bg();
  const time_t t = rtc_get_time() + 5 * PBL_SEC_PER_MIN;
  rtc_set_time(t);
  boulder_dauerteil_takt(t);
  prv_kernel_bg();
  cl_assert_equal_i(prv_lage().stand_utc, t);
}

// Der Ruhepuls der Lage ist der Median der letzten sieben Naechte (V3 aus Schritt 2), wie ihn
// Kieselsport und Boulder fuer die Zonen nehmen - nicht der der letzten Nacht.
void test_boulder_schnittstelle__lage_traegt_den_median_der_naechte(void) {
  prv_kernel_bg();
  BoulderRuheRing ring = { .puls = { 70, 52, 70, 54, 70, 0, 0 }, .neu = 4, .nacht = 1790000000 };
  boulder_nacht_ruhe_laden(&ring);
  prv_eine_minute();
  cl_assert_equal_i(boulder_nacht_ruhepuls(), 0);  // seit dem Boot keine Nacht fertig
  cl_assert_equal_i(prv_lage().ruhepuls_median, 70);
}

// SpO2 nach Stufe UND Pebbles Schalter: Schaltet jemand "Blood Oxygen" in Pebbles Einstellungen
// aus, misst die Uhr nicht, bis der Regler ihn zurueckholt - die Lage sagt dann "aus".
void test_boulder_schnittstelle__spo2_takt_folgt_pebbles_schalter(void) {
  prv_shellpref_schreiben(4);
  prv_kernel_bg();
  prv_eine_minute();
  cl_assert(s_spo2_an);
  cl_assert_equal_i(prv_lage().spo2_takt_min, 10);
  s_spo2_an = false;
  prv_eine_minute();
  cl_assert(!s_spo2_an);
  cl_assert_equal_i(prv_lage().spo2_takt_min, 0);
}

// =============================================================================================
// S4: Messstufe wie aus Settings -> Health (T6.1 am Host, T6.2)

void test_boulder_schnittstelle__messstufe_aus_der_app(void) {
  prv_kernel_bg();
  prv_eine_minute();
  cl_assert_equal_i(prv_lage().messstufe, MESSSTUFE_VORGABE);
  cl_assert(prv_s4(5));
  cl_assert_equal_i(s_app_n, 1);
  prv_kernel_bg();
  uint8_t stufe = 0;
  cl_must_pass(prv_shellpref_stufe(&stufe));
  cl_assert_equal_i(stufe, 5);
  // Die Lage gleich danach, nicht erst nach der naechsten Minute.
  cl_assert_equal_i(prv_lage().messstufe, 5);
  cl_assert_equal_i(boulder_messstufe(), 5);
  // Pebbles Takte folgen der Stufe (messstufe.h): Puls am Tag alle 5 min, SpO2 alle 10 min.
  cl_assert_equal_i(s_puls_takt, HRMonitoringInterval_5Min);
  cl_assert(s_spo2_an);
  cl_assert_equal_i(prv_lage().spo2_takt_min, 10);
  // Stufe 1 ist Pebbles Original: kein SpO2.
  cl_assert(prv_s4(1));
  prv_kernel_bg();
  cl_assert_equal_i(prv_lage().messstufe, 1);
  cl_assert(!s_spo2_an);
  cl_assert_equal_i(prv_lage().spo2_takt_min, 0);
}

void test_boulder_schnittstelle__ungueltige_stufe_aendert_nichts(void) {
  prv_kernel_bg();
  prv_eine_minute();
  const int vorher = s_app_eintraege;
  cl_assert(!prv_s4(0));
  cl_assert(!prv_s4(6));
  cl_assert(!prv_s4(255));
  cl_assert_equal_i(s_app_eintraege, vorher);
  prv_kernel_bg();
  uint8_t stufe = 0;
  cl_assert(prv_shellpref_stufe(&stufe) != S_SUCCESS);
  cl_assert_equal_i(prv_lage().messstufe, MESSSTUFE_VORGABE);
}

// Zwei Wuensche, bevor KernelBG laeuft: ein Auftrag, der zweite gilt.
void test_boulder_schnittstelle__zweiter_wunsch_gilt(void) {
  prv_kernel_bg();
  cl_assert(prv_s4(2));
  cl_assert(prv_s4(4));
  cl_assert_equal_i(s_app_n, 1);
  prv_kernel_bg();
  uint8_t stufe = 0;
  cl_must_pass(prv_shellpref_stufe(&stufe));
  cl_assert_equal_i(stufe, 4);
  cl_assert_equal_i(prv_lage().messstufe, 4);
}

// Ein Wunsch, waehrend der Auftrag schon laeuft und den ersten schon gelesen hat (nach seinem
// Merker): Er geht nicht verloren, ein zweiter Auftrag setzt ihn.
void test_boulder_schnittstelle__wunsch_waehrend_des_setzens_geht_nicht_verloren(void) {
  prv_kernel_bg();
  const uint32_t laeufe = prv_laeufe("Boulder Messstufe setzen");
  cl_assert(prv_s4(5));
  s_wunsch_dazwischen = true;
  prv_kernel_bg();
  cl_assert(!s_wunsch_dazwischen);
  cl_assert_equal_i(prv_laeufe("Boulder Messstufe setzen"), laeufe + 2);
  uint8_t stufe = 0;
  cl_must_pass(prv_shellpref_stufe(&stufe));
  cl_assert_equal_i(stufe, 2);
  cl_assert_equal_i(prv_lage().messstufe, 2);
}

// T6.2: Stufe 20-mal und S1 mit "Spaeter" 20-mal, ohne dass KernelBG dazwischen laeuft:
// hoechstens zwei Boulder-Auftraege der App zugleich; danach gilt der letzte Wunsch.
void test_boulder_schnittstelle__hoechstens_zwei_auftraege_der_app(void) {
  for (int i = 0; i < 20; i++) {
    cl_assert(prv_s4((uint8_t)(1 + i % 5)));
    cl_assert_equal_i(prv_s1(), BoulderLesenSpaeter);
  }
  cl_assert_equal_i(s_app_n, 2);
  cl_assert_equal_i(s_app_eintraege, 2);
  cl_assert_equal_i(s_app_boulder_max, 2);
  prv_kernel_bg();
  cl_assert_equal_i(prv_lage().messstufe, 5);
  cl_assert_equal_i(prv_s1(), BoulderLesenKeine);
  // Noch einmal 20: wieder hoechstens zwei.
  for (int i = 0; i < 20; i++) {
    cl_assert(prv_s4(3));
  }
  cl_assert_equal_i(s_app_n, 1);
  prv_kernel_bg();
  cl_assert_equal_i(s_app_boulder_max, 2);
}

// T6.2: Die Schlange der App ist voll (8 fremde Eintraege, etwa das Puls-Abo): Der Syscall wartet,
// bis KernelBG einen Platz frei macht, und der Wunsch kommt an.
void test_boulder_schnittstelle__volle_schlange_der_app_verliert_nichts(void) {
  for (int i = 0; i < APP_SCHLANGE; i++) {
    s_app_schlange[s_app_n++] = (Eintrag){ prv_fremd_cb, NULL };
  }
  cl_assert(prv_s4(2));
  cl_assert_equal_i(s_app_gewartet, 1);
  cl_assert_equal_i(prv_s1(), BoulderLesenSpaeter);
  cl_assert_equal_i(s_app_gewartet, 2);
  prv_kernel_bg();
  uint8_t stufe = 0;
  cl_must_pass(prv_shellpref_stufe(&stufe));
  cl_assert_equal_i(stufe, 2);
  cl_assert_equal_i(prv_s1(), BoulderLesenKeine);
}

// =============================================================================================
// T6.3: die Ziele ueber die echte BlobDB Health

static BoulderLage prv_nach_ereignis(void) {
  prv_kernel_bg();
  return prv_lage();
}

void test_boulder_schnittstelle__ziele_einfuegen_aendern_loeschen_leeren(void) {
  prv_kernel_bg();
  prv_eine_minute();
  BoulderLage l = prv_lage();
  cl_assert_equal_i(l.ziel_schritte, 10000);
  cl_assert_equal_i(l.ziel_schlaf_min, 480);
  // Einfuegen: nach dem Ereignis, ohne Minute und ohne Ablage.
  cl_assert_equal_i(prv_ziele_schicken(1, 7500, 390, 8), S_SUCCESS);
  l = prv_nach_ereignis();
  cl_assert_equal_i(l.ziel_schritte, 7500);
  cl_assert_equal_i(l.ziel_schlaf_min, 390);
  // Aendern.
  cl_assert_equal_i(prv_ziele_schicken(1, 12000, 450, 8), S_SUCCESS);
  l = prv_nach_ereignis();
  cl_assert_equal_i(l.ziel_schritte, 12000);
  cl_assert_equal_i(l.ziel_schlaf_min, 450);
  // Loeschen: sofort die Vorgabe.
  cl_must_pass(blob_db_delete(BlobDBIdHealth, (const uint8_t *)BOULDER_ZIELE_SCHLUESSEL,
                              strlen(BOULDER_ZIELE_SCHLUESSEL)));
  l = prv_nach_ereignis();
  cl_assert_equal_i(l.ziel_schritte, 10000);
  cl_assert_equal_i(l.ziel_schlaf_min, 480);
  // Wieder einfuegen, dann die ganze Datenbank leeren (Ereignis ohne Schluessel): sofort die
  // Vorgabe.
  cl_assert_equal_i(prv_ziele_schicken(1, 6000, 420, 8), S_SUCCESS);
  cl_assert_equal_i(prv_nach_ereignis().ziel_schritte, 6000);
  cl_must_pass(blob_db_flush(BlobDBIdHealth));
  cl_assert_equal_i(fake_event_get_last().blob_db.type, BlobDBEventTypeFlush);
  cl_assert(fake_event_get_last().blob_db.key == NULL);
  l = prv_nach_ereignis();
  cl_assert_equal_i(l.ziel_schritte, 10000);
  cl_assert_equal_i(l.ziel_schlaf_min, 480);
  // healthdb meldet den Schluessel an keine Metrik (kein _movementData, kein Wochentag).
  cl_assert_equal_i(s_metriken_gesetzt, 0);
}

// Jede Grenze knapp darunter und darueber; dazu Fassung, Laenge und Werte, die es nie gibt.
// "70 000 Schritte" (9.1, T6.3) passt nicht in die 16 Bit des Satzes: das Groesste, was das
// Telefon schicken koennte, ist 65 535 - auch das gilt nicht.
void test_boulder_schnittstelle__ungueltige_ziele_gelten_nicht(void) {
  prv_kernel_bg();
  prv_eine_minute();
  static const struct {
    uint8_t fassung;
    uint16_t schritte;
    uint16_t schlaf;
    int laenge;
    bool gilt;
  } faelle[] = {
    { 1, 1000, 240, 8, true },    { 1, 50000, 720, 8, true },   { 1, 999, 480, 8, false },
    { 1, 50001, 480, 8, false },  { 1, 0, 480, 8, false },      { 1, 65535, 480, 8, false },
    { 1, 10000, 239, 8, false },  { 1, 10000, 721, 8, false },  { 1, 10000, 100, 8, false },
    { 2, 7500, 390, 8, false },   { 0, 7500, 390, 8, false },   { 1, 7500, 390, 4, false },
    { 1, 7500, 390, 12, false },
  };
  for (size_t i = 0; i < ARRAY_LENGTH(faelle); i++) {
    // Erst ein gueltiger anderer Satz, damit "gilt nicht" die Vorgabe heisst, nicht "unveraendert".
    cl_assert_equal_i(prv_ziele_schicken(1, 8888, 444, 8), S_SUCCESS);
    cl_assert_equal_i(prv_nach_ereignis().ziel_schritte, 8888);
    cl_assert_equal_i(prv_ziele_schicken(faelle[i].fassung, faelle[i].schritte, faelle[i].schlaf,
                                         faelle[i].laenge), S_SUCCESS);
    const BoulderLage l = prv_nach_ereignis();
    const BoulderZieleSatz satz = { .fassung = faelle[i].fassung, .schritte = faelle[i].schritte,
                                    .schlaf_min = faelle[i].schlaf };
    cl_assert_equal_b(boulder_ziele_gueltig(&satz, faelle[i].laenge), faelle[i].gilt);
    if (faelle[i].gilt) {
      cl_assert_equal_i(l.ziel_schritte, faelle[i].schritte);
      cl_assert_equal_i(l.ziel_schlaf_min, faelle[i].schlaf);
    } else {
      cl_assert_equal_i(l.ziel_schritte, 10000);
      cl_assert_equal_i(l.ziel_schlaf_min, 480);
    }
  }
}

// Gegenprobe: Ein anderer Health-Schluessel, ein Schluessel, der nur so anfaengt, und derselbe
// Schluessel in einer anderen Datenbank loesen keinen Auftrag aus.
void test_boulder_schnittstelle__andere_schluessel_loesen_nichts_aus(void) {
  prv_kernel_bg();
  prv_eine_minute();
  const uint32_t vorher = prv_laeufe("Boulder Ziele");
  cl_assert(vorher >= 1);
  const uint32_t wert = 1234;
  cl_must_pass(blob_db_insert(BlobDBIdHealth, (const uint8_t *)"average_dailySteps", 18,
                              (const uint8_t *)&wert, sizeof(wert)));
  prv_kernel_bg();
  cl_must_pass(blob_db_insert(BlobDBIdHealth, (const uint8_t *)"boulder_zieleX", 14,
                              (const uint8_t *)&wert, sizeof(wert)));
  prv_kernel_bg();
  cl_must_pass(blob_db_delete(BlobDBIdHealth, (const uint8_t *)"average_dailySteps", 18));
  prv_kernel_bg();
  cl_assert_equal_i(prv_laeufe("Boulder Ziele"), vorher);
  // Mit Nullbyte ist es fuer healthdb ein anderer Schluessel (Gegenpruefung K4): kein Auftrag, und
  // es gilt weiter der Wert unter dem Schluessel ohne.
  cl_assert_equal_i(prv_ziele_schicken(1, 7500, 390, 8), S_SUCCESS);
  cl_assert_equal_i(prv_nach_ereignis().ziel_schritte, 7500);
  const uint32_t mit_wert = prv_laeufe("Boulder Ziele");
  const BoulderZieleSatz satz = { .fassung = 1, .schritte = 9000, .schlaf_min = 420 };
  cl_must_pass(blob_db_insert(BlobDBIdHealth, (const uint8_t *)BOULDER_ZIELE_SCHLUESSEL,
                              strlen(BOULDER_ZIELE_SCHLUESSEL) + 1, (const uint8_t *)&satz,
                              sizeof(satz)));
  prv_kernel_bg();
  cl_assert_equal_i(prv_laeufe("Boulder Ziele"), mit_wert);
  cl_assert_equal_i(prv_lage().ziel_schritte, 7500);
  cl_assert_equal_i(prv_lage().ziel_schlaf_min, 390);
}

// Die Ziele ueberstehen einen Neustart (healthdb); die Ablage liest sie zur Sicherheit nach.
void test_boulder_schnittstelle__ziele_nach_neustart_und_ablage(void) {
  prv_kernel_bg();
  prv_eine_minute();
  cl_assert_equal_i(prv_ziele_schicken(1, 7500, 390, 8), S_SUCCESS);
  cl_assert_equal_i(prv_nach_ereignis().ziel_schritte, 7500);
  activity_algorithm_deinit();
  prv_kernel_bg();
  prv_boot();
  prv_kernel_bg();
  cl_assert_equal_i(prv_lage().ziel_schritte, 10000);
  prv_eine_minute();
  BoulderLage l = prv_lage();
  cl_assert_equal_i(l.ziel_schritte, 7500);
  cl_assert_equal_i(l.ziel_schlaf_min, 390);
  // Ein verlorenes Ereignis: Die Datei aendert sich ohne Meldung; die Ablage holt es nach.
  fake_event_set_callback(NULL);
  cl_assert_equal_i(prv_ziele_schicken(1, 5000, 300, 8), S_SUCCESS);
  kernel_free(fake_event_get_last().blob_db.key);
  fake_event_set_callback(prv_ereignis);
  prv_kernel_bg();
  cl_assert_equal_i(prv_lage().ziel_schritte, 7500);
  for (int i = 0; i < 15; i++) {
    prv_eine_minute();
  }
  l = prv_lage();
  cl_assert_equal_i(l.ziel_schritte, 5000);
  cl_assert_equal_i(l.ziel_schlaf_min, 300);
}

// =============================================================================================
// S3 und die Namen der Stufen

void test_boulder_schnittstelle__spo2_aelteste_zuerst_hoechstens_max(void) {
  prv_kernel_bg();
  const time_t t = rtc_get_time();
  for (int i = 0; i < 12; i++) {
    boulder_spo2_merken(t - (12 - i) * 600, (uint8_t)(88 + i));
  }
  cl_assert_equal_i(prv_s3(t - 24 * 3600, t + 1, SPO2_WERTE_MAX), 12);
  for (int i = 0; i < 12; i++) {
    cl_assert_equal_i(s_app_spo2[i].prozent, 88 + i);
    cl_assert_equal_i(s_app_spo2[i].zeit, t - (12 - i) * 600);
  }
  // Hoechstens max, die aeltesten im Fenster zuerst.
  memset(s_app_spo2, 0, 6 * sizeof(Spo2Wert));
  cl_assert_equal_i(prv_s3(t - 24 * 3600, t + 1, 5), 5);
  cl_assert_equal_i(s_app_spo2[4].prozent, 92);
  cl_assert_equal_i(s_app_spo2[5].prozent, 0);
  // Das Fenster [von, bis).
  cl_assert_equal_i(prv_s3(t - 3 * 600, t - 600, SPO2_WERTE_MAX), 2);
  cl_assert_equal_i(s_app_spo2[0].prozent, 97);
  cl_assert_equal_i(s_app_spo2[1].prozent, 98);
}

// Die Namen fuer die App stehen in messstufe.h; Settings -> Health zeigt dieselben.
void test_boulder_schnittstelle__namen_der_stufen(void) {
  static const char *const soll[] = { "Battery Saver", "Light", "Balanced", "Detailed",
                                      "Precise" };
  const char **menue = boulder_messstufe_namen();
  for (int i = 0; i < MESSSTUFE_MAX; i++) {
    cl_assert_equal_s(BOULDER_MESSSTUFE_NAMEN[i], soll[i]);
    cl_assert(menue[i] == BOULDER_MESSSTUFE_NAMEN[i]);
  }
}
