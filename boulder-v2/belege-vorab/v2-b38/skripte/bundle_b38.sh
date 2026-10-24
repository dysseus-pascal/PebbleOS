#!/bin/bash
# V2 b38: die Firmware fuer die Pebble Time 2 wie ein Release bauen, wie bundle_b37.sh.
# BEIDE SLOTS: die Uhr schreibt ein Update in den gerade unbenutzten Platz, jeder braucht sein
# eigenes, auf seine Adresse gelinktes Abbild. Eigene Bauordner, damit die Arbeitsordner bleiben.
#
# OHNE TAG IM REPO (Auftrag: nicht taggen). Die Fassung, die die Firmware traegt (git describe in
# tools/gitinfo.py), kommt aus einem Wegwerf-Klon im Notizordner: --shared, ohne Checkout, HEAD auf
# demselben Commit, dort das annotierte Tag v4.41.2-b38. GIT_DIR zeigt fuer den Bau auf ihn,
# GIT_WORK_TREE auf diesen Arbeitsbaum; die Pruefung auf "dirty" laeuft damit gegen HEAD wie sonst.
set -eu
R=/home/user/pebbleos-v2
S=/tmp/claude-0/-home-user/7e362f6d-34dc-553d-8345-69bd6588c9e5/scratchpad/b38
AUS=$R/build-boulder
cd "$R"
test -z "$(git status --porcelain --ignore-submodules=dirty)" || { echo "Arbeitsbaum nicht sauber" >&2; exit 1; }
KOPF=$(git rev-parse HEAD)
. .venv/bin/activate
. "$HOME/pebbleos-sdk-$(tr -d '[:space:]' < SDK_VERSION)/env.sh"
export GIT_DIR=$S/versionsrepo/.git GIT_WORK_TREE=$R
test "$(git rev-parse HEAD)" = "$KOPF" || { echo "Klon steht nicht auf $KOPF" >&2; exit 1; }
git update-index -q --refresh
BOARD=obelix@pvt
NAME=obelix_pvt
VERSION=$(git describe --tags)
test "$VERSION" = "v4.41.2-b38" || { echo "Fassung $VERSION statt v4.41.2-b38" >&2; exit 1; }
git diff-index --quiet --ignore-submodules=dirty HEAD -- || { echo "Klon sieht Aenderungen" >&2; exit 1; }
rm -rf "$AUS"
mkdir -p "$AUS"
for SLOT in 0 1; do
  B="build-boulder-slot$SLOT"
  rm -rf "${R:?}/$B"
  pbl configure -b "$B" --board "$BOARD" -DCONFIG_FIRMWARE_SLOT=$SLOT -DCONFIG_RELEASE=y > "$AUS/configure-$SLOT.log" 2>&1
  # Die Uhr bekommt Boulders Variante, Kieselsport, Kieselstrasse und die Health-App, aber nichts vom
  # Pruefbau.
  for X in SERVICE_ACTIVITY_ALGORITHM_BOULDER BOULDER_KIESELSPORT BOULDER_KIESELSTRASSE BOULDER_HEALTH_APP RELEASE; do
    grep -qx "CONFIG_$X=y" "$B/.config" || { echo "Slot $SLOT ohne CONFIG_$X=y" >&2; exit 1; }
  done
  for X in BOULDER_PRUEFLAUF BOULDER_HEALTH_ZEITEN; do
    if grep -qx "CONFIG_$X=y" "$B/.config"; then echo "Slot $SLOT mit CONFIG_$X=y" >&2; exit 1; fi
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
