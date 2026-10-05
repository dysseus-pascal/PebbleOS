#!/bin/sh
# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0
#
# Boulder: bundle.sh baut das Release nur mit allen vier Boulder-Schaltern -
# ohne alten CMake-Cache und ohne menuconfig.conf im Bauordner.
#
# Gespielt wird mit dem echten bundle.sh und dem echten merge_pbz.py in einer
# Kopie des Repos. Nachgebaut ist nur pbl, die Grenze zum Bau: es schreibt
# jeden Aufruf mit, legt bei configure die build/.config des Falls hin und bei
# bundle ein Paket mit dem Namen, den das echte pbl ihm gaebe.
#
# Aufruf: test_bundle_schalter.sh <repo>
set -u
REPO="${1:?Repo fehlt}"
T="$(mktemp -d)"
trap 'rm -rf "$T"' EXIT
mkdir -p "$T/repo/tools/boulder" "$T/repo/.venv/bin" "$T/home/pebbleos-sdk-0.1.8" "$T/bin"
cp "$REPO/tools/boulder/bundle.sh" "$T/repo/tools/boulder/"
cp "$REPO/tools/merge_pbz.py" "$T/repo/tools/"
: > "$T/repo/.venv/bin/activate"
: > "$T/home/pebbleos-sdk-0.1.8/env.sh"
git -C "$T/repo" init -q
git -C "$T/repo" -c user.name=Test -c user.email=test@example.invalid commit -q --allow-empty -m Test
git -C "$T/repo" tag v4.38.4-b999
PBL_PROTOKOLL="$T/pbl.txt"
export PBL_PROTOKOLL
cat > "$T/bin/pbl" <<'PBL'
#!/bin/sh
echo "$*" >> "$PBL_PROTOKOLL"
case "$1" in
  configure)
    mkdir -p build
    cp "$PBL_CONFIG" build/.config
    for a in "$@"; do
      case "$a" in
        -DCONFIG_FIRMWARE_SLOT=*) echo "${a#*=}" > build/slot ;;
      esac
    done
    shift
    test "$1" = "--board" && echo "$2" | tr '@' '_' > build/name
    ;;
  build) ;;
  bundle)
    python3 -c 'import sys, zipfile; zipfile.ZipFile(sys.argv[1], "w").writestr("firmware.bin", b"x")' \
      "build/normal_$(cat build/name)_$(git describe --tags)_slot$(cat build/slot).pbz"
    ;;
  *) echo "pbl (Test): unerwartet: $*" >&2; exit 2 ;;
esac
PBL
chmod +x "$T/bin/pbl"
PATH="$T/bin:$PATH"
ALLE='CONFIG_BOULDER_SCHLAF=y\nCONFIG_BOULDER_HRV=y\nCONFIG_BOULDER_MESSSTUFE=y\nCONFIG_BOULDER_HEALTH_APP=y\n'
FEHLER=0

# fall <name> <board> <erwartet: paket|abbruch> <.config (printf-Format)>
fall() {
  name="$1"; board="$2"; erwartet="$3"; shift 3
  # shellcheck disable=SC2059
  printf "$@" > "$T/config"
  : > "$PBL_PROTOKOLL"
  aus="$(cd "$T/repo" && HOME="$T/home" PBL_CONFIG="$T/config" sh tools/boulder/bundle.sh "$board" 2>&1)"
  rc=$?
  case "$erwartet" in
    paket) test $rc -eq 0 && test -f "$T/repo/$(echo "$aus" | tail -n 1)" ;;
    abbruch) test $rc -ne 0 && ! grep -q "^build" "$PBL_PROTOKOLL" ;;
  esac
  ok=$?
  # Jedes configure ohne den alten Cache.
  if grep "^configure" "$PBL_PROTOKOLL" | grep -qv -- "--fresh"; then
    ok=1
  fi
  if test $ok -eq 0; then
    echo "ok   $name"
  else
    echo "FAIL $name (rc=$rc): $aus"
    sed 's/^/     pbl /' "$PBL_PROTOKOLL"
    FEHLER=1
  fi
}

fall "obelix mit allen vier" obelix@pvt paket "$ALLE"
grep -c "^configure .*--fresh" "$PBL_PROTOKOLL" | grep -qx 2 && grep -c "^build" "$PBL_PROTOKOLL" | grep -qx 2 \
  || { echo "FAIL obelix: nicht beide Slots frisch konfiguriert und gebaut"; FEHLER=1; }
fall "obelix ohne HRV" obelix@pvt abbruch \
  'CONFIG_BOULDER_SCHLAF=y\n# CONFIG_BOULDER_HRV is not set\nCONFIG_BOULDER_MESSSTUFE=y\nCONFIG_BOULDER_HEALTH_APP=y\n'
fall "obelix ohne Health-App" obelix@pvt abbruch \
  'CONFIG_BOULDER_SCHLAF=y\nCONFIG_BOULDER_HRV=y\nCONFIG_BOULDER_MESSSTUFE=y\n'
fall "obelix ohne Schlaf (alle weg)" obelix@pvt abbruch '# CONFIG_BOULDER_SCHLAF is not set\n'
fall "obelix, Schalter nur als Text" obelix@pvt abbruch \
  'CONFIG_BOULDER_SCHLAF=y\nCONFIG_BOULDER_HRV=y\nCONFIG_BOULDER_MESSSTUFE=y\nCONFIG_BOULDER_HEALTH_APP=yes\n'
# Andere Uhren haben ohne Pulssensor nur BOULDER_SCHLAF (BOULDER.md); geprueft wird obelix.
fall "asterix nur mit Schlaf" asterix paket 'CONFIG_BOULDER_SCHLAF=y\n'

# Eine menuconfig.conf im Bauordner: Abbruch vor configure, die Datei bleibt.
mkdir -p "$T/repo/build"
echo "CONFIG_BOULDER_HRV=n" > "$T/repo/build/menuconfig.conf"
fall "menuconfig.conf im Bauordner" obelix@pvt abbruch "$ALLE"
if grep -q "^configure" "$PBL_PROTOKOLL" || ! test -f "$T/repo/build/menuconfig.conf"; then
  echo "FAIL menuconfig.conf: configure lief oder die Datei ist weg"
  FEHLER=1
fi
exit $FEHLER
