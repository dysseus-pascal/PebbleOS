/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "fake_logging.h"
#include "stubs_logging.h"

#include "services/activity/boulder/sport/plattform.h"
#include "services/activity/boulder/sport/schluessel.h"

#include "pbl/services/persist.h"
#include "pbl/services/settings/settings_file.h"
#include "system/status_codes.h"

// Der Persist von Kieselsport (speicher.c) ueber der Datei der Uhr. Die Datei
// ist hier nachgebaut - eine Grenze zum Flash, die auf Wunsch scheitert wie
// eine volle oder kaputte Datei.

// --- Persist-Dienst: eine Datei, gezaehlt geoeffnet ---

static SettingsFile s_datei;
static int s_nutzer;
static int s_gesperrt;

void persist_service_client_open(const Uuid *uuid) { s_nutzer++; }
void persist_service_client_close(const Uuid *uuid) { s_nutzer--; }
SettingsFile *persist_service_lock_and_get_store(const Uuid *uuid) {
  s_gesperrt++;
  return &s_datei;
}
void persist_service_unlock_store(SettingsFile *store) { s_gesperrt--; }

// --- Die Datei: ein Schluessel, ein Wert ---

static bool s_da;
static uint32_t s_key;
static uint8_t s_wert[256];
static size_t s_laenge;
static status_t s_schreiben_ergebnis;
static status_t s_lesen_ergebnis;
static status_t s_loeschen_ergebnis;

bool settings_file_exists(SettingsFile *file, const void *key, size_t key_len) {
  return s_da && key_len == sizeof(uint32_t) && memcmp(key, &s_key, key_len) == 0;
}

int settings_file_get_len(SettingsFile *file, const void *key, size_t key_len) {
  return settings_file_exists(file, key, key_len) ? (int)s_laenge : 0;
}

status_t settings_file_get(SettingsFile *file, const void *key, size_t key_len, void *val_out,
                           size_t val_out_len) {
  memset(val_out, 0, val_out_len);
  if (!settings_file_exists(file, key, key_len)) return E_DOES_NOT_EXIST;
  if (FAILED(s_lesen_ergebnis)) return s_lesen_ergebnis;
  memcpy(val_out, s_wert, val_out_len < s_laenge ? val_out_len : s_laenge);
  return S_SUCCESS;
}

status_t settings_file_set(SettingsFile *file, const void *key, size_t key_len, const void *val,
                           size_t val_len) {
  if (FAILED(s_schreiben_ergebnis)) return s_schreiben_ergebnis;
  s_da = true;
  memcpy(&s_key, key, sizeof(s_key));
  memcpy(s_wert, val, val_len);
  s_laenge = val_len;
  return S_SUCCESS;
}

status_t settings_file_delete(SettingsFile *file, const void *key, size_t key_len) {
  if (FAILED(s_loeschen_ergebnis)) return s_loeschen_ergebnis;
  s_da = false;
  return S_SUCCESS;
}

void test_kieselsport_speicher__initialize(void) {
  s_nutzer = 0;
  s_gesperrt = 0;
  s_da = false;
  s_laenge = 0;
  s_schreiben_ergebnis = S_SUCCESS;
  s_lesen_ergebnis = S_SUCCESS;
  s_loeschen_ergebnis = S_SUCCESS;
  s_log_internal__expected = NULL;
  s_log_internal__expected_regex = NULL;
}

void test_kieselsport_speicher__cleanup(void) {
  s_log_internal__expected = NULL;
  s_log_internal__expected_regex = NULL;
  // Jeder Zugriff gibt die Datei wieder frei.
  cl_assert_equal_i(s_nutzer, 0);
  cl_assert_equal_i(s_gesperrt, 0);
}

void test_kieselsport_speicher__gelingt_ohne_log(void) {
  const char *keins[] = { NULL };
  s_log_internal__expected = keins;
  ks_persist_write_int(PERSIST_KURVE_ANZAHL, 42);
  cl_assert(ks_persist_exists(PERSIST_KURVE_ANZAHL));
  cl_assert_equal_i(ks_persist_read_int(PERSIST_KURVE_ANZAHL), 42);
  // Ein fehlender Schluessel ist kein Fehler.
  cl_assert_equal_i(ks_persist_read_int(PERSIST_KURVE_AB), 0);
  ks_persist_delete(PERSIST_KURVE_ANZAHL);
  cl_assert(!ks_persist_exists(PERSIST_KURVE_ANZAHL));
}

void test_kieselsport_speicher__gescheitertes_schreiben_steht_im_log(void) {
  const char *erwartet[] = {
    "Kieselsport: Persist schreiben 50 gescheitert \\(-",
    "Kieselsport: Persist schreiben 40 gescheitert \\(-",
    NULL,
  };
  s_log_internal__expected_regex = erwartet;
  s_schreiben_ergebnis = E_OUT_OF_STORAGE;
  const uint8_t kurve[4] = { 100, 110, 120, 130 };
  ks_persist_write_data(PERSIST_KURVE_BASIS, kurve, sizeof(kurve));
  ks_persist_write_int(PERSIST_KURVE_ANZAHL, 4);
  cl_assert(!ks_persist_exists(PERSIST_KURVE_BASIS));
  cl_assert(*s_log_internal__expected_regex == NULL);
}

void test_kieselsport_speicher__lese_und_loeschfehler_stehen_im_log(void) {
  ks_persist_write_int(PERSIST_WARTET, 7);
  const char *erwartet[] = {
    "Kieselsport: Persist lesen 30 gescheitert",
    "Kieselsport: Persist lesen 30 gescheitert",
    "Kieselsport: Persist loeschen 30 gescheitert",
    NULL,
  };
  s_log_internal__expected_regex = erwartet;
  s_lesen_ergebnis = E_INTERNAL;
  s_loeschen_ergebnis = E_INTERNAL;
  uint8_t puffer[8];
  cl_assert_equal_i(ks_persist_read_data(PERSIST_WARTET, puffer, sizeof(puffer)), 0);
  cl_assert_equal_i(ks_persist_read_int(PERSIST_WARTET), 0);
  ks_persist_delete(PERSIST_WARTET);
  cl_assert(*s_log_internal__expected_regex == NULL);
}
