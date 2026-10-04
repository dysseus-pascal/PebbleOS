# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0

# Kieselsport, die App: die Einstellungen vom Telefon im Persist.
pbl_clar_test(test_kieselsport_einstellungen
  SOURCES
    src/fw/apps/system/kieselsport/einstellungen.c
    src/fw/util/dict.c
  DEFINES _GNU_SOURCE=1
)

# Der laufende Schirm: gezeichnet wird nur, was sich aendert. Schirm und
# Grafik sind nachgebaut, der Dienst liefert einen Stand, den der Test stellt.
pbl_clar_test(test_kieselsport_lauf
  SOURCES
    src/fw/apps/system/kieselsport/lauf_window.c
    src/fw/apps/system/kieselsport/art_text.c
    src/fw/apps/system/kieselsport/texte.c
  DEFINES _GNU_SOURCE=1
)

# Tote Texte und Funktionen in den Boulder-Apps (tools/boulder/pruefe_unbenutzt.py).
add_test(NAME test_boulder_apps_unbenutzt
  COMMAND ${PYTHON_EXECUTABLE} ${PBL_BASE}/tools/boulder/pruefe_unbenutzt.py
          ${PBL_BASE}/src/fw/apps/system/kieselsport
          ${PBL_BASE}/src/fw/apps/system/launcher/default
          ${PBL_BASE}/tests/fw/services/kieselsport)
add_test(NAME test_boulder_kieselstrasse_unbenutzt
  COMMAND ${PYTHON_EXECUTABLE} ${PBL_BASE}/tools/boulder/pruefe_unbenutzt.py
          ${PBL_BASE}/src/fw/apps/system/kieselstrasse)

# Die Kachel im Startmenue: sie fragt den Dienst nur, solange ein Training
# laeuft.
pbl_clar_test(test_kieselsport_kachel
  SOURCES
    src/fw/apps/system/launcher/default/app_glance_kieselsport.c
)
