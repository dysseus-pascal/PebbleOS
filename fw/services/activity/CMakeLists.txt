# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

set(sources
  activity.c
  activity_calculators.c
  activity_insights.c
  activity_metrics.c
  activity_sessions.c
  health_util.c
  hr_util.c
  insights_settings.c
  workout_service.c
)
pbl_library()
pbl_library_sources(${sources})
# Boulder: Algorithmus-Wahl (Kconfig), die Variante bringt ihre Quellen mit
pbl_library_sources_ifdef(CONFIG_SERVICE_ACTIVITY_ALGORITHM_KRAEPELIN
  kraepelin/activity_algorithm_kraepelin.c kraepelin/kraepelin_algorithm.c)
if(CONFIG_SERVICE_ACTIVITY_ALGORITHM_BOULDER)
  include(boulder/quellen.cmake)
endif()
