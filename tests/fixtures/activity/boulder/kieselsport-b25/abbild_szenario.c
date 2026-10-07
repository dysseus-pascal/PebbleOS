/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2, Schritt 2, AP6 (T6.2, T6.3): der Persist, den b25 hinterlaesst. Kein Test von V2:
// abbild_erzeugen.sh haengt diese Datei an b25s eigenen Test des Dienstes (main a7d8b16e1,
// tests/fw/services/kieselsport/test_kieselsport.c, dessen Persist im Speicher liegt) und baut
// ihn mit b25s Dienst (src/fw/services/kieselsport/*.c) und b25s App-Einstellungen
// (src/fw/apps/system/kieselsport/einstellungen.c). Was b25 schreibt, steht danach Schluessel
// fuer Schluessel in persist.txt.
//
// Der Ablauf: Boulder schickt Einstellungen; ein Lauf von 55 min mit zwei Pausen wird
// gespeichert (Zusammenfassung, Pausen, Ende, Kurve, Archiv), das Telefon ist nicht da; ein
// Krafttraining wird nach 40 s verworfen (Stopp).

// b25s App liest und schreibt die Pin-Werte ueber den Persist der App - auf der Uhr dieselbe
// Datei wie der des Dienstes (beide unter Kieselsports Kennung, Schluessel als uint32).
bool persist_exists(const uint32_t key) { return ks_persist_exists(key); }
int32_t persist_read_int(const uint32_t key) { return ks_persist_read_int(key); }
status_t persist_write_int(const uint32_t key, const int32_t value) {
  ks_persist_write_int(key, value);
  return sizeof(value);
}
int persist_read_string(const uint32_t key, char *buffer, const size_t buffer_size) {
  const int n = ks_persist_read_data(key, buffer, buffer_size);
  if (n > 0) buffer[n - 1] = '\0';
  return n;
}
int persist_write_string(const uint32_t key, const char *cstring) {
  ks_persist_write_data(key, cstring, strlen(cstring) + 1);  // wie applib/persist.c
  return (int)strlen(cstring) + 1;
}

// dict.c (von einstellungen_melden, hier nicht gerufen).
void *task_malloc(size_t bytes) { return malloc(bytes); }
void task_free(void *ptr) { free(ptr); }

// b25s einstellungen.c (A:), ohne ihren Kopf: der zieht die Firmware-Koepfe der App nach, die
// mit denen dieses Tests zusammenstiessen.
void einstellungen_maxpuls_vermerkt(bool gesetzt, int32_t schlaege);
void einstellungen_becken(int32_t meter);
void einstellungen_pausenziel(int32_t sekunden);
void einstellungen_empfindlichkeit(int32_t stufe);
void einstellungen_pin_art(int32_t art);
void einstellungen_pin_zeit(const char *hhmm);

void test_kieselsport__b25_abbild(void) {
  // Einstellungen von Boulder (A: telefon.c reicht sie an einstellungen.c).
  einstellungen_maxpuls_vermerkt(true, 183);
  einstellungen_becken(50);
  einstellungen_pausenziel(120);
  einstellungen_empfindlichkeit(3);
  einstellungen_pin_art(2);
  einstellungen_pin_zeit("07:30");

  // Lauf: 55 min aktiv, mit zwei Pausen; gespeichert aus der Pause.
  s_zaehler[KsZaehlerSchritte] = 4000;
  s_zaehler[KsZaehlerKcal] = 900;
  kieselsport_starte(ArtLaufen, BEGINN);
  for (int i = 0; i < 1800; i++) {
    if (i % 60 == 59) s_zaehler[KsZaehlerKcal] += 9;
    prv_sekunde_mit_puls((uint8_t)(110 + i / 30));
  }
  kieselsport_pause_umschalten();
  for (int i = 0; i < 95; i++) prv_sekunde();
  kieselsport_pause_umschalten();
  for (int i = 0; i < 1500; i++) {
    if (i % 60 == 59) s_zaehler[KsZaehlerKcal] += 11;
    prv_sekunde_mit_puls((uint8_t)(150 + (i / 100) % 20));
  }
  kieselsport_pause_umschalten();
  for (int i = 0; i < 20; i++) prv_sekunde();
  kieselsport_speichern();
  cl_assert(kieselsport_kurve_wartet());

  // Kraft, nach 40 s verworfen.
  s_jetzt += 3600;
  kieselsport_starte(ArtKraft, (uint32_t)s_jetzt);
  for (int i = 0; i < 40; i++) prv_sekunde_mit_puls(100);
  kieselsport_pause_umschalten();
  prv_sekunde();
  kieselsport_verwerfen();
  uint32_t beginn, zeit;
  uint8_t art;
  cl_assert(kieselsport_stopp_laden(&beginn, &zeit, &art));

  // Schluessel fuer Schluessel: "Schluessel Hex".
  const char *aus = getenv("KS_B25_ABBILD");
  cl_assert(aus);
  FILE *f = fopen(aus, "w");
  cl_assert(f);
  for (uint32_t key = 0; key < 1000; key++) {
    Eintrag *e = prv_finde(key);
    if (!e) continue;
    fprintf(f, "%" PRIu32 " ", key);
    for (size_t b = 0; b < e->laenge; b++) fprintf(f, "%02x", e->daten[b]);
    fprintf(f, "\n");
  }
  fclose(f);
}
