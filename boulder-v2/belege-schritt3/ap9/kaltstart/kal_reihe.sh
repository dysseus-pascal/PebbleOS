#!/bin/bash
# Kalte Starts im Wechsel (VARIANTEN), RUNDEN Boots je Variante, mit Host-Kalibrierung.
S=/tmp/claude-0/-home-user/7e362f6d-34dc-553d-8345-69bd6588c9e5/scratchpad/ap9
R=/home/user/pebbleos-v2
cd $R || exit 1
. .venv/bin/activate
. ~/pebbleos-sdk-0.1.10/env.sh
for runde in $(seq 1 ${RUNDEN:-3}); do
  for v in $VARIANTEN; do
    cp $S/$v/* $R/fw/apps/system/boulder_health/
    cmake --build build-pruef > /dev/null 2>&1 && cmake --build build-pruef --target qemu_image_spi > /dev/null 2>&1
    pgrep -x qemu-pebble | xargs -r kill
    sleep 5
    erg=$(timeout 600 python3 $S/kaltstart_kal.py build-pruef 2>&1 | grep -E "^ERGEBNIS" | tr '\n' ' ')
    echo "$v runde $runde $(date +%H:%M:%S) load $(cut -d' ' -f1 /proc/loadavg): $erg"
  done
done
cp $S/app_jetzt/* $R/fw/apps/system/boulder_health/
cmake --build build-pruef > /dev/null 2>&1 && cmake --build build-pruef --target qemu_image_spi > /dev/null 2>&1
cd $R && git status --short fw/apps/system/boulder_health/
echo fertig
