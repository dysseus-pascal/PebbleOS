# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0

# Boulder-Kraepelin: die Quellen der Variante (aus fw/services/activity/CMakeLists.txt,
# nur mit CONFIG_SERVICE_ACTIVITY_ALGORITHM_BOULDER). Pfade relativ zu fw/services/activity.
pbl_library_sources(
  boulder/kraepelin/activity_algorithm_kraepelin.c
  boulder/kraepelin/kraepelin_algorithm.c
  boulder/vertraege.c
  boulder/schlafanalyse.c
  boulder/eingang.c
  boulder/nachlauf.c
  boulder/live.c
  boulder/zyklen.c
  boulder/nacht.c
  boulder/dauerteil.c
  boulder/szene.c
  boulder/kontext.c
  boulder/nachfrage.c
  boulder/takt.c
  boulder/messstufe.c
  boulder/puls.c
  boulder/hrv.c
  boulder/spo2.c
  boulder/wecker.c
  boulder/wecklauf.c
  boulder/ausgabe.c
  boulder/speicher.c
  boulder/umzug.c
  boulder/umzug_prefs.c
  boulder/diagnose.c
  boulder/shell.c
)
# Kieselsport (Schritt 2): der Dienst unter sport/ ohne feste Liste, damit die Pakete
# parallel Dateien anlegen, ohne sich hier zu treffen. Nur mit der Variante und Puls.
if(CONFIG_BOULDER_KIESELSPORT)
  file(GLOB boulder_sport_quellen CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
       ${CMAKE_CURRENT_SOURCE_DIR}/boulder/sport/*.c)
  if(boulder_sport_quellen)
    pbl_library_sources(${boulder_sport_quellen})
  endif()
endif()
if(CONFIG_BOULDER_PRUEFLAUF)
  pbl_library_sources(
    boulder/prueflauf.c
    boulder/prueflauf_daten.c
  )
endif()
