#!/bin/bash
# b39: der Wegwerf-Klon fuer die Fassung (wie b38): --shared, ohne Checkout, HEAD (abgekoppelt) auf
# dem Endstand, der Index aus HEAD, dort das annotierte Tag v4.42.0-b39. Im Repo selbst kein Tag.
set -eu
R=/home/user/pebbleos-v2
K=/tmp/claude-0/-home-user/7e362f6d-34dc-553d-8345-69bd6588c9e5/scratchpad/b39/versionsrepo
KOPF=$(git -C "$R" rev-parse HEAD)
rm -rf "$K"
git clone -q --shared --no-checkout "$R" "$K"
git -C "$K" update-ref --no-deref HEAD "$KOPF"
git -C "$K" read-tree HEAD
git -C "$K" -c tag.gpgSign=false tag -a v4.42.0-b39 -m "Boulder V2 b39: Upstream v4.42.0, SpO2-Satz mit drei Byte, Health-Zeilen als Haken" "$KOPF"
echo "Klon: HEAD $(git -C "$K" rev-parse HEAD), $(git -C "$K" describe --tags)"
echo "Repo: $(git -C "$R" tag -l 'v4.42.0-b39' | wc -l) Tags v4.42.0-b39"
