# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0

# Kieselstrasse: der Stand der Navigation (weg.c) mit nachgebautem Persist -
# neuer Schritt ja/nein, sparsames Schreiben, Lesen eines alten Persists.
pbl_clar_test(test_kieselstrasse_weg
  SOURCES
    src/fw/apps/system/kieselstrasse/weg.c
    src/fw/apps/system/kieselstrasse/texte.c
    src/fw/util/dict.c
)
