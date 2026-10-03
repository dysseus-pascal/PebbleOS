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
    wetter_quelle.c
    wetter_symbol.c
    pebble-fctx/fctx.c
    pebble-fctx/ffont.c
    pebble-utf8/pebble-utf8.c
  )
  pbl_library_include_directories(.)
endif()
