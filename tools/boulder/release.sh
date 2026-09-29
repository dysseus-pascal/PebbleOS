#!/bin/sh
# Boulder: eine Firmware veroeffentlichen, so dass Boulder sie anbietet.
#
# WARUM EIN EIGENES REPO: dieser Fork ist privat, und Boulder fragt GitHub ohne
# Anmeldung. Die fertigen Pakete liegen deshalb im oeffentlichen Repo
# dysseus-pascal/boulder-firmware; der Quellcode bleibt hier.
#
# DIE VERSION IST v4.38.3-bN: die PebbleOS-Fassung, auf der der Fork beruht,
# und die laufende Nummer. Kurz, weil die Uhr ihre Version in 32 Zeichen
# speichert - auch ein Entwicklungsbau wie v4.38.3-b12-3-gabcdef12-dirty muss
# hineinpassen. Boulder vergleicht die Nummer nach dem b.
#
# Aufruf: tools/boulder/release.sh <nummer>
# Voraussetzung: .github/release/v4.38.3-b<nummer>.md, erste Zeile der Titel
# ohne Version, darunter die Notizen (Deutsch).
set -eu
cd "$(dirname "$0")/../.."
NUMMER="${1:?Nummer fehlt, z. B. tools/boulder/release.sh 1}"
BASIS=v4.38.3
TAG="$BASIS-b$NUMMER"
REPO=dysseus-pascal/boulder-firmware
NOTIZEN=".github/release/$TAG.md"

test -f "$NOTIZEN" || { echo "Es fehlen die Notizen $NOTIZEN" >&2; exit 1; }
# Nur aus einem sauberen, veroeffentlichten Stand: sonst traegt die Uhr eine
# Version, zu der es keinen Code gibt.
test -z "$(git status --porcelain --ignore-submodules=dirty)" || { echo "Arbeitsbaum nicht sauber" >&2; exit 1; }
git fetch -q origin main
test "$(git rev-parse HEAD)" = "$(git rev-parse origin/main)" || { echo "HEAD ist nicht origin/main" >&2; exit 1; }

TITEL="$(head -n 1 "$NOTIZEN")"
git tag -a "$TAG" -m "Boulder-Firmware $TAG: $TITEL"
sh tools/boulder/bundle.sh
PAKET="build-boulder/normal_obelix_pvt_$TAG.pbz"
test -f "$PAKET" || { echo "Paket $PAKET fehlt" >&2; exit 1; }

git push -q origin "$TAG"
tail -n +2 "$NOTIZEN" > build-boulder/notizen.md
gh release create "$TAG" "$PAKET" -R "$REPO" --title "$TAG - $TITEL" --notes-file build-boulder/notizen.md
echo "https://github.com/$REPO/releases/tag/$TAG"
