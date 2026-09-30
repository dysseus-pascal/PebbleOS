/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "apps/system/kieselstrasse/schluessel.h"
#include "apps/system/kieselstrasse/texte.h"
#include "apps/system/kieselstrasse/weg.h"

#include "stubs_logging.h"
#include "stubs_pbl_malloc.h"

// Die reine Logik der Navigation: was ein neuer Schritt ist, was in den
// Persist kommt und wie ein alter Persist gelesen wird. Plattform nachgebaut:
// eine Uhr, die der Test stellt, und ein Persist mit genau einem Eintrag.

// --- Zeit ---

static time_t s_jetzt;
time_t rtc_get_time(void) { return s_jetzt; }

// --- Sprache ---

const char *app_get_system_locale(void) { return "en_US"; }

// --- Persist ---

static uint8_t s_persist[128];
static int s_persist_laenge;  // 0 = kein Eintrag
static int s_schreibvorgaenge;

bool persist_exists(const uint32_t key) {
  return key == PERSIST_WEG && s_persist_laenge > 0;
}

int persist_read_data(const uint32_t key, void *buffer, const size_t buffer_size) {
  if (!persist_exists(key)) return E_DOES_NOT_EXIST;
  const size_t n = MIN(buffer_size, (size_t)s_persist_laenge);
  memcpy(buffer, s_persist, n);
  return (int)n;
}

int persist_write_data(const uint32_t key, const void *buffer, const size_t buffer_size) {
  cl_assert_equal_i(key, PERSIST_WEG);
  cl_assert(buffer_size <= sizeof(s_persist));
  memcpy(s_persist, buffer, buffer_size);
  s_persist_laenge = (int)buffer_size;
  s_schreibvorgaenge++;
  return (int)buffer_size;
}

// --- Nachrichten ---

static uint8_t s_puffer[256];
static DictionaryIterator s_iter;

// Baut eine Nachricht wie von Boulder. art/entfernung < 0 = Schluessel fehlt,
// strasse NULL = Schluessel fehlt.
static DictionaryIterator *prv_nachricht(int art, int entfernung, const char *strasse) {
  DictionaryIterator schreiben;
  cl_assert_equal_i(dict_write_begin(&schreiben, s_puffer, sizeof(s_puffer)), DICT_OK);
  if (art >= 0) dict_write_int32(&schreiben, MESSAGE_KEY_ABBIEGEART, art);
  if (entfernung >= 0) dict_write_int32(&schreiben, MESSAGE_KEY_ENTFERNUNG, entfernung);
  if (strasse) dict_write_cstring(&schreiben, MESSAGE_KEY_STRASSE, strasse);
  const uint32_t benutzt = dict_write_end(&schreiben);
  dict_read_begin_from_buffer(&s_iter, s_puffer, (uint16_t)benutzt);
  return &s_iter;
}

void test_kieselstrasse_weg__initialize(void) {
  s_jetzt = 1700000000;
  memset(s_persist, 0, sizeof(s_persist));
  s_persist_laenge = 0;
  s_schreibvorgaenge = 0;
  strasse_texte_lesen();
}

void test_kieselstrasse_weg__cleanup(void) {}

void test_kieselstrasse_weg__ohne_persist_leer_und_veraltet(void) {
  weg_init();
  cl_assert(weg_veraltet());
  cl_assert(!weg_hat_zahl());
  cl_assert_equal_i(weg_balken_prozent(), -1);
  // Nie einen leeren Anfangsstand schreiben.
  weg_sichern();
  cl_assert_equal_i(s_schreibvorgaenge, 0);
}

void test_kieselstrasse_weg__neuer_schritt_nur_bei_anderer_art(void) {
  weg_init();
  cl_assert(weg_uebernimm(prv_nachricht(5, 250, "Bahnhofstrasse")));
  cl_assert_equal_i(s_schreibvorgaenge, 1);
  // Schrumpfende Entfernung: derselbe Schritt, kein Schreiben.
  cl_assert(!weg_uebernimm(prv_nachricht(5, 120, NULL)));
  cl_assert(!weg_uebernimm(prv_nachricht(-1, 80, NULL)));
  cl_assert_equal_i(s_schreibvorgaenge, 1);
  cl_assert_equal_s(weg_stand()->strasse, "Bahnhofstrasse");
  // Unbekannte Art (0) ist kein Schritt.
  cl_assert(!weg_uebernimm(prv_nachricht(0, 80, NULL)));
  cl_assert(weg_uebernimm(prv_nachricht(2, 400, "Hauptstrasse")));
  cl_assert_equal_i(s_schreibvorgaenge, 2);
}

void test_kieselstrasse_weg__leere_strasse_loescht(void) {
  weg_init();
  weg_uebernimm(prv_nachricht(5, 250, "Bahnhofstrasse"));
  weg_uebernimm(prv_nachricht(-1, -1, ""));
  cl_assert_equal_s(weg_stand()->strasse, "");
}

void test_kieselstrasse_weg__balken(void) {
  weg_init();
  weg_uebernimm(prv_nachricht(5, 1000, NULL));
  cl_assert_equal_i(weg_balken_prozent(), -1);
  weg_uebernimm(prv_nachricht(-1, 300, NULL));
  cl_assert_equal_i(weg_balken_prozent(), 100);
  weg_uebernimm(prv_nachricht(-1, 150, NULL));
  cl_assert_equal_i(weg_balken_prozent(), 50);
  weg_uebernimm(prv_nachricht(-1, 0, NULL));
  cl_assert_equal_i(weg_balken_prozent(), 0);
}

void test_kieselstrasse_weg__veraltet_nach_zehn_minuten(void) {
  weg_init();
  weg_uebernimm(prv_nachricht(5, 250, NULL));
  cl_assert(!weg_veraltet());
  s_jetzt += 600;
  cl_assert(!weg_veraltet());
  s_jetzt += 1;
  cl_assert(weg_veraltet());
}

void test_kieselstrasse_weg__sichern_nur_wenn_geaendert(void) {
  weg_init();
  weg_uebernimm(prv_nachricht(5, 250, NULL));
  cl_assert_equal_i(s_schreibvorgaenge, 1);
  // Direkt nach dem neuen Schritt ist nichts mehr offen.
  weg_sichern();
  cl_assert_equal_i(s_schreibvorgaenge, 1);
  // Eine kleinere Entfernung danach kommt beim Beenden einmal hinein.
  weg_uebernimm(prv_nachricht(-1, 100, NULL));
  weg_sichern();
  weg_sichern();
  cl_assert_equal_i(s_schreibvorgaenge, 2);

  // Oeffnen und Schliessen ohne Nachricht: kein Schreibvorgang.
  weg_init();
  cl_assert_equal_i((int)weg_stand()->entfernung_m, 100);
  weg_sichern();
  cl_assert_equal_i(s_schreibvorgaenge, 2);
}

void test_kieselstrasse_weg__zu_kurzer_persist_wird_verworfen(void) {
  memset(s_persist, 'x', sizeof(s_persist));
  s_persist_laenge = 40;
  weg_init();
  cl_assert(weg_veraltet());
  cl_assert(!weg_hat_zahl());
  cl_assert_equal_s(weg_stand()->strasse, "");
}

void test_kieselstrasse_weg__strasse_ohne_abschluss(void) {
  Weg w = { .abbiegeart = 5, .entfernung_m = 200, .rest_m = -1, .empfangen = s_jetzt };
  memset(w.strasse, 'x', sizeof(w.strasse));
  memcpy(s_persist, &w, sizeof(w));
  s_persist_laenge = sizeof(w);
  weg_init();
  cl_assert_equal_i(strlen(weg_stand()->strasse), KST_STRASSE_LEN - 1);
}

void test_kieselstrasse_weg__alte_art_verschluckt_nicht_das_summen(void) {
  // Gestern "rechts" gespeichert ...
  Weg w = { .abbiegeart = 5, .entfernung_m = 200, .rest_m = -1, .empfangen = s_jetzt };
  memcpy(s_persist, &w, sizeof(w));
  s_persist_laenge = sizeof(w);
  s_jetzt += 86400;
  weg_init();
  // ... heute ist der erste Schritt wieder "rechts": er summt trotzdem.
  cl_assert(weg_uebernimm(prv_nachricht(5, 300, NULL)));
}

void test_kieselstrasse_weg__frischer_persist_behaelt_art(void) {
  // Die App wurde waehrend der Navigation kurz geschlossen: derselbe Schritt
  // danach ist kein neuer.
  Weg w = { .abbiegeart = 5, .entfernung_m = 200, .rest_m = -1, .empfangen = s_jetzt };
  memcpy(s_persist, &w, sizeof(w));
  s_persist_laenge = sizeof(w);
  s_jetzt += 30;
  weg_init();
  cl_assert(!weg_uebernimm(prv_nachricht(5, 150, NULL)));
}

void test_kieselstrasse_weg__alter_text(void) {
  char buf[32];
  weg_init();
  weg_alter_text(buf, sizeof(buf));
  cl_assert_equal_s(buf, "never");
  weg_uebernimm(prv_nachricht(5, 250, NULL));
  weg_alter_text(buf, sizeof(buf));
  cl_assert_equal_s(buf, "just now");
  s_jetzt += 12 * 60;
  weg_alter_text(buf, sizeof(buf));
  cl_assert_equal_s(buf, "12 min ago");
}
