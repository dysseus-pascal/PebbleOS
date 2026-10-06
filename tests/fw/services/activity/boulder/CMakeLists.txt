# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0

# Boulder-Kraepelin: Tests der Variante.

# Die Kopie traegt eigene Namen (namen.h). Pebbles Tests laufen gegen die Kopie,
# wenn sie dieselben Namen als -D bekommen.
file(STRINGS ${PBL_BASE}/include/pbl/services/activity/boulder/kraepelin/namen.h
     boulder_namen_zeilen REGEX "^#define kalg_[a-z_]+ boulder_kalg_[a-z_]+$")
set(BOULDER_KALG_NAMEN "")
foreach(zeile ${boulder_namen_zeilen})
  string(REGEX REPLACE "^#define ([a-z_]+) ([a-z_]+)$" "\\1=\\2" name "${zeile}")
  list(APPEND BOULDER_KALG_NAMEN ${name})
endforeach()

set(boulder_pebble_tests ${PBL_BASE}/tests/fw/services/activity)

# Pebbles Pruefkorpus (Schritte, Schlaf, Aktivitaeten) gegen den kopierten Kern.
pbl_clar_test(test_boulder_kraepelin_algorithm
  SOURCE ${boulder_pebble_tests}/test_kraepelin_algorithm.c
  SOURCES
    fw/services/activity/boulder/kraepelin/kraepelin_algorithm.c
    tests/fakes/fake_rtc.c
    tests/fw/services/activity/step_samples.c
    tests/fw/services/activity/sleep_samples_v1.c
    tests/fw/services/activity/kraepelin_reference/fourier.c
    tests/fw/services/activity/kraepelin_reference/helper_worker.c
    tests/fw/services/activity/kraepelin_reference/ProjectK_worker.c
    tests/fw/services/activity/kraepelin_reference/raw_stats.c
    fw/services/activity/boulder/szene.c
  DEFINES DUMA_DISABLED CONFIG_HRM ${BOULDER_KALG_NAMEN}
  OVERRIDES dummy_board
)

# Pebbles Glue-Tests gegen den kopierten Glue. Die KernelBG-Schlange laeuft nach jedem
# Minutenlauf (glue_pebble_zusatz.c: D9, D12 schreiben in eigenen Callbacks).
pbl_clar_test(test_boulder_activity_algorithm_kraepelin
  SOURCE ${boulder_pebble_tests}/test_activity_algorithm_kraepelin.c
  SOURCES
    fw/services/activity/boulder/takt.c
    tests/fw/services/activity/boulder/fake_dauerteil.c
    tests/fw/services/activity/boulder/glue_pebble_zusatz.c
    fw/flash_region/flash_region.c
    fw/flash_region/filesystem_regions.c
    fw/services/clock/time.c
    lib/c/time/gmtime.c
    lib/c/time/mktime.c
    fw/services/regular_timer/service.c
    fw/services/activity/boulder/kraepelin/activity_algorithm_kraepelin.c
    fw/services/filesystem/flash_translation.c
    fw/services/filesystem/pfs.c
    fw/services/settings/settings_file.c
    fw/services/settings/settings_raw_iter.c
    tests/fakes/fake_accel_service.c
    tests/fakes/fake_events.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_spi_flash.c
  DEFINES DUMA_DISABLED ${BOULDER_KALG_NAMEN}
  OVERRIDES dummy_board
  LIBS -Wl,--wrap=activity_algorithm_minute_handler
)

# Die Abweichungen D1-D14 der Glue-Kopie mit dem kopierten Kern (AP1).
pbl_clar_test(test_boulder_glue
  SOURCES
    fw/flash_region/flash_region.c
    fw/flash_region/filesystem_regions.c
    fw/services/clock/time.c
    lib/c/time/gmtime.c
    lib/c/time/mktime.c
    fw/services/regular_timer/service.c
    fw/services/activity/boulder/kraepelin/activity_algorithm_kraepelin.c
    fw/services/activity/boulder/kraepelin/kraepelin_algorithm.c
    fw/services/activity/boulder/szene.c
    fw/services/activity/boulder/takt.c
    fw/services/filesystem/flash_translation.c
    fw/services/filesystem/pfs.c
    fw/services/settings/settings_file.c
    fw/services/settings/settings_raw_iter.c
    tests/fakes/fake_accel_service.c
    tests/fakes/fake_events.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_spi_flash.c
    tests/fw/services/activity/boulder/fake_dauerteil.c
  DEFINES DUMA_DISABLED CONFIG_HRM
  OVERRIDES dummy_board
  LIBS
    -Wl,--wrap=kernel_malloc,--wrap=kernel_zalloc,--wrap=kernel_malloc_check
    -Wl,--wrap=kernel_zalloc_check,--wrap=task_malloc,--wrap=task_zalloc
    -Wl,--wrap=settings_file_open,--wrap=settings_file_each,--wrap=settings_file_set
    -Wl,--wrap=boulder_kalg_analyze_samples,--wrap=boulder_kalg_minute_stats
)

# T2.2b: Glue-Differenztest, dieselbe Quelle mit Original und mit Kopie.
set(boulder_glue_umfeld
    fw/flash_region/flash_region.c
    fw/flash_region/filesystem_regions.c
    fw/services/clock/time.c
    lib/c/time/gmtime.c
    lib/c/time/mktime.c
    fw/services/regular_timer/service.c
    fw/services/filesystem/flash_translation.c
    fw/services/filesystem/pfs.c
    fw/services/settings/settings_file.c
    fw/services/settings/settings_raw_iter.c
    tests/fakes/fake_accel_service.c
    tests/fakes/fake_events.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_spi_flash.c)
pbl_clar_test(test_boulder_glue_differenz_original
  SOURCE test_boulder_glue_differenz.c
  SOURCES
    ${boulder_glue_umfeld}
    fw/services/activity/kraepelin/activity_algorithm_kraepelin.c
    fw/services/activity/kraepelin/kraepelin_algorithm.c
  DEFINES DUMA_DISABLED
  OVERRIDES dummy_board
  LIBS -Wl,--wrap=kalg_analyze_samples,--wrap=kalg_minute_stats
)
pbl_clar_test(test_boulder_glue_differenz_kopie
  SOURCE test_boulder_glue_differenz.c
  SOURCES
    ${boulder_glue_umfeld}
    fw/services/activity/boulder/kraepelin/activity_algorithm_kraepelin.c
    fw/services/activity/boulder/kraepelin/kraepelin_algorithm.c
    fw/services/activity/boulder/takt.c
    tests/fw/services/activity/boulder/fake_dauerteil.c
  DEFINES DUMA_DISABLED BOULDER_KOPIE
  OVERRIDES dummy_board
  LIBS -Wl,--wrap=boulder_kalg_analyze_samples,--wrap=boulder_kalg_minute_stats
)

# Die geteilten Formate (Tag 84, Sitzungstypen, 0x4001/0x4002, NachtBild) und der
# Waechter fuer den Bau (vertraege.c), dazu die Referenzsaetze aus b26.
pbl_clar_test(test_boulder_vertraege
  SOURCES
    fw/services/activity/boulder/vertraege.c
)

# Der Schlafkern (AP2): b26-Rechnung im Nachlauf, Satz 0x4001 wie b26 auf allen
# Referenznaechten.
pbl_clar_test(test_boulder_schlafkern
  SOURCES
    fw/services/activity/boulder/schlafanalyse.c
    fw/services/activity/boulder/eingang.c
    fw/services/activity/boulder/nachlauf.c
)

# Zyklen, Live-Zustand, Nickerchen (AP3): mit der echten activity_sessions.c und der
# Glue-Kopie (get_last_sleep_utc, post_process), Minute fuer Minute wie auf der Uhr.
pbl_clar_test(test_boulder_zyklen
  SOURCES
    ${boulder_glue_umfeld}
    fw/services/activity/activity_sessions.c
    fw/services/activity/boulder/kraepelin/activity_algorithm_kraepelin.c
    fw/services/activity/boulder/kraepelin/kraepelin_algorithm.c
    fw/services/activity/boulder/szene.c
    fw/services/activity/boulder/takt.c
    fw/services/activity/boulder/dauerteil.c
    fw/services/activity/boulder/nacht.c
    fw/services/activity/boulder/live.c
    fw/services/activity/boulder/zyklen.c
    fw/services/activity/boulder/eingang.c
    fw/services/activity/boulder/nachlauf.c
    fw/services/activity/boulder/schlafanalyse.c
  DEFINES DUMA_DISABLED CONFIG_HRM
  OVERRIDES dummy_board
)

# schlafanalyse.c/.h sind bytegleich zu b26 (main a7d8b16e1).
add_test(NAME test_boulder_schlafanalyse_wie_b26
  COMMAND ${CMAKE_COMMAND}
    -DDATEIEN=${PBL_BASE}/fw/services/activity/boulder/schlafanalyse.c|${PBL_BASE}/include/pbl/services/activity/boulder/schlafanalyse.h
    -DSOLL=d75621f7510a147c2f7b3220172f8c94060fd2aeddcf3b290885e1c6035d61dd|6334fe6ae9de942379075a96344479bbf1228748c7ff867fa1db94853110f35a
    -P ${CMAKE_CURRENT_SOURCE_DIR}/bytegleich.cmake)

# Kopie und Original zaehlen gleich: dieselben Pruefkorpus-Laeufe, Ausgabe Zeile fuer Zeile.
add_test(NAME test_boulder_kraepelin_gleich
  COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/gleich_zaehlen.py
          $<TARGET_FILE:test_kraepelin_algorithm>
          $<TARGET_FILE:test_boulder_kraepelin_algorithm>)

# T2.2b: Original und Kopie gleich ohne Historie-Lesen; mit Lesen die Wirkung von D7 (V2).
add_test(NAME test_boulder_glue_differenz
  COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/glue_differenz.py
          $<TARGET_FILE:test_boulder_glue_differenz_original>
          $<TARGET_FILE:test_boulder_glue_differenz_kopie>)

# Die Waechter: Kopie markiert, Kriterium 1, Vertraege, Gegenproben.
add_test(NAME test_boulder_kopie_pruefen
  COMMAND ${PYTHON_EXECUTABLE} ${PBL_BASE}/tools/boulder/kopie_pruefen.py ${PBL_BASE})
add_test(NAME test_boulder_k1_waechter
  COMMAND ${PYTHON_EXECUTABLE} ${PBL_BASE}/tools/boulder/k1_waechter.py ${PBL_BASE})
add_test(NAME test_boulder_vertraege_liste
  COMMAND ${PYTHON_EXECUTABLE} ${PBL_BASE}/tools/boulder/vertraege.py ${PBL_BASE})
add_test(NAME test_boulder_waechter_gegenproben
  COMMAND ${PYTHON_EXECUTABLE} ${PBL_BASE}/tools/boulder/test_kopie_pruefen.py)

# Release-Pruefung, nicht Teil der Baseline: hat upstream das Original oder einen
# Vertrag geaendert? (cmake -DBOULDER_RELEASE=ON)
option(BOULDER_RELEASE "Boulder: Release-Pruefungen gegen upstream/main" OFF)
if(BOULDER_RELEASE)
  add_test(NAME test_boulder_kopie_nachziehen
    COMMAND ${PYTHON_EXECUTABLE} ${PBL_BASE}/tools/boulder/kopie_pruefen.py ${PBL_BASE}
            --nachziehen upstream/main)
endif()
