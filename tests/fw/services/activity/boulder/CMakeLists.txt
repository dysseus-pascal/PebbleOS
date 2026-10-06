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
  DEFINES DUMA_DISABLED CONFIG_HRM ${BOULDER_KALG_NAMEN}
  OVERRIDES dummy_board
)

# Pebbles Glue-Tests gegen den kopierten Glue.
pbl_clar_test(test_boulder_activity_algorithm_kraepelin
  SOURCE ${boulder_pebble_tests}/test_activity_algorithm_kraepelin.c
  SOURCES
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
)

# Die geteilten Formate (Tag 84, Sitzungstypen, 0x4001/0x4002, NachtBild) und der
# Waechter fuer den Bau (vertraege.c), dazu die Referenzsaetze aus b26.
pbl_clar_test(test_boulder_vertraege
  SOURCES
    fw/services/activity/boulder/vertraege.c
)

# Kopie und Original zaehlen gleich: dieselben Pruefkorpus-Laeufe, Ausgabe Zeile fuer Zeile.
add_test(NAME test_boulder_kraepelin_gleich
  COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/gleich_zaehlen.py
          $<TARGET_FILE:test_kraepelin_algorithm>
          $<TARGET_FILE:test_boulder_kraepelin_algorithm>)

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
