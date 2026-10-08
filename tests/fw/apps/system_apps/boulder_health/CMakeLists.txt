# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0

# Boulders Health-App (Schritt 3): Tests der App unter fw/apps/system/boulder_health/. Jede .c der
# App steht unter CONFIG_BOULDER_HEALTH_APP; Pebbles Tests in ../health/ pruefen weiter Pebbles App.

# Der Start der App: Kennung, ohne Systemrechte, Zustand im Heap, Zeitstempel - einmal ohne und
# einmal mit dem Schalter des Pruefbaus.
pbl_clar_test(test_bh_app
  SOURCES
    fw/apps/system/boulder_health/boulder_health.c
  DEFINES CONFIG_BOULDER_HEALTH_APP
)
pbl_clar_test(test_bh_app_zeiten
  SOURCE test_bh_app.c
  SOURCES
    fw/apps/system/boulder_health/boulder_health.c
  DEFINES CONFIG_BOULDER_HEALTH_APP CONFIG_BOULDER_HEALTH_ZEITEN
)
