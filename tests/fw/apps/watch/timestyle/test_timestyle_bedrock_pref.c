/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "apps/watch/timestyle/timestyle.h"
#include "kernel/events.h"
#include "pbl/services/blob_db/settings_blob_db.h"
#include "pbl/services/comm_session/session.h"
#include "pbl/services/filesystem/pfs.h"
#include "pbl/services/notifications/alerts_preferences_private.h"
#include "shell/prefs.h"
#include "shell/prefs_private.h"

#include <string.h>

// Der Weg der Einstellungen von TimeStyle Bedrock durch die Firmware: Boulder
// schreibt "tsBedrock" in die Uhreinstellungen-Synchronisation
// (settings_blob_db_insert), prefs.c bewahrt den Wert auf, und das laufende
// Zifferblatt bekommt genau ein PEBBLE_PREF_CHANGE_EVENT.

// Ohne Kopfzeile, wie in shell/normal/shell_event_loop.c.
extern void shell_prefs_init(void);

// Fakes
////////////////////////////////////////////////////////////////
#include "fake_spi_flash.h"
#include "fake_system_task.h"
#include "fake_kernel_services_notifications.h"

// Stubs
////////////////////////////////////////////////////////////////
#include "stubs_analytics.h"
#include "stubs_app_install_manager.h"
#include "stubs_event_loop.h"
#include "stubs_hexdump.h"
#include "stubs_logging.h"
#include "stubs_mfg_info.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_prompt.h"
#include "stubs_rand_ptr.h"
#include "stubs_sleep.h"
#include "stubs_system_theme.h"
#include "stubs_task_watchdog.h"
#include "stubs_timeline_peek.h"
#include "stubs_ambient_light.h"
#include "stubs_activity.h"

void prefs_sync_init(void) {
}

void i18n_enable(bool enable) {
}

// Was settings_blob_db.c ausser den Einstellungen noch anfasst: das Telefon
// ist nicht verbunden, die Mitteilungs-Einstellungen bleiben unberuehrt.
void bt_persistent_storage_get_cached_system_capabilities(
    PebbleProtocolCapabilities *capabilities_out) {
  memset(capabilities_out, 0, sizeof(*capabilities_out));
}

CommSession *comm_session_get_system_session(void) {
  return NULL;
}

status_t blob_db_sync_db(BlobDBId db_id) {
  return S_SUCCESS;
}

void alerts_preferences_lock(void) {
}

void alerts_preferences_unlock(void) {
}

static int s_mitteilungs_ereignisse;
void alerts_preferences_handle_blob_db_event(PebbleBlobDBEvent *event) {
  s_mitteilungs_ereignisse++;
}

// Die Ereignisse, die prefs.c verschickt.
static int s_ereignisse;
static char s_letzter_schluessel[32];

void event_put(PebbleEvent *event) {
  if (event->type != PEBBLE_PREF_CHANGE_EVENT) {
    return;
  }
  s_ereignisse++;
  strncpy(s_letzter_schluessel, event->pref_change.key, sizeof(s_letzter_schluessel) - 1);
}

// --- Hilfen ---

#define SCHLUESSEL "tsBedrock"

// V1 aus den gemeinsamen Testvektoren (Spezifikation 5.1).
static const uint8_t s_v1[32] = {0x01, 0xf4, 0xc1, 0xdf, 0xff, 0x02, 0x01, 0x02, 0x03, 0x09,
                                 0x0d, 0xfb, 0x2c, 0x8f, 0x03, 0x00, 0x4e, 0x59, 0x43};
// V5
static const uint8_t s_v5[32] = {0x01, 0xc0, 0xff, 0xc0, 0xff, 0x01, 0x01, 0x01, 0x00, 0x02,
                                 0x0c, 0x0e, 0x2e, 0x00, 0x00, 0x00, 0x5a, 0xc3, 0xbc};

// Wie 'pbl feed timestyle' und aeltere Telefone: Insert ohne Zeitstempel,
// Schluessel mit Null am Ende.
static status_t prv_einfuegen(const char *schluessel, const uint8_t *wert, int laenge) {
  return settings_blob_db_insert((const uint8_t *)schluessel, strlen(schluessel) + 1, wert, laenge);
}

// Wie Boulder wirklich schreibt: libpebble3 schickt WatchPrefs immer als
// InsertWithTimestamp. Der Zeitstempel liegt weit vorn, damit kein
// vorhandener Satz "neuer" ist und den Test verfaelscht.
static status_t prv_wie_boulder(const uint8_t *wert, int laenge) {
  return settings_blob_db_insert_with_timestamp((const uint8_t *)SCHLUESSEL, sizeof(SCHLUESSEL),
                                                wert, laenge, (time_t)0x7fffffff);
}

static void prv_wert_ist(const uint8_t *soll) {
  uint8_t ist[TS_BEDROCK_PREF_LAENGE];
  memset(ist, 0xa5, sizeof(ist));
  timestyle_bedrock_blob_lesen(ist);
  cl_assert_equal_m(ist, soll, sizeof(ist));
}

void test_timestyle_bedrock_pref__initialize(void) {
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  // Eine frische Uhr: der Wert in prefs.c steht sonst aus dem vorigen Test.
  static const uint8_t null[TS_BEDROCK_PREF_LAENGE] = {0};
  cl_assert(prefs_private_write_backing((const uint8_t *)SCHLUESSEL, sizeof(SCHLUESSEL), null,
                                        sizeof(null)));
  shell_prefs_init();
  settings_blob_db_init();
  s_ereignisse = 0;
  s_mitteilungs_ereignisse = 0;
  memset(s_letzter_schluessel, 0, sizeof(s_letzter_schluessel));
}

void test_timestyle_bedrock_pref__cleanup(void) {
}

// --- Ohne Boulder ---

void test_timestyle_bedrock_pref__frische_uhr_liefert_nullen(void) {
  // Fassung 0: das Zifferblatt nimmt die Vorgaben.
  static const uint8_t null[TS_BEDROCK_PREF_LAENGE] = {0};
  prv_wert_ist(null);
}

// --- Der Weg aus Boulder ---

void test_timestyle_bedrock_pref__wert_kommt_an_mit_einem_ereignis(void) {
  cl_assert_equal_i(prv_einfuegen(SCHLUESSEL, s_v1, sizeof(s_v1)), S_SUCCESS);

  prv_wert_ist(s_v1);
  cl_assert_equal_i(s_ereignisse, 1);
  cl_assert_equal_s(s_letzter_schluessel, SCHLUESSEL);
  cl_assert_equal_i(s_mitteilungs_ereignisse, 0);

  // Der Nachfolger ersetzt ihn, wieder mit genau einem Ereignis.
  cl_assert_equal_i(prv_einfuegen(SCHLUESSEL, s_v5, sizeof(s_v5)), S_SUCCESS);
  prv_wert_ist(s_v5);
  cl_assert_equal_i(s_ereignisse, 2);
}

// Derselbe Ablauf auf dem Weg, den Boulder wirklich nimmt (mit Zeitstempel).
void test_timestyle_bedrock_pref__mit_zeitstempel_kommt_an(void) {
  cl_assert_equal_i(prv_wie_boulder(s_v1, sizeof(s_v1)), S_SUCCESS);
  prv_wert_ist(s_v1);
  cl_assert_equal_i(s_ereignisse, 1);
  cl_assert_equal_s(s_letzter_schluessel, SCHLUESSEL);

  cl_assert_equal_i(prv_wie_boulder(s_v5, sizeof(s_v5)), S_SUCCESS);
  prv_wert_ist(s_v5);
  cl_assert_equal_i(s_ereignisse, 2);
}

void test_timestyle_bedrock_pref__schluessel_ohne_null_geht_auch(void) {
  cl_assert_equal_i(
      settings_blob_db_insert((const uint8_t *)SCHLUESSEL, strlen(SCHLUESSEL), s_v1, sizeof(s_v1)),
      S_SUCCESS);
  prv_wert_ist(s_v1);
  cl_assert_equal_i(s_ereignisse, 1);
}

// Die Uhr meldet ihn einem neuen Telefon unveraendert zurueck.
void test_timestyle_bedrock_pref__lesbar_fuer_den_abgleich(void) {
  cl_assert_equal_i(prv_einfuegen(SCHLUESSEL, s_v1, sizeof(s_v1)), S_SUCCESS);

  const uint8_t *k = (const uint8_t *)SCHLUESSEL;
  cl_assert_equal_i(settings_blob_db_get_len(k, sizeof(SCHLUESSEL)), 32);
  uint8_t gelesen[32];
  cl_assert_equal_i(settings_blob_db_read(k, sizeof(SCHLUESSEL), gelesen, sizeof(gelesen)),
                    S_SUCCESS);
  cl_assert_equal_m(gelesen, s_v1, sizeof(s_v1));
}

void test_timestyle_bedrock_pref__ueberlebt_den_neustart(void) {
  cl_assert_equal_i(prv_einfuegen(SCHLUESSEL, s_v1, sizeof(s_v1)), S_SUCCESS);
  shell_prefs_init();
  prv_wert_ist(s_v1);
}

// --- Was abgewiesen wird ---

void test_timestyle_bedrock_pref__falsche_laenge_aendert_nichts(void) {
  cl_assert_equal_i(prv_einfuegen(SCHLUESSEL, s_v1, sizeof(s_v1)), S_SUCCESS);
  cl_assert_equal_i(s_ereignisse, 1);

  // 31 Byte (V6): abgewiesen, der vorige Wert bleibt, kein Ereignis.
  cl_assert(FAILED(prv_einfuegen(SCHLUESSEL, s_v5, 31)));
  prv_wert_ist(s_v1);
  cl_assert_equal_i(s_ereignisse, 1);

  // 33 Byte: ebenso.
  uint8_t lang[33] = {0};
  memcpy(lang, s_v5, sizeof(s_v5));
  cl_assert(FAILED(prv_einfuegen(SCHLUESSEL, lang, sizeof(lang))));
  prv_wert_ist(s_v1);
  cl_assert_equal_i(s_ereignisse, 1);

  // Auch nach einem Neustart: in der Datei steht noch der gute Wert.
  shell_prefs_init();
  prv_wert_ist(s_v1);
}

// Wie oben, aber mit Zeitstempel - so schreibt Boulder. Frueher fehlte die
// Pruefung hier: 31 Byte landeten in der Datei, der Wert war nach dem
// naechsten Lesen weg, und das Telefon bekam trotzdem Success.
void test_timestyle_bedrock_pref__mit_zeitstempel_falsche_laenge_aendert_nichts(void) {
  cl_assert_equal_i(prv_wie_boulder(s_v1, sizeof(s_v1)), S_SUCCESS);
  cl_assert_equal_i(s_ereignisse, 1);

  // InvalidData fuer das Telefon: es markiert den Eintrag nicht als synchron.
  cl_assert_equal_i(prv_wie_boulder(s_v5, 31), E_INVALID_ARGUMENT);
  prv_wert_ist(s_v1);
  uint8_t lang[33] = {0};
  memcpy(lang, s_v5, sizeof(s_v5));
  cl_assert_equal_i(prv_wie_boulder(lang, sizeof(lang)), E_INVALID_ARGUMENT);
  prv_wert_ist(s_v1);
  // Laenge 0 hiesse in settings_file_set "loeschen" - auch das nicht.
  cl_assert_equal_i(prv_wie_boulder(s_v5, 0), E_INVALID_ARGUMENT);
  prv_wert_ist(s_v1);
  cl_assert_equal_i(s_ereignisse, 1);

  // In der Datei steht weiter der gute Wert, auch fuer den Abgleich.
  cl_assert_equal_i(settings_blob_db_get_len((const uint8_t *)SCHLUESSEL, sizeof(SCHLUESSEL)), 32);
  shell_prefs_init();
  prv_wert_ist(s_v1);
}

void test_timestyle_bedrock_pref__unbekannter_schluessel_wird_abgewiesen(void) {
  static const uint8_t null[TS_BEDROCK_PREF_LAENGE] = {0};

  // Mit Null am Ende ...
  cl_assert_equal_i(prv_einfuegen("tsBedrockX", s_v1, sizeof(s_v1)), E_INVALID_OPERATION);
  // ... und ohne: frueher ging das als "tsBedrock" durch (ein Byte laenger).
  cl_assert_equal_i(
      settings_blob_db_insert((const uint8_t *)"tsBedrockX", strlen("tsBedrockX"), s_v1, 32),
      E_INVALID_OPERATION);
  // Ein kuerzerer Anfang ist auch nicht derselbe Schluessel.
  cl_assert_equal_i(prv_einfuegen("tsBedroc", s_v1, sizeof(s_v1)), E_INVALID_OPERATION);

  prv_wert_ist(null);
  cl_assert_equal_i(s_ereignisse, 0);
}

// Die Pruefung der Laenge gilt nur fuer tsBedrock: ein anderer Schluessel der
// Whitelist geht weiter seinen alten Weg.
void test_timestyle_bedrock_pref__andere_schluessel_unveraendert(void) {
  const bool an = true;
  cl_assert_equal_i(prv_einfuegen("clock24h", (const uint8_t *)&an, sizeof(an)), S_SUCCESS);
  cl_assert_equal_b(shell_prefs_get_clock_24h_style(), true);
  cl_assert_equal_i(s_ereignisse, 1);
  cl_assert_equal_s(s_letzter_schluessel, "clock24h");
}
