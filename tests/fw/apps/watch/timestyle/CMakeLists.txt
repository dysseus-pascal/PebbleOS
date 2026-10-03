# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0

# TimeStyle (System-Zifferblatt): die reinen Teile - Wetterart der Firmware
# auf TimeStyles Symbole, Tag oder Nacht aus dem Sonnenstand, Alter der
# Wetterdaten und die Schrittzahl der Leiste ("5.5k").
pbl_clar_test(test_timestyle
  SOURCES
    src/fw/apps/watch/timestyle/wetter_symbol.c
    src/fw/apps/watch/timestyle/schritte_text.c
)

# Die einmalige Umstellung des Standard-Zifferblatts auf TimeStyle (prefs.c).
# Eigener Test, weil prefs.c nur mit CONFIG_APPS_WATCH_TIMESTYLE den neuen
# Weg baut; test_prefs_db baut ohne.
pbl_clar_test(test_timestyle_standard
  SOURCES
    src/fw/util/rand/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    src/fw/util/time/time.c
    src/fw/flash_region/filesystem_regions.c
    src/fw/flash_region/flash_region.c
    src/fw/services/filesystem/flash_translation.c
    src/fw/services/filesystem/pfs.c
    src/fw/services/settings/settings_file.c
    src/fw/services/settings/settings_raw_iter.c
    src/fw/shell/normal/prefs.c
    tests/stubs/stubs_i18n.c
    src/fw/util/crc8.c
    src/fw/util/legacy_checksum.c
    tests/fakes/fake_accel_service.c
    tests/fakes/fake_spi_flash.c
    tests/fakes/fake_rtc.c
  DEFINES
    CONFIG_APPS_WATCH_TIMESTYLE=1
  OVERRIDES dummy_board
)
