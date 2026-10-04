/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "apps/system/kieselstrasse/schluessel.h"
#include "apps/system/kieselstrasse/texte.h"
#include "apps/system/kieselstrasse/weg.h"

#include "applib/graphics/utf8.h"

#include "stubs_logging.h"
#include "stubs_passert.h"
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

void test_kieselstrasse_weg__neuer_schritt_bei_anderer_art(void) {
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

void test_kieselstrasse_weg__zweimal_dieselbe_art_ist_ein_neuer_schritt(void) {
  weg_init();
  // "Rechts, dann wieder rechts": an der ersten Abzweigung springt die
  // Entfernung zur naechsten nach oben.
  cl_assert(weg_uebernimm(prv_nachricht(5, 250, "A")));
  cl_assert(!weg_uebernimm(prv_nachricht(5, 120, NULL)));
  cl_assert(!weg_uebernimm(prv_nachricht(5, 30, NULL)));
  cl_assert(weg_uebernimm(prv_nachricht(5, 400, "B")));
  cl_assert_equal_i(s_schreibvorgaenge, 2);
  // Dicht hintereinander: zehn Meter vor der ersten, achtzig zur zweiten.
  cl_assert(!weg_uebernimm(prv_nachricht(5, 10, NULL)));
  cl_assert(weg_uebernimm(prv_nachricht(5, 80, NULL)));
  // Eine neue Route weit weg zaehlt auch ohne Abzweigung davor.
  cl_assert(!weg_uebernimm(prv_nachricht(5, 60, NULL)));
  cl_assert(weg_uebernimm(prv_nachricht(5, 900, NULL)));
}

void test_kieselstrasse_weg__flattern_summt_nicht_doppelt(void) {
  weg_init();
  cl_assert(weg_uebernimm(prv_nachricht(5, 300, "Bahnhofstrasse")));
  // Der Name flattert: kein neuer Schritt.
  cl_assert(!weg_uebernimm(prv_nachricht(5, 290, "Bahnhofstr.")));
  cl_assert(!weg_uebernimm(prv_nachricht(5, 280, "Bahnhofstrasse")));
  // Die Entfernung zappelt um ein paar Dutzend Meter - weit weg wie nah.
  cl_assert(!weg_uebernimm(prv_nachricht(5, 320, NULL)));
  cl_assert(!weg_uebernimm(prv_nachricht(5, 40, NULL)));
  cl_assert(!weg_uebernimm(prv_nachricht(5, 70, NULL)));
  cl_assert(!weg_uebernimm(prv_nachricht(5, 45, NULL)));
  // Eine Nachricht ohne Entfernung und dann wieder eine: kein Sprung.
  cl_assert(!weg_uebernimm(prv_nachricht(5, -1, NULL)));
  cl_assert(!weg_uebernimm(prv_nachricht(5, 40, NULL)));
}

// --- Lange Namen: an Zeichengrenzen gekuerzt ---

void test_kieselstrasse_weg__langer_name_bleibt_gueltiges_utf8(void) {
  weg_init();
  // 51 Byte, das "â" faellt genau auf die Grenze von 47.
  const char *neuenburg = "Exit 2 · Route de la Gare de Neuchâtel-Serrières";
  cl_assert(strlen(neuenburg) > KST_STRASSE_LEN);
  weg_uebernimm(prv_nachricht(13, 200, neuenburg));
  cl_assert(utf8_is_valid_string(weg_stand()->strasse));
  cl_assert(strlen(weg_stand()->strasse) <= KST_STRASSE_LEN - 1);
  cl_assert(strncmp(weg_stand()->strasse, neuenburg, strlen(weg_stand()->strasse)) == 0);
  cl_assert(strlen(weg_stand()->strasse) >= KST_STRASSE_LEN - 4);
  // Kyrillisch: jedes Zeichen zwei Byte.
  weg_uebernimm(prv_nachricht(5, 100, "ПроспектПроспектПроспект"));
  cl_assert(utf8_is_valid_string(weg_stand()->strasse));
  cl_assert_equal_s(weg_stand()->strasse, "ПроспектПроспектПроспек");
}

void test_kieselstrasse_weg__kaputt_gespeicherter_name_wird_repariert(void) {
  // So kuerzte die vorige Fassung: mitten im "â" (0xC3 0xA2).
  Weg w = { .abbiegeart = 5, .entfernung_m = 200, .rest_m = -1, .empfangen = s_jetzt };
  strcpy(w.strasse, "Rue de Neuch\xC3");
  memcpy(s_persist, &w, sizeof(w));
  s_persist_laenge = sizeof(w);
  weg_init();
  cl_assert(utf8_is_valid_string(weg_stand()->strasse));
  cl_assert_equal_s(weg_stand()->strasse, "Rue de Neuch");
}

// --- Zahlen und Texte jeder Form ---

void test_kieselstrasse_weg__zahlen_jeder_breite(void) {
  weg_init();
  DictionaryIterator schreiben;
  dict_write_begin(&schreiben, s_puffer, sizeof(s_puffer));
  dict_write_int8(&schreiben, MESSAGE_KEY_ABBIEGEART, 5);
  dict_write_uint16(&schreiben, MESSAGE_KEY_ENTFERNUNG, 450);
  dict_write_uint8(&schreiben, MESSAGE_KEY_AUSFAHRT, 2);
  dict_write_int16(&schreiben, MESSAGE_KEY_WINKEL, -90);
  dict_read_begin_from_buffer(&s_iter, s_puffer, (uint16_t)dict_write_end(&schreiben));
  cl_assert(weg_uebernimm(&s_iter));
  cl_assert_equal_i(weg_stand()->abbiegeart, 5);
  cl_assert_equal_i((int)weg_stand()->entfernung_m, 450);
  cl_assert_equal_i(weg_ausfahrt(), 2);
  cl_assert_equal_i(weg_winkel(), -90);
}

// DIE ANKUNFT NACH 2038: Boulder schickt TUPLE_INT mit den Bits der vorzeichenlosen
// Sekundenzahl. Als int32 gelesen waere der 1.9.2039 negativ, und die Ankunft verschwaende
// vom Schirm (nav_window.c zeigt nur eine gesetzte). Ein anderer Sender darf TUPLE_UINT
// schicken; ein Zeitpunkt in zwei Byte ist keiner und laesst den vorigen stehen.
static void prv_ankunft(TupleType typ, uint8_t breite, uint32_t bits) {
  DictionaryIterator schreiben;
  dict_write_begin(&schreiben, s_puffer, sizeof(s_puffer));
  dict_write_int32(&schreiben, MESSAGE_KEY_ABBIEGEART, 5);
  if (typ == TUPLE_INT && breite == 4) dict_write_int32(&schreiben, MESSAGE_KEY_ANKUNFT, (int32_t)bits);
  if (typ == TUPLE_UINT && breite == 4) dict_write_uint32(&schreiben, MESSAGE_KEY_ANKUNFT, bits);
  if (typ == TUPLE_INT && breite == 2) dict_write_int16(&schreiben, MESSAGE_KEY_ANKUNFT, (int16_t)bits);
  dict_read_begin_from_buffer(&s_iter, s_puffer, (uint16_t)dict_write_end(&schreiben));
  weg_uebernimm(&s_iter);
}

void test_kieselstrasse_weg__ankunft_nach_2038_bleibt_positiv(void) {
  weg_init();
  const uint32_t heute = 1790445600u;      // 26.9.2026
  const uint32_t spaeter = 2198995200u;    // 1.9.2039, ueber INT32_MAX
  prv_ankunft(TUPLE_INT, 4, heute);
  cl_assert(weg_stand()->ankunft == heute);
  prv_ankunft(TUPLE_INT, 4, spaeter);
  cl_assert(weg_stand()->ankunft == spaeter);
  cl_assert((uint64_t)weg_stand()->ankunft > (uint64_t)INT32_MAX);
  prv_ankunft(TUPLE_UINT, 4, spaeter + 60);
  cl_assert(weg_stand()->ankunft == spaeter + 60);
  prv_ankunft(TUPLE_INT, 2, 1234);
  cl_assert(weg_stand()->ankunft == spaeter + 60);
  // Und der Persist traegt dieselben vier Byte.
  weg_sichern();
  weg_init();
  cl_assert(weg_stand()->ankunft == spaeter + 60);
}

void test_kieselstrasse_weg__text_ohne_abschluss_bleibt_im_tupel(void) {
  weg_init();
  DictionaryIterator schreiben;
  dict_write_begin(&schreiben, s_puffer, sizeof(s_puffer));
  dict_write_int32(&schreiben, MESSAGE_KEY_ABBIEGEART, 5);
  dict_write_cstring(&schreiben, MESSAGE_KEY_STRASSE, "Abc");
  // Dahinter steht etwas anderes - wie im Eingangspuffer der Uhr.
  dict_write_cstring(&schreiben, MESSAGE_KEY_REST + 100, "YYYYYYYYYYYYYYYYYYYY");
  dict_read_begin_from_buffer(&s_iter, s_puffer, (uint16_t)dict_write_end(&schreiben));
  // Die Null des Namens ueberschreiben: ein Sender, der sie vergisst.
  Tuple *t = dict_find(&s_iter, MESSAGE_KEY_STRASSE);
  cl_assert(t && t->length == 4);
  t->value->data[3] = 'X';
  cl_assert(weg_uebernimm(&s_iter));
  cl_assert(strlen(weg_stand()->strasse) <= 4);
  cl_assert(strncmp(weg_stand()->strasse, "Abc", 3) == 0);
}

// --- Unsinnige Entfernungen ---

void test_kieselstrasse_weg__unsinnige_entfernung_ist_keine(void) {
  weg_init();
  weg_uebernimm(prv_nachricht(5, INT32_MAX, NULL));
  cl_assert(!weg_hat_zahl());
  weg_uebernimm(prv_nachricht(-1, 1200, NULL));
  cl_assert(weg_hat_zahl());
  // Auch aus einem alten Persist.
  Weg w = { .abbiegeart = 5, .entfernung_m = INT32_MAX - 10, .rest_m = INT32_MAX,
            .empfangen = s_jetzt };
  memcpy(s_persist, &w, sizeof(w));
  s_persist_laenge = sizeof(w);
  weg_init();
  cl_assert(!weg_hat_zahl());
  cl_assert_equal_i((int)weg_stand()->rest_m, -1);
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

// Ein Kreisverkehr wie von Boulder: Art, Entfernung, Ausfahrt und Winkel.
static DictionaryIterator *prv_kreisel(int art, int ausfahrt, int winkel) {
  DictionaryIterator schreiben;
  cl_assert_equal_i(dict_write_begin(&schreiben, s_puffer, sizeof(s_puffer)), DICT_OK);
  dict_write_int32(&schreiben, MESSAGE_KEY_ABBIEGEART, art);
  dict_write_int32(&schreiben, MESSAGE_KEY_ENTFERNUNG, 120);
  dict_write_int32(&schreiben, MESSAGE_KEY_AUSFAHRT, ausfahrt);
  dict_write_int32(&schreiben, MESSAGE_KEY_WINKEL, winkel);
  const uint32_t benutzt = dict_write_end(&schreiben);
  dict_read_begin_from_buffer(&s_iter, s_puffer, (uint16_t)benutzt);
  return &s_iter;
}

void test_kieselstrasse_weg__ausfahrt_und_winkel(void) {
  weg_init();
  cl_assert_equal_i(weg_ausfahrt(), 0);
  cl_assert_equal_i(weg_winkel(), WEG_KEIN_WINKEL);
  weg_uebernimm(prv_kreisel(13, 3, -90));
  cl_assert_equal_i(weg_ausfahrt(), 3);
  cl_assert_equal_i(weg_winkel(), -90);
  // Nur die Entfernung: die Ausfahrt bleibt.
  weg_uebernimm(prv_nachricht(-1, 60, NULL));
  cl_assert_equal_i(weg_ausfahrt(), 3);
  // Die naechste Abbiegeart ohne Ausfahrt loescht sie.
  weg_uebernimm(prv_nachricht(5, 300, NULL));
  cl_assert_equal_i(weg_ausfahrt(), 0);
  cl_assert_equal_i(weg_winkel(), WEG_KEIN_WINKEL);
  // Ein Winkel ausserhalb von -180..180 ist keiner.
  weg_uebernimm(prv_kreisel(13, 2, 400));
  cl_assert_equal_i(weg_winkel(), WEG_KEIN_WINKEL);
}

void test_kieselstrasse_weg__ausfahrt_nicht_im_persist(void) {
  weg_init();
  weg_uebernimm(prv_kreisel(13, 3, 0));
  weg_sichern();
  cl_assert_equal_i(s_persist_laenge, (int)sizeof(Weg));
  weg_init();
  cl_assert_equal_i(weg_stand()->abbiegeart, 13);
  cl_assert_equal_i(weg_ausfahrt(), 0);
}
