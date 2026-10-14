#!/bin/bash
# Kieselstrasse-Demo (-DKST_DEMO) fuer qemu_emery bauen: b26 als Bezug (SDK 0.1.10 wie sein
# build-qemu) und V2 (SDK aus SDK_VERSION). Eigene Bauordner build-kst, die Arbeitsordner bleiben.
set -u
S=/tmp/claude-0/-home-user/7e362f6d-34dc-553d-8345-69bd6588c9e5/scratchpad/kst
bau() {
  local R=$1 SDK=$2 NAME=$3
  (
    cd "$R" || exit 1
    . .venv/bin/activate
    . "$HOME/pebbleos-sdk-$SDK/env.sh"
    rm -rf "${R:?}/build-kst"
    pbl configure -b build-kst --board qemu_emery -DCMAKE_C_FLAGS=-DKST_DEMO > "$S/demo-$NAME-configure.log" 2>&1 || { echo "$NAME configure rot"; exit 1; }
    grep -qx "CONFIG_BOULDER_KIESELSTRASSE=y" build-kst/.config && echo "$NAME: Schalter an" || echo "$NAME: ohne Schalter"
    cmake --build build-kst > "$S/demo-$NAME-bau.log" 2>&1 || { echo "$NAME Bau rot"; exit 1; }
    cmake --build build-kst --target qemu_image_spi > "$S/demo-$NAME-spi.log" 2>&1 || { echo "$NAME SPI rot"; exit 1; }
    echo "$NAME: $(grep -c 'warning:' "$S/demo-$NAME-bau.log") Warnungen, KST_DEMO im Bau: $(grep -c 'KST_DEMO' build-kst/build.ninja)"
  )
}
bau /home/user/pebbleos-b26 0.1.10 b26
bau /home/user/pebbleos-v2 "$(tr -d '[:space:]' < /home/user/pebbleos-v2/SDK_VERSION)" v2
