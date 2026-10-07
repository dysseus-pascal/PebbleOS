# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0

# Kieselsport (Schritt 2): Tests der App unter fw/apps/system/kieselsport/. Aus b25 kommen Telefon
# und Meldungen (dort tests/fw/services/kieselsport/, sie pruefen aber App-Dateien) und
# Einstellungen und Laufschirm. Jede .c der App steht unter CONFIG_BOULDER_KIESELSPORT.

# Der Start der App: Kennung, sichtbar, Dienst angestossen, wohin sie beim Oeffnen fuehrt.
pbl_clar_test(test_kieselsport_app
  SOURCES
    fw/apps/system/kieselsport/kieselsport.c
  DEFINES CONFIG_BOULDER_KIESELSPORT
)

# Die Schlange der Zustandsmeldungen der App - ohne SDK, darum hier pruefbar.
pbl_clar_test(test_kieselsport_meldungen
  SOURCES
    fw/apps/system/kieselsport/meldungen.c
  DEFINES CONFIG_BOULDER_KIESELSPORT
)

# Der Draht der App zum Telefon: wer wann in den Postausgang darf, und die Nachrichten Byte fuer
# Byte wie b25 (Golden-Diktate). Mit den Koepfen der Firmware; Postausgang, Wecker und Dienst sind
# nachgebaut. dict.c liegt auf v2 unter fw/applib/.
pbl_clar_test(test_kieselsport_telefon
  SOURCES
    fw/apps/system/kieselsport/meldungen.c
    fw/apps/system/kieselsport/telefon.c
    fw/applib/dict.c
  # strnlen: in der Firmware-libc da, unter glibc mit -std=c11 nur so.
  DEFINES CONFIG_BOULDER_KIESELSPORT _GNU_SOURCE=1
)

# Die Einstellungen vom Telefon im Persist.
pbl_clar_test(test_kieselsport_einstellungen
  SOURCES
    fw/apps/system/kieselsport/einstellungen.c
    fw/applib/dict.c
  DEFINES CONFIG_BOULDER_KIESELSPORT _GNU_SOURCE=1
)

# Der laufende Schirm: gezeichnet wird nur, was sich aendert, und der Endschirm geht zu, bevor
# Pebbles Meldung kommt (V4). Schirm und Grafik sind nachgebaut, der Dienst liefert einen Stand,
# den der Test stellt.
pbl_clar_test(test_kieselsport_lauf
  SOURCES
    fw/apps/system/kieselsport/lauf_window.c
    fw/apps/system/kieselsport/art_text.c
    fw/apps/system/kieselsport/texte.c
  DEFINES CONFIG_BOULDER_KIESELSPORT _GNU_SOURCE=1
)

# Tote Texte und Funktionen der App (tools/boulder/pruefe_unbenutzt.py, aus b25).
add_test(NAME test_boulder_apps_unbenutzt
  COMMAND ${PYTHON_EXECUTABLE} ${PBL_BASE}/tools/boulder/pruefe_unbenutzt.py
          ${PBL_BASE}/fw/apps/system/kieselsport
          ${PBL_BASE}/tests/fw/apps/system_apps/kieselsport)
