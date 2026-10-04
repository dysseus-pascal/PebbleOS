#!/bin/sh
# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0
#
# Boulder: release.sh verweigert ein Release ohne gueltige Zeile
# "Mindest-App: X.Y.Z" in den Notizen.
#
# Gespielt wird mit dem echten release.sh in einer Kopie des Werkzeugordners:
# ohne Signaturschluessel endet es spaetestens an dessen Pruefung, also vor
# Tag, Bau und Push. Kommt es bis dorthin, war die Zeile in Ordnung.
#
# Aufruf: test_release_mindest_app.sh <repo>
set -u
REPO="${1:?Repo fehlt}"
T="$(mktemp -d)"
trap 'rm -rf "$T"' EXIT
mkdir -p "$T/tools/boulder" "$T/.github/release"
cp "$REPO/tools/boulder/release.sh" "$REPO/tools/boulder/mindest_app.py" \
   "$REPO/tools/boulder/mindest_app.txt" "$T/tools/boulder/"
NOTIZEN="$T/.github/release/v4.38.4-b999.md"
FEHLER=0

# fall <name> <erwartet: notizen|schluessel> <inhalt (printf-Format)>
fall() {
  name="$1"; erwartet="$2"; shift 2
  # shellcheck disable=SC2059
  printf "$@" > "$NOTIZEN"
  aus="$(BOULDER_SIGNATUR="$T/kein-schluessel.pem" sh "$T/tools/boulder/release.sh" 999 2>&1)"
  rc=$?
  case "$erwartet" in
    notizen) echo "$aus" | grep -q "Mindest-App" && test $rc -ne 0 ;;
    schluessel) echo "$aus" | grep -q "Signaturschluessel fehlt" && test $rc -ne 0 ;;
  esac
  if test $? -eq 0; then
    echo "ok   $name"
  else
    echo "FAIL $name (rc=$rc): $aus"
    FEHLER=1
  fi
}

fall "ohne Zeile" notizen 'Titel\n- etwas\n'
fall "Zeile mit Fassung der Untergrenze" schluessel 'Titel\n- etwas\nMindest-App: %s\n' "$(cat "$T/tools/boulder/mindest_app.txt")"
fall "hoehere Fassung" schluessel 'Titel\nMindest-App: 1.0.0\n- etwas\n'
fall "unter der Untergrenze" notizen 'Titel\nMindest-App: 0.49.9\n'
fall "nur im Titel" notizen 'Mindest-App: 0.50.0\n- etwas\n'
fall "zweimal" notizen 'Titel\nMindest-App: 0.50.0\nMindest-App: 0.51.0\n'
fall "Leerzeichen dahinter" notizen 'Titel\nMindest-App: 0.50.0 \n'
fall "zwei Stellen" notizen 'Titel\nMindest-App: 0.50\n'
fall "Windows-Zeilenenden" notizen 'Titel\r\nMindest-App: 0.50.0\r\n'
fall "als Aufzaehlung" notizen 'Titel\n- Mindest-App: 0.50.0\n'
exit $FEHLER
