#!/bin/sh
# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0
#
# Boulder: release.sh verweigert ein Release ohne gueltige Zeile
# "Mindest-App: X.Y.Z" in den Notizen - und solange das App-Release dieser
# Fassung fehlt oder ein Entwurf ist.
#
# Gespielt wird mit dem echten release.sh in einer Kopie des Werkzeugordners:
# ohne Signaturschluessel endet es spaetestens an dessen Pruefung, also vor
# Tag, Bau und Push. Kommt es bis dorthin, war alles davor in Ordnung.
#
# GitHub ist nachgebaut: ein gh im PATH, das nur "release view" kennt, je
# Fall mit veroeffentlichtem, entworfenem oder fehlendem App-Release antwortet
# (wie das echte, auch mit --jq) und jeden Aufruf mitschreibt.
#
# Aufruf: test_release_mindest_app.sh <repo>
set -u
REPO="${1:?Repo fehlt}"
T="$(mktemp -d)"
trap 'rm -rf "$T"' EXIT
mkdir -p "$T/tools/boulder" "$T/.github/release" "$T/bin"
cp "$REPO/tools/boulder/release.sh" "$REPO/tools/boulder/mindest_app.py" \
   "$REPO/tools/boulder/mindest_app.txt" "$T/tools/boulder/"
NOTIZEN="$T/.github/release/v4.38.4-b999.md"
GH_PROTOKOLL="$T/gh.txt"
export GH_PROTOKOLL
cat > "$T/bin/gh" <<'GH'
#!/bin/sh
echo "$*" >> "$GH_PROTOKOLL"
test "$1 $2" = "release view" || { echo "gh (Test): unerwartet: $*" >&2; exit 2; }
case "$GH_FALL" in
  veroeffentlicht) entwurf=false ;;
  entwurf) entwurf=true ;;
  *) echo "release not found" >&2; exit 1 ;;
esac
case " $* " in
  *" --jq .isDraft "*) echo "$entwurf" ;;
  *) echo "{\"isDraft\":$entwurf}" ;;
esac
GH
chmod +x "$T/bin/gh"
PATH="$T/bin:$PATH"
FEHLER=0

# fall <name> <erwartet: notizen|schluessel|fehlt|entwurf> <inhalt (printf-Format)>
# GH_FALL (veroeffentlicht, entwurf, fehlt) sagt, was GitHub zum App-Release antwortet.
GH_FALL=veroeffentlicht
fall() {
  name="$1"; erwartet="$2"; shift 2
  # shellcheck disable=SC2059
  printf "$@" > "$NOTIZEN"
  : > "$GH_PROTOKOLL"
  aus="$(GH_FALL="$GH_FALL" BOULDER_SIGNATUR="$T/kein-schluessel.pem" sh "$T/tools/boulder/release.sh" 999 2>&1)"
  rc=$?
  case "$erwartet" in
    notizen) echo "$aus" | grep -q "Mindest-App" && test $rc -ne 0 ;;
    schluessel) echo "$aus" | grep -q "Signaturschluessel fehlt" && test $rc -ne 0 ;;
    fehlt) echo "$aus" | grep -q "App-Release v.* nicht gefunden" && test $rc -ne 0 \
             && ! echo "$aus" | grep -q "Signaturschluessel" ;;
    entwurf) echo "$aus" | grep -q "App-Release v.* ist ein Entwurf" && test $rc -ne 0 \
               && ! echo "$aus" | grep -q "Signaturschluessel" ;;
  esac
  if test $? -eq 0; then
    echo "ok   $name"
  else
    echo "FAIL $name (rc=$rc): $aus"
    FEHLER=1
  fi
}

# gefragt <name> <erwarteter gh-Aufruf>: der letzte Fall fragte GitHub genau so.
gefragt() {
  if grep -qxF "$2" "$GH_PROTOKOLL"; then
    echo "ok   $1"
  else
    echo "FAIL $1: gh gefragt mit: $(cat "$GH_PROTOKOLL")"
    FEHLER=1
  fi
}

fall "ohne Zeile" notizen 'Titel\n- etwas\n'
fall "Zeile mit Fassung der Untergrenze" schluessel 'Titel\n- etwas\nMindest-App: %s\n' "$(cat "$T/tools/boulder/mindest_app.txt")"
gefragt "fragt nach dem App-Release der Untergrenze" \
  "release view v$(cat "$T/tools/boulder/mindest_app.txt") -R dysseus-pascal/Boulder --json isDraft --jq .isDraft"
fall "hoehere Fassung" schluessel 'Titel\nMindest-App: 1.0.0\n- etwas\n'
gefragt "fragt nach der Fassung aus den Notizen" \
  "release view v1.0.0 -R dysseus-pascal/Boulder --json isDraft --jq .isDraft"
fall "unter der Untergrenze" notizen 'Titel\nMindest-App: 0.49.9\n'
fall "nur im Titel" notizen 'Mindest-App: 0.50.0\n- etwas\n'
fall "zweimal" notizen 'Titel\nMindest-App: 0.50.0\nMindest-App: 0.51.0\n'
fall "Leerzeichen dahinter" notizen 'Titel\nMindest-App: 0.50.0 \n'
fall "zwei Stellen" notizen 'Titel\nMindest-App: 0.50\n'
fall "Windows-Zeilenenden" notizen 'Titel\r\nMindest-App: 0.50.0\r\n'
fall "als Aufzaehlung" notizen 'Titel\n- Mindest-App: 0.50.0\n'
# Die Notizen sind in Ordnung, aber die App ist noch nicht draussen: eine App
# bis 0.49 liest die Zeile nicht und boete die Firmware trotzdem an.
GH_FALL=fehlt
fall "App-Release fehlt" fehlt 'Titel\nMindest-App: 0.50.0\n'
GH_FALL=entwurf
fall "App-Release ist ein Entwurf" entwurf 'Titel\nMindest-App: 0.50.0\n'
exit $FEHLER
