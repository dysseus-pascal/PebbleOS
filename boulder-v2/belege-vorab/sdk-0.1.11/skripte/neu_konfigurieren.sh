#!/bin/bash
# Alle acht Bauordner mit dem SDK aus SDK_VERSION neu anlegen (Bauordner sind Erzeugnisse).
R=/home/user/pebbleos-v2
cd $R || exit 1
. .venv/bin/activate
. ~/pebbleos-sdk-$(tr -d '[:space:]' < SDK_VERSION)/env.sh
while read -r d brett optionen; do
  [ -f "$R/$d/CMakeCache.txt" ] || { echo "$d: kein Bauordner"; exit 1; }
  rm -rf "${R:?}/$d"
  pbl configure -b "$d" --board "$brett" $optionen > /dev/null 2>&1
  echo "$d $brett $optionen: rc=$? $(grep -m1 '^CMAKE_C_COMPILER:' $R/$d/CMakeCache.txt | sed 's/.*sdk-/sdk-/')"
done <<'LISTE'
build-obelix obelix@pvt
build-obelixrel obelix@pvt -DCONFIG_RELEASE=y
build qemu_emery
build-asterix asterix
build-getafix getafix@dvt2
build-flint qemu_flint
build-gabbro qemu_gabbro
build-pruef qemu_emery -DCONFIG_BOULDER_PRUEFLAUF=y -DCONFIG_BOULDER_HEALTH_ZEITEN=y
LISTE
rm -rf "${R:?}/build-obelix-15"
