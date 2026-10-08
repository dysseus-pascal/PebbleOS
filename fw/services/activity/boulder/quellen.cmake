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
  # Pulskalorien und Verwerfen: Boulders Seite von P1 in workout_service.c.
  pbl_library_sources(boulder/kalorien.c)
  file(GLOB boulder_sport_quellen CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
       ${CMAKE_CURRENT_SOURCE_DIR}/boulder/sport/*.c)
  if(boulder_sport_quellen)
    pbl_library_sources(${boulder_sport_quellen})
  endif()
endif()
# Die Schnittstelle zur eigenen Health-App und die Ziele (Schritt 3, AP2), die Naechte (AP3): nur
# mit der App.
if(CONFIG_BOULDER_HEALTH_APP)
  pbl_library_sources(
    boulder/schnittstelle.c
    boulder/ziele.c
    boulder/naechte.c
  )
endif()
if(CONFIG_BOULDER_PRUEFLAUF)
  pbl_library_sources(
    boulder/prueflauf.c
    boulder/prueflauf_daten.c
  )
  # Kieselsport im Pruefbau (Schritt 2, AP7): Shell-Befehle und der Puls wie vom Sensor.
  if(CONFIG_BOULDER_KIESELSPORT)
    pbl_library_sources(boulder/prueflauf_sport.c)
  endif()
endif()
