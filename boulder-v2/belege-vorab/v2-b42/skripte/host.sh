#!/bin/bash
# b42: voller Host-Lauf auf d7258032c (Upstream main nachgezogen), Bau in build-test von Grund auf.
R=/home/user/pebbleos-v2
AUS=/tmp/claude-0/-home-user/7e362f6d-34dc-553d-8345-69bd6588c9e5/scratchpad/b42/pruef
cd $R || exit 1
. .venv/bin/activate
. ~/pebbleos-sdk-$(tr -d '[:space:]' < $R/SDK_VERSION)/env.sh
echo "Stand $(git rev-parse --short HEAD) $(date +%H:%M:%S); Arbeitsbaum: $(git status --short | wc -l) Aenderungen"
sysctl -qw vm.max_map_count=1048576 2>/dev/null
pbl test -b build-test -j3 > $AUS/host.txt 2>&1
echo "host rc=$? $(grep -E 'tests passed|tests failed' $AUS/host.txt) Ende $(date +%H:%M:%S)"
