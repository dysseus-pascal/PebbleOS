#!/bin/bash
# Alle Bretter, obelix Release und der Pruefbau; je Brett ein eigener Bauordner.
cd /home/user/pebbleos-v2
. .venv/bin/activate; . ~/pebbleos-sdk-0.1.10/env.sh
L=/tmp/claude-0/-home-user/7e362f6d-34dc-553d-8345-69bd6588c9e5/scratchpad/bretter
mkdir -p $L
bau() {  # name board [optionen]
  local name=$1 board=$2; shift 2
  if pbl configure -b build-$name --board $board "$@" > $L/$name.log 2>&1 && pbl build -b build-$name >> $L/$name.log 2>&1; then
    w=$(grep -c 'warning:' $L/$name.log)
    echo "ok   $name (Warnungen: $w)"
  else
    echo "ROT  $name"; grep -E 'error' $L/$name.log | head -5
  fi
}
bau obelix obelix@pvt
bau emery qemu_emery
bau asterix asterix
bau getafix getafix@dvt2
bau flint qemu_flint
bau gabbro qemu_gabbro
bau obelixrel obelix@pvt -DCONFIG_RELEASE=y
bau pruef qemu_emery -DCONFIG_BOULDER_PRUEFLAUF=y -DCONFIG_BOULDER_HEALTH_ZEITEN=y
echo fertig
