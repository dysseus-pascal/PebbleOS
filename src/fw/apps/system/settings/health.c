/* SPDX-License-Identifier: Apache-2.0 */

#include "health.h"
#include "menu.h"
#include "option_menu.h"
#include "window.h"
#ifdef CONFIG_BOULDER_MESSSTUFE
#include "pbl/services/activity/boulder/messstufe.h"
#endif

#include "applib/ui/option_menu_window.h"
#include "kernel/pbl_malloc.h"
#include "process_state/app_state/app_state.h"
#include "pbl/services/i18n/i18n.h"
#include "pbl/services/activity/activity.h"
#include "shell/prefs.h"
#include "system/passert.h"
#include "pbl/util/size.h"

typedef struct SettingsHealthData {
  SettingsCallbacks callbacks;
} SettingsHealthData;

static const char *s_units_distance_labels[] = {
  i18n_noop("Kilometers"),
  i18n_noop("Miles"),
};

#if defined(CONFIG_HRM) && defined(CONFIG_BOULDER_MESSSTUFE)
// Boulder: die Messstufe (boulder/messstufe.h), sparsam zuerst.
static const char *s_messstufe_labels[] = {
  i18n_noop("Battery Saver"),
  i18n_noop("Light"),
  i18n_noop("Balanced"),
  i18n_noop("Detailed"),
  i18n_noop("Precise"),
};
#elif defined(CONFIG_HRM)
static const char *s_hrm_interval_labels[] = {
  i18n_noop("10 Minutes"),
  i18n_noop("30 Minutes"),
  i18n_noop("1 Hour"),
  i18n_noop("Disabled"),
};
#endif

enum SettingsHealthItem {
  SettingsHealthTrackingEnabled,
  SettingsHealthUnitDistance,
#ifdef CONFIG_HRM
#ifdef CONFIG_BOULDER_MESSSTUFE
  // BOULDER: EIN REGLER STATT ZWEIER TAKTE. Puls- und SpO2-Takt stellt die
  // Messstufe ein; die beiden Auswahlen "HR Monitoring" und "SpO2 Monitoring"
  // entfallen, damit nicht zwei Einstellungen dasselbe behaupten.
  SettingsHealthMessstufe,
#else
  SettingsHealthHRMonitoringInterval,
#endif
  SettingsHealthHRActivityTracking,
  SettingsHealthBloodOxygenEnabled,
#ifndef CONFIG_BOULDER_MESSSTUFE
  SettingsHealthSpO2MonitoringInterval,
#endif
  SettingsHealthBloodOxygenActivityTracking,
#endif
  NumSettingsHealthItems
};

#if defined(CONFIG_HRM) && defined(CONFIG_BOULDER_MESSSTUFE)
// Messstufe option menu
/////////////////////////////

static void prv_messstufe_menu_select(OptionMenu *option_menu, int selection, void *context) {
  boulder_messstufe_setzen((uint8_t)(selection + MESSSTUFE_MIN));
  app_window_stack_remove(&option_menu->window, true /*animated*/);
}

static void prv_messstufe_menu_push(SettingsHealthData *data) {
  const int index = (int)boulder_messstufe() - MESSSTUFE_MIN;
  const OptionMenuCallbacks callbacks = {
    .select = prv_messstufe_menu_select,
  };
  const char *title = i18n_noop("Precision");
  settings_option_menu_push(title, OptionMenuContentType_SingleLine, index, &callbacks,
                            ARRAY_LENGTH(s_messstufe_labels), true /* icons_enabled */,
                            s_messstufe_labels, data);
}
#elif defined(CONFIG_HRM)
// HRM Interval option menu
/////////////////////////////

static void prv_hrm_interval_menu_select(OptionMenu *option_menu, int selection, void *context) {
  activity_prefs_set_hrm_measurement_interval((HRMonitoringInterval)selection);
  app_window_stack_remove(&option_menu->window, true /*animated*/);
}

static void prv_hrm_interval_menu_push(SettingsHealthData *data) {
  const int index = (int)activity_prefs_get_hrm_measurement_interval();
  const OptionMenuCallbacks callbacks = {
    .select = prv_hrm_interval_menu_select,
  };
  const char *title = i18n_noop("HR Monitoring");
  settings_option_menu_push(title, OptionMenuContentType_SingleLine, index, &callbacks,
                            ARRAY_LENGTH(s_hrm_interval_labels), true /* icons_enabled */,
                            s_hrm_interval_labels, data);
}

// SpO2 Interval option menu
/////////////////////////////

static void prv_spo2_interval_menu_select(OptionMenu *option_menu, int selection, void *context) {
  activity_prefs_set_spo2_measurement_interval((HRMonitoringInterval)selection);
  app_window_stack_remove(&option_menu->window, true /*animated*/);
}

static void prv_spo2_interval_menu_push(SettingsHealthData *data) {
  const int index = (int)activity_prefs_get_spo2_measurement_interval();
  const OptionMenuCallbacks callbacks = {
    .select = prv_spo2_interval_menu_select,
  };
  const char *title = i18n_noop("Blood Oxygen");
  settings_option_menu_push(title, OptionMenuContentType_SingleLine, index, &callbacks,
                            ARRAY_LENGTH(s_hrm_interval_labels), true /* icons_enabled */,
                            s_hrm_interval_labels, data);
}
#endif

// Menu Callbacks
/////////////////////////////

static void prv_deinit_cb(SettingsCallbacks *context) {
  SettingsHealthData *data = (SettingsHealthData *)context;

  i18n_free_all(data);
  app_free(data);
}

static void prv_draw_row_cb(SettingsCallbacks *context, GContext *ctx, const Layer *cell_layer,
                            uint16_t row, bool selected) {
  SettingsHealthData *data = (SettingsHealthData *)context;

  const char *title = NULL;
  const char *subtitle = NULL;

  switch (row) {
    case SettingsHealthTrackingEnabled: {
      title = i18n_noop("Health Tracking");
      subtitle = activity_prefs_tracking_is_enabled() ? i18n_noop("On") : i18n_noop("Off");
      break;
    }
    case SettingsHealthUnitDistance: {
      title = i18n_noop("Distance Unit");
      UnitsDistance unit = shell_prefs_get_units_distance();
      if (unit >= UnitsDistanceCount) {
        subtitle = i18n_noop("Unknown");
      } else {
        subtitle = s_units_distance_labels[unit];
      }
      break;
    }
#ifdef CONFIG_HRM
#ifdef CONFIG_BOULDER_MESSSTUFE
    case SettingsHealthMessstufe: {
      title = i18n_noop("Precision");
      const uint8_t stufe = boulder_messstufe();
      subtitle = (stufe >= MESSSTUFE_MIN && stufe <= MESSSTUFE_MAX)
                     ? s_messstufe_labels[stufe - MESSSTUFE_MIN]
                     : i18n_noop("Unknown");
      break;
    }
#else
    case SettingsHealthHRMonitoringInterval: {
      title = i18n_noop("HR Monitoring");
      HRMonitoringInterval interval = activity_prefs_get_hrm_measurement_interval();
      if (interval >= HRMonitoringIntervalCount) {
        subtitle = i18n_noop("Unknown");
      } else {
        subtitle = s_hrm_interval_labels[interval];
      }
      break;
    }
#endif
    case SettingsHealthHRActivityTracking: {
      title = i18n_noop("HR During Activity");
      subtitle =
          activity_prefs_hrm_activity_tracking_is_enabled() ? i18n_noop("On") : i18n_noop("Off");
      break;
    }
    case SettingsHealthBloodOxygenEnabled: {
      title = i18n_noop("Blood Oxygen");
      subtitle = activity_prefs_blood_oxygen_is_enabled() ? i18n_noop("On") : i18n_noop("Off");
      break;
    }
#ifndef CONFIG_BOULDER_MESSSTUFE
    case SettingsHealthSpO2MonitoringInterval: {
      title = i18n_noop("SpO2 Monitoring");
      HRMonitoringInterval interval = activity_prefs_get_spo2_measurement_interval();
      if (interval >= HRMonitoringIntervalCount) {
        subtitle = i18n_noop("Unknown");
      } else {
        subtitle = s_hrm_interval_labels[interval];
      }
      break;
    }
#endif
    case SettingsHealthBloodOxygenActivityTracking: {
      title = i18n_noop("Blood O2 During Activity");
      if (!activity_prefs_hrm_activity_tracking_is_enabled()) {
        // Requires HR During Activity to be on first.
        subtitle = i18n_noop("Needs HR During Activity");
      } else {
        subtitle = activity_prefs_blood_oxygen_activity_tracking_is_enabled() ? i18n_noop("On")
                                                                              : i18n_noop("Off");
      }
      break;
    }
#endif
    default:
      WTF;
  }
  menu_cell_basic_draw(ctx, cell_layer, i18n_get(title, data), i18n_get(subtitle, data), NULL);
}

static void prv_select_click_cb(SettingsCallbacks *context, uint16_t row) {
  switch (row) {
    case SettingsHealthTrackingEnabled: {
      bool new_value = !activity_prefs_tracking_is_enabled();
      activity_prefs_tracking_set_enabled(new_value);
      if (new_value) {
        activity_start_tracking(false);
      } else {
        activity_stop_tracking();
      }
      break;
    }
    case SettingsHealthUnitDistance: {
      UnitsDistance unit = shell_prefs_get_units_distance();
      unit = (unit + 1) % UnitsDistanceCount;
      shell_prefs_set_units_distance(unit);
      break;
    }
#ifdef CONFIG_HRM
#ifdef CONFIG_BOULDER_MESSSTUFE
    case SettingsHealthMessstufe:
      prv_messstufe_menu_push((SettingsHealthData *)context);
      break;
#else
    case SettingsHealthHRMonitoringInterval:
      prv_hrm_interval_menu_push((SettingsHealthData *)context);
      break;
#endif
    case SettingsHealthHRActivityTracking: {
      bool new_value = !activity_prefs_hrm_activity_tracking_is_enabled();
      activity_prefs_set_hrm_activity_tracking_enabled(new_value);
      if (!new_value) {
        // Blood O2 in activity depends on HR in activity; clear it when the parent goes off.
        activity_prefs_set_blood_oxygen_activity_tracking_enabled(false);
      }
      break;
    }
    case SettingsHealthBloodOxygenEnabled:
      activity_prefs_set_blood_oxygen_enabled(!activity_prefs_blood_oxygen_is_enabled());
      break;
#ifndef CONFIG_BOULDER_MESSSTUFE
    case SettingsHealthSpO2MonitoringInterval:
      prv_spo2_interval_menu_push((SettingsHealthData *)context);
      break;
#endif
    case SettingsHealthBloodOxygenActivityTracking:
      // Only togglable once HR During Activity is on (see draw); otherwise ignore the press.
      if (activity_prefs_hrm_activity_tracking_is_enabled()) {
        activity_prefs_set_blood_oxygen_activity_tracking_enabled(
            !activity_prefs_blood_oxygen_activity_tracking_is_enabled());
      }
      break;
#endif
    default:
      WTF;
  }
  settings_menu_reload_data(SettingsMenuItemHealth);
  settings_menu_mark_dirty(SettingsMenuItemHealth);
}

static uint16_t prv_num_rows_cb(SettingsCallbacks *context) {
  if (!activity_prefs_tracking_is_enabled()) {
    return 1; // Only show the Health Tracking toggle
  }
  return NumSettingsHealthItems;
}

static void prv_appear_cb(SettingsCallbacks *context) {
}

static void prv_hide_cb(SettingsCallbacks *context) {
}

static Window *prv_init(void) {
  SettingsHealthData *data = app_malloc_check(sizeof(*data));
  *data = (SettingsHealthData){};

  data->callbacks = (SettingsCallbacks){
    .deinit = prv_deinit_cb,
    .draw_row = prv_draw_row_cb,
    .select_click = prv_select_click_cb,
    .num_rows = prv_num_rows_cb,
    .appear = prv_appear_cb,
    .hide = prv_hide_cb,
  };

  return settings_window_create(SettingsMenuItemHealth, &data->callbacks);
}

const SettingsModuleMetadata *settings_health_get_info(void) {
  static const SettingsModuleMetadata s_module_info = {
    .name = i18n_noop("Health"),
    .init = prv_init,
  };

  return &s_module_info;
}