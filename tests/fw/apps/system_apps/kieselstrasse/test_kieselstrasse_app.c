/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "fake_logging.h"
#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"

#include "apps/system/kieselstrasse/schluessel.h"
#include "apps/system/kieselstrasse/weg.h"
#include "process_management/pebble_process_md.h"

// Die App Kieselstrasse (kieselstrasse.c) mit der echten Logik dahinter
// (weg.c, texte.c). Nachgebaut sind die Grenzen: der Eingang des Telefons
// (AppMessage), die Uhr, der Persist, das Brummen - und Schirm und Pfeil, die
// hier nur gerufen, nicht gezeichnet werden.

const PebbleProcessMd *kieselstrasse_app_get_info(void);

// --- Uhr, Sprache, Ruhezeit ---

static time_t s_jetzt;
time_t rtc_get_time(void) { return s_jetzt; }
const char *app_get_system_locale(void) { return "en_US"; }
bool sys_do_not_disturb_is_active(void) { return false; }

// --- Persist: nichts gespeichert ---

bool persist_exists(const uint32_t key) { return false; }
int persist_read_data(const uint32_t key, void *buffer, const size_t buffer_size) {
  return E_DOES_NOT_EXIST;
}
int persist_write_data(const uint32_t key, const void *buffer, const size_t buffer_size) {
  return (int)buffer_size;
}

// --- Schirm und Pfeil ---

void nav_window_push(void) {}
void nav_window_refresh(void) {}
void nav_window_zuruecksetzen(void) {}
void pfeil_zuruecksetzen(void) {}
void app_event_loop(void) {}

// --- Brummen ---

static int s_brummen;
void vibes_short_pulse(void) { s_brummen++; }

// --- Der Eingang ---

static AppMessageInboxReceived s_empfangen;
static AppMessageInboxDropped s_verworfen;
static uint32_t s_eingang;
static uint32_t s_ausgang;

AppMessageInboxReceived app_message_register_inbox_received(AppMessageInboxReceived cb) {
  s_empfangen = cb;
  return NULL;
}
AppMessageInboxDropped app_message_register_inbox_dropped(AppMessageInboxDropped cb) {
  s_verworfen = cb;
  return NULL;
}
AppMessageResult app_message_open(const uint32_t ein, const uint32_t aus) {
  s_eingang = ein;
  s_ausgang = aus;
  return APP_MSG_OK;
}

// Eine Nachricht wie von Boulder: alle sieben Felder, int32 und der Name.
static uint8_t s_puffer[1024];
static DictionaryIterator s_iter;

static uint32_t prv_nachricht(int art, int meter, const char *strasse) {
  DictionaryIterator schreiben;
  cl_assert_equal_i(dict_write_begin(&schreiben, s_puffer, sizeof(s_puffer)), DICT_OK);
  dict_write_int32(&schreiben, MESSAGE_KEY_ABBIEGEART, art);
  dict_write_int32(&schreiben, MESSAGE_KEY_ENTFERNUNG, meter);
  dict_write_cstring(&schreiben, MESSAGE_KEY_STRASSE, strasse);
  dict_write_int32(&schreiben, MESSAGE_KEY_ANKUNFT, (int32_t)s_jetzt + 600);
  dict_write_int32(&schreiben, MESSAGE_KEY_REST, 5000);
  dict_write_int32(&schreiben, MESSAGE_KEY_AUSFAHRT, 0);
  dict_write_int32(&schreiben, MESSAGE_KEY_WINKEL, 0);
  const uint32_t benutzt = dict_write_end(&schreiben);
  dict_read_begin_from_buffer(&s_iter, s_puffer, (uint16_t)benutzt);
  return benutzt;
}

static void prv_app_starten(void) {
  const PebbleProcessMdSystem *md = (const PebbleProcessMdSystem *)kieselstrasse_app_get_info();
  md->common.main_func();
}

void test_kieselstrasse_app__initialize(void) {
  s_jetzt = 1700000000;
  s_brummen = 0;
  s_empfangen = NULL;
  s_verworfen = NULL;
  s_eingang = s_ausgang = 0;
  s_log_internal__expected = NULL;
  s_log_internal__expected_regex = NULL;
  prv_app_starten();
}

void test_kieselstrasse_app__cleanup(void) {
  s_log_internal__expected = NULL;
  s_log_internal__expected_regex = NULL;
}

void test_kieselstrasse_app__ein_langer_name_passt_in_den_eingang(void) {
  // Eine Ausfahrt mit Wegweisern, auf Kyrillisch: ueber 300 Byte Name.
  char name[400] = "Exit 23 · ";
  for (int i = 0; i < 14; i++) {
    strcat(name, "Улица Ленина ");
  }
  cl_assert(strlen(name) > 300);
  const uint32_t groesse = prv_nachricht(13, 800, name);
  cl_assert(groesse <= s_eingang);
  // Die App schickt nie etwas: kein Speicher fuer einen Ausgang.
  cl_assert_equal_i(s_ausgang, 0);
}

void test_kieselstrasse_app__verworfene_nachricht_steht_im_log(void) {
  cl_assert(s_verworfen);
  const char *erwartet[] = { "Nachricht verworfen: ", NULL };
  s_log_internal__expected_regex = erwartet;
  s_verworfen(APP_MSG_BUSY, NULL);
  cl_assert(*s_log_internal__expected_regex == NULL);
}

void test_kieselstrasse_app__summt_auch_bei_zweimal_rechts(void) {
  cl_assert(s_empfangen);
  prv_nachricht(5, 250, "Bahnhofstrasse");
  s_empfangen(&s_iter, NULL);
  cl_assert_equal_i(s_brummen, 1);
  // Naeher heran: kein Summen.
  s_jetzt += 25;
  prv_nachricht(5, 30, "Bahnhofstrasse");
  s_empfangen(&s_iter, NULL);
  cl_assert_equal_i(s_brummen, 1);
  // Abgebogen, und wieder rechts.
  s_jetzt += 4;
  prv_nachricht(5, 400, "Seestrasse");
  s_empfangen(&s_iter, NULL);
  cl_assert_equal_i(s_brummen, 2);
}
