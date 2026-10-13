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
# Waechter fuer den Bau (vertraege.c), dazu die Referenzsaetze aus b26. Wie auf obelix und
# qemu_emery mit Kieselsport (Tag 84 in Fassung 7, P4); ohne den Schalter die Fassung 3 der
# Boards ohne Variante.
pbl_clar_test(test_boulder_vertraege
  SOURCES
    fw/services/activity/boulder/vertraege.c
  DEFINES CONFIG_BOULDER_KIESELSPORT
)
pbl_clar_test(test_boulder_vertraege_ohne_kieselsport
  SOURCE test_boulder_vertraege.c
  SOURCES
    fw/services/activity/boulder/vertraege.c
)

# Die Schlafrechnung selbst, aus b26 (main a7d8b16e1) uebernommen: die Naechte neben einem Fitbit
# und die Regeln an gebauten Naechten.
pbl_clar_test(test_boulder_schlaf
  SOURCES
    fw/services/activity/boulder/schlafanalyse.c
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
# AP4: Saetze 0x4001/0x4002 und NachtBild Byte fuer Byte wie b26, Ablage in der echten
# settings_file.c, Neustarts und Zeitspruenge. Schritt 2, AP4 (V3): der Ruhepuls aus sieben
# Naechten, bis in das Profil der Pulskalorien (kalorien.c).
pbl_clar_test(test_boulder_zyklen
  SOURCES
    ${boulder_glue_umfeld}
    fw/services/activity/boulder/kalorien.c
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
    fw/services/activity/boulder/spo2.c
    fw/services/activity/boulder/wecker.c
    fw/services/activity/boulder/ausgabe.c
    fw/services/activity/boulder/speicher.c
    fw/services/activity/boulder/diagnose.c
    tests/fw/services/activity/boulder/fake_heap.c
    fw/services/activity/boulder/umzug.c
    fw/services/activity/boulder/umzug_prefs.c
    fw/services/activity/boulder/kontext.c
    subsys/cron/cron.c
    tests/fw/services/activity/boulder/fake_messung.c
    tests/fw/services/activity/boulder/fake_nachfrage.c
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

# AP5: Messstufe, Puls-Abo, HRV-Fenster gegen den echten hrm_manager.c
pbl_clar_test(test_boulder_messung
  SOURCES
    fw/services/hrm/hrm_manager.c
    fw/services/activity/boulder/messstufe.c
    fw/services/activity/boulder/puls.c
    fw/services/activity/boulder/hrv.c
    fw/services/activity/boulder/takt.c
    fw/flash_region/flash_region.c
    fw/flash_region/filesystem_regions.c
    fw/services/filesystem/flash_translation.c
    fw/services/filesystem/pfs.c
    fw/services/settings/settings_file.c
    fw/services/settings/settings_raw_iter.c
    kernel/tick.c
    tests/fakes/fake_accel_service.c
    tests/fakes/fake_events.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_spi_flash.c
  DEFINES DUMA_DISABLED CONFIG_HRM CONFIG_HRM_HRV
  OVERRIDES dummy_board
)

# Die strenge Weckregel (AP7, T4.2): der echte alarm.c mit E4 und als Vergleich unveraendert,
# mit der echten activity_sessions.c und dem Live-Zustand der Variante.
# alarm.c bindet der Test selbst ein (#include), damit ein Neustart auch sein RAM loescht.
set(boulder_wecker_quellen
    ${boulder_glue_umfeld}
    subsys/cron/cron.c
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
    fw/services/activity/boulder/spo2.c
    fw/services/activity/boulder/wecker.c
    fw/services/activity/boulder/wecklauf.c
    fw/services/activity/boulder/ausgabe.c
    fw/services/activity/boulder/speicher.c
    fw/services/activity/boulder/diagnose.c
    tests/fw/services/activity/boulder/fake_heap.c
    fw/services/activity/boulder/umzug.c
    fw/services/activity/boulder/umzug_prefs.c
    fw/services/activity/boulder/kontext.c
    tests/fw/services/activity/boulder/fake_messung.c
    tests/fw/services/activity/boulder/fake_nachfrage.c)
pbl_clar_test(test_boulder_wecker
  SOURCES ${boulder_wecker_quellen}
  DEFINES DUMA_DISABLED CONFIG_HRM CONFIG_SERVICE_ACTIVITY_ALGORITHM_BOULDER
  OVERRIDES dummy_board
)
pbl_clar_test(test_boulder_wecker_vergleich
  SOURCE test_boulder_wecker.c
  SOURCES ${boulder_wecker_quellen}
  DEFINES DUMA_DISABLED CONFIG_HRM
  OVERRIDES dummy_board
)

# Release-Pruefung, nicht Teil der Baseline: hat upstream das Original oder einen
# Vertrag geaendert? (cmake -DBOULDER_RELEASE=ON)
option(BOULDER_RELEASE "Boulder: Release-Pruefungen gegen upstream/main" OFF)
if(BOULDER_RELEASE)
  add_test(NAME test_boulder_kopie_nachziehen
    COMMAND ${PYTHON_EXECUTABLE} ${PBL_BASE}/tools/boulder/kopie_pruefen.py ${PBL_BASE}
            --nachziehen upstream/main)
endif()

# AP8, Wechsel von b25 (Spezifikation 8): shellpref mit dem b25-Bestand unter V2 oeffnen und
# danach schreiben (T8.5), mit dem echten settings_file und PFS.
pbl_clar_test(test_boulder_umzug_prefs
  SOURCES
    fw/applib/dict.c
    fw/services/filesystem/flash_translation.c
    fw/services/filesystem/pfs.c
    tests/fakes/fake_spi_flash.c
    fw/flash_region/filesystem_regions.c
    fw/flash_region/flash_region.c
    tests/fakes/fake_rtc.c
    fw/system/hexdump.c
    fw/services/settings/settings_file.c
    fw/services/settings/settings_raw_iter.c
    fw/kernel/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    fw/services/activity/boulder/umzug_prefs.c
  DEFINES DUMA_DISABLED
  OVERRIDES dummy_board
)

# T8.4: Spool-Koepfe, Activity-Datei und Sitzung wie in b25 (main 217428e90), mit Gegenprobe.
add_test(NAME test_boulder_umzug_aufbau
  COMMAND ${PYTHON_EXECUTABLE} ${PBL_BASE}/tools/boulder/umzug_pruefen.py ${PBL_BASE})
add_test(NAME test_boulder_umzug_aufbau_gegenprobe
  COMMAND ${PYTHON_EXECUTABLE} ${PBL_BASE}/tools/boulder/umzug_pruefen.py ${PBL_BASE} --gegenprobe)

# AP6: Velo und Auto (T5.1-T5.3, T2.3, V5-V8) mit der echten Glue-Kopie, dem echten
# Dauerteil, der echten Nachfrage (nur Pebbles Dialog-Bibliothek als Attrappe im Test) und
# der echten activity_sessions.c. Schritt 2, AP4 (T4.1-T4.4, T5.2 Fahrt): dazu Pebbles echter
# Workout-Dienst, wie er auf obelix und qemu_emery gebaut wird (P1 und kalorien.c), damit Start
# und Stopp die Variante ueber D15 erreichen und Pebble die Sitzungen schliesst und speichert.
pbl_clar_test(test_boulder_fahrt
  SOURCES
    ${boulder_glue_umfeld}
    fw/services/activity/activity_sessions.c
    fw/services/activity/activity_calculators.c
    fw/services/activity/hr_util.c
    fw/services/activity/workout_service.c
    fw/services/activity/boulder/kalorien.c
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
    fw/services/activity/boulder/spo2.c
    fw/services/activity/boulder/wecker.c
    fw/services/activity/boulder/ausgabe.c
    fw/services/activity/boulder/speicher.c
    fw/services/activity/boulder/diagnose.c
    tests/fw/services/activity/boulder/fake_heap.c
    fw/services/activity/boulder/umzug.c
    fw/services/activity/boulder/umzug_prefs.c
    fw/services/activity/boulder/kontext.c
    fw/services/activity/boulder/nachfrage.c
    subsys/cron/cron.c
    tests/fw/services/activity/boulder/fake_messung.c
  DEFINES DUMA_DISABLED CONFIG_HRM CONFIG_BOULDER_KIESELSPORT
  OVERRIDES dummy_board
  LIBS
    -Wl,--wrap=kernel_malloc,--wrap=kernel_zalloc,--wrap=kernel_malloc_check
    -Wl,--wrap=kernel_zalloc_check
    -Wl,--wrap=boulder_kalg_analyze_samples,--wrap=boulder_kalg_minute_stats
)

# AP9: der taegliche Diagnosesatz - Laufzeit je Arbeit (auch bei fruehem Ruecksprung),
# Heap-Niedrigstand aus dem echten Heap, Neustarts ueber die echte settings_file.c.
pbl_clar_test(test_boulder_diagnose
  SOURCES
    ${boulder_glue_umfeld}
    fw/services/activity/boulder/takt.c
    fw/services/activity/boulder/diagnose.c
    tests/fw/services/activity/boulder/fake_heap.c
  DEFINES DUMA_DISABLED
  OVERRIDES dummy_board
)

# Schritt 3, AP2: die Schnittstelle zur Health-App (S1-S4, Lage, Ziele; T2.3, T2.4, T6.2, T6.3)
# gegen die echte Variante wie test_boulder_zyklen, die echte messstufe.c und Pebbles echte BlobDB
# (api.c) mit der echten healthdb (health_db.c). --wrap zaehlt Dateien und Heap im Syscall.
# AP3: dasselbe Umfeld (schnittstelle_umfeld.h) fuer die Naechte (S5, T4.3) in
# test_boulder_naechte.
set(boulder_schnittstelle_quellen
    ${boulder_glue_umfeld}
    fw/services/activity/activity_sessions.c
    fw/services/activity/boulder/kalorien.c
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
    fw/services/activity/boulder/spo2.c
    fw/services/activity/boulder/wecker.c
    fw/services/activity/boulder/ausgabe.c
    fw/services/activity/boulder/speicher.c
    fw/services/activity/boulder/diagnose.c
    tests/fw/services/activity/boulder/fake_heap.c
    fw/services/activity/boulder/umzug.c
    fw/services/activity/boulder/umzug_prefs.c
    fw/services/activity/boulder/kontext.c
    fw/services/activity/boulder/messstufe.c
    fw/services/activity/boulder/schnittstelle.c
    fw/services/activity/boulder/ziele.c
    fw/services/activity/boulder/naechte.c
    fw/services/activity/boulder/herz.c
    fw/services/activity/boulder/woche.c
    fw/services/blob_db/api.c
    fw/services/blob_db/health_db.c
    subsys/cron/cron.c
    tests/fw/services/activity/boulder/fake_nachfrage.c)
set(boulder_schnittstelle_libs
    -Wl,--wrap=settings_file_open,--wrap=settings_file_open_growable,--wrap=pfs_open
    -Wl,--wrap=kernel_malloc,--wrap=kernel_zalloc,--wrap=kernel_malloc_check
    -Wl,--wrap=kernel_zalloc_check)
pbl_clar_test(test_boulder_schnittstelle
  SOURCES ${boulder_schnittstelle_quellen}
  DEFINES DUMA_DISABLED CONFIG_HRM CONFIG_BOULDER_KIESELSPORT CONFIG_BOULDER_HEALTH_APP
          UNITTEST_WITH_SYSCALL_PRIVILEGES=1
  OVERRIDES dummy_board
  LIBS ${boulder_schnittstelle_libs}
)
# Die Liste der Naechte zaehlt dazu, wie oft settings_file_set sie schreibt (Schreiblast).
pbl_clar_test(test_boulder_naechte
  SOURCES ${boulder_schnittstelle_quellen}
  DEFINES DUMA_DISABLED CONFIG_HRM CONFIG_BOULDER_KIESELSPORT CONFIG_BOULDER_HEALTH_APP
          UNITTEST_WITH_SYSCALL_PRIVILEGES=1
  OVERRIDES dummy_board
  LIBS ${boulder_schnittstelle_libs} -Wl,--wrap=settings_file_set
)
# AP4: die Woche (T4.5) gegen Pebbles echtes activity.c, activity_metrics.c und die healthdb; die
# Umgebung wie Pebbles test_activity.
pbl_clar_test(test_boulder_woche
  SOURCES
    fw/applib/health_service.c
    fw/flash_region/flash_region.c
    fw/flash_region/filesystem_regions.c
    fw/services/clock/time.c
    lib/c/time/gmtime.c
    lib/c/time/mktime.c
    fw/services/regular_timer/service.c
    fw/services/activity/activity.c
    fw/services/activity/activity_metrics.c
    fw/services/activity/activity_sessions.c
    fw/services/activity/activity_calculators.c
    fw/services/activity/hr_util.c
    fw/services/blob_db/health_db.c
    fw/services/activity/boulder/woche.c
    fw/services/filesystem/flash_translation.c
    fw/services/filesystem/pfs.c
    fw/services/settings/settings_file.c
    fw/services/settings/settings_raw_iter.c
    fw/shell/normal/prefs.c
    tests/stubs/stubs_i18n.c
    tests/fakes/fake_accel_service.c
    tests/fakes/fake_events.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_spi_flash.c
  DEFINES DUMA_DISABLED CAPABILITY_HAS_HEALTH_TRACKING=1 CONFIG_HRM
          CONFIG_SERVICE_ACTIVITY_BATCH_SAMPLES=25 CONFIG_BOULDER_HEALTH_APP
  OVERRIDES dummy_board
  LIBS -Wl,--wrap=settings_file_set
)
# AP4: Herzband und Zonen (S6, T4.4) im selben Umfeld. Die Minuten laufen durch die Glue-Kopie in
# Pebbles Minutendatei; Bewegung je Minute aus dem Test (boulder_kalg_*). Gezaehlt: der Durchlauf
# (boulder_kraepelin_minuten, settings_file_each, flash_read_bytes) und was "woche" schreibt.
pbl_clar_test(test_boulder_herz
  SOURCES ${boulder_schnittstelle_quellen}
  DEFINES DUMA_DISABLED CONFIG_HRM CONFIG_BOULDER_KIESELSPORT CONFIG_BOULDER_HEALTH_APP
          UNITTEST_WITH_SYSCALL_PRIVILEGES=1
  OVERRIDES dummy_board
  LIBS ${boulder_schnittstelle_libs} -Wl,--wrap=settings_file_set,--wrap=settings_file_each
       -Wl,--wrap=boulder_kalg_analyze_samples,--wrap=boulder_kalg_minute_stats
       -Wl,--wrap=boulder_kraepelin_minuten,--wrap=flash_read_bytes
)
