#!/bin/bash
# Baut alle Firmware-Ordner sauber neu und sammelt Warnungen (AP9).
cd /home/user/pebbleos-v2 || exit 1
. .venv/bin/activate
. ~/pebbleos-sdk-0.1.10/env.sh
AUS=/tmp/claude-0/-home-user/7e362f6d-34dc-553d-8345-69bd6588c9e5/scratchpad/up441
for d in build-obelix build-obelixrel build build-asterix build-getafix build-flint build-gabbro build-pruef; do
  echo "== $d $(date +%T)"
  cmake --build /home/user/pebbleos-v2/$d --target clean > /dev/null 2>&1
  cmake --build /home/user/pebbleos-v2/$d > $AUS/bau-$d.log 2>&1
  echo "rc=$? warnungen=$(grep -c 'warning:' $AUS/bau-$d.log) fehler=$(grep -c 'error:' $AUS/bau-$d.log)"
done
echo "== fertig $(date +%T)"
