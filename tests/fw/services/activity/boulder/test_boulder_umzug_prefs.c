/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "flash_region/flash_region.h"
#include "pbl/services/activity/boulder/messstufe.h"
#include "pbl/services/activity/boulder/umzug.h"
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
#include "stubs_rand_ptr.h"
#include "stubs_serial.h"
#include "stubs_sleep.h"
#include "stubs_system_reset.h"
#include "stubs_task_wdt.h"
#include "fake_rtc.h"
#include "fake_spi_flash.h"

// T8.5 (Spezifikation 8): shellpref beim Wechsel von b25 (3072 B) auf V2 (Upstream, 2048 B),
// mit dem echten settings_file und PFS auf nachgebautem Flash. V2 oeffnet die Datei, die b25
// hinterlaesst, liest alle Werte und SCHREIBT DANACH - das Oeffnen verliert nichts, aber ein
// Bestand ueber 2048 B liesse spaetere Schreibvorgaenge scheitern (settings_file.c:513).
//
// DER BESTAND: die 67 Prefs von b25 (M: tests/fw/services/settings/test_shellpref_groesse.c,
// aus s_prefs_table der Uhr gelesen) und die 64 von V2 (s_prefs_table aus pebbleos.elf von
// obelix@pvt, v4.39). b25 hat oft ueberschrieben: tote Saetze bis nahe an seine Grenze.

#define NAME SHELL_PREFS_FILE_NAME
#define B25_GROESSE 3072  // M: src/fw/shell/prefs_private.h:18

typedef struct {
  const char *key;
  int len;
} Eintrag;

static const Eintrag s_b25[] = {
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
#define N_B25 ((int)(sizeof(s_b25) / sizeof(s_b25[0])))

// V2 (v4.39): dieselben ohne die drei Boulder-Prefs; die Messstufe schreibt messstufe.c.
static const Eintrag s_v2[] = {
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
};
#define N_V2 ((int)(sizeof(s_v2) / sizeof(s_v2[0])))

// Gegenpruefung B1: zwei Prefs, die b25 nicht kannte, das Telefon unter V2 aber schreiben darf
// (settings_blob_db.c:93-94, GColor 1 B) - im Bestand oben fehlen sie.
static const Eintrag s_telefon[] = {{"settingsMenuHighlightColor", 1}, {"appsMenuHighlightColor", 1}};
#define N_TELEFON ((int)(sizeof(s_telefon) / sizeof(s_telefon[0])))

// b25-Prefs, die V2 nicht mehr liest und der Umzug loescht (umzug_prefs.c).
static bool prv_geloescht(const char *key) {
  return !strcmp(key, "tsBedrock") || !strcmp(key, "boulderTimeStyle");
}

// Die Prefs-Sperre (prefs.c): umzug_prefs.c muss sie halten und wieder freigeben.
static int s_prefs_sperre;
void prefs_private_lock(void) {
  s_prefs_sperre++;
}
void prefs_private_unlock(void) {
  cl_assert(s_prefs_sperre > 0);
  s_prefs_sperre--;
}

static int s_runde[N_B25];  //< mit welcher Runde jeder b25-Pref zuletzt geschrieben wurde

// Ein Wert, der von Pref und Runde abhaengt: so faellt jede Verwechslung auf.
static void prv_wert(const char *key, int runde, uint8_t *v, int len) {
  for (int i = 0; i < len; i++) {
    v[i] = (uint8_t)(key[i % strlen(key)] * 7 + i + runde * 31);
  }
}

static status_t prv_setze(SettingsFile *f, const char *key, int len, int runde) {
  uint8_t v[64];
  prv_wert(key, runde, v, len);
  return settings_file_set(f, key, strlen(key) + 1, v, len);
}

static void prv_pruefe(SettingsFile *f, const char *key, int len, int runde) {
  uint8_t soll[64], ist[64];
  prv_wert(key, runde, soll, len);
  cl_assert_equal_i(settings_file_get_len(f, key, strlen(key) + 1), len);
  cl_must_pass(settings_file_get(f, key, strlen(key) + 1, ist, len));
  cl_assert_equal_m(soll, ist, len);
}

static int prv_b25_index(const char *key) {
  for (int i = 0; i < N_B25; i++) {
    if (!strcmp(s_b25[i].key, key)) {
      return i;
    }
  }
  return -1;
}

void test_boulder_umzug_prefs__initialize(void) {
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
}

void test_boulder_umzug_prefs__cleanup(void) {
}

void test_boulder_umzug_prefs__v2_oeffnet_und_schreibt_den_b25_bestand(void) {
  // b25: alle Prefs, dann Runde um Runde ueberschrieben (Messstufe, Licht, Zifferblatt ...),
  // bis die Datei nahe an ihrer Grenze steht - so viel Totes, wie b25 je liegen liess.
  SettingsFile f;
  cl_must_pass(settings_file_open(&f, NAME, B25_GROESSE));
  for (int i = 0; i < N_B25; i++) {
    cl_must_pass(prv_setze(&f, s_b25[i].key, s_b25[i].len, 0));
    s_runde[i] = 0;
  }
  const int lebend = f.used_space;
  // Bis lebend und tot zusammen 2700 B belegen: weit ueber V2s Grenze, und b25 verdichtet
  // erst bei 3072.
  for (int r = 1; f.used_space + f.dead_space < 2700; r++) {
    cl_assert(r < 1000);
    const int i = (r * 7) % N_B25;
    cl_must_pass(prv_setze(&f, s_b25[i].key, s_b25[i].len, r));
    s_runde[i] = r;
  }
  const int b25_belegt = f.used_space + f.dead_space;
  settings_file_close(&f);
  printf("\nshellpref von b25: %d Prefs, lebend %d B, mit Totem %d von %d B\n", N_B25, lebend,
         b25_belegt, B25_GROESSE);
  cl_assert(b25_belegt > SHELL_PREFS_FILE_LEN);
  // DIE GRENZE: Lebend passt der b25-Bestand in V2s 2048 B - sonst scheiterte jedes Schreiben
  // nach dem Umzug, und es braeuchte eine Aufraeum-Bruecke (Spezifikation 8, Rueckfrage).
  cl_assert(lebend <= SHELL_PREFS_FILE_LEN);

  // V2 oeffnet mit seiner Groesse: alles lesbar.
  cl_assert_equal_i(SHELL_PREFS_FILE_LEN, 2048);
  cl_must_pass(settings_file_open(&f, NAME, SHELL_PREFS_FILE_LEN));
  for (int i = 0; i < N_B25; i++) {
    prv_pruefe(&f, s_b25[i].key, s_b25[i].len, s_runde[i]);
  }
  // DIE MESSSTUFE ist dieselbe Einstellung (Schluessel samt Null, 1 Byte, wie messstufe.c sie
  // liest und schreibt).
  cl_assert_equal_s(BOULDER_MESSSTUFE_SCHLUESSEL, "boulderMessstufe");
  settings_file_close(&f);

  // Der Umzug (B1): b25s eigene Prefs weg, einmal - beim naechsten Boot schreibt er nichts.
  cl_assert_equal_i(boulder_umzug_prefs_aufraeumen(), 2);
  cl_assert_equal_i(boulder_umzug_prefs_aufraeumen(), 0);
  cl_assert_equal_i(s_prefs_sperre, 0);

  // Das Telefon schreibt die beiden Farben, dann schreibt V2 jeden Pref und die Messstufe
  // zweimal neu - jeder Satz muss passen (settings_file.c:513 zaehlt auch beim Ueberschreiben).
  cl_must_pass(settings_file_open(&f, NAME, SHELL_PREFS_FILE_LEN));
  for (int i = 0; i < N_TELEFON; i++) {
    cl_must_pass(prv_setze(&f, s_telefon[i].key, s_telefon[i].len, 50));
  }
  for (int r = 100; r < 102; r++) {
    for (int i = 0; i < N_V2; i++) {
      cl_must_pass(prv_setze(&f, s_v2[i].key, s_v2[i].len, r));
      s_runde[prv_b25_index(s_v2[i].key)] = r;
    }
    cl_must_pass(prv_setze(&f, BOULDER_MESSSTUFE_SCHLUESSEL, 1, r));
    s_runde[prv_b25_index(BOULDER_MESSSTUFE_SCHLUESSEL)] = r;
  }
  printf("nach dem Schreiben unter V2: lebend %d B von %d B (Luft %d B)\n", f.used_space,
         SHELL_PREFS_FILE_LEN, SHELL_PREFS_FILE_LEN - f.used_space);
  cl_assert(f.used_space <= SHELL_PREFS_FILE_LEN);
  settings_file_close(&f);

  // Der groesste Pref muss noch passen, sonst scheiterte sein naechstes Ueberschreiben.
  int groesster = 0;
  for (int i = 0; i < N_V2; i++) {
    const int satz = (int)sizeof(SettingsRecordHeader) + (int)strlen(s_v2[i].key) + 1 +
                     s_v2[i].len;
    groesster = satz > groesster ? satz : groesster;
  }
  cl_assert(f.used_space + groesster <= SHELL_PREFS_FILE_LEN);
  settings_file_close(&f);

  // Nichts verloren (Spezifikation 8): alle b25-Prefs, die V2 liest, und die Farben. Die zwei
  // geloeschten sind weg.
  cl_must_pass(settings_file_open(&f, NAME, SHELL_PREFS_FILE_LEN));
  for (int i = 0; i < N_B25; i++) {
    if (prv_geloescht(s_b25[i].key)) {
      cl_assert(!settings_file_exists(&f, s_b25[i].key, strlen(s_b25[i].key) + 1));
    } else {
      prv_pruefe(&f, s_b25[i].key, s_b25[i].len, s_runde[i]);
    }
  }
  for (int i = 0; i < N_TELEFON; i++) {
    prv_pruefe(&f, s_telefon[i].key, s_telefon[i].len, 50);
  }
  settings_file_close(&f);
}
