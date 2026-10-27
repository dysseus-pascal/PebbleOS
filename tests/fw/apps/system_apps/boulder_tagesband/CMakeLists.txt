# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0

# Das Zifferblatt Tagesband (fw/apps/system/boulder_tagesband/). Jede .c steht unter
# CONFIG_BOULDER_TAGESBAND und liest ueber die Datenschicht der Health-App.

# Die Rechnung: Band, Strecken, Puls, Stundenmarken ueber die Zeitumstellung, Datum und Zeit.
pbl_clar_test(test_tagesband_band
  SOURCES
    fw/apps/system/boulder_tagesband/band.c
    fw/apps/system/boulder_health/nachttage.c
    fw/apps/system/boulder_health/rechnung.c
    fw/applib/graphics/gcolor_definitions.c
  DEFINES CONFIG_BOULDER_HEALTH_APP CONFIG_BOULDER_TAGESBAND
)

# Start, Takt und Ende: Zifferblatt ohne Systemrechte, Laden vor dem ersten Bild, Nachladen nach dem
# Boot, Minutentakt, Wechsel 12/24 h, die Timeline-Vorschau. Fenster, Ebene, Timer und Daten sind
# nachgebaut, echt ist tagesband.c.
pbl_clar_test(test_tagesband_app
  SOURCES
    fw/apps/system/boulder_tagesband/tagesband.c
    fw/applib/graphics/gcolor_definitions.c
  DEFINES CONFIG_BOULDER_HEALTH_APP CONFIG_BOULDER_TAGESBAND
)

# Das Bild mit Pebbles echter Grafik und Schrift, gegen Bilder in tests/test_images/
# (test_tagesband_bild__*~emery.png): Tag, 12 h, Schlafmodus, verdeckt, leer.
pbl_clar_test(test_tagesband_bild
  SOURCES
    fw/applib/fonts/codepoint.c
    fw/applib/graphics/@BITDEPTH@_bit/framebuffer.c
    fw/applib/graphics/@BITDEPTH@_bit/bitblt_private.c
    fw/applib/graphics/bitblt.c
    fw/applib/graphics/framebuffer.c
    fw/applib/graphics/gbitmap.c
    fw/applib/graphics/gbitmap_png.c
    fw/applib/graphics/gcolor_definitions.c
    fw/applib/graphics/gpath.c
    fw/applib/graphics/graphics.c
    fw/applib/graphics/graphics_bitmap.c
    fw/applib/graphics/graphics_circle.c
    fw/applib/graphics/graphics_line.c
    fw/applib/graphics/graphics_private.c
    fw/applib/graphics/graphics_private_raw.c
    fw/applib/graphics/gtypes.c
    fw/applib/graphics/perimeter.c
    fw/applib/graphics/text_layout.c
    fw/applib/graphics/text_render.c
    fw/applib/graphics/text_resources.c
    fw/applib/graphics/utf8.c
    fw/applib/graphics/bidi.c
    fw/applib/graphics/arabic_shaping.c
    third_party/uzlib/uzlib/src/tinflate.c
    third_party/uzlib/uzlib/src/adler32.c
    third_party/uzlib/uzlib/src/crc32.c
    third_party/upng/upng/upng.c
    drivers/flash/flash_crc.c
    fw/flash_region/filesystem_regions.c
    fw/flash_region/flash_region.c
    fw/resource/resource.c
    fw/resource/resource_storage.c
    fw/resource/resource_storage_builtin.c
    fw/resource/resource_storage_file.c
    fw/resource/resource_storage_flash.c
    fw/services/filesystem/app_file.c
    fw/services/filesystem/flash_translation.c
    fw/services/filesystem/pfs.c
    fw/system/hexdump.c
    tests/fakes/fake_applib_resource.c
    tests/fakes/fake_display.c
    tests/fakes/fake_gbitmap_get_data_row.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_spi_flash.c
    tests/fakes/fake_fonts.c
    tests/fixtures/resources/builtin_resources.auto.c
    tests/fixtures/resources/pfs_resource_table.c
    fw/apps/system/boulder_health/nachttage.c
    fw/apps/system/boulder_health/rechnung.c
    fw/apps/system/boulder_health/texte.c
    fw/apps/system/boulder_health/zeichnen.c
    fw/apps/system/boulder_tagesband/band.c
    fw/apps/system/boulder_tagesband/bild.c
  PLATFORMS emery
  DEFINES CONFIG_BOULDER_HEALTH_APP CONFIG_BOULDER_TAGESBAND USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
  TEST_IMAGES
)

# Tote Funktionen des Zifferblatts (tools/boulder/pruefe_unbenutzt.py), ohne den Testordner.
add_test(NAME test_tagesband_unbenutzt
  COMMAND ${PYTHON_EXECUTABLE} ${PBL_BASE}/tools/boulder/pruefe_unbenutzt.py
          ${PBL_BASE}/fw/apps/system/boulder_tagesband)
