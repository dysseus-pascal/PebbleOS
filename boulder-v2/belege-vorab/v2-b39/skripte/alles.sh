#!/bin/bash
# b39: Host-Lauf, acht Bauten mit Werkzeugen.
S=/tmp/claude-0/-home-user/7e362f6d-34dc-553d-8345-69bd6588c9e5/scratchpad
R=/home/user/pebbleos-v2
cd $R || exit 1
. .venv/bin/activate
. ~/pebbleos-sdk-0.1.11/env.sh
sysctl -qw vm.max_map_count=1048576
ninja -C build-test > $S/b39/pruef/host-bau.log 2>&1; echo "host-bau rc=$?"
(cd build-test && ctest -j8 --output-on-failure > $S/b39/pruef/host.txt 2>&1; echo "ctest-exit $?" >> $S/b39/pruef/host.txt)
grep -E "tests passed|ctest-exit" $S/b39/pruef/host.txt
bash $S/b39/skripte/pruefen.sh > $S/b39/pruef/pruefung.txt 2>&1
cat $S/b39/pruef/pruefung.txt
