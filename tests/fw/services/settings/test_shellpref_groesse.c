/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "flash_region/flash_region.h"
#include "pbl/services/filesystem/pfs.h"
#include "pbl/services/settings/settings_file.h"
#include "shell/prefs_private.h"

#include <stdio.h>
#include <string.h>

#include "stubs_analytics.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_tasks.h"
#include "stubs_print.h"
#include "stubs_prompt.h"
#include "stubs_rand_ptr.h"
#include "stubs_serial.h"
#include "stubs_sleep.h"
#include "stubs_system_reset.h"
#include "stubs_task_watchdog.h"
#include "fake_rtc.h"
#include "fake_spi_flash.h"

// Die Datei shellpref beim Update auf SHELL_PREFS_FILE_LEN und beim Rueckweg auf eine
// Firmware mit 2048 - mit dem echten settings_file und PFS auf nachgebautem Flash.
//
// DER BESTAND IST DER DER UHR: Schluessel und Wertlaengen aller 67 Prefs aus s_prefs_table
// von b24 (obelix, aus build/pebbleos.elf gelesen), zehn davon zweimal ueberschrieben -
// so liegen tote Saetze in der Datei wie nach einiger Zeit auf der Uhr.

#define NAME SHELL_PREFS_FILE_NAME
#define ALT 2048   // SHELL_PREFS_FILE_LEN bis b24
#define SEITE 4011 // eine PFS-Seite fuer shellpref (pfs_sector_optimal_size)

typedef struct {
  const char *key;
  int len;
} Eintrag;

static const Eintrag s_b24[] = {
  {"clock24h", 1}, {"timezoneSource", 1}, {"timeSource", 1}, {"automaticTimezoneID", 2},
  {"unitsDistance", 1}, {"unitsWind", 1}, {"lightEnabled", 1},
  {"lightAmbientSensorEnabled", 1}, {"lightTimeoutMs", 4}, {"lightIntensity", 1},
  {"lightColor", 4}, {"lightMotion", 1}, {"lightTouch", 1}, {"touchEnabled", 1},
  {"touchNavMenuEnabled", 1}, {"motionSensitivity", 1}, {"lightDynamicMode", 1},
  {"lightPreset", 1}, {"lightAmbientThreshold", 4}, {"stationaryMode", 1}, {"workerId", 16},
  {"textStyle", 1}, {"systemTextSize", 1}, {"langEnglish", 1}, {"language", 1},
  {"qlUp", 17}, {"qlDown", 17}, {"qlSelect", 17}, {"qlBack", 17}, {"qlSingleClickUp", 17},
  {"qlSingleClickDown", 17}, {"qlComboBackUp", 17}, {"qlComboUpDown", 17},
  {"qlSetupOpened", 1}, {"watchface", 16}, {"welcomeVersion", 1},
  {"displayOrientationLeftHanded", 1}, {"activityPreferences", 9}, {"activityActivated", 4},
  {"activityActivationDelayInsights", 4}, {"activityHealthAppOpened", 1},
  {"activityWorkoutAppOpened", 1}, {"alarmsAppOpened", 1}, {"hrmPreferences", 3},
  {"spo2Preferences", 1}, {"bloodOxygenPreferences", 1}, {"bloodOxygenActivityPreferences", 1},
  {"heartRatePreferences", 6}, {"timelineSettingsOpened", 1}, {"timelineQuickViewEnabled", 1},
  {"timelineQuickViewBeforeTimeMin", 2}, {"timelineQuickViewWatchfaceFit", 1},
  {"coredumpOnRequest", 1}, {"accelShakeLogInfo", 1}, {"vibeLogInfo", 1},
  {"settingsDbsCompactedV1", 1}, {"alsThresholdMigratedV1", 1}, {"alsThresholdMigratedV2", 1},
  {"legacyAppRenderMode", 1}, {"menuScrollWrapAround", 1}, {"menuScrollVibeBehavior", 1},
  {"musicShowVolumeControls", 1}, {"musicShowProgressBar", 1}, {"musicShowAlbumArt", 1},
  {"boulderMessstufe", 1}, {"boulderTimeStyle", 1}, {"tsBedrock", 32},
};
#define N_B24 ((int)(sizeof(s_b24) / sizeof(s_b24[0])))
#define TS_BEDROCK (N_B24 - 1)

static char s_neue[64][16];

// Ein Wert, der von Pref und Runde abhaengt: so faellt jede Verwechslung auf.
static void prv_wert(int idx, int runde, uint8_t *v, int len) {
  for (int i = 0; i < len; i++) {
    v[i] = (uint8_t)(idx * 7 + i + runde * 31);
  }
}

static status_t prv_setze(SettingsFile *f, const char *key, int idx, int len, int runde) {
  uint8_t v[64];
  prv_wert(idx, runde, v, len);
  return settings_file_set(f, key, strlen(key) + 1, v, len);
}

static void prv_pruefe(SettingsFile *f, const char *key, int idx, int len, int runde) {
  uint8_t soll[64], ist[64];
  prv_wert(idx, runde, soll, len);
  cl_assert_equal_i(settings_file_get_len(f, key, strlen(key) + 1), len);
  cl_must_pass(settings_file_get(f, key, strlen(key) + 1, ist, len));
  cl_assert_equal_m(soll, ist, len);
}

static int prv_runde(int i) {
  return i < 10 ? 2 : 0;
}

// Der Bestand wie auf der Uhr, geschrieben von b24 (2048).
static void prv_bestand_b24(void) {
  SettingsFile f;
  cl_must_pass(settings_file_open(&f, NAME, ALT));
  for (int i = 0; i < N_B24; i++) {
    cl_must_pass(prv_setze(&f, s_b24[i].key, i, s_b24[i].len, 0));
  }
  for (int r = 1; r <= 2; r++) {
    for (int i = 0; i < 10; i++) {
      cl_must_pass(prv_setze(&f, s_b24[i].key, i, s_b24[i].len, r));
    }
  }
  cl_assert_equal_i(f.used_space, 1990);
  cl_assert_equal_i(pfs_get_file_size(f.iter.fd), SEITE);
  settings_file_close(&f);
}

static void prv_pruefe_bestand(SettingsFile *f) {
  for (int i = 0; i < N_B24; i++) {
    prv_pruefe(f, s_b24[i].key, i, s_b24[i].len, prv_runde(i));
  }
}

// Neue Prefs zu 50 Byte (Satz 71 Byte), bis nichts mehr passt. Rueckgabe: wie viele.
static int prv_fuellen(SettingsFile *f) {
  int n = 0;
  while (n < 64) {
    snprintf(s_neue[n], sizeof(s_neue[n]), "boulderNeu%02d", n);
    if (prv_setze(f, s_neue[n], 100 + n, 50, 0) != S_SUCCESS) {
      break;
    }
    n++;
  }
  return n;
}

void test_shellpref_groesse__initialize(void) {
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
}

void test_shellpref_groesse__cleanup(void) {
}

// DAS UPDATE: dieselbe Datei, dieselbe eine Seite. settings_file schreibt sie nicht um (das
// taete es erst, wenn die neue Groesse eine zweite Seite braeuchte), die toten Saetze bleiben
// stehen, alle Werte sind da.
void test_shellpref_groesse__update_laesst_die_datei_wie_sie_ist(void) {
  prv_bestand_b24();
  SettingsFile f;
  cl_must_pass(settings_file_open(&f, NAME, SHELL_PREFS_FILE_LEN));
  cl_assert_equal_i(pfs_get_file_size(f.iter.fd), SEITE);
  cl_assert_equal_i(f.dead_space, 484);
  prv_pruefe_bestand(&f);
  settings_file_close(&f);
}

// DER NAECHSTE PREF PASST: ein weiterer Boulder-Pref von 50 Byte, und danach laesst sich
// tsBedrock, der groesste Satz, noch ueberschreiben. Mit 2048 scheiterte schon der neue Pref.
void test_shellpref_groesse__ein_weiterer_pref_und_ueberschreiben_passen(void) {
  prv_bestand_b24();
  SettingsFile f;
  cl_must_pass(settings_file_open(&f, NAME, SHELL_PREFS_FILE_LEN));
  cl_must_pass(prv_setze(&f, "boulderNeu", 99, 50, 0));
  cl_must_pass(prv_setze(&f, s_b24[TS_BEDROCK].key, TS_BEDROCK, 32, 5));
  prv_pruefe(&f, s_b24[TS_BEDROCK].key, TS_BEDROCK, 32, 5);
  prv_pruefe(&f, "boulderNeu", 99, 50, 0);
  settings_file_close(&f);
}

// DER RUECKWEG: eine Firmware mit 2048 oeffnet die volle Datei. Sie liest alles; Schreiben
// scheitert hoechstens (der Bestand liegt ueber ihrer Grenze), aber Verdichten verliert
// nichts, weil der Bestand in die eine Seite passt.
void test_shellpref_groesse__rueckweg_verdichtet_ohne_verlust(void) {
  prv_bestand_b24();
  SettingsFile f;
  cl_must_pass(settings_file_open(&f, NAME, SHELL_PREFS_FILE_LEN));
  const int neue = prv_fuellen(&f);
  printf("\nshellpref mit %d: %d neue Prefs zu 71 Byte passen, belegt %d\n",
         SHELL_PREFS_FILE_LEN, neue, f.used_space);
  cl_assert(f.used_space <= SHELL_PREFS_FILE_LEN);
  cl_assert(f.used_space > ALT);
  settings_file_close(&f);

  cl_must_pass(settings_file_open(&f, NAME, ALT));
  prv_pruefe_bestand(&f);
  cl_must_pass(settings_file_compact(&f));
  cl_assert_equal_i(pfs_get_file_size(f.iter.fd), SEITE);
  prv_pruefe_bestand(&f);
  for (int k = 0; k < neue; k++) {
    prv_pruefe(&f, s_neue[k], 100 + k, 50, 0);
  }
  settings_file_close(&f);

  // Und zurueck auf die neue Firmware: wieder alles da.
  cl_must_pass(settings_file_open(&f, NAME, SHELL_PREFS_FILE_LEN));
  prv_pruefe_bestand(&f);
  settings_file_close(&f);
}
