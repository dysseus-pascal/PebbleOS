# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0

# Boulders Health-App gibt es nur mit diesen Schaltern (services/activity/boulder/Kconfig),
# also nur auf emery, mit Pulssensor.
set(BOULDER_HEALTH_AN CONFIG_HRM CONFIG_BOULDER_SCHLAF CONFIG_BOULDER_HRV CONFIG_BOULDER_MESSSTUFE
    CONFIG_BOULDER_HEALTH_APP)

# Health, die Daten: Schritte je Stunde auch an Tagen der Zeitumstellung.
pbl_clar_test(test_health_daten
  SOURCES
    src/fw/apps/system/health/daten.c
  DEFINES ${BOULDER_HEALTH_AN}
)

# Health, die App: Live-Puls, Neuzeichnen, Ende des Fensters.
pbl_clar_test(test_health_app
  SOURCES
    src/fw/apps/system/health/health.c
    src/fw/apps/system/health/daten.c
  DEFINES ${BOULDER_HEALTH_AN}
)

# Health, die Karten: das Hypnogramm liest nie hinter die Abschnitte.
pbl_clar_test(test_health_karten
  SOURCES
    src/fw/apps/system/health/karten.c
  DEFINES ${BOULDER_HEALTH_AN}
)
