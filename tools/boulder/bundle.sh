#!/bin/sh
# Boulder: die Firmware fuer die Pebble Time 2 bauen, wie ein Release.
#
# WARUM BEIDE SLOTS: die Uhr haelt zwei Firmware-Plaetze und schreibt ein
# Update in den gerade unbenutzten. Jeder Platz braucht ein eigenes, auf
# seine Adresse gelinktes Abbild. Ein Paket mit nur slot0 ginge schief,
# sobald die Uhr gerade aus slot0 laeuft. Das Vorbild ist der Release-Lauf
# von Core Devices (.github/workflows/build-firmware.yml).
#
# Aufruf: tools/boulder/bundle.sh [board]   (Vorgabe obelix@pvt)
# Ergebnis: build-boulder/normal_<board>_<version>.pbz
set -eu
BOARD="${1:-obelix@pvt}"
NAME="$(echo "$BOARD" | tr '@' '_')"
cd "$(dirname "$0")/../.."
. "$HOME/pebbleos-sdk-0.1.8/env.sh"
. .venv/bin/activate
AUS=build-boulder
rm -rf "$AUS" && mkdir -p "$AUS"
# Die Fassung aus git describe, wie sie auch im Paketnamen steht. Nur genau
# diese Dateien kopieren: im Bauordner liegen sonst Pakete frueherer Staende.
VERSION=$(git describe --tags)
for SLOT in 0 1; do
  rm -f build/*.pbz
  pbl configure --board "$BOARD" -DCONFIG_FIRMWARE_SLOT=$SLOT -DCONFIG_RELEASE=y >/dev/null
  pbl build >/dev/null
  pbl bundle >/dev/null
  cp "build/normal_${NAME}_${VERSION}_slot${SLOT}.pbz" "$AUS/"
done
python3 tools/merge_pbz.py \
  --slot0-pbz "$AUS/normal_${NAME}_${VERSION}_slot0.pbz" \
  --slot1-pbz "$AUS/normal_${NAME}_${VERSION}_slot1.pbz" \
  --output "$AUS/normal_${NAME}_${VERSION}.pbz"
echo "$AUS/normal_${NAME}_${VERSION}.pbz"
