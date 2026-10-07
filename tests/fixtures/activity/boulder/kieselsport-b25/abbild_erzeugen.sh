#!/bin/bash
# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0
#
# Erzeugt persist.txt neu: b25s Dienst und Test (main a7d8b16e1) mit abbild_szenario.c, gebaut
# im Testbau dieses Baums (build-test, schon konfiguriert). Aufruf: abbild_erzeugen.sh <repo>
# WARUM SO: b25s Quellen liegen unter src/ und lassen sich im V2-Baum nicht als Teil des Baus
# uebersetzen; fuer den einen Lauf kommen sie in einen Ordner im Bauordner und als
# voruebergehendes Ziel in den Testbau, danach ist die CMakeLists.txt wieder wie vorher.
set -eu
REPO=$(cd "$1" && pwd)
B25=a7d8b16e1
HIER=$REPO/tests/fixtures/activity/boulder/kieselsport-b25
G=$REPO/build-test/b25-abbild
C=$REPO/tests/fw/services/activity/boulder/sport/CMakeLists.txt
rm -rf "$G"
mkdir -p "$G/fw/services/kieselsport" "$G/include/pbl/services/kieselsport" "$G/app" "$G/test"
for f in $(git -C "$REPO" ls-tree --name-only $B25 src/fw/services/kieselsport/); do
  git -C "$REPO" show $B25:$f > "$G/fw/services/kieselsport/$(basename $f)"
done
git -C "$REPO" show $B25:include/pbl/services/kieselsport/kieselsport.h \
  > "$G/include/pbl/services/kieselsport/kieselsport.h"
for f in einstellungen.c einstellungen.h schluessel.h; do
  git -C "$REPO" show $B25:src/fw/apps/system/kieselsport/$f > "$G/app/$f"
done
# Die SDK-Namen der App (pebble_compat.h) aus V2: b25s Fassung zieht Koepfe aus src/ nach.
cp "$REPO/fw/apps/system/kieselsport/pebble_compat.h" "$G/app/"
{ git -C "$REPO" show $B25:tests/fw/services/kieselsport/test_kieselsport.c
  echo '#include <inttypes.h>'
  cat "$HIER/abbild_szenario.c"; } > "$G/test/test_kieselsport.c"
cp "$C" "$G/CMakeLists.sicher"
trap 'cp "$G/CMakeLists.sicher" "$C"; cmake "$REPO/build-test" > /dev/null' EXIT
cat >> "$C" <<EOT
pbl_clar_test(test_kieselsport_b25_abbild
  SOURCE $G/test/test_kieselsport.c
  SOURCES
    $G/fw/services/kieselsport/abschnitt.c $G/fw/services/kieselsport/art.c
    $G/fw/services/kieselsport/bahnen.c $G/fw/services/kieselsport/dienst.c
    $G/fw/services/kieselsport/kurve.c $G/fw/services/kieselsport/pausen.c
    $G/fw/services/kieselsport/puls.c $G/fw/services/kieselsport/reps.c
    $G/fw/services/kieselsport/training.c $G/fw/services/kieselsport/wartend.c
    $G/app/einstellungen.c fw/applib/dict.c
  INCLUDES $G/fw $G/include $G/app
  DEFINES CONFIG_BOULDER_KIESELSPORT _GNU_SOURCE=1)
EOT
cmake "$REPO/build-test" > "$G/konfig.log"
ninja -C "$REPO/build-test" test_kieselsport_b25_abbild > "$G/bau.log"
LAUF=$(dirname "$(find "$REPO/build-test" -type f -name runme -path '*/test_kieselsport_b25_abbild/runme')")
(cd "$LAUF" && KS_B25_ABBILD="$G/persist.roh" ./runme -tb25_abbild > "$G/lauf.log" 2>&1)
{ echo "# Kieselsports Persist nach b25 (main $B25): Schluessel Wert-Hex, je Zeile ein Eintrag."
  echo "# Erzeugt mit abbild_erzeugen.sh aus abbild_szenario.c (dieser Ordner)."
  cat "$G/persist.roh"; } > "$HIER/persist.txt"
echo "persist.txt: $(grep -vc '^#' "$HIER/persist.txt") Schluessel"
