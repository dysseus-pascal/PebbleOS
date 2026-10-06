# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0
# Boulder: Dateien bytegleich zu ihrer Quelle (SHA-256). -DDATEIEN=a|b -DSOLL=x|y
string(REPLACE "|" ";" dateien "${DATEIEN}")
string(REPLACE "|" ";" soll "${SOLL}")
foreach(datei wert IN ZIP_LISTS dateien soll)
  file(SHA256 "${datei}" ist)
  if(NOT ist STREQUAL wert)
    message(FATAL_ERROR "${datei}: nicht mehr bytegleich (${ist})")
  endif()
endforeach()
message(STATUS "bytegleich")
