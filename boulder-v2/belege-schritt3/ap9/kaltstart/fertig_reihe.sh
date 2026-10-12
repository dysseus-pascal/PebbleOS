#!/bin/bash
# Kaltstarts mit dem heutigen Stand (HEAD): je Boot, ob der Zeitraffer vor dem ersten Start fertig war.
# MODI: Liste aus "sofort" und "warten".
S=/tmp/claude-0/-home-user/7e362f6d-34dc-553d-8345-69bd6588c9e5/scratchpad/ap9
R=/home/user/pebbleos-v2
cd $R || exit 1
. .venv/bin/activate
. ~/pebbleos-sdk-0.1.10/env.sh
git status --short fw/apps/system/boulder_health/
for modus in $MODI; do
  pgrep -x qemu-pebble | xargs -r kill
  sleep 5
  arg=""; [ "$modus" = warten ] && arg=warten
  erg=$(timeout 900 python3 $S/kaltstart_fertig.py build-pruef $arg 2>&1 | grep -E "^ERGEBNIS|Error|Traceback" | tr '\n' ' ')
  echo "$modus $(date +%H:%M:%S): $erg"
done
echo fertig
