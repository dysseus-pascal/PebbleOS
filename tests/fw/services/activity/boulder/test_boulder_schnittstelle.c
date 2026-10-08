/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2, Schritt 3, AP2: die Schnittstelle zur Health-App (Spezifikation Schritt 3, 3.1-3.4,
// 3.8.4; 9.1: T2.3, T2.4, T6.2, T6.3).
//
// ECHT: die Syscalls S1-S4 (schnittstelle.c) mit Pebbles Pruefung der Rechte
// (UNITTEST_WITH_SYSCALL_PRIVILEGES), die ganze Variante wie in test_boulder_zyklen (Minutenteil,
// Nachlauf, Lebenslauf, Ausgabe, Ablage und Laden auf der echten settings_file.c), die echte
// messstufe.c auf shellpref, die Ziele (ziele.c) ueber Pebbles echte BlobDB (api.c) und die echte
// healthdb (health_db.c), die echte activity_sessions.c und die Glue-Kopie.
// ATTRAPPEN nur an den Grenzen: die beiden Schlangen von KernelBG (die der App mit 8 Plaetzen und
// die des Kerns, wie system_task/service.c), KernelMain (BlobDB-Ereignisse gehen an die Abos),
// die Sperren (sie zaehlen, wer was haelt), die Pruefung der App-Puffer gegen ein Feld "App-RAM",
// Pebbles Prefs, die Puls-Messung (test_boulder_messung) und Kieselsports Maximalpuls (der echte
// Wert: test_kieselsport). settings_blob_db.c braucht gcc; der Test liest shellpref so, wie es
// settings_blob_db_read tut (Schluessel mit Nullbyte), wie test_boulder_messung.

#include "applib/data_logging.h"
#include "applib/event_service_client.h"
#include "kernel/event_loop.h"
#include "kernel/events.h"
#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/activity_algorithm.h"
#include "pbl/services/activity/activity_private.h"
#include "pbl/services/activity/boulder/ausgabe.h"
#include "pbl/services/activity/boulder/live.h"
#include "pbl/services/activity/boulder/messstufe.h"
#include "pbl/services/activity/boulder/nacht.h"
#include "pbl/services/activity/boulder/nacht_satz.h"
#include "pbl/services/activity/boulder/puls.h"
#include "pbl/services/activity/boulder/schnittstelle.h"
#include "pbl/services/activity/boulder/spo2.h"
#include "pbl/services/activity/boulder/takt.h"
#include "pbl/services/activity/boulder/variante.h"
#include "pbl/services/activity/boulder/ziele.h"
#include "pbl/services/battery/battery_state.h"
#include "pbl/services/blob_db/api.h"
#include "pbl/services/data_logging/data_logging_service.h"
#include "pbl/services/filesystem/pfs.h"
#include "pbl/services/hrm/hrm_manager_private.h"
#include "pbl/services/settings/settings_file.h"
#include "pbl/services/time.h"
#include "pbl/util/size.h"
#include "pbl/util/units.h"
#include "shell/prefs_private.h"
#include "syscall/syscall_internal.h"
#include <pbl/cron/cron.h>
#include <pbl/drivers/ambient_light.h>
#include <pbl/drivers/rtc.h>

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clar.h"

#include "stubs_activity_insights.h"
#define alarm_get_next_enabled_alarm stubs_alarm_get_next_enabled_alarm
#include "stubs_alarm.h"
#undef alarm_get_next_enabled_alarm
#include "stubs_analytics.h"
#include "stubs_app_db.h"
#include "stubs_app_glance_db.h"
#include "stubs_contacts_db.h"
#include "stubs_hexdump.h"
#include "stubs_hr_util.h"
#include "stubs_ios_notif_pref_db.h"
#include "stubs_irq.h"
#include "stubs_logging.h"
#include "stubs_notif_db.h"
#include "stubs_passert.h"
#include "stubs_pin_db.h"
#include "stubs_prefs_db.h"
#include "stubs_reminder_db.h"
#define settings_blob_db_read stubs_settings_blob_db_read
#include "stubs_settings_blob_db.h"
#undef settings_blob_db_read
#include "stubs_sleep.h"
#include "stubs_task_wdt.h"
#include "stubs_watch_app_prefs_db.h"
#include "stubs_weather_db.h"

#include "fake_accel_service.h"
#include "fake_events.h"
#include "fake_new_timer.h"
#include "fake_pbl_malloc.h"
#include "fake_pebble_tasks.h"
#include "fake_rtc.h"
#include "fake_spi_flash.h"

// =============================================================================================
// Sperren: wer was haelt (T2.4). Pebbles Sperren sind rekursiv; gezaehlt wird die aeussere.

extern struct pbl_mutex s_lage_sperre;  // schnittstelle.c (PBL_T_STATIC)
extern struct pbl_mutex s_bild_sperre;  // ausgabe.c

#define GEHALTEN_MAX 8
static struct pbl_mutex *s_gehalten[GEHALTEN_MAX];
static int s_gehalten_anzahl;

static bool s_im_syscall;
static int s_syscall_sperren;       //< Sperren, die der laufende Syscall genommen hat
static int s_syscall_tiefe_max;     //< hoechstens so viele zugleich gehalten
static int s_blatt_verstoss;        //< unter einer Sperre der Schnittstelle eine weitere
static int s_pebble_unter_sperre;   //< Pebble unter einer Sperre der Schnittstelle gerufen

static bool prv_schnittstellen_sperre(const struct pbl_mutex *m) {
  return m == &s_lage_sperre || m == &s_bild_sperre;
}

static bool prv_haelt_schnittstellen_sperre(void) {
  for (int i = 0; i < s_gehalten_anzahl; i++) {
    if (prv_schnittstellen_sperre(s_gehalten[i])) {
      return true;
    }
  }
  return false;
}

// Jede Grenze zu Pebble meldet sich hier: unter einer Sperre der Schnittstelle ist das ein
// Verstoss (3.4).
static void prv_pebble(void) {
  s_pebble_unter_sperre += prv_haelt_schnittstellen_sperre();
}

void pbl_mutex_init(struct pbl_mutex *m) {
}

void pbl_mutex_deinit(struct pbl_mutex *m) {
}

int pbl_mutex_lock_lr(struct pbl_mutex *m, pbl_timeout_t timeout, uintptr_t lr) {
  if (m->count++ > 0) {
    return 0;
  }
  s_blatt_verstoss += prv_haelt_schnittstellen_sperre();
  cl_assert(s_gehalten_anzahl < GEHALTEN_MAX);
  s_gehalten[s_gehalten_anzahl++] = m;
  if (s_im_syscall) {
    s_syscall_sperren++;
    if (s_gehalten_anzahl > s_syscall_tiefe_max) {
      s_syscall_tiefe_max = s_gehalten_anzahl;
    }
  }
  return 0;
}

void pbl_mutex_unlock(struct pbl_mutex *m) {
  cl_assert(m->count > 0);
  if (--m->count > 0) {
    return;
  }
  for (int i = 0; i < s_gehalten_anzahl; i++) {
    if (s_gehalten[i] == m) {
      s_gehalten[i] = s_gehalten[--s_gehalten_anzahl];
      return;
    }
  }
  cl_fail("Sperre freigegeben, die niemand hielt");
}

bool pbl_mutex_is_owner(const struct pbl_mutex *m) {
  return m->count > 0;
}

void pbl_mutex_assert_held(const struct pbl_mutex *m, bool held) {
}

// =============================================================================================
// Kein Heap und keine Datei auf dem Syscall-Weg (T2.4): --wrap zaehlt, wer oeffnet und holt.

static int s_syscall_dateien;
static int s_syscall_heap;
static int s_bild_datei_unter_sperre;  //< boulder_nacht geoeffnet, waehrend s_bild_sperre gehalten
static int s_bild_datei_geoeffnet;

status_t __real_settings_file_open(SettingsFile *file, const char *name, int max_used_space);
status_t __wrap_settings_file_open(SettingsFile *file, const char *name, int max_used_space) {
  s_syscall_dateien += s_im_syscall;
  if (!strcmp(name, "boulder_nacht")) {
    s_bild_datei_geoeffnet++;
    s_bild_datei_unter_sperre += (s_bild_sperre.count > 0);
  }
  return __real_settings_file_open(file, name, max_used_space);
}

status_t __real_settings_file_open_growable(SettingsFile *file, const char *name,
                                            int max_used_space, int initial_size);
status_t __wrap_settings_file_open_growable(SettingsFile *file, const char *name,
                                            int max_used_space, int initial_size) {
  s_syscall_dateien += s_im_syscall;
  return __real_settings_file_open_growable(file, name, max_used_space, initial_size);
}

int __real_pfs_open(const char *name, uint8_t op_flags, uint8_t file_type, size_t start_size);
int __wrap_pfs_open(const char *name, uint8_t op_flags, uint8_t file_type, size_t start_size) {
  s_syscall_dateien += s_im_syscall;
  return __real_pfs_open(name, op_flags, file_type, start_size);
}

void *__real_kernel_malloc(size_t bytes);
void *__wrap_kernel_malloc(size_t bytes) {
  s_syscall_heap += s_im_syscall;
  return __real_kernel_malloc(bytes);
}
void *__real_kernel_zalloc(size_t bytes);
void *__wrap_kernel_zalloc(size_t bytes) {
  s_syscall_heap += s_im_syscall;
  return __real_kernel_zalloc(bytes);
}
void *__real_kernel_malloc_check(size_t bytes);
void *__wrap_kernel_malloc_check(size_t bytes) {
  s_syscall_heap += s_im_syscall;
  return __real_kernel_malloc_check(bytes);
}
void *__real_kernel_zalloc_check(size_t bytes);
void *__wrap_kernel_zalloc_check(size_t bytes) {
  s_syscall_heap += s_im_syscall;
  return __real_kernel_zalloc_check(bytes);
}

// =============================================================================================
// Rechte und Puffer (T2.3): Ein Aufruf "aus der App" hat erhoehte Rechte; ihre Puffer muessen
// ganz im App-RAM liegen, sonst endet die App (syscall_failed).

static uint8_t s_app_ram[4096] __attribute__((aligned(8)));
static bool s_erhoeht;
static int s_puffer_geprueft;

bool syscall_internal_check_return_address(void *ret_addr) {
  return s_erhoeht;
}

PBL_NORETURN void syscall_failed(void) {
  PBL_ASSERTN(false);
  while (1) {
  }
}

void syscall_assert_userspace_buffer(const void *buf, size_t num_bytes) {
  s_puffer_geprueft++;
  const uintptr_t a = (uintptr_t)buf;
  const uintptr_t start = (uintptr_t)s_app_ram;
  const uintptr_t ende = start + sizeof(s_app_ram);
  if (a < start || a > ende || num_bytes > ende - a) {
    syscall_failed();
  }
}

static NachtBild *const s_app_bild = (NachtBild *)&s_app_ram[0];
static BoulderLage *const s_app_lage = (BoulderLage *)&s_app_ram[1024];
static Spo2Wert *const s_app_spo2 = (Spo2Wert *)&s_app_ram[2048];

// =============================================================================================
// KernelBG mit zwei Schlangen wie system_task/service.c: Aus der App-Task landet
// system_task_add_callback in der Schlange der App (8 Plaetze, die App wartet, wenn sie voll ist),
// sonst und mit _droppable in der des Kerns.

#define APP_SCHLANGE 8
#define KERN_SCHLANGE 30

typedef struct {
  SystemTaskEventCallback cb;
  void *daten;
} Eintrag;

static Eintrag s_app_schlange[APP_SCHLANGE];
static int s_app_n;
static Eintrag s_kern_schlange[64];
static int s_kern_n;
static int s_app_eintraege;        //< so viele hat die App insgesamt eingestellt
static int s_app_boulder_max;      //< hoechstens so viele Boulder-Auftraege zugleich von der App
static int s_app_gewartet;         //< so oft war die Schlange der App voll
static void prv_fremd_cb(void *daten) {
}

static void prv_lauf(Eintrag e) {
  const PebbleTask vorher = pebble_task_get_current();
  stub_pebble_tasks_set_current(PebbleTask_KernelBackground);
  e.cb(e.daten);
  stub_pebble_tasks_set_current(vorher);
}

static Eintrag prv_vorne(Eintrag *schlange, int *n) {
  const Eintrag e = schlange[0];
  memmove(schlange, schlange + 1, (size_t)(--*n) * sizeof(Eintrag));
  return e;
}

static void prv_kernel_bg(void) {
  while (s_kern_n || s_app_n) {
    if (s_kern_n) {
      prv_lauf(prv_vorne(s_kern_schlange, &s_kern_n));
    }
    if (s_app_n) {
      prv_lauf(prv_vorne(s_app_schlange, &s_app_n));
    }
  }
}

bool system_task_add_callback(SystemTaskEventCallback cb, void *daten) {
  prv_pebble();
  if (pebble_task_get_current() == PebbleTask_App) {
    if (s_app_n == APP_SCHLANGE) {
      // Die App wartet ohne Frist, bis KernelBG einen Platz frei macht (PBL_FOREVER).
      s_app_gewartet++;
      prv_lauf(prv_vorne(s_app_schlange, &s_app_n));
    }
    s_app_schlange[s_app_n++] = (Eintrag){ cb, daten };
    s_app_eintraege++;
    int boulder = 0;
    for (int i = 0; i < s_app_n; i++) {
      boulder += s_app_schlange[i].cb != prv_fremd_cb;
    }
    if (boulder > s_app_boulder_max) {
      s_app_boulder_max = boulder;
    }
    return true;
  }
  cl_assert(s_kern_n < (int)ARRAY_LENGTH(s_kern_schlange));
  s_kern_schlange[s_kern_n++] = (Eintrag){ cb, daten };
  return true;
}

bool system_task_add_callback_droppable(SystemTaskEventCallback cb, void *daten) {
  cl_assert(pebble_task_get_current() != PebbleTask_App);
  if (s_kern_n >= KERN_SCHLANGE) {
    return false;
  }
  s_kern_schlange[s_kern_n++] = (Eintrag){ cb, daten };
  return true;
}

void system_task_watchdog_feed(void) {
}

uint32_t system_task_get_available_space(void) {
  return (pebble_task_get_current() == PebbleTask_App) ? APP_SCHLANGE - s_app_n
                                                       : KERN_SCHLANGE - s_kern_n;
}

// =============================================================================================
// KernelMain: Abos merken; BlobDB-Ereignisse gehen gleich an sie (wie event_service.c).

static EventServiceInfo *s_abos[4];
static int s_abo_anzahl;

void event_service_client_subscribe(EventServiceInfo *info) {
  // Wie event_service_client.c: ein zweites Abo desselben Eintrags zaehlt nicht.
  for (int i = 0; i < s_abo_anzahl; i++) {
    if (s_abos[i] == info) {
      return;
    }
  }
  cl_assert(s_abo_anzahl < (int)ARRAY_LENGTH(s_abos));
  s_abos[s_abo_anzahl++] = info;
}

void event_service_client_unsubscribe(EventServiceInfo *info) {
}

void launcher_task_add_callback(CallbackEventCallback cb, void *daten) {
  const PebbleTask vorher = pebble_task_get_current();
  stub_pebble_tasks_set_current(PebbleTask_KernelMain);
  cb(daten);
  stub_pebble_tasks_set_current(vorher);
}

static void prv_ereignis(PebbleEvent *e) {
  if (e->type != PEBBLE_BLOBDB_EVENT) {
    return;
  }
  const PebbleTask vorher = pebble_task_get_current();
  stub_pebble_tasks_set_current(PebbleTask_KernelMain);
  for (int i = 0; i < s_abo_anzahl; i++) {
    if (s_abos[i]->type == PEBBLE_BLOBDB_EVENT) {
      s_abos[i]->handler(e, s_abos[i]->context);
    }
  }
  stub_pebble_tasks_set_current(vorher);
  // Wie die Ereignis-Schleife: Den Schluessel hat blob_db_event_put kopiert.
  kernel_free(e->blob_db.key);
}

// =============================================================================================
// Pebbles Prefs (prefs.c): Lesen ohne Sperre; die Setter des Reglers zaehlen.

static bool s_tracking = true;
static bool s_spo2_an;
static HRMonitoringInterval s_puls_takt;
static HRMonitoringInterval s_spo2_takt;
static int s_setter;

bool activity_prefs_tracking_is_enabled(void) {
  prv_pebble();
  return s_tracking;
}
bool activity_prefs_blood_oxygen_is_enabled(void) {
  prv_pebble();
  return s_spo2_an;
}
bool activity_prefs_heart_rate_is_enabled(void) {
  return true;
}
HRMonitoringInterval activity_prefs_get_hrm_measurement_interval(void) {
  return s_puls_takt;
}
HRMonitoringInterval activity_prefs_get_spo2_measurement_interval(void) {
  return s_spo2_takt;
}
void activity_prefs_set_hrm_measurement_interval(HRMonitoringInterval i) {
  s_puls_takt = i;
  s_setter++;
}
void activity_prefs_set_spo2_measurement_interval(HRMonitoringInterval i) {
  s_spo2_takt = i;
  s_setter++;
}
void activity_prefs_set_blood_oxygen_enabled(bool an) {
  s_spo2_an = an;
  s_setter++;
}
bool activity_prefs_hrm_activity_tracking_is_enabled(void) {
  return true;
}
uint16_t activity_prefs_get_weight_dag(void) {
  return 7000;
}
uint8_t activity_prefs_get_age_years(void) {
  return 41;
}
ActivityGender activity_prefs_get_gender(void) {
  return ActivityGenderMale;
}

// boulder_messstufe_setzen nimmt die Sperre der Prefs, nachdem der Auftrag den Wunsch gelesen hat:
// Hier kann der Test einen Wunsch der App "waehrenddessen" einschieben.
static bool s_wunsch_dazwischen;
static bool prv_s4(uint8_t stufe);
void prefs_private_lock(void) {
  if (s_wunsch_dazwischen) {
    s_wunsch_dazwischen = false;
    cl_assert(prv_s4(2));
  }
}
void prefs_private_unlock(void) {
}

// shellpref vor der BlobDB, wie settings_blob_db_read (Schluessel kanonisch mit Nullbyte).
status_t settings_blob_db_read(const uint8_t *key, int key_len, uint8_t *val_out, int val_len) {
  char k[40];
  cl_assert(key_len < (int)sizeof(k));
  memcpy(k, key, key_len);
  k[key_len] = 0;
  SettingsFile f;
  status_t st = settings_file_open(&f, SHELL_PREFS_FILE_NAME, SHELL_PREFS_FILE_LEN);
  if (st == S_SUCCESS) {
    st = settings_file_get(&f, k, key_len + 1, val_out, val_len);
    settings_file_close(&f);
  }
  return (st == S_SUCCESS) ? S_SUCCESS : E_DOES_NOT_EXIST;
}

// Was healthdb an Pebbles Metriken meldet (nur Schluessel mit Pebbles Endungen).
static int s_metriken_gesetzt;
void activity_metrics_prv_set_metric(ActivityMetric metric, enum pbl_weekday day, int32_t value) {
  s_metriken_gesetzt++;
}

// =============================================================================================
// Puls-Messung (puls.c prueft test_boulder_messung) und Kieselsport (test_kieselsport).

void boulder_puls_minute(time_t jetzt_utc, bool nachtruhe) {
}
void boulder_puls_stufe_neu(void) {
}
void boulder_puls_tracking_aus(void) {
}
void boulder_puls_takt(void) {
}
void boulder_puls_aktiv(bool an) {
}
void boulder_puls_aktiv_pausieren(bool pausiert) {
}
void kieselsport_init(void) {
}
static uint16_t s_kieselsport_maxpuls;
uint16_t kieselsport_maxpuls_wirksam(void) {
  prv_pebble();
  return s_kieselsport_maxpuls;
}

// =============================================================================================
// Pebble rund um activity_sessions.c und die Glue-Kopie (wie test_boulder_zyklen)

static ActivityState s_state;
ActivityState *activity_private_state(void) {
  return &s_state;
}
SettingsFile *activity_private_settings_open(void) {
  return NULL;
}
void activity_private_settings_close(SettingsFile *file) {
}
uint32_t light_get_ambient_lux(void) {
  return 64;
}
AmbientLightLevel ambient_light_level_to_enum(uint32_t light_level) {
  return AmbientLightLevelLight;
}
BatteryChargeState battery_get_charge_state(void) {
  return (BatteryChargeState){ .charge_percent = 80 };
}
bool alarm_get_next_enabled_alarm(time_t *next_alarm_time_out) {
  return false;
}
void boulder_wecklauf_minute(void) {
}
bool activity_tracking_on(void) {
  return true;
}
void activity_metrics_prv_get_spo2_sample(uint8_t *percent_out, uint8_t *quality_out) {
  *percent_out = 0;
  *quality_out = 0;
}
void activity_metrics_prv_reset_hr_stats(void) {
}
bool activity_metrics_prv_is_hrm_offwrist(time_t now_utc) {
  return false;
}
void activity_metrics_prv_set_hrm_worn_status(time_t now_utc, bool is_offwrist) {
}
void activity_metrics_prv_get_median_hr_bpm(int32_t *median, int32_t *total_weight) {
  if (median) {
    *median = 60;
  }
  if (total_weight) {
    *total_weight = 100;
  }
}
HRZone activity_metrics_prv_get_hr_zone(void) {
  return HRZone_Zone0;
}
uint32_t activity_metrics_prv_get_steps(void) {
  return 0;
}
uint32_t activity_metrics_prv_get_distance_mm(void) {
  return 0;
}
uint32_t activity_metrics_prv_get_resting_calories(void) {
  return 0;
}
uint32_t activity_metrics_prv_get_active_calories(void) {
  return 0;
}
HRMSessionRef hrm_manager_subscribe_with_callback(AppInstallId app_id, uint32_t update_interval_s,
                                                  uint16_t expire_s, HRMFeature features,
                                                  bool low_latency, HRMSubscriberCallback callback,
                                                  void *context) {
  return 1;
}
bool sys_hrm_manager_unsubscribe(HRMSessionRef session) {
  return true;
}
bool sys_hrm_manager_set_update_interval(HRMSessionRef session, uint32_t update_interval_s,
                                         uint16_t expire_s) {
  return true;
}
void hrm_manager_set_activity_scene(HRMActivityScene scene) {
}
bool workout_service_stop_workout(void) {
  return false;
}
DataLoggingSession *dls_create(uint32_t tag, DataLoggingItemType item_type, uint16_t item_size,
                               bool buffered, bool resume, const Uuid *uuid) {
  return (DataLoggingSession *)(uintptr_t)tag;
}
DataLoggingResult dls_log(DataLoggingSession *logging_session, const void *data,
                          uint32_t num_items) {
  return DATA_LOGGING_SUCCESS;
}
void dls_send_all_sessions(void) {
}

// =============================================================================================
// Eine Referenznacht (wie test_boulder_zyklen): Eingabe, HRV-Fenster und das Bild von b26

#define MINUTEN SCHLAF_MINUTEN_MAX

typedef struct {
  uint32_t beginn_utc;
  bool gueltig[MINUTEN];
  uint16_t vmc[MINUTEN];
  uint8_t puls[MINUTEN];
  int hrv_anzahl;
  SchlafHrv hrv[SCHLAF_HRV_MAX];
  NachtBild bild;
} Referenz;

static Referenz s_ref;
static int s_hrv_naechstes;  //< das naechste HRV-Fenster der Referenz

static void prv_hex(const char *hex, void *aus, size_t n) {
  uint8_t *bytes = aus;
  for (size_t k = 0; k < n; k++) {
    const char z[3] = { hex[2 * k], hex[2 * k + 1], 0 };
    bytes[k] = (uint8_t)strtoul(z, NULL, 16);
  }
}

static void prv_lade(const char *name) {
  static char zeile[4096];
  memset(&s_ref, 0, sizeof(s_ref));
  char pfad[512];
  snprintf(pfad, sizeof(pfad), "%s/activity/boulder/referenz-b26/%s.txt", CLAR_FIXTURE_PATH,
           name);
  FILE *f = fopen(pfad, "r");
  cl_assert(f);
  while (fgets(zeile, sizeof(zeile), f)) {
    zeile[strcspn(zeile, "\r\n")] = 0;
    char *wert = strchr(zeile, ' ');
    if (zeile[0] == '#' || !wert) {
      continue;
    }
    *wert++ = 0;
    int a, b, c;
    if (!strcmp(zeile, "beginn_utc")) {
      s_ref.beginn_utc = (uint32_t)atol(wert);
    } else if (!strcmp(zeile, "eingabe") && sscanf(wert, "%d %d %d", &a, &b, &c) == 3) {
      s_ref.gueltig[a] = true;
      s_ref.vmc[a] = (uint16_t)b;
      s_ref.puls[a] = (uint8_t)c;
    } else if (!strcmp(zeile, "hrv") && sscanf(wert, "%d %d %d", &a, &b, &c) == 3) {
      cl_assert(s_ref.hrv_anzahl < SCHLAF_HRV_MAX);
      s_ref.hrv[s_ref.hrv_anzahl++] =
          (SchlafHrv){ .minute = (uint16_t)a, .rmssd = (uint16_t)b, .puls = (uint8_t)c };
    } else if (!strcmp(zeile, "bild")) {
      prv_hex(wert, &s_ref.bild, sizeof(s_ref.bild));
    }
  }
  fclose(f);
  cl_assert(s_ref.beginn_utc);
  s_hrv_naechstes = 0;
}

// Eine Minute wie auf der Uhr: Minutenteil, Pebbles Minutenlauf, KernelBG, der eigene Takt zur
// Sekunde 10, KernelBG.
static void prv_minute(time_t beginn, int m, uint16_t vmc, uint8_t puls, bool gueltig) {
  const time_t utc = beginn + (time_t)m * PBL_SEC_PER_MIN;
  const BoulderMinute minute = {
    .utc = utc, .vmc = gueltig ? vmc : 0, .puls = gueltig ? puls : 0,
    .nicht_getragen = !gueltig,
  };
  rtc_set_time(utc + PBL_SEC_PER_MIN);
  boulder_dauerteil_minute(&minute);
  // Der Minutenteil selbst veroeffentlicht die Lage (3.1, Regel 5), nicht erst der Takt.
  BoulderLage l;
  boulder_lage_lesen(&l);
  cl_assert_equal_i(l.stand_utc, utc + PBL_SEC_PER_MIN);
  activity_sessions_prv_minute_handler(utc + PBL_SEC_PER_MIN);
  prv_kernel_bg();
  rtc_set_time(utc + PBL_SEC_PER_MIN + 10);
  boulder_dauerteil_takt(utc + PBL_SEC_PER_MIN + 10);
  prv_kernel_bg();
}

// Die Referenznacht von Minute `von` bis `bis` (des Fensters), danach wach und unterwegs.
// Die HRV-Fenster wie test_boulder_zyklen: sobald ihre Mitte vor dem Nachlauf der Minute liegt.
static void prv_nacht(int von, int bis) {
  int hrv = s_hrv_naechstes;
  for (int m = von; m < bis; m++) {
    while (hrv < s_ref.hrv_anzahl && s_ref.hrv[hrv].minute + 2 < m + 1 && m < NACHT_MINUTEN) {
      boulder_dauerteil_hrv_fenster(s_ref.beginn_utc + s_ref.hrv[hrv].minute * PBL_SEC_PER_MIN,
                                    s_ref.hrv[hrv].rmssd, s_ref.hrv[hrv].puls);
      hrv++;
    }
    if (m >= MINUTEN) {
      prv_minute(s_ref.beginn_utc, m, 3000, 82, true);
    } else {
      prv_minute(s_ref.beginn_utc, m, s_ref.vmc[m], s_ref.puls[m], s_ref.gueltig[m]);
    }
  }
  s_hrv_naechstes = hrv;
}

// =============================================================================================
// Die App ruft: mit erhoehten Rechten, auf der App-Task, und der Test zaehlt, was der Syscall tut.

static void prv_app_beginn(void) {
  stub_pebble_tasks_set_current(PebbleTask_App);
  s_erhoeht = true;
  s_im_syscall = true;
  s_syscall_sperren = 0;
  s_syscall_tiefe_max = 0;
}

static int s_syscalls;
static void prv_app_ende(void) {
  s_im_syscall = false;
  s_erhoeht = false;
  stub_pebble_tasks_set_current(PebbleTask_KernelBackground);
  s_syscalls++;
  // Hoechstens eine Sperre je Syscall, und nie zwei zugleich (3.1, Regel 4).
  cl_assert(s_syscall_sperren <= 1);
  cl_assert(s_syscall_tiefe_max <= 1);
}

static BoulderLesen prv_s1(void) {
  prv_app_beginn();
  const BoulderLesen r = sys_boulder_nacht(s_app_bild);
  prv_app_ende();
  return r;
}

static bool prv_s2(void) {
  prv_app_beginn();
  const bool r = sys_boulder_lage(s_app_lage);
  prv_app_ende();
  return r;
}

static uint32_t prv_s3(time_t von, time_t bis, uint32_t max) {
  prv_app_beginn();
  const uint32_t r = sys_boulder_spo2(von, bis, s_app_spo2, max);
  prv_app_ende();
  return r;
}

static bool prv_s4(uint8_t stufe) {
  prv_app_beginn();
  const bool r = sys_boulder_messstufe_setzen(stufe);
  prv_app_ende();
  return r;
}

// Nach einem Abbruch (syscall_failed) wieder als KernelBG.
static void prv_app_abbruch(void) {
  s_im_syscall = false;
  s_erhoeht = false;
  stub_pebble_tasks_set_current(PebbleTask_KernelBackground);
}

// =============================================================================================
// Das Telefon: Ziele in healthdb ueber Pebbles BlobDB (api.c), mit dem Ereignis.

static status_t prv_ziele_schicken(uint8_t fassung, uint16_t schritte, uint16_t schlaf,
                                   int laenge) {
  const BoulderZieleSatz satz = { .fassung = fassung, .schritte = schritte, .schlaf_min = schlaf };
  uint8_t wert[12] = { 0 };
  memcpy(wert, &satz, sizeof(satz));
  return blob_db_insert(BlobDBIdHealth, (const uint8_t *)BOULDER_ZIELE_SCHLUESSEL,
                        strlen(BOULDER_ZIELE_SCHLUESSEL), wert, laenge);
}

static status_t prv_shellpref_stufe(uint8_t *stufe) {
  return settings_blob_db_read((const uint8_t *)BOULDER_MESSSTUFE_SCHLUESSEL,
                               strlen(BOULDER_MESSSTUFE_SCHLUESSEL), stufe, 1);
}

static void prv_shellpref_schreiben(uint8_t stufe) {
  SettingsFile f;
  cl_must_pass(settings_file_open(&f, SHELL_PREFS_FILE_NAME, SHELL_PREFS_FILE_LEN));
  cl_must_pass(settings_file_set(&f, BOULDER_MESSSTUFE_SCHLUESSEL,
                                 strlen(BOULDER_MESSSTUFE_SCHLUESSEL) + 1, &stufe, 1));
  settings_file_close(&f);
}

// Die Lage, wie die App sie sieht.
static BoulderLage prv_lage(void) {
  cl_assert(prv_s2());
  return *s_app_lage;
}

// Wie viele Laeufe hatte eine Arbeit der Variante bisher (takt.c misst jede)?
static uint32_t prv_laeufe(const char *name) {
  BoulderAufgabe *const *liste;
  const int n = boulder_messung_liste(&liste);
  for (int i = 0; i < n; i++) {
    if (liste[i] && !strcmp(liste[i]->name, name)) {
      return liste[i]->laeufe;
    }
  }
  return 0;
}

// =============================================================================================

static AccelSamplingRate s_rate;
static time_t s_jetzt;

// Ein Boot: alles Statische auf null, Dateien bleiben; init der Kopie reiht "Laden" ein. KernelBG
// laeuft noch nicht.
static void prv_boot(void) {
  s_abo_anzahl = 0;
  s_kern_n = 0;
  s_app_n = 0;
  boulder_dauerteil_test_neustart();
  boulder_messstufe_test_zuruecksetzen();
  cl_assert(activity_algorithm_init(&s_rate));
}

void test_boulder_schnittstelle__initialize(void) {
  TimezoneInfo tz = { .tm_zone = "CEST", .tm_gmtoff = 2 * PBL_SEC_PER_HOUR };
  time_util_update_timezone(&tz);
  s_jetzt = 1790704800 + 16 * PBL_SEC_PER_HOUR;  // 30.9.2026 12:00 Ortszeit (CEST)
  fake_rtc_init(100, s_jetzt);
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  pfs_format(false);
  fake_event_init();
  fake_event_set_callback(prv_ereignis);
  stub_pebble_tasks_set_current(PebbleTask_KernelBackground);
  memset(&s_state, 0, sizeof(s_state));
  memset(s_app_ram, 0, sizeof(s_app_ram));
  s_gehalten_anzahl = 0;
  s_blatt_verstoss = 0;
  s_pebble_unter_sperre = 0;
  s_syscall_dateien = 0;
  s_syscall_heap = 0;
  s_bild_datei_unter_sperre = 0;
  s_bild_datei_geoeffnet = 0;
  s_puffer_geprueft = 0;
  s_app_eintraege = 0;
  s_app_boulder_max = 0;
  s_app_gewartet = 0;
  s_syscalls = 0;
  s_tracking = true;
  s_spo2_an = false;
  s_puls_takt = HRMonitoringInterval_10Min;
  s_spo2_takt = HRMonitoringInterval_10Min;
  s_setter = 0;
  s_metriken_gesetzt = 0;
  s_kieselsport_maxpuls = 179;
  s_wunsch_dazwischen = false;
  pbl_cron_init();
  prv_boot();
}

void test_boulder_schnittstelle__cleanup(void) {
  prv_kernel_bg();
  activity_algorithm_deinit();
  prv_kernel_bg();
  pbl_cron_deinit();
  fake_event_set_callback(NULL);
  // Was keiner der Syscalls darf, ueber alle Faelle (T2.4).
  cl_assert_equal_i(s_syscall_dateien, 0);
  cl_assert_equal_i(s_syscall_heap, 0);
  cl_assert_equal_i(s_blatt_verstoss, 0);
  cl_assert_equal_i(s_pebble_unter_sperre, 0);
  cl_assert_equal_i(s_bild_datei_unter_sperre, 0);
  cl_assert(s_app_boulder_max <= 2);
}

// Eine Minute ab jetzt (ausserhalb einer Referenznacht).
static void prv_eine_minute(void) {
  const time_t minute = s_jetzt - (s_jetzt % PBL_SEC_PER_MIN);
  prv_minute(minute, 0, 1000, 70, true);
  s_jetzt = minute + PBL_SEC_PER_MIN;
}

// =============================================================================================
// T2.3: Puffer und Ueberlauf

void test_boulder_schnittstelle__puffer_ausserhalb_der_app_beenden_sie(void) {
  prv_kernel_bg();
  static uint8_t kern[1024];  // Kernel-RAM aus Sicht der App
  // Ganz ausserhalb.
  prv_app_beginn();
  cl_assert_passert(sys_boulder_nacht((NachtBild *)kern));
  prv_app_abbruch();
  prv_app_beginn();
  cl_assert_passert(sys_boulder_lage((BoulderLage *)kern));
  prv_app_abbruch();
  prv_app_beginn();
  cl_assert_passert(sys_boulder_spo2(0, INT32_MAX, (Spo2Wert *)kern, 1));
  prv_app_abbruch();
  // Ueber das Ende des App-RAMs hinaus: ein Byte zu wenig.
  prv_app_beginn();
  cl_assert_passert(sys_boulder_nacht(
      (NachtBild *)&s_app_ram[sizeof(s_app_ram) - sizeof(NachtBild) + 1]));
  prv_app_abbruch();
  prv_app_beginn();
  cl_assert_passert(sys_boulder_lage(
      (BoulderLage *)&s_app_ram[sizeof(s_app_ram) - sizeof(BoulderLage) + 1]));
  prv_app_abbruch();
  // SpO2: genug Platz fuer 10 Werte, verlangt 11.
  Spo2Wert *zehn = (Spo2Wert *)&s_app_ram[sizeof(s_app_ram) - 10 * sizeof(Spo2Wert)];
  prv_app_beginn();
  cl_assert_passert(sys_boulder_spo2(0, INT32_MAX, zehn, 11));
  prv_app_abbruch();
  // Gerade passend geht.
  cl_assert_equal_i(prv_s3(0, INT32_MAX, 10), 0);
  prv_app_beginn();
  cl_assert_equal_i(sys_boulder_spo2(0, INT32_MAX, zehn, 10), 0);
  prv_app_ende();
}

// Die Zahl der SpO2-Werte laeuft in Bytes ueber (32 Bit wie auf der Uhr): Ein kleiner Puffer, der
// die Pruefung mit dem uebergelaufenen Wert bestuende, darf nicht durchgehen.
void test_boulder_schnittstelle__spo2_ueberlauf_beendet_die_app(void) {
  const uint32_t ueber = (uint32_t)(UINT32_MAX / sizeof(Spo2Wert)) + 1;
  // 32 Bit: ueber * sizeof ergaebe 0 oder wenige Byte.
  cl_assert((uint32_t)(ueber * sizeof(Spo2Wert)) < sizeof(Spo2Wert));
  prv_app_beginn();
  cl_assert_passert(sys_boulder_spo2(0, INT32_MAX, s_app_spo2, ueber));
  prv_app_abbruch();
  prv_app_beginn();
  cl_assert_passert(sys_boulder_spo2(0, INT32_MAX, s_app_spo2, UINT32_MAX));
  prv_app_abbruch();
  // Ohne Wunsch keine Pruefung und keine Kopie.
  cl_assert_equal_i(prv_s3(0, INT32_MAX, 0), 0);
}

// Mit Systemrechten (Kern) prueft der Syscall keinen Puffer - wie Pebbles eigene.
void test_boulder_schnittstelle__kern_mit_systemrechten_ohne_pruefung(void) {
  prv_kernel_bg();
  BoulderLage lage;
  cl_assert(sys_boulder_lage(&lage));
  NachtBild bild;
  sys_boulder_nacht(&bild);
  Spo2Wert w[2];
  sys_boulder_spo2(0, INT32_MAX, w, 2);
  cl_assert_equal_i(s_puffer_geprueft, 0);
  // Aus der App wird jeder Puffer geprueft.
  prv_s1();
  prv_s2();
  prv_s3(0, INT32_MAX, 2);
  cl_assert_equal_i(s_puffer_geprueft, 3);
}

// =============================================================================================
// S1: die Nacht ohne Datei

// Nach dem Boot, bevor "Laden" lief: Spaeter, der Auftrag laedt die Datei auf KernelBG (nicht im
// Syscall, nicht unter der Sperre), danach Da mit den Bytes aus der Datei.
void test_boulder_schnittstelle__nacht_nach_dem_boot_erst_spaeter(void) {
  prv_lade("nacht-2026-09-29");
  NachtBild datei = s_ref.bild;
  SettingsFile f;
  cl_must_pass(settings_file_open(&f, "boulder_nacht", 2048));
  cl_must_pass(settings_file_set(&f, "letzte", sizeof("letzte"), &datei, sizeof(datei)));
  settings_file_close(&f);
  s_bild_datei_geoeffnet = 0;
  const uint32_t laeufe = prv_laeufe("Boulder Bild laden");

  // Zehnmal "Spaeter" (2.6.2: nach 1 s erneut, hoechstens 10-mal), ein einziger Auftrag.
  for (int i = 0; i < 10; i++) {
    cl_assert_equal_i(prv_s1(), BoulderLesenSpaeter);
  }
  cl_assert_equal_i(s_app_n, 1);
  cl_assert_equal_i(s_bild_datei_geoeffnet, 0);
  prv_kernel_bg();
  cl_assert(s_bild_datei_geoeffnet >= 1);
  cl_assert_equal_i(prv_laeufe("Boulder Bild laden"), laeufe + 1);
  cl_assert_equal_i(prv_s1(), BoulderLesenDa);
  cl_assert(memcmp(s_app_bild, &datei, sizeof(datei)) == 0);
  // Jetzt ohne Auftrag.
  cl_assert_equal_i(prv_s1(), BoulderLesenDa);
  cl_assert_equal_i(s_app_n, 0);
}

void test_boulder_schnittstelle__nacht_ohne_datei_keine(void) {
  cl_assert_equal_i(prv_s1(), BoulderLesenSpaeter);
  prv_kernel_bg();
  cl_assert_equal_i(prv_s1(), BoulderLesenKeine);
  cl_assert_equal_i(prv_s1(), BoulderLesenKeine);
  cl_assert_equal_i(s_app_n, 0);
}

// Ein Bild von b24 (Fassung im Fuellbyte) kommt wie bisher als Fassung 1 heraus (Wechsel, AP8 von
// Schritt 1) - auch ueber den neuen Weg.
void test_boulder_schnittstelle__nacht_von_b24(void) {
  prv_lade("nacht-2026-09-29");
  NachtBild b24 = s_ref.bild;
  b24.version = NACHT_BILD_VERSION_B24;
  SettingsFile f;
  cl_must_pass(settings_file_open(&f, "boulder_nacht", 2048));
  cl_must_pass(settings_file_set(&f, "letzte", sizeof("letzte"), &b24, sizeof(b24)));
  settings_file_close(&f);
  cl_assert_equal_i(prv_s1(), BoulderLesenSpaeter);
  prv_kernel_bg();
  cl_assert_equal_i(prv_s1(), BoulderLesenDa);
  cl_assert(memcmp(s_app_bild, &s_ref.bild, sizeof(s_ref.bild)) == 0);
}

// =============================================================================================
// Die Lage nach Boot, Nacht, Messstufe und Zielen

// Vor der ersten Veroeffentlichung: false, die Vorgaben, Pebbles Schalter frisch. "Laden"
// veroeffentlicht; die Ziele und die Stufe kommen nach dem ersten Minutenlauf.
void test_boulder_schnittstelle__lage_nach_dem_boot(void) {
  cl_assert_equal_i(prv_ziele_schicken(1, 7500, 390, 8), S_SUCCESS);
  prv_shellpref_schreiben(4);
  s_tracking = false;
  cl_assert(!prv_s2());
  cl_assert_equal_i(s_app_lage->fassung, BOULDER_LAGE_FASSUNG);
  cl_assert_equal_i(s_app_lage->stand_utc, 0);
  cl_assert_equal_i(s_app_lage->ziel_schritte, 10000);
  cl_assert_equal_i(s_app_lage->ziel_schlaf_min, 480);
  cl_assert_equal_i(s_app_lage->messstufe, 0);
  cl_assert(!s_app_lage->tracking);
  s_tracking = true;
  prv_kernel_bg();  // "Laden"
  BoulderLage l = prv_lage();
  cl_assert_equal_i(l.stand_utc, rtc_get_time());
  cl_assert(l.tracking);
  cl_assert_equal_i(l.maxpuls, 179);
  cl_assert_equal_i(l.fenster_utc, boulder_nacht_fenster());
  cl_assert_equal_i(l.ziel_schritte, 10000);  // noch nicht gelesen
  prv_eine_minute();
  l = prv_lage();
  cl_assert(l.fenster_utc != 0);
  cl_assert_equal_i(l.fenster_utc, boulder_nacht_fenster());
  cl_assert_equal_i(l.ziel_schritte, 7500);
  cl_assert_equal_i(l.ziel_schlaf_min, 390);
  cl_assert_equal_i(l.messstufe, 4);
  // Stufe 4: SpO2 alle 10 min, sobald der Regler Pebbles Schalter gesetzt hat.
  cl_assert(s_spo2_an);
  cl_assert_equal_i(l.spo2_takt_min, 10);
  cl_assert_equal_i(l.nickerchen, 0);
  // Tracking aus: die App sieht es sofort, auch ohne neue Veroeffentlichung.
  s_tracking = false;
  cl_assert(prv_s2());
  cl_assert(!s_app_lage->tracking);
}

// Durch eine Referenznacht: Fenster, Nachtstufe, Live-Zustand und Ruhepuls stehen in der Lage, S1
// liefert das Bild der Nacht Byte fuer Byte wie b26.
void test_boulder_schnittstelle__lage_und_bild_durch_die_nacht(void) {
  prv_lade("nacht-2026-09-29");
  activity_algorithm_deinit();
  prv_kernel_bg();
  rtc_set_time(s_ref.beginn_utc - PBL_SEC_PER_MIN);
  prv_boot();
  prv_kernel_bg();
  int schlaeft = 0;
  int offen = 0;
  for (int m = 0; m < 20 * 60; m++) {
    prv_nacht(m, m + 1);
    const BoulderLage l = prv_lage();
    cl_assert_equal_i(l.stand_utc, rtc_get_time());
    cl_assert_equal_i(l.fenster_utc, boulder_nacht_fenster());
    cl_assert_equal_i(l.nacht_stufe, boulder_nacht_stufe());
    cl_assert_equal_b(l.schlaeft, boulder_live_schlaeft());
    cl_assert_equal_i(l.ruhepuls_median, boulder_nacht_ruhepuls_median());
    schlaeft += l.schlaeft;
    offen += (l.nacht_stufe == NachtOffen);
    if (m == 3 * 60) {
      cl_assert(l.schlaeft);
    }
  }
  const BoulderLage l = prv_lage();
  cl_assert_equal_i(l.fenster_utc, s_ref.beginn_utc);
  cl_assert_equal_i(l.nacht_stufe, NachtErledigt);
  cl_assert(l.ruhepuls_median > 0);
  cl_assert(schlaeft > 5 * 60);
  cl_assert(offen > 0);
  printf("\nnacht-2026-09-29: %d min schlaeft, %d min offen, Ruhepuls-Median %d", schlaeft, offen,
         l.ruhepuls_median);
  cl_assert_equal_i(prv_s1(), BoulderLesenDa);
  cl_assert(memcmp(s_app_bild, &s_ref.bild, sizeof(s_ref.bild)) == 0);
  NachtBild kern;
  cl_assert(boulder_nacht_letzte(&kern));
  cl_assert(memcmp(s_app_bild, &kern, sizeof(kern)) == 0);
}

// Am Kabel und mit Tracking aus laeuft kein Minutenteil; der Takt (Sekunde 10) haelt die Lage
// trotzdem frisch, denn der Lebenslauf der Nacht laeuft weiter.
void test_boulder_schnittstelle__takt_ohne_minutenteil_veroeffentlicht(void) {
  prv_kernel_bg();
  const time_t t = rtc_get_time() + 5 * PBL_SEC_PER_MIN;
  rtc_set_time(t);
  boulder_dauerteil_takt(t);
  prv_kernel_bg();
  cl_assert_equal_i(prv_lage().stand_utc, t);
}

// Der Ruhepuls der Lage ist der Median der letzten sieben Naechte (V3 aus Schritt 2), wie ihn
// Kieselsport und Boulder fuer die Zonen nehmen - nicht der der letzten Nacht.
void test_boulder_schnittstelle__lage_traegt_den_median_der_naechte(void) {
  prv_kernel_bg();
  BoulderRuheRing ring = { .puls = { 70, 52, 70, 54, 70, 0, 0 }, .neu = 4, .nacht = 1790000000 };
  boulder_nacht_ruhe_laden(&ring);
  prv_eine_minute();
  cl_assert_equal_i(boulder_nacht_ruhepuls(), 0);  // seit dem Boot keine Nacht fertig
  cl_assert_equal_i(prv_lage().ruhepuls_median, 70);
}

// SpO2 nach Stufe UND Pebbles Schalter: Schaltet jemand "Blood Oxygen" in Pebbles Einstellungen
// aus, misst die Uhr nicht, bis der Regler ihn zurueckholt - die Lage sagt dann "aus".
void test_boulder_schnittstelle__spo2_takt_folgt_pebbles_schalter(void) {
  prv_shellpref_schreiben(4);
  prv_kernel_bg();
  prv_eine_minute();
  cl_assert(s_spo2_an);
  cl_assert_equal_i(prv_lage().spo2_takt_min, 10);
  s_spo2_an = false;
  prv_eine_minute();
  cl_assert(!s_spo2_an);
  cl_assert_equal_i(prv_lage().spo2_takt_min, 0);
}

// =============================================================================================
// S4: Messstufe wie aus Settings -> Health (T6.1 am Host, T6.2)

void test_boulder_schnittstelle__messstufe_aus_der_app(void) {
  prv_kernel_bg();
  prv_eine_minute();
  cl_assert_equal_i(prv_lage().messstufe, MESSSTUFE_VORGABE);
  cl_assert(prv_s4(5));
  cl_assert_equal_i(s_app_n, 1);
  prv_kernel_bg();
  uint8_t stufe = 0;
  cl_must_pass(prv_shellpref_stufe(&stufe));
  cl_assert_equal_i(stufe, 5);
  // Die Lage gleich danach, nicht erst nach der naechsten Minute.
  cl_assert_equal_i(prv_lage().messstufe, 5);
  cl_assert_equal_i(boulder_messstufe(), 5);
  // Pebbles Takte folgen der Stufe (messstufe.h): Puls am Tag alle 5 min, SpO2 alle 10 min.
  cl_assert_equal_i(s_puls_takt, HRMonitoringInterval_5Min);
  cl_assert(s_spo2_an);
  cl_assert_equal_i(prv_lage().spo2_takt_min, 10);
  // Stufe 1 ist Pebbles Original: kein SpO2.
  cl_assert(prv_s4(1));
  prv_kernel_bg();
  cl_assert_equal_i(prv_lage().messstufe, 1);
  cl_assert(!s_spo2_an);
  cl_assert_equal_i(prv_lage().spo2_takt_min, 0);
}

void test_boulder_schnittstelle__ungueltige_stufe_aendert_nichts(void) {
  prv_kernel_bg();
  prv_eine_minute();
  const int vorher = s_app_eintraege;
  cl_assert(!prv_s4(0));
  cl_assert(!prv_s4(6));
  cl_assert(!prv_s4(255));
  cl_assert_equal_i(s_app_eintraege, vorher);
  prv_kernel_bg();
  uint8_t stufe = 0;
  cl_assert(prv_shellpref_stufe(&stufe) != S_SUCCESS);
  cl_assert_equal_i(prv_lage().messstufe, MESSSTUFE_VORGABE);
}

// Zwei Wuensche, bevor KernelBG laeuft: ein Auftrag, der zweite gilt.
void test_boulder_schnittstelle__zweiter_wunsch_gilt(void) {
  prv_kernel_bg();
  cl_assert(prv_s4(2));
  cl_assert(prv_s4(4));
  cl_assert_equal_i(s_app_n, 1);
  prv_kernel_bg();
  uint8_t stufe = 0;
  cl_must_pass(prv_shellpref_stufe(&stufe));
  cl_assert_equal_i(stufe, 4);
  cl_assert_equal_i(prv_lage().messstufe, 4);
}

// Ein Wunsch, waehrend der Auftrag schon laeuft und den ersten schon gelesen hat (nach seinem
// Merker): Er geht nicht verloren, ein zweiter Auftrag setzt ihn.
void test_boulder_schnittstelle__wunsch_waehrend_des_setzens_geht_nicht_verloren(void) {
  prv_kernel_bg();
  const uint32_t laeufe = prv_laeufe("Boulder Messstufe setzen");
  cl_assert(prv_s4(5));
  s_wunsch_dazwischen = true;
  prv_kernel_bg();
  cl_assert(!s_wunsch_dazwischen);
  cl_assert_equal_i(prv_laeufe("Boulder Messstufe setzen"), laeufe + 2);
  uint8_t stufe = 0;
  cl_must_pass(prv_shellpref_stufe(&stufe));
  cl_assert_equal_i(stufe, 2);
  cl_assert_equal_i(prv_lage().messstufe, 2);
}

// T6.2: Stufe 20-mal und S1 mit "Spaeter" 20-mal, ohne dass KernelBG dazwischen laeuft:
// hoechstens zwei Boulder-Auftraege der App zugleich; danach gilt der letzte Wunsch.
void test_boulder_schnittstelle__hoechstens_zwei_auftraege_der_app(void) {
  for (int i = 0; i < 20; i++) {
    cl_assert(prv_s4((uint8_t)(1 + i % 5)));
    cl_assert_equal_i(prv_s1(), BoulderLesenSpaeter);
  }
  cl_assert_equal_i(s_app_n, 2);
  cl_assert_equal_i(s_app_eintraege, 2);
  cl_assert_equal_i(s_app_boulder_max, 2);
  prv_kernel_bg();
  cl_assert_equal_i(prv_lage().messstufe, 5);
  cl_assert_equal_i(prv_s1(), BoulderLesenKeine);
  // Noch einmal 20: wieder hoechstens zwei.
  for (int i = 0; i < 20; i++) {
    cl_assert(prv_s4(3));
  }
  cl_assert_equal_i(s_app_n, 1);
  prv_kernel_bg();
  cl_assert_equal_i(s_app_boulder_max, 2);
}

// T6.2: Die Schlange der App ist voll (8 fremde Eintraege, etwa das Puls-Abo): Der Syscall wartet,
// bis KernelBG einen Platz frei macht, und der Wunsch kommt an.
void test_boulder_schnittstelle__volle_schlange_der_app_verliert_nichts(void) {
  for (int i = 0; i < APP_SCHLANGE; i++) {
    s_app_schlange[s_app_n++] = (Eintrag){ prv_fremd_cb, NULL };
  }
  cl_assert(prv_s4(2));
  cl_assert_equal_i(s_app_gewartet, 1);
  cl_assert_equal_i(prv_s1(), BoulderLesenSpaeter);
  cl_assert_equal_i(s_app_gewartet, 2);
  prv_kernel_bg();
  uint8_t stufe = 0;
  cl_must_pass(prv_shellpref_stufe(&stufe));
  cl_assert_equal_i(stufe, 2);
  cl_assert_equal_i(prv_s1(), BoulderLesenKeine);
}

// =============================================================================================
// T6.3: die Ziele ueber die echte BlobDB Health

static BoulderLage prv_nach_ereignis(void) {
  prv_kernel_bg();
  return prv_lage();
}

void test_boulder_schnittstelle__ziele_einfuegen_aendern_loeschen_leeren(void) {
  prv_kernel_bg();
  prv_eine_minute();
  BoulderLage l = prv_lage();
  cl_assert_equal_i(l.ziel_schritte, 10000);
  cl_assert_equal_i(l.ziel_schlaf_min, 480);
  // Einfuegen: nach dem Ereignis, ohne Minute und ohne Ablage.
  cl_assert_equal_i(prv_ziele_schicken(1, 7500, 390, 8), S_SUCCESS);
  l = prv_nach_ereignis();
  cl_assert_equal_i(l.ziel_schritte, 7500);
  cl_assert_equal_i(l.ziel_schlaf_min, 390);
  // Aendern.
  cl_assert_equal_i(prv_ziele_schicken(1, 12000, 450, 8), S_SUCCESS);
  l = prv_nach_ereignis();
  cl_assert_equal_i(l.ziel_schritte, 12000);
  cl_assert_equal_i(l.ziel_schlaf_min, 450);
  // Loeschen: sofort die Vorgabe.
  cl_must_pass(blob_db_delete(BlobDBIdHealth, (const uint8_t *)BOULDER_ZIELE_SCHLUESSEL,
                              strlen(BOULDER_ZIELE_SCHLUESSEL)));
  l = prv_nach_ereignis();
  cl_assert_equal_i(l.ziel_schritte, 10000);
  cl_assert_equal_i(l.ziel_schlaf_min, 480);
  // Wieder einfuegen, dann die ganze Datenbank leeren (Ereignis ohne Schluessel): sofort die
  // Vorgabe.
  cl_assert_equal_i(prv_ziele_schicken(1, 6000, 420, 8), S_SUCCESS);
  cl_assert_equal_i(prv_nach_ereignis().ziel_schritte, 6000);
  cl_must_pass(blob_db_flush(BlobDBIdHealth));
  cl_assert_equal_i(fake_event_get_last().blob_db.type, BlobDBEventTypeFlush);
  cl_assert(fake_event_get_last().blob_db.key == NULL);
  l = prv_nach_ereignis();
  cl_assert_equal_i(l.ziel_schritte, 10000);
  cl_assert_equal_i(l.ziel_schlaf_min, 480);
  // healthdb meldet den Schluessel an keine Metrik (kein _movementData, kein Wochentag).
  cl_assert_equal_i(s_metriken_gesetzt, 0);
}

// Jede Grenze knapp darunter und darueber; dazu Fassung, Laenge und Werte, die es nie gibt.
// "70 000 Schritte" (9.1, T6.3) passt nicht in die 16 Bit des Satzes: das Groesste, was das
// Telefon schicken koennte, ist 65 535 - auch das gilt nicht.
void test_boulder_schnittstelle__ungueltige_ziele_gelten_nicht(void) {
  prv_kernel_bg();
  prv_eine_minute();
  static const struct {
    uint8_t fassung;
    uint16_t schritte;
    uint16_t schlaf;
    int laenge;
    bool gilt;
  } faelle[] = {
    { 1, 1000, 240, 8, true },    { 1, 50000, 720, 8, true },   { 1, 999, 480, 8, false },
    { 1, 50001, 480, 8, false },  { 1, 0, 480, 8, false },      { 1, 65535, 480, 8, false },
    { 1, 10000, 239, 8, false },  { 1, 10000, 721, 8, false },  { 1, 10000, 100, 8, false },
    { 2, 7500, 390, 8, false },   { 0, 7500, 390, 8, false },   { 1, 7500, 390, 4, false },
    { 1, 7500, 390, 12, false },
  };
  for (size_t i = 0; i < ARRAY_LENGTH(faelle); i++) {
    // Erst ein gueltiger anderer Satz, damit "gilt nicht" die Vorgabe heisst, nicht "unveraendert".
    cl_assert_equal_i(prv_ziele_schicken(1, 8888, 444, 8), S_SUCCESS);
    cl_assert_equal_i(prv_nach_ereignis().ziel_schritte, 8888);
    cl_assert_equal_i(prv_ziele_schicken(faelle[i].fassung, faelle[i].schritte, faelle[i].schlaf,
                                         faelle[i].laenge), S_SUCCESS);
    const BoulderLage l = prv_nach_ereignis();
    const BoulderZieleSatz satz = { .fassung = faelle[i].fassung, .schritte = faelle[i].schritte,
                                    .schlaf_min = faelle[i].schlaf };
    cl_assert_equal_b(boulder_ziele_gueltig(&satz, faelle[i].laenge), faelle[i].gilt);
    if (faelle[i].gilt) {
      cl_assert_equal_i(l.ziel_schritte, faelle[i].schritte);
      cl_assert_equal_i(l.ziel_schlaf_min, faelle[i].schlaf);
    } else {
      cl_assert_equal_i(l.ziel_schritte, 10000);
      cl_assert_equal_i(l.ziel_schlaf_min, 480);
    }
  }
}

// Gegenprobe: Ein anderer Health-Schluessel, ein Schluessel, der nur so anfaengt, und derselbe
// Schluessel in einer anderen Datenbank loesen keinen Auftrag aus.
void test_boulder_schnittstelle__andere_schluessel_loesen_nichts_aus(void) {
  prv_kernel_bg();
  prv_eine_minute();
  const uint32_t vorher = prv_laeufe("Boulder Ziele");
  cl_assert(vorher >= 1);
  const uint32_t wert = 1234;
  cl_must_pass(blob_db_insert(BlobDBIdHealth, (const uint8_t *)"average_dailySteps", 18,
                              (const uint8_t *)&wert, sizeof(wert)));
  prv_kernel_bg();
  cl_must_pass(blob_db_insert(BlobDBIdHealth, (const uint8_t *)"boulder_zieleX", 14,
                              (const uint8_t *)&wert, sizeof(wert)));
  prv_kernel_bg();
  cl_must_pass(blob_db_delete(BlobDBIdHealth, (const uint8_t *)"average_dailySteps", 18));
  prv_kernel_bg();
  cl_assert_equal_i(prv_laeufe("Boulder Ziele"), vorher);
  // Positivprobe mit Nullbyte am Schluessel (wie Pebbles kanonische Schluessel): zaehlt.
  const BoulderZieleSatz satz = { .fassung = 1, .schritte = 9000, .schlaf_min = 420 };
  cl_must_pass(blob_db_insert(BlobDBIdHealth, (const uint8_t *)BOULDER_ZIELE_SCHLUESSEL,
                              strlen(BOULDER_ZIELE_SCHLUESSEL) + 1, (const uint8_t *)&satz,
                              sizeof(satz)));
  prv_kernel_bg();
  cl_assert_equal_i(prv_laeufe("Boulder Ziele"), vorher + 1);
}

// Die Ziele ueberstehen einen Neustart (healthdb); die Ablage liest sie zur Sicherheit nach.
void test_boulder_schnittstelle__ziele_nach_neustart_und_ablage(void) {
  prv_kernel_bg();
  prv_eine_minute();
  cl_assert_equal_i(prv_ziele_schicken(1, 7500, 390, 8), S_SUCCESS);
  cl_assert_equal_i(prv_nach_ereignis().ziel_schritte, 7500);
  activity_algorithm_deinit();
  prv_kernel_bg();
  prv_boot();
  prv_kernel_bg();
  cl_assert_equal_i(prv_lage().ziel_schritte, 10000);
  prv_eine_minute();
  BoulderLage l = prv_lage();
  cl_assert_equal_i(l.ziel_schritte, 7500);
  cl_assert_equal_i(l.ziel_schlaf_min, 390);
  // Ein verlorenes Ereignis: Die Datei aendert sich ohne Meldung; die Ablage holt es nach.
  fake_event_set_callback(NULL);
  cl_assert_equal_i(prv_ziele_schicken(1, 5000, 300, 8), S_SUCCESS);
  kernel_free(fake_event_get_last().blob_db.key);
  fake_event_set_callback(prv_ereignis);
  prv_kernel_bg();
  cl_assert_equal_i(prv_lage().ziel_schritte, 7500);
  for (int i = 0; i < 15; i++) {
    prv_eine_minute();
  }
  l = prv_lage();
  cl_assert_equal_i(l.ziel_schritte, 5000);
  cl_assert_equal_i(l.ziel_schlaf_min, 300);
}

// =============================================================================================
// S3 und die Namen der Stufen

void test_boulder_schnittstelle__spo2_aelteste_zuerst_hoechstens_max(void) {
  prv_kernel_bg();
  const time_t t = rtc_get_time();
  for (int i = 0; i < 12; i++) {
    boulder_spo2_merken(t - (12 - i) * 600, (uint8_t)(88 + i));
  }
  cl_assert_equal_i(prv_s3(t - 24 * 3600, t + 1, SPO2_WERTE_MAX), 12);
  for (int i = 0; i < 12; i++) {
    cl_assert_equal_i(s_app_spo2[i].prozent, 88 + i);
    cl_assert_equal_i(s_app_spo2[i].zeit, t - (12 - i) * 600);
  }
  // Hoechstens max, die aeltesten im Fenster zuerst.
  memset(s_app_spo2, 0, 6 * sizeof(Spo2Wert));
  cl_assert_equal_i(prv_s3(t - 24 * 3600, t + 1, 5), 5);
  cl_assert_equal_i(s_app_spo2[4].prozent, 92);
  cl_assert_equal_i(s_app_spo2[5].prozent, 0);
  // Das Fenster [von, bis).
  cl_assert_equal_i(prv_s3(t - 3 * 600, t - 600, SPO2_WERTE_MAX), 2);
  cl_assert_equal_i(s_app_spo2[0].prozent, 97);
  cl_assert_equal_i(s_app_spo2[1].prozent, 98);
}

// Die Namen fuer die App stehen in messstufe.h; Settings -> Health zeigt dieselben.
void test_boulder_schnittstelle__namen_der_stufen(void) {
  static const char *const soll[] = { "Battery Saver", "Light", "Balanced", "Detailed",
                                      "Precise" };
  const char **menue = boulder_messstufe_namen();
  for (int i = 0; i < MESSSTUFE_MAX; i++) {
    cl_assert_equal_s(BOULDER_MESSSTUFE_NAMEN[i], soll[i]);
    cl_assert(menue[i] == BOULDER_MESSSTUFE_NAMEN[i]);
  }
}
