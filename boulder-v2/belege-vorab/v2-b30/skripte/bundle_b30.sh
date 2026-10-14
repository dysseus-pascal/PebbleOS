#!/bin/bash
# V2 b30: die Firmware fuer die Pebble Time 2 wie ein Release bauen, nach b26s tools/boulder/bundle.sh.
# BEIDE SLOTS: die Uhr schreibt ein Update in den gerade unbenutzten Platz, jeder braucht sein
# eigenes, auf seine Adresse gelinktes Abbild. Eigene Bauordner, damit die Arbeitsordner bleiben.
set -eu
R=/home/user/pebbleos-v2
AUS=$R/build-boulder
cd "$R"
. .venv/bin/activate
. "$HOME/pebbleos-sdk-$(tr -d '[:space:]' < SDK_VERSION)/env.sh"
BOARD=obelix@pvt
NAME=obelix_pvt
VERSION=$(git describe --tags)
test "$VERSION" = "v4.41.1-b30" || { echo "Fassung $VERSION statt v4.41.1-b30" >&2; exit 1; }
test -z "$(git status --porcelain --ignore-submodules=dirty)" || { echo "Arbeitsbaum nicht sauber" >&2; exit 1; }
rm -rf "$AUS"
mkdir -p "$AUS"
for SLOT in 0 1; do
  B="build-boulder-slot$SLOT"
  rm -rf "${R:?}/$B"
  pbl configure -b "$B" --board "$BOARD" -DCONFIG_FIRMWARE_SLOT=$SLOT -DCONFIG_RELEASE=y > "$AUS/configure-$SLOT.log" 2>&1
  # Die Uhr bekommt Boulders Variante, Kieselsport, Kieselstrasse und die Health-App, aber nichts vom
  # Pruefbau.
  for S in SERVICE_ACTIVITY_ALGORITHM_BOULDER BOULDER_KIESELSPORT BOULDER_KIESELSTRASSE BOULDER_HEALTH_APP RELEASE; do
    grep -qx "CONFIG_$S=y" "$B/.config" || { echo "Slot $SLOT ohne CONFIG_$S=y" >&2; exit 1; }
  done
  for S in BOULDER_PRUEFLAUF BOULDER_HEALTH_ZEITEN; do
    if grep -qx "CONFIG_$S=y" "$B/.config"; then echo "Slot $SLOT mit CONFIG_$S=y" >&2; exit 1; fi
  done
  grep -qx "CONFIG_FIRMWARE_SLOT=$SLOT" "$B/.config" || { echo "Slot $SLOT falsch konfiguriert" >&2; exit 1; }
  pbl build -b "$B" > "$AUS/build-$SLOT.log" 2>&1
  pbl bundle -b "$B" > "$AUS/bundle-$SLOT.log" 2>&1
  cp "$B/normal_${NAME}_${VERSION}_slot${SLOT}.pbz" "$AUS/"
  echo "Slot $SLOT: $(grep -c 'warning:' "$AUS/build-$SLOT.log") Warnungen"
done
python3 tools/merge_pbz.py \
  --slot0-pbz "$AUS/normal_${NAME}_${VERSION}_slot0.pbz" \
  --slot1-pbz "$AUS/normal_${NAME}_${VERSION}_slot1.pbz" \
  --output "$AUS/normal_${NAME}_${VERSION}.pbz"
sha256sum "$AUS/normal_${NAME}_${VERSION}.pbz"
ls -la "$AUS"/*.pbz
