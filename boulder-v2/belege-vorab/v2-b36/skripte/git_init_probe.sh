#!/bin/bash
# Waehrend des vollen Host-Laufs: alle 0,1 s git init in einem frischen Tempordner; jeder Fehler mit
# Zeit und Meldung.
while [ ! -e /tmp/claude-0/-home-user/7e362f6d-34dc-553d-8345-69bd6588c9e5/scratchpad/b36/.probe_ende ]; do
  d=$(mktemp -d)
  if ! out=$(git -C "$d" -c user.name=k1 -c user.email=k1@test -c commit.gpgsign=false -c core.hooksPath=/dev/null init -q 2>&1); then
    echo "$(date +%H:%M:%S.%N) FEHLER: $out"
  fi
  rm -rf "$d"
  sleep 0.1
done
