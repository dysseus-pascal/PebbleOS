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
    src/fw/services/kieselsport/pausen.c
    src/fw/services/kieselsport/puls.c
    src/fw/services/kieselsport/reps.c
    src/fw/services/kieselsport/training.c
    src/fw/services/kieselsport/wartend.c
)

# Die Schlange der Zustandsmeldungen der App - ohne SDK, darum hier pruefbar.
pbl_clar_test(test_kieselsport_meldungen
  SOURCES
    src/fw/apps/system/kieselsport/meldungen.c
)

# Der Draht der App zum Telefon: wer wann in den Postausgang darf. Mit den
# Koepfen der Firmware, Postausgang, Wecker und Dienst sind nachgebaut.
pbl_clar_test(test_kieselsport_telefon
  SOURCES
    src/fw/apps/system/kieselsport/meldungen.c
    src/fw/apps/system/kieselsport/telefon.c
    src/fw/util/dict.c
  # strnlen: in der Firmware-libc da, unter glibc mit -std=c11 nur so.
  DEFINES _GNU_SOURCE=1
)

# Der Persist von Kieselsport ueber der Datei der Uhr: Fehler beim Lesen,
# Schreiben und Loeschen muessen im Log stehen.
pbl_clar_test(test_kieselsport_speicher
  SOURCES
    src/fw/services/kieselsport/speicher.c
)
