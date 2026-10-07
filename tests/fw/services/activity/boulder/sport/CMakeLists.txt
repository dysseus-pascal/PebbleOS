# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0

# Kieselsport (Schritt 2): Tests des Dienstes unter fw/services/activity/boulder/sport/.

# Pebbles Workout-Dienst, wie er auf obelix und qemu_emery gebaut wird: die echte
# workout_service.c mit P1 (Kieselsport an, mit Puls) und Boulders Seite davon (kalorien.c).
set(kieselsport_workout_dienst
  fw/services/activity/activity_calculators.c
  fw/services/activity/hr_util.c
  fw/services/activity/workout_service.c
  fw/services/activity/boulder/kalorien.c
  tests/fw/services/activity/boulder/sport/fake_variante.c
  tests/fakes/fake_events.c
  tests/fakes/fake_rtc.c
)

# Der Vertrag mit Pebbles Workout-Dienst: worauf Kieselsport baut.
pbl_clar_test(test_kieselsport_vertrag
  SOURCES ${kieselsport_workout_dienst}
  DEFINES CONFIG_HRM CONFIG_BOULDER_KIESELSPORT
  OVERRIDES dummy_board
)

# AP1: Pulskalorien und Verwerfen. Die Rechnung allein (T5.1 gegen Boulders Code) ...
pbl_clar_test(test_boulder_kalorien
  SOURCES
    fw/services/activity/boulder/kalorien.c
    tests/fw/services/activity/boulder/sport/fake_variante.c
  OVERRIDES dummy_board
)

# ... und ueber P1 im echten Dienst bis in Sitzung und Meldung (T5.1, T5.2, T3.1).
pbl_clar_test(test_kieselsport_kalorien
  SOURCES ${kieselsport_workout_dienst}
  DEFINES CONFIG_HRM CONFIG_BOULDER_KIESELSPORT
  OVERRIDES dummy_board
)

# Pebbles 24 Tests des Workout-Dienstes mit P1 (Kriterium 8): Ohne Puls rechnet P1 wie Pebble
# (F5 a = B), also muessen sie unveraendert gruen bleiben. Pebbles eigener Lauf ohne den
# Schalter bleibt daneben bestehen (tests/fw/services/activity/CMakeLists.txt).
pbl_clar_test(test_workout_service_kieselsport
  SOURCE ${PBL_BASE}/tests/fw/services/activity/test_workout_service.c
  SOURCES ${kieselsport_workout_dienst}
  DEFINES CONFIG_HRM CONFIG_BOULDER_KIESELSPORT
  OVERRIDES dummy_board
)

# AP2: der Dienst (Port von b25 services/kieselsport/) auf Pebbles Workout-Dienst. Die b25-Tests
# laufen mit der echten Plattform gegen das echte workout_service.c (mit P1 und kalorien.c) und
# die echte activity_sessions.c (T2.1); dazu die Sperrfolge (T7.2) in jedem Test. Persist und
# HRM-Manager sind im Test nachgebildet; speicher.c hat seinen eigenen Test.
set(kieselsport_dienst
  fw/services/activity/boulder/sport/abschnitt.c
  fw/services/activity/boulder/sport/art.c
  fw/services/activity/boulder/sport/bahnen.c
  fw/services/activity/boulder/sport/dienst.c
  fw/services/activity/boulder/sport/kurve.c
  fw/services/activity/boulder/sport/pausen.c
  fw/services/activity/boulder/sport/plattform.c
  fw/services/activity/boulder/sport/puls.c
  fw/services/activity/boulder/sport/reps.c
  fw/services/activity/boulder/sport/training.c
  fw/services/activity/boulder/sport/wartend.c
  fw/services/activity/boulder/takt.c
)
pbl_clar_test(test_kieselsport
  SOURCES ${kieselsport_dienst} ${kieselsport_workout_dienst}
    fw/services/activity/activity_sessions.c
  DEFINES CONFIG_HRM CONFIG_MAG CONFIG_BOULDER_KIESELSPORT
  OVERRIDES dummy_board
)

# Der Persist von Kieselsport ueber der Datei der Uhr: Fehler beim Lesen, Schreiben und Loeschen
# muessen im Log stehen (aus b25).
pbl_clar_test(test_kieselsport_speicher
  SOURCES
    fw/services/activity/boulder/sport/speicher.c
)

# T2.2 (F2 = A): die HRM-Abos im Training gegen den echten hrm_manager.c - genau ein Abo mit 1 s
# (Kieselsports), keines von Pebbles Workout-Dienst, Pause 10 s, nach Stopp und Verwerfen keines.
pbl_clar_test(test_kieselsport_abo
  SOURCES
    ${kieselsport_dienst}
    fw/services/activity/activity_calculators.c
    fw/services/activity/hr_util.c
    fw/services/activity/workout_service.c
    fw/services/activity/boulder/kalorien.c
    fw/services/hrm/hrm_manager.c
    kernel/tick.c
    tests/fw/services/activity/boulder/sport/fake_variante.c
    tests/fakes/fake_events.c
    tests/fakes/fake_rtc.c
  DEFINES CONFIG_HRM CONFIG_BOULDER_KIESELSPORT
  OVERRIDES dummy_board
)

# AP6 (T6.2, T6.3): der Wechsel von b25. Kieselsports Persist, wie b25 ihn hinterlaesst
# (tests/fixtures/activity/boulder/kieselsport-b25/), im echten Persist-Dienst; V2s Dienst liest
# ihn ueber speicher.c, die App meldet Einstellungen und Liegengebliebenes ans Telefon.
pbl_clar_test(test_kieselsport_wechsel
  SOURCES
    ${kieselsport_dienst}
    ${kieselsport_workout_dienst}
    fw/services/activity/activity_sessions.c
    fw/services/activity/boulder/sport/speicher.c
    fw/services/persist/service.c
    fw/applib/persist.c
    fw/services/settings/settings_file.c
    fw/services/settings/settings_raw_iter.c
    fw/services/filesystem/pfs.c
    fw/services/filesystem/app_file.c
    fw/services/filesystem/flash_translation.c
    fw/flash_region/flash_region.c
    fw/flash_region/filesystem_regions.c
    fw/kernel/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    tests/fakes/fake_spi_flash.c
    fw/apps/system/kieselsport/telefon.c
    fw/apps/system/kieselsport/meldungen.c
    fw/apps/system/kieselsport/einstellungen.c
    fw/applib/dict.c
  DEFINES CONFIG_HRM CONFIG_MAG CONFIG_BOULDER_KIESELSPORT _GNU_SOURCE=1
  OVERRIDES dummy_board
)
