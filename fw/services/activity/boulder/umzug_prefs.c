/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2: b25s eigene Prefs aus shellpref raeumen (umzug.h, Gegenpruefung B1).
//
// WARUM: V2 hat Upstreams Grenze fuer shellpref (2048 B, b25 hatte 3072 B). Lebend belegt der
// b25-Bestand danach rund 1990 B (T8.5), und settings_file prueft die Grenze auch beim
// Ueberschreiben eines vorhandenen Satzes (settings_file.c:513). Schreibt das Telefon die zwei
// Farb-Schluessel, die b25 nicht kannte (settings_blob_db.c:93-94, 36 und 32 B), scheitert
// danach jeder groessere Satz - Zifferblatt, Kurzwahl, Pulseinstellung (Kriterium 8).
// tsBedrock (b25s Zifferblatt, entfaellt) und boulderTimeStyle liest V2 nicht mehr: beide frei
// gibt etwa 76 B.
//
// BEI JEDEM BOOT, nicht nur beim ersten: wer auf b25 zurueck und wieder auf V2 geht, bringt
// sie neu mit. Geschrieben wird nur, wenn ein Schluessel da ist (ein Loeschen ist ein Satz).
// Ans Telefon geht nichts: beide stehen nicht in settings_blob_db's Liste.

#include "pbl/services/activity/boulder/umzug.h"

#include "pbl/services/settings/settings_file.h"
#include "shell/prefs_private.h"
#include <pbl/logging/logging.h>

#include <string.h>

PBL_LOG_MODULE_DECLARE(service_activity, CONFIG_SERVICE_ACTIVITY_LOG_LEVEL);

static const char *const s_b25_schluessel[] = {"tsBedrock", "boulderTimeStyle"};

int boulder_umzug_prefs_aufraeumen(void) {
  int geloescht = 0;
  // Unter der Prefs-Sperre wie Pebbles eigene Prefs: prefs.c oeffnet shellpref je Zugriff.
  prefs_private_lock();
  SettingsFile datei;
  status_t st = settings_file_open(&datei, SHELL_PREFS_FILE_NAME, SHELL_PREFS_FILE_LEN);
  if (st != S_SUCCESS) {
    prefs_private_unlock();
    PBL_LOG_WRN("Boulder: shellpref nicht geoeffnet (%d)", (int)st);
    return 0;
  }
  for (size_t i = 0; i < sizeof(s_b25_schluessel) / sizeof(s_b25_schluessel[0]); i++) {
    // Mit Null am Ende, wie prefs.c und settings_blob_db (kanonisch) schreiben.
    const size_t n = strlen(s_b25_schluessel[i]) + 1;
    if (!settings_file_exists(&datei, s_b25_schluessel[i], n)) {
      continue;
    }
    st = settings_file_delete(&datei, s_b25_schluessel[i], n);
    if (st == S_SUCCESS) {
      geloescht++;
    } else {
      PBL_LOG_WRN("Boulder: %s nicht geloescht (%d)", s_b25_schluessel[i], (int)st);
    }
  }
  settings_file_close(&datei);
  prefs_private_unlock();
  if (geloescht) {
    PBL_LOG_INFO("Boulder: %d b25-Prefs aus shellpref geloescht", geloescht);
  }
  return geloescht;
}
