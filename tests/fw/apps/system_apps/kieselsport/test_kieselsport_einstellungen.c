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

void test_kieselsport_einstellungen__initialize(void) {
  s_art_da = false;
  s_art = 0;
  s_zeit[0] = '\0';
  s_schreibvorgaenge = 0;
  s_maxpuls = 190;
  s_maxpuls_gesetzt = false;
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
  einstellungen_pin_art(9);
  einstellungen_pin_zeit("1830");
  cl_assert_equal_i(s_schreibvorgaenge, 4);
}

// --- Golden-Diktat: bytegleich zu b25 (Kriterium 3) ---
//
// Die Meldung der Einstellungen, wie Boulder sie erkennt (MAXPULS da, ART fehlt). DIE SOLL-BYTES
// KOMMEN NICHT AUS DIESER FIRMWARE: Erzeugt hat sie dieser Test, gebaut gegen einstellungen.c und
// util/dict.c von b25 (main a7d8b16e1) und gestartet mit KS_GOLDEN_DRUCKEN=1.

#define GOLDEN_EINST_VORGABE \
  "0818270000030400be00000038270000030400000000001a270000030400190000001b2700000304005a0000" \
  "001c2700000304000200000022270000030400000000002327000001060031383a3030002727000003040000" \
  "000000"
#define GOLDEN_EINST_GESETZT \
  "0818270000030400ac00000038270000030400010000001a270000030400190000001b2700000304005a0000" \
  "001c27000003040002000000222700000304000300000023270000010500373a333000272700000304000000" \
  "0000"

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
  prv_golden("einst_gesetzt", GOLDEN_EINST_GESETZT);
}
