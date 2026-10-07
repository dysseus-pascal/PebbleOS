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

# T6.1 (Kriterium 6): Startmenue und Quick-Launch-Auswahl mit der Registry des Boards. generate.py
# baut sie aus der echten JSON, wie fw/shell/CMakeLists.txt es fuer die Firmware tut: mit
# CONFIG_BOULDER_KIESELSPORT wie obelix und qemu_emery, ohne wie asterix. Dazu Pebbles echte
# Workout-App (P2) und der echte App-Speicher.
set(kieselsport_startmenue_quellen
  fw/apps/system/kieselsport/kieselsport.c
  fw/apps/system/workout/workout.c
  fw/applib/graphics/gcolor_definitions.c
  fw/applib/graphics/gtypes.c
  fw/drivers/flash/flash_crc.c
  fw/flash_region/filesystem_regions.c
  fw/flash_region/flash_region.c
  fw/process_management/app_install_manager.c
  fw/process_management/app_menu_data_source.c
  fw/process_management/pebble_process_info.c
  fw/process_management/pebble_process_md.c
  fw/resource/resource.c
  fw/resource/resource_storage.c
  fw/resource/resource_storage_builtin.c
  fw/resource/resource_storage_file.c
  fw/resource/resource_storage_flash.c
  fw/services/app_cache/service.c
  fw/services/blob_db/app_db.c
  fw/services/filesystem/app_file.c
  fw/services/filesystem/flash_translation.c
  fw/services/filesystem/pfs.c
  fw/services/process_management/app_order_storage.c
  fw/services/process_management/app_storage.c
  fw/services/settings/settings_file.c
  fw/services/settings/settings_raw_iter.c
  fw/kernel/rand.c
  third_party/tinymt/TinyMT/tinymt/tinymt32.c
  tests/fakes/fake_rtc.c
  tests/fakes/fake_spi_flash.c
  tests/fixtures/resources/builtin_resources.auto.c
  tests/fw/apps/system_apps/kieselsport/startmenue_nie.c
)
foreach(variante mit ohne)
  set(ziel ${CMAKE_CURRENT_BINARY_DIR}/startmenue_${variante})
  if(variante STREQUAL "mit")
    file(WRITE ${ziel}/konfig "CONFIG_BOULDER_KIESELSPORT=y\n")
    set(board obelix)
  else()
    file(WRITE ${ziel}/konfig "")
    set(board asterix)
  endif()
  add_custom_command(
    OUTPUT ${ziel}/shell/system_app_registry_list.auto.h ${ziel}/shell/system_app_ids.auto.h
    COMMAND ${PYTHON_EXECUTABLE} ${PBL_BASE}/tools/cmake/generate.py app-registry
            --input ${PBL_BASE}/fw/shell/normal/system_app_registry_list.json
            --registry ${ziel}/shell/system_app_registry_list.auto.h
            --enum ${ziel}/shell/system_app_ids.auto.h
            --config ${ziel}/konfig --board ${board}
    DEPENDS ${PBL_BASE}/fw/shell/normal/system_app_registry_list.json
            ${PBL_BASE}/tools/cmake/generate.py ${ziel}/konfig
    WORKING_DIRECTORY ${PBL_BASE}
    VERBATIM)
  add_custom_target(kieselsport_registry_${variante}
    DEPENDS ${ziel}/shell/system_app_registry_list.auto.h)
endforeach()
# Die Registry kommt ueber registry/shell/ (OVERRIDES stehen vor tests/overrides/default, INCLUDES
# dahinter); der Pfad dorthin fuehrt aus tests/overrides/ hinaus in diesen Ordner.
foreach(variante mit ohne)
  set(ziel ${CMAKE_CURRENT_BINARY_DIR}/startmenue_${variante})
  set(registry_${variante}
    "KS_REGISTRY_LISTE=\"${ziel}/shell/system_app_registry_list.auto.h\""
    "KS_REGISTRY_IDS=\"${ziel}/shell/system_app_ids.auto.h\"")
endforeach()
pbl_clar_test(test_kieselsport_startmenue
  SOURCES ${kieselsport_startmenue_quellen}
  DEFINES CONFIG_BOULDER_KIESELSPORT _GNU_SOURCE=1 ${registry_mit}
  OVERRIDES ../fw/apps/system_apps/kieselsport/registry
  DEPENDS kieselsport_registry_mit
)
pbl_clar_test(test_kieselsport_startmenue_ohne_variante
  SOURCE test_kieselsport_startmenue.c
  SOURCES ${kieselsport_startmenue_quellen}
  DEFINES _GNU_SOURCE=1 ${registry_ohne}
  OVERRIDES ../fw/apps/system_apps/kieselsport/registry
  DEPENDS kieselsport_registry_ohne
)
