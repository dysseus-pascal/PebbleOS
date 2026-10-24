#!/bin/bash
# b38: der Wegwerf-Klon fuer die Fassung (wie b37): --shared, ohne Checkout, HEAD (abgekoppelt) auf
# dem Endstand, der Index aus HEAD, dort das annotierte Tag v4.41.2-b38. Im Repo selbst kein Tag.
set -eu
R=/home/user/pebbleos-v2
K=/tmp/claude-0/-home-user/7e362f6d-34dc-553d-8345-69bd6588c9e5/scratchpad/b38/versionsrepo
KOPF=$(git -C "$R" rev-parse HEAD)
rm -rf "$K"
git clone -q --shared --no-checkout "$R" "$K"
git -C "$K" update-ref --no-deref HEAD "$KOPF"
git -C "$K" read-tree HEAD
git -C "$K" -c tag.gpgSign=false tag -a v4.41.2-b38 -m "Boulder V2 b38: Kieselsport mit Unterarten, GPS nach Wahl und Custom" "$KOPF"
echo "Klon: HEAD $(git -C "$K" rev-parse HEAD), $(git -C "$K" describe --tags)"
echo "Repo: $(git -C "$R" tag -l 'v4.41.2-b38' | wc -l) Tags v4.41.2-b38"
