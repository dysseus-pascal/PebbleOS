# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0

# Boulder: die eigenen Teile des Aktivitaetsdienstes, je nach Schalter (Kconfig
# hier). Die Waechter (vertraege.c) immer: sie bewachen auch, was keinen
# Schalter hat.
pbl_library()
pbl_library_sources(vertraege.c)
pbl_library_sources_ifdef(CONFIG_BOULDER_SCHLAF nacht.c schlafanalyse.c)
pbl_library_sources_ifdef(CONFIG_BOULDER_HRV hrv.c)
pbl_library_sources_ifdef(CONFIG_BOULDER_MESSSTUFE messstufe.c)
pbl_library_sources_ifdef(CONFIG_BOULDER_HEALTH_APP spo2.c)

# Die mit Upstream geteilten Vertraege (Sitzungstypen, Datalogging-Tags, Tag 84)
# gegen die eingefrorene Liste, vor jedem Bau (vertraege.c bewacht den Rest).
set(boulder_vertraege ${CMAKE_CURRENT_BINARY_DIR}/boulder_vertraege.stamp)
add_custom_command(
  OUTPUT ${boulder_vertraege}
  COMMAND ${PYTHON_EXECUTABLE} ${PBL_BASE}/tools/boulder/vertraege.py ${PBL_BASE}
          --stempel ${boulder_vertraege}
  DEPENDS ${PBL_BASE}/tools/boulder/vertraege.py
          ${PBL_BASE}/include/pbl/services/activity/activity.h
          ${PBL_BASE}/include/pbl/services/activity/activity_private.h
          ${PBL_BASE}/include/pbl/services/activity/boulder/nacht.h
          ${PBL_BASE}/include/pbl/services/data_logging/data_logging_service.h
  COMMENT "Boulder: geteilte Vertraege pruefen"
  VERBATIM
)
add_custom_target(pbl_boulder_vertraege DEPENDS ${boulder_vertraege})
add_dependencies(pbl_generated_headers pbl_boulder_vertraege)
