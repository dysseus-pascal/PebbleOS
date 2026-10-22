/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "stubs_logging.h"
#include "stubs_pbl_malloc.h"

#include "apps/system/kieselsport/einstellungen.h"
#include "apps/system/kieselsport/schluessel.h"
#undef malloc
#undef calloc
#undef free
#undef time

#include <stdio.h>
#include <stdlib.h>

// Die Einstellungen vom Telefon (einstellungen.c): der Timeline-Pin liegt im
// Persist der App. Nachgebaut ist der Persist - eine Grenze zum Flash, die
// jeden Schreibvorgang zaehlt - und der Dienst, der hier nichts tut.
//
// Aus b25 (main a7d8b16e1); neu sind die Golden-Diktate am Ende.

// --- Persist ---

static bool s_art_da;
static int32_t s_art;
static char s_zeit[8];
static int s_schreibvorgaenge;

bool persist_exists(const uint32_t key) {
  if (key == PERSIST_PIN_ART) return s_art_da;
  if (key == PERSIST_PIN_ZEIT) return s_zeit[0] != '\0';
  return false;
}
int32_t persist_read_int(const uint32_t key) {
  cl_assert_equal_i(key, PERSIST_PIN_ART);
  return s_art;
}
status_t persist_write_int(const uint32_t key, const int32_t value) {
  cl_assert_equal_i(key, PERSIST_PIN_ART);
  s_art_da = true;
  s_art = value;
  s_schreibvorgaenge++;
  return S_SUCCESS;
}
int persist_read_string(const uint32_t key, char *buffer, const size_t buffer_size) {
  cl_assert_equal_i(key, PERSIST_PIN_ZEIT);
  strncpy(buffer, s_zeit, buffer_size);
  buffer[buffer_size - 1] = '\0';
  return (int)strlen(buffer) + 1;
}
int persist_write_string(const uint32_t key, const char *cstring) {
  cl_assert_equal_i(key, PERSIST_PIN_ZEIT);
  strncpy(s_zeit, cstring, sizeof(s_zeit));
  s_zeit[sizeof(s_zeit) - 1] = '\0';
  s_schreibvorgaenge++;
  return (int)strlen(cstring) + 1;
}

// --- Der Dienst ---

void kieselsport_maxpuls_alt(uint16_t schlaege) {}
void kieselsport_maxpuls_nicht_setzen(void) {}
void kieselsport_maxpuls_setze(uint16_t schlaege) {}
void kieselsport_becken_setze(uint16_t meter) {}
void kieselsport_pausenziel_setze(uint16_t sekunden) {}
void kieselsport_empfindlichkeit_setze(uint8_t stufe) {}
static uint16_t s_maxpuls;
static bool s_maxpuls_gesetzt;
uint16_t kieselsport_maxpuls(void) { return s_maxpuls; }
bool kieselsport_maxpuls_gesetzt(void) { return s_maxpuls_gesetzt; }
uint16_t kieselsport_becken(void) { return 25; }
uint16_t kieselsport_pausenziel(void) { return 90; }
uint8_t kieselsport_empfindlichkeit(void) { return 2; }
static Atemrhythmus s_atem;
static int s_atem_aufrufe;
static AtemWert s_atem_welcher;
static int32_t s_atem_wert;
void kieselsport_atem(Atemrhythmus *aus) { *aus = s_atem; }
void kieselsport_atem_setze(AtemWert welcher, int32_t wert) {
  s_atem_aufrufe++;
  s_atem_welcher = welcher;
  s_atem_wert = wert;
}

void test_kieselsport_einstellungen__initialize(void) {
  s_art_da = false;
  s_art = 0;
  s_zeit[0] = '\0';
  s_schreibvorgaenge = 0;
  s_maxpuls = 190;
  s_maxpuls_gesetzt = false;
  s_atem = (Atemrhythmus){.wert = {4, 4, 4, 4, 10}};
  s_atem_aufrufe = 0;
}

void test_kieselsport_einstellungen__cleanup(void) {}

void test_kieselsport_einstellungen__gleicher_pin_schreibt_nichts(void) {
  einstellungen_pin_art(3);
  einstellungen_pin_zeit("7:30");
  cl_assert_equal_i(s_schreibvorgaenge, 2);
  // Boulder schickt beim Speichern alles noch einmal.
  einstellungen_pin_art(3);
  einstellungen_pin_zeit("7:30");
  cl_assert_equal_i(s_schreibvorgaenge, 2);
  // Was sich aendert, kommt an.
  einstellungen_pin_art(0);
  einstellungen_pin_zeit("18:00");
  cl_assert_equal_i(s_schreibvorgaenge, 4);
  cl_assert_equal_i(s_art, 0);
  cl_assert_equal_s(s_zeit, "18:00");
  // Unsinn bleibt draussen.
  einstellungen_pin_art(ArtAnzahl + 1);
  einstellungen_pin_art(-1);
  einstellungen_pin_zeit("1830");
  cl_assert_equal_i(s_schreibvorgaenge, 4);
}

// Der Pin traegt Art + 1 (0 = kein Pin): die letzte Art ist ArtAnzahl, nicht eine feste Zahl.
void test_kieselsport_einstellungen__pin_fuer_jede_art(void) {
  for (int32_t pin = 0; pin <= ArtAnzahl; pin++) {
    einstellungen_pin_art(pin);
    cl_assert_equal_i(s_art, pin);
  }
  cl_assert_equal_i(s_schreibvorgaenge, ArtAnzahl + 1);
}

// Der Rhythmus geht unveraendert an den Dienst, auch Unsinn: die Grenzen prueft er (rhythmus.c).
void test_kieselsport_einstellungen__atem_an_den_dienst(void) {
  einstellungen_atem(AtemAus, 7);
  cl_assert_equal_i(s_atem_aufrufe, 1);
  cl_assert_equal_i(s_atem_welcher, AtemAus);
  cl_assert_equal_i(s_atem_wert, 7);
  einstellungen_atem(AtemRunden, 1000);
  cl_assert_equal_i(s_atem_aufrufe, 2);
  cl_assert_equal_i(s_atem_welcher, AtemRunden);
  cl_assert_equal_i(s_atem_wert, 1000);
  // Den Timeline-Pin der App beruehrt das nicht.
  cl_assert_equal_i(s_schreibvorgaenge, 0);
}

// --- Golden-Diktat: bytegleich zu b25 (Kriterium 3), der Rhythmus dahinter ---
//
// Die Meldung der Einstellungen, wie Boulder sie erkennt (MAXPULS da, ART fehlt). DIE BYTES VON
// b25 KOMMEN NICHT AUS DIESER FIRMWARE: Erzeugt hat sie dieser Test, gebaut gegen einstellungen.c
// und util/dict.c von b25 (main a7d8b16e1) und gestartet mit KS_GOLDEN_DRUCKEN=1. Seit der
// Atemuebung haengen fuenf Tupel daran (10041-10045, je int32), und das erste Byte, die Zahl der
// Tupel, ist 13 statt 8; alles dazwischen steht wie in b25.

#define B25_EINST_VORGABE \
  "18270000030400be00000038270000030400000000001a270000030400190000001b2700000304005a0000" \
  "001c2700000304000200000022270000030400000000002327000001060031383a3030002727000003040000" \
  "000000"
#define B25_EINST_GESETZT \
  "18270000030400ac00000038270000030400010000001a270000030400190000001b2700000304005a0000" \
  "001c27000003040002000000222700000304000300000023270000010500373a333000272700000304000000" \
  "0000"
// 4-4-4-4, zehn Runden, und 6-0-8-2 mit zwoelf.
#define ATEM_VORGABE \
  "39270000030400040000003a270000030400040000003b270000030400040000003c27000003040004000000" \
  "3d2700000304000a000000"
#define ATEM_GESETZT \
  "39270000030400060000003a270000030400000000003b270000030400080000003c27000003040002000000" \
  "3d2700000304000c000000"
#define GOLDEN_EINST_VORGABE "0d" B25_EINST_VORGABE ATEM_VORGABE
#define GOLDEN_EINST_GESETZT "0d" B25_EINST_GESETZT ATEM_GESETZT

static void prv_golden(const char *name, const char *soll) {
  static uint8_t puffer[256];
  DictionaryIterator it;
  dict_write_begin(&it, puffer, sizeof(puffer));
  einstellungen_melden(&it);
  const uint32_t n = dict_write_end(&it);
  static char hex[2 * sizeof(puffer) + 1];
  for (uint32_t k = 0; k < n; k++) {
    snprintf(&hex[2 * k], 3, "%02x", puffer[k]);
  }
  hex[2 * n] = 0;
  if (getenv("KS_GOLDEN_DRUCKEN")) {
    printf("GOLDEN %s %s\n", name, hex);
    return;
  }
  cl_assert_equal_s(hex, soll);
}

void test_kieselsport_einstellungen__golden(void) {
  // Nichts eingestellt: Maximalpuls nach Alter, der Pin aus, "18:00".
  prv_golden("einst_vorgabe", GOLDEN_EINST_VORGABE);
  s_maxpuls = 172;
  s_maxpuls_gesetzt = true;
  einstellungen_pin_art(3);
  einstellungen_pin_zeit("7:30");
  s_atem = (Atemrhythmus){.wert = {6, 0, 8, 2, 12}};
  prv_golden("einst_gesetzt", GOLDEN_EINST_GESETZT);
}
