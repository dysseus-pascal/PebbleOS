#!/bin/sh
# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0
#
# Boulder: die freie Ersatzschrift fuer Avenir Next in TimeStyle Bedrock neu
# bauen - Kumbh Sans (SIL OFL 1.1) als "Regular" (Staerke 350) und "Bold"
# (Staerke 600), nur Leerzeichen, Ziffern und Doppelpunkt.
#
# WARUM DIESE SCHRIFT UND STAERKEN: unter 19 freien Schriften aus
# github.com/google/fonts deckt Kumbh Sans die Avenir-Next-Ziffern des
# Originals bei "Regular" am besten (mittlere Flaechenueberdeckung 0.760 bei
# 350, die 6 und 9 mit geradem Strich wie Avenir); 350 und 600 sind die
# Staerken mit der besten Deckung zu Avenir Next Regular bzw. Demi Bold. Fuer
# Bold allein laegen Hanken Grotesk und Open Sans vorn - eine Familie, weil
# "Bold Hour"/"Bold Minute" beide Staerken mischen. Vergleich und Zahlen:
# tools/boulder/timestyle_schriftvergleich.py und BOULDER.md (Schriften).
#
# Aufruf: tools/boulder/timestyle_schriften.sh <KumbhSans[YOPQ,wght].ttf>
# (aus github.com/google/fonts, ofl/kumbhsans). Braucht fontTools und node;
# pebble-fctx-compiler 1.2.2 ist das Werkzeug, mit dem auch TimeStyle seine
# .ffont gebaut hat (package.json des Originals).
set -eu
QUELLE="$1"
cd "$(dirname "$0")/../.."
ZIEL=resources/normal/base/timestyle
ARBEIT="$(mktemp -d)"
trap 'rm -rf "$ARBEIT"' EXIT
python3 tools/boulder/ttf_zu_svgfont.py "$QUELLE" 350 KumbhSans-Regular "$ARBEIT/regular.svg"
python3 tools/boulder/ttf_zu_svgfont.py "$QUELLE" 600 KumbhSans-Bold "$ARBEIT/bold.svg"
(cd "$ARBEIT" && npx --yes pebble-fctx-compiler@1.2.2 regular.svg -r "[0-9: ]" \
               && npx --yes pebble-fctx-compiler@1.2.2 bold.svg -r "[0-9: ]")
cp "$ARBEIT/KumbhSans-Regular.ffont" "$ARBEIT/KumbhSans-Bold.ffont" "$ZIEL/"
ls -l "$ZIEL"/KumbhSans-*.ffont
