# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0

# Kieselsport: der Trainingsdienst mit nachgebauter Plattform.
pbl_clar_test(test_kieselsport
  SOURCES
    src/fw/services/kieselsport/abschnitt.c
    src/fw/services/kieselsport/art.c
    src/fw/services/kieselsport/bahnen.c
    src/fw/services/kieselsport/dienst.c
    src/fw/services/kieselsport/kurve.c
    src/fw/services/kieselsport/puls.c
    src/fw/services/kieselsport/reps.c
    src/fw/services/kieselsport/training.c
    src/fw/services/kieselsport/wartend.c
)
