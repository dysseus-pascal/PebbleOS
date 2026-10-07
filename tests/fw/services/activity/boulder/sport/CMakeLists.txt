# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0

# Kieselsport (Schritt 2): Tests des Dienstes unter fw/services/activity/boulder/sport/.

# Der Vertrag mit Pebbles Workout-Dienst: die echte workout_service.c, gebaut wie auf obelix
# und qemu_emery (Kieselsport an, mit Puls).
pbl_clar_test(test_kieselsport_vertrag
  SOURCES
    fw/services/activity/activity_calculators.c
    fw/services/activity/hr_util.c
    fw/services/activity/workout_service.c
    tests/fakes/fake_events.c
    tests/fakes/fake_rtc.c
  DEFINES CONFIG_HRM CONFIG_BOULDER_KIESELSPORT
  OVERRIDES dummy_board
)
