#!/bin/bash
# b37: wie b36 auf dem Endstand. Die Quellen der SpO2-Schalter (Variante, Pebbles activity.c,
# hrm_manager.c, settings/health.c, settings_blob_db.c) werden angefasst, damit jeder der acht Bauten
# sie neu uebersetzt und ihre Warnungen zaehlt (die Firmware baut mit -Werror: jede Warnung bricht
# den Bau); Pebbles Dateien bauen auch die Boards ohne Variante. Dann der volle Host-Lauf und alle
# Werkzeuge.
R=/home/user/pebbleos-v2
AUS=/tmp/claude-0/-home-user/7e362f6d-34dc-553d-8345-69bd6588c9e5/scratchpad/b37/pruef
mkdir -p $AUS
cd $R || exit 1
. .venv/bin/activate
. ~/pebbleos-sdk-$(tr -d '[:space:]' < /home/user/pebbleos-v2/SDK_VERSION)/env.sh
echo "Stand $(git rev-parse --short HEAD) $(date +%H:%M:%S); Arbeitsbaum: $(git status --short | wc -l) Aenderungen; frei $(df -m / | awk 'NR==2{print $4}') MB"
touch fw/services/activity/boulder/messstufe.c fw/services/activity/boulder/puls.c \
      fw/services/activity/boulder/spo2_plan.c fw/services/activity/boulder/schnittstelle.c \
      fw/services/activity/activity.c fw/services/hrm/hrm_manager.c \
      fw/apps/system/settings/health.c fw/services/blob_db/settings_blob_db.c
for d in build-obelix build-obelixrel build build-asterix build-getafix build-flint build-gabbro build-pruef; do
  cmake --build $R/$d > $AUS/bau-$d.log 2>&1
  echo "$d rc=$? warnungen=$(grep -c 'warning:' $AUS/bau-$d.log) fehler=$(grep -c 'error:' $AUS/bau-$d.log) uebersetzt=$(grep -c 'Building C' $AUS/bau-$d.log)"
done
cmake --build $R/build-pruef --target qemu_image_spi > /dev/null 2>&1
echo "qemu_image_spi rc=$?"
sysctl -qw vm.max_map_count=1048576 2>/dev/null
pbl test -b build-test -j4 > $AUS/host.txt 2>&1
echo "host rc=$? $(grep -E 'tests passed|tests failed' $AUS/host.txt)"
echo "frei nach dem Host-Lauf: $(df -m / | awk 'NR==2{print $4}') MB"
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
python3 $T/pruefe_unbenutzt.py fw/apps/system/kieselstrasse > $AUS/werkzeug-unbenutzt-kieselstrasse.txt 2>&1; echo "rc=$? pruefe_unbenutzt.py fw/apps/system/kieselstrasse"
echo "Ende $(date +%H:%M:%S); Arbeitsbaum: $(git status --short | wc -l) Aenderungen; frei $(df -m / | awk 'NR==2{print $4}') MB"
