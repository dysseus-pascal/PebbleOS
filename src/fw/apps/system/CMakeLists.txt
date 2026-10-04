# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_SHELL_SDK)
  set(source)
  file(GLOB _glob0 CONFIGURE_DEPENDS timeline/*.c)
  list(APPEND source ${_glob0})
  file(GLOB_RECURSE _glob1 CONFIGURE_DEPENDS launcher/default/*.c)
  list(APPEND source ${_glob1})
  list(APPEND source app_fetch_ui.c watchfaces.c)
else()
  file(GLOB_RECURSE source CONFIGURE_DEPENDS *.c)
  # Boulder: GENAU EINE HEALTH-APP. Beide tragen dieselbe Kennung und exportieren
  # health_app_get_info (Registry, Zifferblatt-Taste, Insights bleiben, wie sie
  # sind); gebaut wird Boulders auf emery (BOULDER_HEALTH_APP), sonst Pebbles.
  if(CONFIG_BOULDER_HEALTH_APP)
    list(FILTER source EXCLUDE REGEX "/apps/system/health/")
  else()
    list(FILTER source EXCLUDE REGEX "/apps/system/boulder_health/")
  endif()
endif()
pbl_library()
pbl_library_sources(${source})
