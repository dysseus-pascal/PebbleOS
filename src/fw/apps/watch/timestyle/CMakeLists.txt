# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0

# Boulder: TimeStyle als System-Zifferblatt. Der uebernommene Code (TimeStyle,
# pebble-fctx, pebble-utf8; je MIT, Lizenz neben dem Code) bindet <pebble.h>
# ein - das ist hier pebble.h in diesem Ordner, darum der eigene Suchpfad.
if(CONFIG_APPS_WATCH_TIMESTYLE)
  pbl_library()
  pbl_library_sources(
    main.c
    clock_area.c
    languages.c
    settings.c
    sidebar.c
    sidebar_widgets.c
    util.c
    weather.c
    schritte_text.c
    distanz_text.c
    ts_format.c
    schriftwahl.c
    zweitzeit.c
    wetter_quelle.c
    wetter_symbol.c
    pebble-fctx/fctx.c
    pebble-fctx/ffont.c
    pebble-utf8/pebble-utf8.c
  )
  pbl_library_include_directories(.)

  # Die globalen Namen bekommen das Praefix ts_ (ts_namen.h, dort das Warum),
  # ohne den uebernommenen Code anzufassen: die Datei steht vor jeder Quelle.
  pbl_library_compile_options("SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/ts_namen.h")

  # Und der Bau scheitert, wenn ein globaler Name ohne Praefix dazukommt.
  set(_ts_geprueft ${CMAKE_CURRENT_BINARY_DIR}/ts_symbole.geprueft)
  add_custom_command(
    OUTPUT ${_ts_geprueft}
    COMMAND ${PYTHON_EXECUTABLE} ${PBL_BASE}/tools/boulder/pruefe_ts_symbole.py
            ${CMAKE_NM} $<TARGET_OBJECTS:${PBL_CURRENT_LIBRARY}>
    COMMAND ${CMAKE_COMMAND} -E touch ${_ts_geprueft}
    DEPENDS $<TARGET_OBJECTS:${PBL_CURRENT_LIBRARY}>
            ${PBL_BASE}/tools/boulder/pruefe_ts_symbole.py
    COMMAND_EXPAND_LISTS
    VERBATIM
  )
  add_custom_target(${PBL_CURRENT_LIBRARY}_symbole ALL DEPENDS ${_ts_geprueft})
endif()
