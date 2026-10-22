/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2: Settings -> Health auf der Uhr (Pebbles settings/health.c). Seit b37 steht dort mit
// der Variante die Zeile "Blood O2 During Sleep" direkt nach Pebbles "Blood Oxygen" (Entscheid vom
// 22.10.2026); Select schaltet sie wie Pebbles Zeile fuer Trainings.
//
// ECHT: Pebbles health.c, die Variante messstufe.c und takt.c, settings_file.c auf der
// Flash-Attrappe (shellpref). ATTRAPPEN: Pebbles Einstellungsmenue (Fenster, Zeilen, Auswahl),
// Pebbles Prefs, die BlobDB vor shellpref wie in test_boulder_messung und das Puls-Abo.
// ZWEIMAL GEBAUT: mit der Variante wie auf obelix und ohne sie wie auf den anderen Boards, wo
// Pebbles Menue bleibt, wie es war (SpO2-Takt statt der neuen Zeile).

#include "clar.h"

// Ueber fw/ statt mit dem Ordner im Suchpfad: Dort laege settings/time.h vor <time.h>.
#include "apps/system/settings/health.h"
#include "apps/system/settings/option_menu.h"
#include "apps/system/settings/window.h"
#include "pbl/services/activity/activity.h"
#include "pbl/services/settings/settings_file.h"
#include "shell/prefs_private.h"

#include <pbl/services/filesystem/pfs.h>
#ifdef CONFIG_SERVICE_ACTIVITY_ALGORITHM_BOULDER
#include "pbl/services/activity/boulder/messstufe.h"
#include "pbl/services/blob_db/api.h"
#endif

#include "fake_pbl_malloc.h"
#include "fake_spi_flash.h"
#include "fake_system_task.h"
#include "stubs_logging.h"

#include <string.h>

#define TITEL_SCHLAF "Blood O2 During Sleep"

// =============================================================================================
// Pebbles Einstellungsmenue: das Fenster merkt sich die Rueckrufe, Zeilen zeichnen merkt sich Titel
// und Untertitel.

static SettingsCallbacks *s_menue;
static int s_neu_geladen;
static int s_markiert;
static int s_auswahlmenues;
static char s_titel[40];
static char s_untertitel[40];

Window *settings_window_create(SettingsMenuItem category, SettingsCallbacks *callbacks) {
  cl_assert_equal_i(category, SettingsMenuItemHealth);
  s_menue = callbacks;
  return (Window *)1;
}

void settings_menu_reload_data(SettingsMenuItem category) {
  cl_assert_equal_i(category, SettingsMenuItemHealth);
  s_neu_geladen++;
}

void settings_menu_mark_dirty(SettingsMenuItem category) {
  cl_assert_equal_i(category, SettingsMenuItemHealth);
  s_markiert++;
}

OptionMenu *settings_option_menu_push(const char *i18n_title_key,
                                      OptionMenuContentType content_type, int choice,
                                      const OptionMenuCallbacks *callbacks, uint16_t num_rows,
                                      bool icons_enabled, const char **rows, void *context) {
  s_auswahlmenues++;
  return NULL;
}

void menu_cell_basic_draw(GContext *ctx, const Layer *cell_layer, const char *title,
                          const char *subtitle, GBitmap *icon) {
  strncpy(s_titel, title ? title : "", sizeof(s_titel) - 1);
  strncpy(s_untertitel, subtitle ? subtitle : "", sizeof(s_untertitel) - 1);
}

// =============================================================================================
// Pebbles Prefs (prefs.c)

static bool s_blutsauerstoff;
static bool s_puls_im_training;
static bool s_spo2_im_training;
static int s_prefs_gesperrt;

bool activity_prefs_tracking_is_enabled(void) {
  return true;
}
void activity_prefs_tracking_set_enabled(bool an) {
}
void shell_prefs_set_units_distance(UnitsDistance einheit) {
}
bool activity_prefs_blood_oxygen_is_enabled(void) {
  return s_blutsauerstoff;
}
void activity_prefs_set_blood_oxygen_enabled(bool an) {
  s_blutsauerstoff = an;
}
bool activity_prefs_hrm_activity_tracking_is_enabled(void) {
  return s_puls_im_training;
}
void activity_prefs_set_hrm_activity_tracking_enabled(bool an) {
  s_puls_im_training = an;
}
bool activity_prefs_blood_oxygen_activity_tracking_is_enabled(void) {
  return s_spo2_im_training;
}
void activity_prefs_set_blood_oxygen_activity_tracking_enabled(bool an) {
  s_spo2_im_training = an;
}
HRMonitoringInterval activity_prefs_get_hrm_measurement_interval(void) {
  return HRMonitoringInterval_10Min;
}
void activity_prefs_set_hrm_measurement_interval(HRMonitoringInterval i) {
}
HRMonitoringInterval activity_prefs_get_spo2_measurement_interval(void) {
  return HRMonitoringInterval_30Min;
}
void activity_prefs_set_spo2_measurement_interval(HRMonitoringInterval i) {
}
void prefs_private_lock(void) {
  s_prefs_gesperrt++;
}
void prefs_private_unlock(void) {
  s_prefs_gesperrt--;
}

// shellpref, wie settings_blob_db den Schluessel ablegt (mit Nullbyte, prv_canonical_shell_key).
static status_t prv_shellpref(bool schreiben, const char *schluessel, uint8_t *wert) {
  SettingsFile f;
  status_t st = settings_file_open(&f, SHELL_PREFS_FILE_NAME, SHELL_PREFS_FILE_LEN);
  if (st == S_SUCCESS) {
    st = schreiben ? settings_file_set(&f, schluessel, strlen(schluessel) + 1, wert, 1)
                   : settings_file_get(&f, schluessel, strlen(schluessel) + 1, wert, 1);
    settings_file_close(&f);
  }
  return st;
}

#ifdef CONFIG_SERVICE_ACTIVITY_ALGORITHM_BOULDER
// Die Variante liest ihre Schluessel ueber die BlobDB; die liest shellpref.
status_t blob_db_read(BlobDBId db_id, const uint8_t *key, int key_len, uint8_t *val_out,
                      int val_len) {
  cl_assert_equal_i(db_id, BlobDBIdSettings);
  char schluessel[32];
  cl_assert(key_len < (int)sizeof(schluessel));
  memcpy(schluessel, key, key_len);
  schluessel[key_len] = 0;
  return prv_shellpref(false, schluessel, val_out);
}

void boulder_puls_stufe_neu(void) {
}
#endif

// =============================================================================================
// Ablauf

static int s_zeilen;

static void prv_zeile(uint16_t zeile) {
  s_titel[0] = 0;
  s_untertitel[0] = 0;
  s_menue->draw_row(s_menue, NULL, NULL, zeile, false);
}

// Die Zeile mit diesem Titel, -1 ohne.
static int prv_finde(const char *titel) {
  for (int i = 0; i < s_zeilen; i++) {
    prv_zeile(i);
    if (!strcmp(s_titel, titel)) {
      return i;
    }
  }
  return -1;
}

static void prv_select(int zeile) {
  s_menue->select_click(s_menue, zeile);
}

void test_boulder_einstellungen__initialize(void) {
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  pfs_format(false);
  fake_system_task_callbacks_cleanup();
  system_task_set_available_space(100);
  s_blutsauerstoff = false;
  s_puls_im_training = true;
  s_spo2_im_training = false;
  s_prefs_gesperrt = 0;
  s_neu_geladen = 0;
  s_markiert = 0;
  s_auswahlmenues = 0;
#ifdef CONFIG_SERVICE_ACTIVITY_ALGORITHM_BOULDER
  boulder_messstufe_test_zuruecksetzen();
#endif
  s_menue = NULL;
  cl_assert(settings_health_get_info()->init() != NULL);
  cl_assert(s_menue);
  s_zeilen = s_menue->num_rows(s_menue);
}

void test_boulder_einstellungen__cleanup(void) {
  if (s_menue) {
    s_menue->deinit(s_menue);
  }
  fake_system_task_callbacks_cleanup();
}

// Mit der Variante steht die Zeile direkt nach "Blood Oxygen", vor "Blood O2 During Activity", an
// der Stelle von Pebbles SpO2-Takt (den die Messstufe stellt, F3). Ohne die Variante: Pebbles
// Menue.
void test_boulder_einstellungen__zeilen(void) {
  cl_assert_equal_i(s_zeilen, 7);
  const int blutsauerstoff = prv_finde("Blood Oxygen");
  cl_assert(blutsauerstoff > 0);
  prv_zeile(blutsauerstoff + 1);
#ifdef CONFIG_SERVICE_ACTIVITY_ALGORITHM_BOULDER
  cl_assert_equal_s(s_titel, TITEL_SCHLAF);
  cl_assert_equal_s(s_untertitel, "Off");
  cl_assert_equal_i(prv_finde("SpO2 Monitoring"), -1);
  cl_assert(prv_finde(BOULDER_MESSSTUFE_TITEL) >= 0);
#else
  cl_assert_equal_s(s_titel, "SpO2 Monitoring");
  cl_assert_equal_i(prv_finde(TITEL_SCHLAF), -1);
  cl_assert(prv_finde("HR Monitoring") >= 0);
#endif
  cl_assert_equal_i(prv_finde("Blood O2 During Activity"), blutsauerstoff + 2);
}

// Select schaltet die Zeile wie Pebbles Schalter: gespeichert unter dem kanonischen Schluessel
// (geht ans Telefon), die Zeile zeigt es gleich, das Menue laedt neu; Pebbles "Blood Oxygen"
// bleibt, wie er ist. Ohne die Variante oeffnet die Zeile Pebbles Auswahl des SpO2-Takts.
void test_boulder_einstellungen__schlaf_umschalten(void) {
  const int zeile = prv_finde("Blood Oxygen") + 1;
  uint8_t wert = 0xff;
#ifdef CONFIG_SERVICE_ACTIVITY_ALGORITHM_BOULDER
  prv_select(zeile);
  cl_assert_equal_i(prv_shellpref(false, BOULDER_SPO2_SCHLAF_SCHLUESSEL, &wert), S_SUCCESS);
  cl_assert_equal_i(wert, 1);
  cl_assert_equal_i(s_prefs_gesperrt, 0);
  cl_assert_equal_i(s_neu_geladen, 1);
  cl_assert_equal_i(s_markiert, 1);
  prv_zeile(zeile);
  cl_assert_equal_s(s_titel, TITEL_SCHLAF);
  cl_assert_equal_s(s_untertitel, "On");
  fake_system_task_callbacks_invoke_pending(); // der Kern liest ihn
  cl_assert(boulder_spo2_schlaf_geladen());
  cl_assert(!s_blutsauerstoff);
  prv_select(zeile);
  cl_assert_equal_i(prv_shellpref(false, BOULDER_SPO2_SCHLAF_SCHLUESSEL, &wert), S_SUCCESS);
  cl_assert_equal_i(wert, 0);
  prv_zeile(zeile);
  cl_assert_equal_s(s_untertitel, "Off");
  fake_system_task_callbacks_invoke_pending();
  cl_assert(!boulder_spo2_schlaf_geladen());
  cl_assert_equal_i(s_neu_geladen, 2);
  cl_assert_equal_i(s_auswahlmenues, 0);
  // "Blood Oxygen" schaltet nur Pebbles Schalter.
  prv_select(zeile - 1);
  cl_assert(s_blutsauerstoff);
  prv_zeile(zeile);
  cl_assert_equal_s(s_untertitel, "Off");
  cl_assert_equal_i(prv_shellpref(false, BOULDER_SPO2_SCHLAF_SCHLUESSEL, &wert), S_SUCCESS);
  cl_assert_equal_i(wert, 0);
#else
  prv_select(zeile);
  cl_assert_equal_i(s_auswahlmenues, 1);
  cl_assert_equal_i(prv_shellpref(false, "boulderSpo2Schlaf", &wert), E_DOES_NOT_EXIST);
#endif
}

// Die Zeile zeigt den gespeicherten Schalter auch, bevor der Kern ihn gelesen hat (nach dem Boot,
// vor der ersten Minute).
void test_boulder_einstellungen__zeile_vor_dem_ersten_lesen(void) {
  uint8_t an = 1;
  cl_assert_equal_i(prv_shellpref(true, "boulderSpo2Schlaf", &an), S_SUCCESS);
  const int zeile = prv_finde("Blood Oxygen") + 1;
  prv_zeile(zeile);
#ifdef CONFIG_SERVICE_ACTIVITY_ALGORITHM_BOULDER
  cl_assert(!boulder_spo2_schlaf_geladen());
  cl_assert_equal_s(s_titel, TITEL_SCHLAF);
  cl_assert_equal_s(s_untertitel, "On");
  // Select schaltet dann aus, nicht noch einmal an.
  prv_select(zeile);
  uint8_t wert = 0xff;
  cl_assert_equal_i(prv_shellpref(false, BOULDER_SPO2_SCHLAF_SCHLUESSEL, &wert), S_SUCCESS);
  cl_assert_equal_i(wert, 0);
#else
  cl_assert_equal_s(s_titel, "SpO2 Monitoring");
#endif
}
