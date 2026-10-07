# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0

# Kieselsport (Schritt 2): Tests der App unter fw/apps/system/kieselsport/.

pbl_clar_test(test_kieselsport_app
  SOURCES
    fw/apps/system/kieselsport/kieselsport.c
  DEFINES CONFIG_BOULDER_KIESELSPORT
)
