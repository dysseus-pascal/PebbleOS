# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0

# TimeStyle (System-Zifferblatt): die reinen Teile - Wetterart der Firmware
# auf TimeStyles Symbole, Tag oder Nacht aus dem Sonnenstand, Alter der
# Wetterdaten, der UV-Wert der Stunde und die Texte der Leiste ("5.5k", "4km").
pbl_clar_test(test_timestyle
  SOURCES
    src/fw/apps/watch/timestyle/wetter_symbol.c
    src/fw/apps/watch/timestyle/schritte_text.c
    src/fw/apps/watch/timestyle/distanz_text.c
)

# Der Lesepfad des Wetters (wetter_quelle.c): Ort 0 der Reihenfolge, was ein
# Satz je nach Fassung und Laenge hergibt (Zeitzone ab v4.1, UV je Stunde ab
# v4.4), und welche Werte an wetter_uv_jetzt gehen. Datenbank und
# Standardvorhersage stellt der Test selbst.
pbl_clar_test(test_timestyle_wetter_quelle
  SOURCES
    src/fw/apps/watch/timestyle/wetter_quelle.c
    src/fw/apps/watch/timestyle/wetter_symbol.c
)

# Die Einstellungen aus Boulder: Leseregeln mit den gemeinsamen Testvektoren,
# die erlaubten Sprachen, die Schrift je Zeile (schriftwahl.c) und die Stunde
# des Widgets "Time zone". shim/ steht
# fuer das <pebble.h> des Zifferblatts (dort nur die Typen).
pbl_clar_test(test_timestyle_einstellungen
  SOURCES
    src/fw/apps/watch/timestyle/ts_format.c
    src/fw/apps/watch/timestyle/schriftwahl.c
    src/fw/apps/watch/timestyle/zweitzeit.c
  INCLUDES
    tests/fw/apps/watch/timestyle/shim
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

# Der Weg der Einstellungen aus Boulder in der Firmware: Whitelist der
# Uhreinstellungen-Synchronisation (settings_blob_db.c), Laengenpruefung,
# Wert in prefs.c, genau ein Ereignis fuer das laufende Zifferblatt.
pbl_clar_test(test_timestyle_bedrock_pref
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
    src/fw/services/blob_db/settings_blob_db.c
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
