# Tinte (erstes/letztes gesetztes Pixel in x) einer Zeile Text aus Vorschub und Glyphenkopf.
import sys
sys.path.insert(0, sys.argv[1])
from pbf import lade
def tinte(pfad, text, x0=0):
    maxh, g, wild = lade(pfad)
    x = x0; links = None; rechts = None
    for ch in text:
        w, h, left, top, adv = g.get(ord(ch)) or g[wild]
        if w and h:
            a = x + left; b = x + left + w - 1
            links = a if links is None else min(links, a)
            rechts = b if rechts is None else max(rechts, b)
        x += adv
    return links, rechts, x
if __name__ == '__main__':
    fonts = sys.argv[2]
    L38 = fonts + '/LECO_38_BOLD_NUMBERS.pbf'; L26 = fonts + '/LECO_26_BOLD_NUMBERS_AM_PM.pbf'; G18B = fonts + '/GOTHIC_18_BOLD.pbf'
    # Night: Zahl bei x=8 in LECO 38, Einheit "h" bei 8+breite+3 (zeichnen.c:53-56), Urteil rechtsbuendig bis 192
    from pbf import breite
    _, gl, wl = lade(L38)
    _, gg, wg = lade(G18B)
    _, g26, w26 = lade(L26)
    for urteil in ['Well rested', 'Short night', 'Very short night', 'so far']:
        ub = breite(gg, wg, urteil)
        ul, ur, _ = tinte(G18B, urteil, 192 - ub)
        worst = []
        for m in range(600, 780):
            z = '%d:%02d' % (m // 60, m % 60)
            zb = breite(gl, wl, z)
            hl, hr, _ = tinte(G18B, 'h', 8 + zb + 3)
            if hr >= ul:
                worst.append((z, hr - ul + 1))
        print('Night', urteil, 'Urteil-Tinte ab x=%d' % ul, 'Ueberlappung bei', worst[:3], '... Anzahl', len(worst))
    for urteil in ['Well rested', 'Short night', 'Very short night']:
        ub = breite(gg, wg, urteil)
        ul, ur, _ = tinte(G18B, urteil, 192 - ub)
        worst = []
        for m in range(60, 780):
            z = '%d:%02d' % (m // 60, m % 60)
            zb = breite(g26, w26, z)
            hl, hr, _ = tinte(G18B, 'h', 8 + zb + 3)
            if hr >= ul:
                worst.append((z, hr - ul + 1))
        print('Today', urteil, 'Urteil-Tinte ab x=%d' % ul, 'Ueberlappung bei', worst[:3], '... Anzahl', len(worst))
