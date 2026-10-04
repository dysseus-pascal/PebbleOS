# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

set(sources
  activity.c
  activity_calculators.c
  activity_insights.c
  activity_metrics.c
  activity_sessions.c
  boulder/hrv.c
  boulder/messstufe.c
  boulder/nacht.c
  boulder/schlafanalyse.c
  boulder/spo2.c
  boulder/vertraege.c
  health_util.c
  hr_util.c
  insights_settings.c
  kraepelin/activity_algorithm_kraepelin.c
  kraepelin/kraepelin_algorithm.c
  workout_service.c
)
pbl_library()
pbl_library_sources(${sources})

# Boulder: die mit Upstream geteilten Vertraege (Sitzungstypen, Datalogging-Tags, Tag 84)
# gegen die eingefrorene Liste, vor jedem Bau (boulder/vertraege.c bewacht den Rest).
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
