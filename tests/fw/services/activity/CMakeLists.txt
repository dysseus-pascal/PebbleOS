# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

# Boulder: die Schalter wie auf obelix (services/activity/boulder/Kconfig). Was Pebbles
# Code mit Boulders Haken aendert, laeuft zweimal: mit allen Schaltern und als *_upstream
# ohne einen - dann muss Pebbles Verhalten gelten.
set(BOULDER_AN CONFIG_BOULDER_SCHLAF CONFIG_BOULDER_HRV CONFIG_BOULDER_MESSSTUFE
    CONFIG_BOULDER_HEALTH_APP)

set(test_activity_quellen
    src/fw/applib/health_service.c
    src/fw/flash_region/flash_region.c
    src/fw/flash_region/filesystem_regions.c
    src/fw/util/base64.c
    src/fw/util/stats.c
    src/fw/util/time/time.c
    src/fw/util/time/mktime.c
    src/fw/services/regular_timer/service.c
    src/fw/services/activity/activity.c
    src/fw/services/activity/activity_metrics.c
    src/fw/services/activity/activity_sessions.c
    src/fw/services/activity/activity_calculators.c
    src/fw/services/activity/hr_util.c
    src/fw/services/filesystem/flash_translation.c
    src/fw/services/filesystem/pfs.c
    src/fw/services/settings/settings_file.c
    src/fw/services/settings/settings_raw_iter.c
    src/fw/shell/normal/prefs.c
    tests/stubs/stubs_i18n.c
    tests/fakes/fake_accel_service.c
    src/fw/util/crc8.c
    src/fw/util/legacy_checksum.c
    tests/fakes/fake_events.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_spi_flash.c)

pbl_clar_test(test_activity
  SOURCES ${test_activity_quellen}
    src/fw/services/activity/boulder/messstufe.c
    src/fw/services/activity/boulder/spo2.c
  DEFINES DUMA_DISABLED CAPABILITY_HAS_HEALTH_TRACKING=1 CONFIG_HRM CONFIG_SERVICE_ACTIVITY_BATCH_SAMPLES=25
          ${BOULDER_AN}
  OVERRIDES dummy_board
)

pbl_clar_test(test_activity_upstream
  SOURCE test_activity.c
  SOURCES ${test_activity_quellen}
  DEFINES DUMA_DISABLED CAPABILITY_HAS_HEALTH_TRACKING=1 CONFIG_HRM CONFIG_SERVICE_ACTIVITY_BATCH_SAMPLES=25
  OVERRIDES dummy_board
)

pbl_clar_test(test_activity_insights
  SOURCES
    src/fw/util/base64.c
    src/fw/util/crc8.c
    src/fw/util/stats.c
    src/fw/util/time/time.c
    src/fw/util/time/mktime.c
    src/fw/services/activity/activity_insights.c
    src/fw/services/activity/insights_settings.c
    src/fw/services/timeline/metricgroup.c
    src/fw/util/rand/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    tests/fakes/fake_clock.c
    tests/fakes/fake_settings_file.c
    tests/fakes/fake_kernel_services_notifications.c
    tests/fakes/fake_rtc.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_kraepelin_algorithm
  SOURCES
    src/fw/services/activity/kraepelin/kraepelin_algorithm.c
    tests/fakes/fake_rtc.c
    tests/fw/services/activity/step_samples.c
    tests/fw/services/activity/sleep_samples_v1.c
    tests/fw/services/activity/kraepelin_reference/fourier.c
    tests/fw/services/activity/kraepelin_reference/helper_worker.c
    tests/fw/services/activity/kraepelin_reference/ProjectK_worker.c
    tests/fw/services/activity/kraepelin_reference/raw_stats.c
  DEFINES DUMA_DISABLED CONFIG_HRM
  OVERRIDES dummy_board
)

set(test_activity_algorithm_kraepelin_quellen
    src/fw/flash_region/flash_region.c
    src/fw/flash_region/filesystem_regions.c
    src/fw/util/base64.c
    src/fw/util/crc8.c
    src/fw/util/shared_circular_buffer.c
    src/fw/util/time/time.c
    src/fw/util/time/mktime.c
    src/fw/services/regular_timer/service.c
    src/fw/services/activity/kraepelin/activity_algorithm_kraepelin.c
    src/fw/services/filesystem/flash_translation.c
    src/fw/services/filesystem/pfs.c
    src/fw/services/settings/settings_file.c
    src/fw/services/settings/settings_raw_iter.c
    tests/fakes/fake_accel_service.c
    src/fw/util/legacy_checksum.c
    tests/fakes/fake_events.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_spi_flash.c)

pbl_clar_test(test_activity_algorithm_kraepelin_upstream
  SOURCE test_activity_algorithm_kraepelin.c
  SOURCES ${test_activity_algorithm_kraepelin_quellen}
  DEFINES DUMA_DISABLED
  OVERRIDES dummy_board
)

pbl_clar_test(test_activity_algorithm_kraepelin
  SOURCES ${test_activity_algorithm_kraepelin_quellen}
  DEFINES DUMA_DISABLED ${BOULDER_AN}
  OVERRIDES dummy_board
)

# Boulder: der Gehfilter (b19) und der Trainingsstart auf dem ganzen Weg - echte
# Minutenverarbeitung, echte Sitzungserkennung von Kraepelin, echte Nachtrechnung.
# Nur die Schrittzaehlung aus den Rohdaten ersetzt der Test (--wrap): fuer eine
# Velofahrt gibt es keine. Und die Minutendatei, aus der die Nacht liest: sie
# liefert die echte Nacht vom 26./27.9.2026.
pbl_clar_test(test_boulder_gehfilter
  SOURCES
    src/fw/services/activity/boulder/nacht.c
    src/fw/services/activity/boulder/schlafanalyse.c
    src/fw/flash_region/flash_region.c
    src/fw/flash_region/filesystem_regions.c
    src/fw/util/base64.c
    src/fw/util/crc8.c
    src/fw/util/shared_circular_buffer.c
    src/fw/util/time/time.c
    src/fw/util/time/mktime.c
    src/fw/services/regular_timer/service.c
    src/fw/services/activity/kraepelin/activity_algorithm_kraepelin.c
    src/fw/services/activity/kraepelin/kraepelin_algorithm.c
    src/fw/services/filesystem/flash_translation.c
    src/fw/services/filesystem/pfs.c
    src/fw/services/settings/settings_file.c
    src/fw/services/settings/settings_raw_iter.c
    tests/fakes/fake_accel_service.c
    src/fw/util/legacy_checksum.c
    tests/fakes/fake_events.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_spi_flash.c
  DEFINES DUMA_DISABLED ${BOULDER_AN}
  LIBS -Wl,--wrap=kalg_analyze_samples -Wl,--wrap=activity_algorithm_get_minute_history
  OVERRIDES dummy_board
)

pbl_clar_test(test_health_util
  SOURCES
    src/fw/services/activity/health_util.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_hr_util
  SOURCES
    src/fw/services/activity/hr_util.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_workout_service
  SOURCES
    src/fw/services/activity/activity_calculators.c
    src/fw/services/activity/hr_util.c
    src/fw/services/activity/workout_service.c
    tests/fakes/fake_events.c
    tests/fakes/fake_rtc.c
  DEFINES CONFIG_HRM
  OVERRIDES dummy_board
)

pbl_clar_test(test_activity_calculators
  SOURCES
    src/fw/services/activity/activity_calculators.c
  OVERRIDES dummy_board
)

# Boulder: die Schlafanalyse (Uebertragung von Kiesel-Helper), gegen drei echte
# Naechte neben einem Fitbit, einen Snapshot (26.9.) und erfundene Naechte.
pbl_clar_test(test_boulder_schlaf
  SOURCES
    src/fw/services/activity/boulder/schlafanalyse.c
)

# Boulder: die Nachtrechnung im Takt der Uhr, mit der echten Nacht - und mit dem
# echten Sitzungsspeicher und Schlafzustand der Uhr (activity_sessions.c). Gezaehlt
# wird, wie oft das Bild der Nacht in die Datei kommt (--wrap).
set(test_boulder_nacht_quellen
    src/fw/services/activity/boulder/nacht.c
    src/fw/services/activity/boulder/schlafanalyse.c
    src/fw/services/activity/activity_sessions.c
    src/fw/util/crc8.c
    tests/fakes/fake_settings_file.c)
pbl_clar_test(test_boulder_nacht
  SOURCES ${test_boulder_nacht_quellen}
  DEFINES CONFIG_SERVICE_KIESELSPORT ${BOULDER_AN}
  LIBS -Wl,--wrap=settings_file_set
)

# Dieselbe Nacht mit einzelnen Schaltern aus: ohne HRV keine HRV-Saetze, ohne Messstufe
# Stufe 0 im Satz - die Antworten aus boulder/haken.h.
pbl_clar_test(test_boulder_nacht_ohne_hrv
  SOURCE test_boulder_nacht.c
  SOURCES ${test_boulder_nacht_quellen}
  DEFINES CONFIG_SERVICE_KIESELSPORT CONFIG_BOULDER_SCHLAF CONFIG_BOULDER_MESSSTUFE
  LIBS -Wl,--wrap=settings_file_set
)
pbl_clar_test(test_boulder_nacht_ohne_messstufe
  SOURCE test_boulder_nacht.c
  SOURCES ${test_boulder_nacht_quellen}
  DEFINES CONFIG_SERVICE_KIESELSPORT CONFIG_BOULDER_SCHLAF CONFIG_BOULDER_HRV
  LIBS -Wl,--wrap=settings_file_set
)

# Boulder: HRV-Fenster in der Nacht, mit den Abstaenden ueber den Rueckruf des
# HRM-Dienstes (wie auf der Uhr) und den Fenstern in einer Datei.
pbl_clar_test(test_boulder_hrv
  SOURCES
    src/fw/services/activity/boulder/hrv.c
    src/fw/util/crc8.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_settings_file.c
  DEFINES CONFIG_HRM ${BOULDER_AN}
)

# Boulder: der echte Smart Alarm (alarm.c) in der echten Nacht - der Schlafzustand
# kommt aus der echten activity_sessions.c, die Schlafbloecke aus der echten
# Nachtrechnung.
pbl_clar_test(test_boulder_wecker
  SOURCES
    src/fw/services/activity/boulder/nacht.c
    src/fw/services/activity/boulder/schlafanalyse.c
    src/fw/services/activity/activity_sessions.c
    src/fw/services/alarms/alarm.c
    src/fw/services/alarms/alarm_pin.c
    src/fw/flash_region/flash_region.c
    src/fw/flash_region/filesystem_regions.c
    src/fw/util/time/time.c
    src/fw/util/time/mktime.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    subsys/cron/cron.c
    src/fw/services/filesystem/flash_translation.c
    src/fw/services/filesystem/pfs.c
    src/fw/services/blob_db/timeline_item_storage.c
    src/fw/services/settings/settings_file.c
    src/fw/services/settings/settings_raw_iter.c
    src/fw/services/timeline/attribute.c
    src/fw/services/timeline/item.c
    src/fw/services/timeline/attributes_actions.c
    src/fw/services/timeline/attribute_group.c
    src/fw/util/crc8.c
    src/fw/util/legacy_checksum.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_spi_flash.c
  DEFINES CAPABILITY_HAS_HEALTH_TRACKING=1 ${BOULDER_AN}
  OVERRIDES dummy_board
)
