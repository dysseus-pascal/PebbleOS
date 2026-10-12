#!/bin/bash
# Kaltstarts unter -icount im Wechsel (VARIANTEN), am Ende wieder der heutige Stand.
S=/tmp/claude-0/-home-user/7e362f6d-34dc-553d-8345-69bd6588c9e5/scratchpad/ap9
R=/home/user/pebbleos-v2
cd $R || exit 1
. .venv/bin/activate
. ~/pebbleos-sdk-0.1.10/env.sh
for v in $VARIANTEN; do
  cp $S/$v/* $R/fw/apps/system/boulder_health/
  cmake --build build-pruef > /dev/null 2>&1 && cmake --build build-pruef --target qemu_image_spi > /dev/null 2>&1
  pgrep -x qemu-pebble | xargs -r kill
  sleep 3
  erg=$(timeout 600 python3 $S/icount_kaltstart.py build-pruef ${SHIFT:-3} 2>&1 | grep -E "^ERGEBNIS|Error|Traceback|QEMU" | tr '\n' ' ')
  echo "$v $(date +%H:%M:%S): $erg"
done
cp $S/app_jetzt/* $R/fw/apps/system/boulder_health/
cmake --build build-pruef > /dev/null 2>&1 && cmake --build build-pruef --target qemu_image_spi > /dev/null 2>&1
git status --short
echo fertig
