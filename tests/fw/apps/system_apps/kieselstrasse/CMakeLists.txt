# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0

# Kieselstrasse (F6): Tests der App unter fw/apps/system/kieselstrasse/, aus b26. Jede .c der App
# steht unter CONFIG_BOULDER_KIESELSTRASSE; dict.c und utf8.c liegen auf v2 unter fw/applib/.

# Der Stand der Navigation (weg.c) mit nachgebautem Persist - neuer Schritt ja/nein, sparsames
# Schreiben, Lesen eines alten Persists.
pbl_clar_test(test_kieselstrasse_weg
  SOURCES
    fw/apps/system/kieselstrasse/weg.c
    fw/apps/system/kieselstrasse/texte.c
    fw/applib/graphics/utf8.c
    fw/applib/dict.c
  # strnlen: in der Firmware-libc da, unter glibc mit -std=c11 nur so.
  DEFINES CONFIG_BOULDER_KIESELSTRASSE _GNU_SOURCE=1
)

# Die App selbst (kieselstrasse.c): Eingang, verworfene Nachrichten und das Summen bei einem neuen
# Schritt. Schirm und Pfeil sind nachgebaut.
pbl_clar_test(test_kieselstrasse_app
  SOURCES
    fw/apps/system/kieselstrasse/kieselstrasse.c
    fw/apps/system/kieselstrasse/weg.c
    fw/apps/system/kieselstrasse/texte.c
    fw/applib/graphics/utf8.c
    fw/applib/dict.c
  DEFINES CONFIG_BOULDER_KIESELSTRASSE _GNU_SOURCE=1
)

# Tote Texte und Funktionen der App (tools/boulder/pruefe_unbenutzt.py, wie in b26).
add_test(NAME test_boulder_kieselstrasse_unbenutzt
  COMMAND ${PYTHON_EXECUTABLE} ${PBL_BASE}/tools/boulder/pruefe_unbenutzt.py
          ${PBL_BASE}/fw/apps/system/kieselstrasse)
