#!/bin/bash
# Schlussmessung AP9: zweimal qemu_health.py auf HEAD (Echtzeit-QEMU, wie spezifiziert).
S=/tmp/claude-0/-home-user/7e362f6d-34dc-553d-8345-69bd6588c9e5/scratchpad/up440
R=/home/user/pebbleos-v2
cd $R || exit 1
. .venv/bin/activate
. ~/pebbleos-sdk-0.1.10/env.sh
cmake --build build-pruef > /dev/null 2>&1 && cmake --build build-pruef --target qemu_image_spi > /dev/null 2>&1 || { echo "Bau rot"; exit 1; }
for lauf in ${LAEUFE:-a b}; do
  pgrep -x qemu-pebble | xargs -r kill
  sleep 3
  rm -rf $S/schluss_$lauf
  echo "== Lauf $lauf $(date +%H:%M:%S) $(git rev-parse --short HEAD)"
  python3 tools/boulder/qemu_health.py build-pruef --aus $S/schluss_$lauf > $S/schluss_$lauf.txt 2>&1
  echo "rc=$? $(date +%H:%M:%S)"
  grep -E "^(ok|ROT)" $S/schluss_$lauf.txt
done
echo fertig
