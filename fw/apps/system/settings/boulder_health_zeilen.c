/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder: die Zeilen aus boulder_health_zeilen.h. fw/apps/system/ baut jedes Board: ohne die
// Variante bleibt die Datei leer.
#ifdef CONFIG_SERVICE_ACTIVITY_ALGORITHM_BOULDER

#include "boulder_health_zeilen.h"

#include "option_menu.h"

#include <pbl/services/activity/boulder/messstufe.h>
#include <pbl/services/i18n/i18n.h>

#include <applib/ui/app_window_stack.h>
#include <applib/ui/option_menu_window.h>

void boulder_health_messstufe_zeile(const char **title, const char **subtitle) {
  *title = BOULDER_MESSSTUFE_TITEL;
  *subtitle = boulder_messstufe_namen()[boulder_messstufe() - MESSSTUFE_MIN];
}

static void prv_messstufe_gewaehlt(OptionMenu *option_menu, int selection, void *context) {
  boulder_messstufe_setzen((uint8_t)(selection + MESSSTUFE_MIN));
  app_window_stack_remove(&option_menu->window, true /*animated*/);
}

void boulder_health_messstufe_waehlen(void *context) {
  const OptionMenuCallbacks callbacks = { .select = prv_messstufe_gewaehlt };
  settings_option_menu_push(BOULDER_MESSSTUFE_TITEL, OptionMenuContentType_SingleLine,
                            boulder_messstufe() - MESSSTUFE_MIN, &callbacks, MESSSTUFE_MAX,
                            true /* icons_enabled */, boulder_messstufe_namen(), context);
}

void boulder_health_spo2_schlaf_zeile(const char **title, const char **subtitle) {
  *title = i18n_noop("SpO2 During Sleep");
  *subtitle = boulder_spo2_schlaf() ? i18n_noop("On") : i18n_noop("Off");
}

void boulder_health_spo2_schlaf_waehlen(void) {
  boulder_spo2_schlaf_setzen(!boulder_spo2_schlaf());
}

#endif  // CONFIG_SERVICE_ACTIVITY_ALGORITHM_BOULDER
