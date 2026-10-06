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
  boulder/ausgabe.c
  boulder/speicher.c
  boulder/umzug.c
)
