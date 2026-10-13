#!/bin/bash
# AP9 Schlusslauf auf dem Endstand: alle Boulder-Quellen neu uebersetzen (touch) und die acht Bauten
# mit Warnungen zaehlen, dann der volle Host-Lauf und alle Werkzeuge.
R=/home/user/pebbleos-v2
AUS=/tmp/claude-0/-home-user/7e362f6d-34dc-553d-8345-69bd6588c9e5/scratchpad/sdk11/pruef
mkdir -p $AUS
cd $R || exit 1
. .venv/bin/activate
. ~/pebbleos-sdk-$(tr -d '[:space:]' < /home/user/pebbleos-v2/SDK_VERSION)/env.sh
echo "Stand $(git rev-parse --short HEAD) $(date +%H:%M:%S); Arbeitsbaum: $(git status --short | wc -l) Aenderungen"
for d in build-obelix build-obelixrel build build-asterix build-getafix build-flint build-gabbro build-pruef; do
  cmake --build $R/$d > $AUS/bau-$d.log 2>&1
  echo "$d rc=$? warnungen=$(grep -c 'warning:' $AUS/bau-$d.log) fehler=$(grep -c 'error:' $AUS/bau-$d.log) uebersetzt=$(grep -c 'Building C' $AUS/bau-$d.log)"
done
cmake --build $R/build-pruef --target qemu_image_spi > /dev/null 2>&1
sysctl -qw vm.max_map_count=1048576 2>/dev/null
pbl test -b build-test -j4 > $AUS/host.txt 2>&1
echo "host rc=$? $(grep -E 'tests passed|tests failed' $AUS/host.txt)"
T=$R/tools/boulder
lauf() { python3 "$@" > $AUS/werkzeug-$(basename $1).txt 2>&1; echo "rc=$? $(basename $1) ${*:2}"; }
lauf $T/k1_waechter.py $R
lauf $T/kopie_pruefen.py $R
lauf $T/vertraege.py $R
lauf $T/umzug_pruefen.py $R
python3 $T/umzug_pruefen.py $R --gegenprobe > $AUS/werkzeug-umzug_gegenprobe.txt 2>&1; echo "rc=$? umzug_pruefen.py --gegenprobe"
lauf $T/prueflauf_daten.py $R
python3 $T/pruefe_unbenutzt.py fw/apps/system/boulder_health > $AUS/werkzeug-unbenutzt-health.txt 2>&1; echo "rc=$? pruefe_unbenutzt.py fw/apps/system/boulder_health"
python3 $T/pruefe_unbenutzt.py fw/apps/system/kieselsport tests/fw/apps/system_apps/kieselsport > $AUS/werkzeug-unbenutzt-kieselsport.txt 2>&1; echo "rc=$? pruefe_unbenutzt.py fw/apps/system/kieselsport tests/fw/apps/system_apps/kieselsport"
lauf $T/test_kopie_pruefen.py
lauf $T/test_pruefe_ohne_rechte.py
lauf $T/test_pruefe_unbenutzt.py
for b in build-obelix build-obelixrel build build-pruef; do
  python3 $T/pruefe_ohne_rechte.py $b $R > $AUS/werkzeug-ohne_rechte-$b.txt 2>&1; echo "rc=$? pruefe_ohne_rechte.py $b"
done
/home/user/pebbleos-notizen/boulder-v2/belege-schritt3/ap9/k2/mehrwort.sh $R/build-obelixrel/pebbleos.elf > $AUS/mehrwort.txt 2>&1; echo "rc=$? mehrwort.sh build-obelixrel"
echo "Ende $(date +%H:%M:%S); Arbeitsbaum: $(git status --short | wc -l) Aenderungen"
