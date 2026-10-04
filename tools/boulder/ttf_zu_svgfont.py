# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0
"""Eine TrueType-Schrift als SVG-Schrift fuer pebble-fctx-compiler.

Aufruf (braucht fontTools; Schritte siehe tools/boulder/timestyle_schriften.sh):
  tools/boulder/ttf_zu_svgfont.py <schrift.ttf> <wght> <id> <ziel.svg>

WARUM: TimeStyle zeichnet die Uhrzeit mit pebble-fctx aus .ffont-Dateien, und
die baut das Werkzeug des Originals (pebble-fctx-compiler 1.2.2) aus einer
SVG-Schrift - so entstanden auch die Avenir-Next-Dateien von TimeStyle. Dieses
Skript liefert nur die SVG-Schrift; die .ffont schreibt pebble-fctx-compiler.

Was es an der Schrift einstellt, und WARUM - die freie Schrift soll dort
stehen, wo im Original Avenir Next stand (clock_area.c rechnet mit der
Geviertgroesse und dem Anker "oben"):
- Ziffernhoehe: die Oberkante der "1" liegt wie bei Avenir Next bei 816/1152
  Geviert; dafuer wird units-per-em passend gesetzt (die Umrisse bleiben).
- Oberlaenge (ascent, der Anker "oben"): wie bei Avenir 10/1152 Geviert unter
  der Oberkante der Ziffern.
- Tabellarische Ziffern wie bei Avenir: jede Ziffer bekommt die Breite der "0"
  und steht mittig darin; das Leerzeichen ebenso breit, damit "%k"/"%l" eine
  einstellige Stunde wie im Original um eine Ziffernbreite verschiebt.
- Nur Leerzeichen, 0-9 und der Doppelpunkt (die Uhr zeichnet nichts anderes).
- Variable Schriften werden bei der Staerke [wght] als feste Schrift genommen.
"""
import sys

from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont

# Avenir Next in TimeStyles .ffont (1152 Einheiten je Geviert): Oberkante der
# "1" bei 816, Oberlaenge 806.
AVENIR_EM = 1152
AVENIR_ZIFFERN_OBEN = 816
AVENIR_OBERLAENGE = 806

ZEICHEN = " 0123456789:"


def baue(quelle, wght, kennung, ziel):
    f = TTFont(quelle)
    if "fvar" in f:
        f = instantiateVariableFont(f, {"wght": float(wght)})
    cmap = f.getBestCmap()
    gs = f.getGlyphSet()
    upem = f["head"].unitsPerEm

    # Oberkante der "1" in Schrifteinheiten.
    from fontTools.pens.boundsPen import BoundsPen
    bp = BoundsPen(gs)
    gs[cmap[ord("1")]].draw(bp)
    oben = bp.bounds[3]

    geviert = round(oben * AVENIR_EM / AVENIR_ZIFFERN_OBEN)
    oberlaenge = round(oben - (AVENIR_ZIFFERN_OBEN - AVENIR_OBERLAENGE) * geviert / AVENIR_EM)
    zelle = round(gs[cmap[ord("0")]].width)
    unterlaenge = f["hhea"].descent
    versal = getattr(f["OS/2"], "sCapHeight", oben) or oben

    zeilen = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<svg xmlns="http://www.w3.org/2000/svg"><defs>',
        f'<font id="{kennung}" horiz-adv-x="{zelle}">',
        f'<font-face units-per-em="{geviert}" ascent="{oberlaenge}" descent="{unterlaenge}" cap-height="{versal}"/>',
    ]
    for z in ZEICHEN:
        name = cmap[ord(z)]
        breite = round(gs[name].width)
        verschiebung = 0
        if z == " " or z.isdigit():
            verschiebung = (zelle - breite) / 2
            breite = zelle
        pen = SVGPathPen(gs)
        gs[name].draw(TransformPen(pen, (1, 0, 0, 1, verschiebung, 0)))
        d = pen.getCommands()
        unicode = "&#32;" if z == " " else z
        zeilen.append(f'<glyph unicode="{unicode}" glyph-name="{name}" horiz-adv-x="{breite}"'
                      + (f' d="{d}"' if d else "") + "/>")
    zeilen += ["</font>", "</defs></svg>", ""]
    open(ziel, "w").write("\n".join(zeilen))
    print(f"{ziel}: wght {wght}, Geviert {geviert} (Schrift {upem}), Oberlaenge {oberlaenge}, "
          f"Ziffern oben {oben}, Zelle {zelle}")


if __name__ == "__main__":
    if len(sys.argv) != 5:
        raise SystemExit(__doc__)
    baue(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4])
