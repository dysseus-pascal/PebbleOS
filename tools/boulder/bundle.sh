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
# NUR DIE VORGABEN DES BOARDS: eine menuconfig.conf im Bauordner ginge als
# Fragment in Kconfig ein. Sie gehoert jemandem - nicht still loeschen.
test ! -e build/menuconfig.conf \
  || { echo "build/menuconfig.conf liegt im Bauordner - fuer den Release-Bau entfernen" >&2; exit 1; }
for SLOT in 0 1; do
  rm -f build/*.pbz
  # --fresh: OHNE DEN ALTEN CACHE. CMakeLists.txt reicht jede gecachte
  # CONFIG_*-Variable als Kconfig-Override weiter; ein Probebau in build/ mit
  # -DCONFIG_BOULDER_SCHLAF=n bliebe sonst still im Release.
  pbl configure --board "$BOARD" -DCONFIG_FIRMWARE_SLOT=$SLOT -DCONFIG_RELEASE=y --fresh >/dev/null
  # DIE UHR BEKOMMT ALLE VIER SCHALTER (BOULDER.md, Schalter). Fehlt einer im
  # Ergebnis von Kconfig, gingen etwa keine Naechte mehr hinaus - und die
  # Ausgabe oben sieht niemand.
  case "$BOARD" in
    obelix*)
      for SCHALTER in SCHLAF HRV MESSSTUFE HEALTH_APP; do
        grep -qx "CONFIG_BOULDER_$SCHALTER=y" build/.config \
          || { echo "Release-Bau ohne CONFIG_BOULDER_$SCHALTER=y (build/.config) - abgebrochen" >&2; exit 1; }
      done
      ;;
  esac
  pbl build >/dev/null
  pbl bundle >/dev/null
  cp "build/normal_${NAME}_${VERSION}_slot${SLOT}.pbz" "$AUS/"
done
python3 tools/merge_pbz.py \
  --slot0-pbz "$AUS/normal_${NAME}_${VERSION}_slot0.pbz" \
  --slot1-pbz "$AUS/normal_${NAME}_${VERSION}_slot1.pbz" \
  --output "$AUS/normal_${NAME}_${VERSION}.pbz"
echo "$AUS/normal_${NAME}_${VERSION}.pbz"
