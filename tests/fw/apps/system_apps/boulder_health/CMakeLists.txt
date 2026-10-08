# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0

# Boulders Health-App (Schritt 3): Tests der App unter fw/apps/system/boulder_health/. Jede .c der
# App steht unter CONFIG_BOULDER_HEALTH_APP; Pebbles Tests in ../health/ pruefen weiter Pebbles App.

# Der Start der App: Kennung, ohne Systemrechte, Zustand im Heap, erste Karte je Einstieg, Oben
# und Unten, Zeitstempel - einmal ohne und einmal mit dem Schalter des Pruefbaus.
set(bh_app_quellen
  fw/apps/system/boulder_health/boulder_health.c
  fw/apps/system/boulder_health/einstieg.c
  fw/apps/system/boulder_health/karten.c
  fw/applib/graphics/gcolor_definitions.c
)
pbl_clar_test(test_bh_app
  SOURCES ${bh_app_quellen}
  DEFINES CONFIG_BOULDER_HEALTH_APP
)
pbl_clar_test(test_bh_app_zeiten
  SOURCE test_bh_app.c
  SOURCES ${bh_app_quellen}
  DEFINES CONFIG_BOULDER_HEALTH_APP CONFIG_BOULDER_HEALTH_ZEITEN
)

# T2.2: das Pruefskript der Objekte ohne Systemrechte (tools/boulder/pruefe_ohne_rechte.py) mit
# seinen Gegenproben. Die Objekte der echten App prueft es am Firmware-Bau, nicht hier.
add_test(NAME test_bh_ohne_rechte_gegenproben
  COMMAND ${PYTHON_EXECUTABLE} ${PBL_BASE}/tools/boulder/test_pruefe_ohne_rechte.py)

# T4.2: die Rechnungen der App (Urteile, HRV-Band, Pulszonen, Formate) gegen das Orakel aus
# Boulders Kotlin (orakel_rechnung.h, erzeugt in boulder-v2/belege-schritt3/ap5-orakel/).
pbl_clar_test(test_bh_rechnung
  SOURCES
    fw/apps/system/boulder_health/rechnung.c
  DEFINES CONFIG_BOULDER_HEALTH_APP
)

# T3.2, T3.4 (Spezifikation 2.4): Start und Blaettern der App - welche Karte jeder Einstieg
# oeffnet, Kreis, Spiegel und der Weg zum Zifferblatt.
pbl_clar_test(test_bh_einstieg
  SOURCES
    fw/apps/system/boulder_health/einstieg.c
  DEFINES CONFIG_BOULDER_HEALTH_APP
)

# T3.1 (Kriterium 3): Startmenue und Quick-Launch-Auswahl mit der Registry des Boards. generate.py
# baut sie aus der echten JSON, wie fw/shell/CMakeLists.txt es fuer die Firmware tut: mit den
# Schaltern von obelix (qemu_emery hat dieselben) oder ohne wie asterix. Dazu die App, die
# fw/apps/system/CMakeLists.txt (P7) fuer das Board baut: Boulders oder Pebbles.
set(bh_startmenue_quellen
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
  tests/fw/apps/system_apps/boulder_health/startmenue_nie.c
)
foreach(board obelix asterix)
  set(ziel ${CMAKE_CURRENT_BINARY_DIR}/bh_startmenue_${board})
  if(board STREQUAL "obelix")
    # Die Schalter von obelix, nach denen die JSON fragt (boards/obelix/defconfig, Kconfig der
    # Variante); die Demo-Apps sind auf keinem Board an.
    file(WRITE ${ziel}/konfig
      "CONFIG_HRM=y\nCONFIG_TOUCH=y\nCONFIG_BOULDER_KIESELSPORT=y\nCONFIG_BOULDER_HEALTH_APP=y\n")
  else()
    file(WRITE ${ziel}/konfig "")
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
  add_custom_target(bh_registry_${board}
    DEPENDS ${ziel}/shell/system_app_registry_list.auto.h)
  set(bh_registry_${board}
    "BH_REGISTRY_LISTE=\"${ziel}/shell/system_app_registry_list.auto.h\""
    "BH_REGISTRY_IDS=\"${ziel}/shell/system_app_ids.auto.h\"")
endforeach()
pbl_clar_test(test_bh_startmenue
  SOURCES ${bh_startmenue_quellen}
    fw/apps/system/boulder_health/boulder_health.c
    fw/apps/system/boulder_health/einstieg.c
    fw/apps/system/boulder_health/karten.c
  DEFINES CONFIG_BOULDER_HEALTH_APP _GNU_SOURCE=1 ${bh_registry_obelix}
  OVERRIDES ../fw/apps/system_apps/boulder_health/registry
  DEPENDS bh_registry_obelix
)
pbl_clar_test(test_bh_startmenue_asterix
  SOURCE test_bh_startmenue.c
  SOURCES ${bh_startmenue_quellen}
    fw/apps/system/health/health.c
  DEFINES _GNU_SOURCE=1 ${bh_registry_asterix}
  OVERRIDES ../fw/apps/system_apps/boulder_health/registry
  DEPENDS bh_registry_asterix
)

# T3.2, T3.3, T3.4 durch Pebbles echten Code: jede Quick-Launch-Geste am Zifferblatt
# (watchface.c, click.c), der Start vom Telefon (app_run_state.c), die Start-Syscalls der App
# (process_manager.c, app_launch_*.c) und der Uebergang der Shell (shell.c), bis zu Boulders
# einstieg.c.
pbl_clar_test(test_bh_einstiege
  SOURCES
    fw/shell/normal/watchface.c
    fw/applib/ui/click.c
    fw/shell/normal/shell.c
    fw/process_management/process_manager.c
    fw/process_management/pebble_process_info.c
    fw/process_management/app_run_state.c
    fw/applib/app_launch_reason.c
    fw/applib/app_launch_button.c
    fw/applib/graphics/gcolor_definitions.c
    fw/apps/system/boulder_health/einstieg.c
    fw/apps/system/boulder_health/karten.c
    tests/fw/apps/system_apps/boulder_health/einstiege_umfeld.c
  DEFINES CONFIG_BOULDER_HEALTH_APP ${bh_registry_obelix}
  OVERRIDES ../fw/apps/system_apps/boulder_health/registry
  DEPENDS bh_registry_obelix
)
