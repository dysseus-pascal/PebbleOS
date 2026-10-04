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

// Die Einstellungen vom Telefon (einstellungen.c): der Timeline-Pin liegt im
// Persist der App. Nachgebaut ist der Persist - eine Grenze zum Flash, die
// jeden Schreibvorgang zaehlt - und der Dienst, der hier nichts tut.

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
uint16_t kieselsport_maxpuls(void) { return 190; }
bool kieselsport_maxpuls_gesetzt(void) { return false; }
uint16_t kieselsport_becken(void) { return 25; }
uint16_t kieselsport_pausenziel(void) { return 90; }
uint8_t kieselsport_empfindlichkeit(void) { return 2; }

void test_kieselsport_einstellungen__initialize(void) {
  s_art_da = false;
  s_art = 0;
  s_zeit[0] = '\0';
  s_schreibvorgaenge = 0;
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
