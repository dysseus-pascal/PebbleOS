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
# JEDES PAKET IST SIGNIERT (Ed25519, siehe SIGNATUR.md): signiert wird nicht
# die Datei, sondern eine Aussage aus Tag, Dateiname, Groesse und sha256 -
# so gehoert die Signatur zu genau diesem Release, und ein alt signiertes
# Paket laesst sich nicht unter neuem Tag wiederverwenden. Boulder prueft
# die Signatur vor dem Einspielen und nimmt nichts Unsigniertes an.
#
# Aufruf: tools/boulder/release.sh <nummer>
# Voraussetzung: .github/release/v4.38.3-b<nummer>.md, erste Zeile der Titel
# ohne Version, darunter die Notizen (Deutsch). Der private Schluessel liegt
# ausserhalb des Repos (BOULDER_SIGNATUR, sonst ~/.config/boulder-firmware/signatur.pem).
set -eu
cd "$(dirname "$0")/../.."
NUMMER="${1:?Nummer fehlt, z. B. tools/boulder/release.sh 1}"
BASIS=v4.38.3
TAG="$BASIS-b$NUMMER"
REPO=dysseus-pascal/boulder-firmware
NOTIZEN=".github/release/$TAG.md"
SCHLUESSEL="${BOULDER_SIGNATUR:-$HOME/.config/boulder-firmware/signatur.pem}"
OEFFENTLICH=tools/boulder/signatur.pub

# ALLE VORBEDINGUNGEN VOR DEM TAG: ein Abbruch danach liesse ein Tag ohne
# Release zurueck.
test -f "$NOTIZEN" || { echo "Es fehlen die Notizen $NOTIZEN" >&2; exit 1; }
test -r "$SCHLUESSEL" || { echo "Signaturschluessel fehlt: $SCHLUESSEL (siehe tools/boulder/SIGNATUR.md)" >&2; exit 1; }
test "$(openssl pkey -in "$SCHLUESSEL" -pubout)" = "$(cat "$OEFFENTLICH")" \
  || { echo "Der Schluessel $SCHLUESSEL passt nicht zu $OEFFENTLICH - die App kennt ihn nicht" >&2; exit 1; }
# Nur aus einem sauberen, veroeffentlichten Stand: sonst traegt die Uhr eine
# Version, zu der es keinen Code gibt.
test -z "$(git status --porcelain --ignore-submodules=dirty)" || { echo "Arbeitsbaum nicht sauber" >&2; exit 1; }
git fetch -q origin main
test "$(git rev-parse HEAD)" = "$(git rev-parse origin/main)" || { echo "HEAD ist nicht origin/main" >&2; exit 1; }

TITEL="$(head -n 1 "$NOTIZEN")"
git tag -a "$TAG" -m "Boulder-Firmware $TAG: $TITEL"
# Bis zum Push ist das Tag nur lokal - bei einem Fehler kommt es wieder weg.
trap 'git tag -d "$TAG" >/dev/null 2>&1 || true' EXIT
sh tools/boulder/bundle.sh
PAKET="build-boulder/normal_obelix_pvt_$TAG.pbz"
test -f "$PAKET" || { echo "Paket $PAKET fehlt" >&2; exit 1; }

# Die Aussage, die signiert wird - Boulder baut sie aus dem Release genauso
# zusammen (FirmwareSignatur.aussage): Zeilen, LF, ohne Leerzeichen.
AUSSAGE="build-boulder/aussage.txt"
printf 'boulder-firmware/v1\n%s\n%s\n%s\nsha256:%s\n' \
  "$TAG" "$(basename "$PAKET")" "$(wc -c < "$PAKET" | tr -d ' ')" "$(sha256sum "$PAKET" | cut -c1-64)" > "$AUSSAGE"
openssl pkeyutl -sign -inkey "$SCHLUESSEL" -rawin -in "$AUSSAGE" -out "$PAKET.sig"
test "$(wc -c < "$PAKET.sig" | tr -d ' ')" -eq 64 || { echo "Signatur hat nicht 64 Byte" >&2; exit 1; }
openssl pkeyutl -verify -pubin -inkey "$OEFFENTLICH" -rawin -in "$AUSSAGE" -sigfile "$PAKET.sig" >/dev/null \
  || { echo "Die Signatur prueft nicht gegen $OEFFENTLICH" >&2; exit 1; }
echo "Signiert mit Schluessel $(openssl pkey -pubin -in "$OEFFENTLICH" -outform DER | tail -c 32 | xxd -p -c 32 | cut -c1-8)…"

git push -q origin "$TAG"
trap - EXIT
tail -n +2 "$NOTIZEN" > build-boulder/notizen.md
# Erst als Entwurf mit beiden Dateien, dann freischalten: Boulder ueberspringt
# Entwuerfe, so erscheint das Release nie ohne seine Signatur.
# GitHub antwortet gelegentlich mit 500 - drei Anlaeufe, bevor es einen Fehler gibt.
for versuch in 1 2 3; do
  gh release create "$TAG" "$PAKET" "$PAKET.sig" -R "$REPO" --draft --title "$TAG - $TITEL" --notes-file build-boulder/notizen.md >/dev/null && break
  test "$versuch" -lt 3 || { echo "Release nicht angelegt - Tag ist gepusht; nachholen mit: gh release create $TAG $PAKET $PAKET.sig -R $REPO --draft ... (siehe SIGNATUR.md)" >&2; exit 1; }
  sleep 10
done
for name in "$(basename "$PAKET")" "$(basename "$PAKET").sig"; do
  gh release view "$TAG" -R "$REPO" --json assets --jq '.assets[].name' | grep -qx "$name" \
    || { echo "Im Release fehlt $name - Entwurf bleibt stehen, nachreichen mit: gh release upload $TAG <datei> -R $REPO" >&2; exit 1; }
done
gh release edit "$TAG" -R "$REPO" --draft=false >/dev/null
echo "https://github.com/$REPO/releases/tag/$TAG"
