/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "plattform.h"

#include "applib/persist.h"
#include "pbl/services/persist.h"
#include "pbl/services/settings/settings_file.h"
#include "pbl/util/math.h"
#include "system/status_codes.h"

#include <inttypes.h>
#include <pbl/logging/logging.h>

// Der Persist von Kieselsport (plattform.h).
//
// DER SPEICHER VON KIESELSPORT, NICHT DER DER LAUFENDEN APP. Jeder Zugriff
// meldet sich als Nutzer der Datei an und wieder ab; laeuft die App oder ein
// Training, bleibt sie dazwischen offen, und das An- und Abmelden kostet
// nur einen Zaehler.
//
// EIN FEHLER STEHT IM LOG. Frueher fiel ein gescheitertes Schreiben still
// weg - und eine halb geschriebene Kurve oder Zusammenfassung sah danach
// niemand an. Die Aufrufer machen weiter wie bisher; was im Persist fehlt,
// faengt die App beim Senden ab (telefon.c, Kurve mit Luecke).

static const Uuid s_uuid = KIESELSPORT_UUID_INIT;

static SettingsFile *prv_auf(void) {
  persist_service_client_open(&s_uuid);
  return persist_service_lock_and_get_store(&s_uuid);
}

static void prv_zu(SettingsFile *datei) {
  persist_service_unlock_store(datei);
  persist_service_client_close(&s_uuid);
}

static void prv_fehler(const char *was, uint32_t key, status_t status) {
  PBL_LOG_ERR("Kieselsport: Persist %s %" PRIu32 " gescheitert (%" PRId32 ")", was, key,
              (int32_t)status);
}

void ks_persist_offen_halten(bool an) {
  if (an) {
    persist_service_client_open(&s_uuid);
  } else {
    persist_service_client_close(&s_uuid);
  }
}

bool ks_persist_exists(uint32_t key) {
  SettingsFile *datei = prv_auf();
  const bool da = settings_file_exists(datei, &key, sizeof(key));
  prv_zu(datei);
  return da;
}

int32_t ks_persist_read_int(uint32_t key) {
  int32_t wert = 0;
  SettingsFile *datei = prv_auf();
  const status_t status = settings_file_get(datei, &key, sizeof(key), &wert, sizeof(wert));
  prv_zu(datei);
  // Ein fehlender Schluessel ist kein Fehler: dann gilt 0, wie bisher.
  if (FAILED(status) && status != E_DOES_NOT_EXIST) {
    prv_fehler("lesen", key, status);
  }
  return wert;
}

void ks_persist_write_int(uint32_t key, int32_t wert) {
  SettingsFile *datei = prv_auf();
  const status_t status = settings_file_set(datei, &key, sizeof(key), &wert, sizeof(wert));
  prv_zu(datei);
  if (FAILED(status)) {
    prv_fehler("schreiben", key, status);
  }
}

int ks_persist_read_data(uint32_t key, void *puffer, size_t groesse) {
  SettingsFile *datei = prv_auf();
  int gelesen = settings_file_get_len(datei, &key, sizeof(key));
  status_t status = S_SUCCESS;
  if (gelesen > 0) {
    gelesen = MIN((int)groesse, gelesen);
    status = settings_file_get(datei, &key, sizeof(key), puffer, gelesen);
    if (FAILED(status)) {
      gelesen = 0;
    }
  }
  prv_zu(datei);
  if (FAILED(status)) {
    prv_fehler("lesen", key, status);
  }
  return gelesen > 0 ? gelesen : 0;
}

void ks_persist_write_data(uint32_t key, const void *daten, size_t groesse) {
  SettingsFile *datei = prv_auf();
  const status_t status =
      settings_file_set(datei, &key, sizeof(key), daten, MIN(groesse, PERSIST_DATA_MAX_LENGTH));
  prv_zu(datei);
  if (FAILED(status)) {
    prv_fehler("schreiben", key, status);
  }
}

void ks_persist_delete(uint32_t key) {
  SettingsFile *datei = prv_auf();
  status_t status = S_SUCCESS;
  if (settings_file_exists(datei, &key, sizeof(key))) {
    status = settings_file_delete(datei, &key, sizeof(key));
  }
  prv_zu(datei);
  if (FAILED(status)) {
    prv_fehler("loeschen", key, status);
  }
}
