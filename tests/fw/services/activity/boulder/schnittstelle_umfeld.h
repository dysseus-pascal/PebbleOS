/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// DAS UMFELD DER SCHNITTSTELLEN-TESTS (Schritt 3): test_boulder_schnittstelle (AP2, S1-S4, Lage,
// Ziele) und test_boulder_naechte (AP3, S5). Beide brauchen dieselbe echte Variante und dieselben
// Grenzen; die Testfaelle stehen in ihren Dateien, der Rahmen einmal hier. Jeder Test bindet den
// Kopf genau einmal ein und ruft prv_umfeld_beginn und prv_umfeld_ende aus seinem initialize und
// cleanup (clar sucht die nur in der Testdatei). Nicht jeder braucht alles.
//
// ECHT: die Syscalls (schnittstelle.c) mit Pebbles Pruefung der Rechte
// (UNITTEST_WITH_SYSCALL_PRIVILEGES), die ganze Variante wie in test_boulder_zyklen (Minutenteil,
// Nachlauf, Lebenslauf, Ausgabe, Ablage und Laden auf der echten settings_file.c), die echte
// messstufe.c auf shellpref, die Ziele (ziele.c) ueber Pebbles echte BlobDB (api.c) und die echte
// healthdb (health_db.c), die Naechte (naechte.c), die echte activity_sessions.c und die
// Glue-Kopie.
// ATTRAPPEN nur an den Grenzen: die beiden Schlangen von KernelBG (die der App mit 8 Plaetzen und
// die des Kerns, wie system_task/service.c), KernelMain (BlobDB-Ereignisse gehen an die Abos),
// die Sperren (sie zaehlen, wer was haelt), die Pruefung der App-Puffer gegen ein Feld "App-RAM",
// Pebbles Prefs, die Puls-Messung (test_boulder_messung), Kieselsports Maximalpuls (der echte
// Wert: test_kieselsport) und das Datalogging (es merkt sich die Saetze 0x4001). settings_blob_db.c
// braucht gcc; der Test liest shellpref so, wie es settings_blob_db_read tut (Schluessel mit
// Nullbyte), wie test_boulder_messung.
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"

#include "applib/data_logging.h"
#include "applib/event_service_client.h"
#include "kernel/event_loop.h"
#include "kernel/events.h"
#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/activity_algorithm.h"
#include "pbl/services/activity/activity_private.h"
#include "pbl/services/activity/boulder/ausgabe.h"
#include "pbl/services/activity/boulder/herz.h"
#include "pbl/services/activity/boulder/live.h"
#include "pbl/services/activity/boulder/messstufe.h"
#include "pbl/services/activity/boulder/naechte.h"
#include "pbl/services/activity/boulder/nacht.h"
#include "pbl/services/activity/boulder/nacht_satz.h"
#include "pbl/services/activity/boulder/puls.h"
#include "pbl/services/activity/boulder/schnittstelle.h"
#include "pbl/services/activity/boulder/spo2.h"
#include "pbl/services/activity/boulder/takt.h"
#include "pbl/services/activity/boulder/variante.h"
#include "pbl/services/activity/boulder/woche.h"
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
extern struct pbl_mutex s_naechte_sperre;  // naechte.c (AP3)
extern struct pbl_mutex s_herz_sperre;     // herz.c (AP4)
extern struct pbl_mutex s_woche_sperre;    // woche.c (AP4)

#define GEHALTEN_MAX 8
static struct pbl_mutex *s_gehalten[GEHALTEN_MAX];
static int s_gehalten_anzahl;

static bool s_im_syscall;
static int s_syscall_sperren;       //< Sperren, die der laufende Syscall genommen hat
static int s_syscall_tiefe_max;     //< hoechstens so viele zugleich gehalten
static int s_blatt_verstoss;        //< unter einer Sperre der Schnittstelle eine weitere
static int s_pebble_unter_sperre;   //< Pebble unter einer Sperre der Schnittstelle gerufen

static bool prv_schnittstellen_sperre(const struct pbl_mutex *m) {
  return m == &s_lage_sperre || m == &s_bild_sperre || m == &s_naechte_sperre ||
         m == &s_herz_sperre || m == &s_woche_sperre;
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
// AP4: dasselbe fuer einen Abschnitt, den ein Test waehlt (Minutenteil, "Herz nachrechnen"). Der
// Heap beim Oeffnen einer Datei (der Name, settings_file.c) zaehlt getrennt.
static bool s_zaehlen;
static int s_gezaehlt_dateien;
static int s_gezaehlt_heap;
static int s_gezaehlt_heap_ausser_oeffnen;
static int s_im_oeffnen;
static int s_bild_datei_unter_sperre;  //< boulder_nacht geoeffnet, waehrend s_bild_sperre gehalten
static int s_bild_datei_geoeffnet;

status_t __real_settings_file_open(SettingsFile *file, const char *name, int max_used_space);
status_t __wrap_settings_file_open(SettingsFile *file, const char *name, int max_used_space) {
  s_syscall_dateien += s_im_syscall;
  s_gezaehlt_dateien += s_zaehlen;
  if (!strcmp(name, "boulder_nacht")) {
    s_bild_datei_geoeffnet++;
    s_bild_datei_unter_sperre += (s_bild_sperre.count > 0);
  }
  s_im_oeffnen++;
  const status_t st = __real_settings_file_open(file, name, max_used_space);
  s_im_oeffnen--;
  return st;
}

status_t __real_settings_file_open_growable(SettingsFile *file, const char *name,
                                            int max_used_space, int initial_size);
status_t __wrap_settings_file_open_growable(SettingsFile *file, const char *name,
                                            int max_used_space, int initial_size) {
  s_syscall_dateien += s_im_syscall;
  s_gezaehlt_dateien += s_zaehlen;
  return __real_settings_file_open_growable(file, name, max_used_space, initial_size);
}

int __real_pfs_open(const char *name, uint8_t op_flags, uint8_t file_type, size_t start_size);
int __wrap_pfs_open(const char *name, uint8_t op_flags, uint8_t file_type, size_t start_size) {
  s_syscall_dateien += s_im_syscall;
  s_gezaehlt_dateien += s_zaehlen && !s_im_oeffnen;
  return __real_pfs_open(name, op_flags, file_type, start_size);
}

void *__real_kernel_malloc(size_t bytes);
void *__wrap_kernel_malloc(size_t bytes) {
  s_syscall_heap += s_im_syscall;
  s_gezaehlt_heap += s_zaehlen;
  s_gezaehlt_heap_ausser_oeffnen += s_zaehlen && !s_im_oeffnen;
  return __real_kernel_malloc(bytes);
}
void *__real_kernel_zalloc(size_t bytes);
void *__wrap_kernel_zalloc(size_t bytes) {
  s_syscall_heap += s_im_syscall;
  s_gezaehlt_heap += s_zaehlen;
  s_gezaehlt_heap_ausser_oeffnen += s_zaehlen && !s_im_oeffnen;
  return __real_kernel_zalloc(bytes);
}
void *__real_kernel_malloc_check(size_t bytes);
void *__wrap_kernel_malloc_check(size_t bytes) {
  s_syscall_heap += s_im_syscall;
  s_gezaehlt_heap += s_zaehlen;
  s_gezaehlt_heap_ausser_oeffnen += s_zaehlen && !s_im_oeffnen;
  return __real_kernel_malloc_check(bytes);
}
void *__real_kernel_zalloc_check(size_t bytes);
void *__wrap_kernel_zalloc_check(size_t bytes) {
  s_syscall_heap += s_im_syscall;
  s_gezaehlt_heap += s_zaehlen;
  s_gezaehlt_heap_ausser_oeffnen += s_zaehlen && !s_im_oeffnen;
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

// Wie die Uhr (syscall_internal.c): Das erste und das letzte Byte muessen im App-RAM liegen, das
// letzte in 32 Bit gerechnet (buf + num_bytes - 1 laeuft dort um). Relativ zum App-RAM gerechnet,
// damit der Ueberlauf auf dem 64-Bit-Host derselbe ist wie auf der Uhr.
void syscall_assert_userspace_buffer(const void *buf, size_t num_bytes) {
  s_puffer_geprueft++;
  const uintptr_t a = (uintptr_t)buf;
  const uintptr_t start = (uintptr_t)s_app_ram;
  const uint32_t groesse = sizeof(s_app_ram);
  if (a < start || a - start >= groesse ||
      (uint32_t)((uint32_t)(a - start) + (uint32_t)num_bytes - 1u) >= groesse) {
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

// Ein Absturz mitten in KernelBG (AP3): Sagt der Haken nach einem Callback ja, laeuft bis zum
// naechsten Boot kein Callback mehr - was ansteht oder dazukommt, geht verloren wie bei einem
// Neustart in diesem Moment.
static bool (*s_absturz_nach)(void);
static bool s_abgestuerzt;

static void prv_kernel_bg(void) {
  while ((s_kern_n || s_app_n) && !s_abgestuerzt) {
    if (s_kern_n) {
      prv_lauf(prv_vorne(s_kern_schlange, &s_kern_n));
    }
    if (s_app_n) {
      prv_lauf(prv_vorne(s_app_schlange, &s_app_n));
    }
    if (s_absturz_nach && s_absturz_nach()) {
      s_absturz_nach = NULL;
      s_abgestuerzt = true;
    }
  }
  if (s_abgestuerzt) {
    s_kern_n = 0;
    s_app_n = 0;
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
// Der Puls der Minute, wie ihn die Glue-Kopie in den Minutensatz schreibt (AP4: test_boulder_herz).
static int32_t s_median_puls = 60;
void activity_metrics_prv_get_median_hr_bpm(int32_t *median, int32_t *total_weight) {
  if (median) {
    *median = s_median_puls;
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
// Pebbles Metriken von heute (AP4: die Woche liest die Schritte). Wie activity_metrics.c nimmt der
// Aufruf Pebbles Activity-Sperre - hier gemeldet als Grenze zu Pebble.
static int32_t s_pebble_schritte;
static int s_metrik_abfragen;
bool activity_get_metric(ActivityMetric metric, uint32_t history_len, int32_t *history) {
  prv_pebble();
  s_metrik_abfragen++;
  cl_assert_equal_i(metric, ActivityMetricStepCount);
  cl_assert_equal_i(history_len, 1);
  history[0] = s_pebble_schritte;
  return true;
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
// Was Boulder von der Nacht bekommt (AP3): jeder Satz 0x4001, wie ihn das Datalogging nimmt.
#define SAETZE_MAX 128
static NachtDlsRecord s_saetze[SAETZE_MAX];
static int s_saetze_anzahl;

DataLoggingResult dls_log(DataLoggingSession *logging_session, const void *data,
                          uint32_t num_items) {
  if (logging_session == (DataLoggingSession *)(uintptr_t)BOULDER_DLS_TAG_NACHT) {
    cl_assert(s_saetze_anzahl < SAETZE_MAX);
    memcpy(&s_saetze[s_saetze_anzahl++], data, sizeof(NachtDlsRecord));
  }
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

// Hinter S5 (test_boulder_naechte: ab 3072, 230 B).
static BoulderHerz *const s_app_herz = (BoulderHerz *)&s_app_ram[3328];
static BoulderWoche *const s_app_woche = (BoulderWoche *)&s_app_ram[3584];

static bool prv_s6(void) {
  prv_app_beginn();
  const bool r = sys_boulder_herz(s_app_herz);
  prv_app_ende();
  return r;
}

static bool prv_s7(void) {
  prv_app_beginn();
  const bool r = sys_boulder_woche(s_app_woche);
  prv_app_ende();
  return r;
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
  s_abgestuerzt = false;
  s_abo_anzahl = 0;
  s_kern_n = 0;
  s_app_n = 0;
  boulder_dauerteil_test_neustart();
  boulder_messstufe_test_zuruecksetzen();
  boulder_naechte_test_neustart();
  boulder_herz_test_neustart();
  boulder_woche_test_neustart();
  cl_assert(activity_algorithm_init(&s_rate));
}

static void prv_umfeld_beginn(void) {
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
  s_zaehlen = false;
  s_gezaehlt_dateien = 0;
  s_gezaehlt_heap = 0;
  s_gezaehlt_heap_ausser_oeffnen = 0;
  s_im_oeffnen = 0;
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
  s_absturz_nach = NULL;
  s_saetze_anzahl = 0;
  s_pebble_schritte = 0;
  s_metrik_abfragen = 0;
  s_median_puls = 60;
  pbl_cron_init();
  prv_boot();
}

static void prv_umfeld_ende(void) {
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
