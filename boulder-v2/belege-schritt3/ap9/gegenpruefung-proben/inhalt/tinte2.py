import sys
sys.path.insert(0, sys.argv[1])
from pbf import lade, breite
from tinte import tinte
fonts = sys.argv[2]
L38 = fonts + '/LECO_38_BOLD_NUMBERS.pbf'; L26 = fonts + '/LECO_26_BOLD_NUMBERS_AM_PM.pbf'; G18B = fonts + '/GOTHIC_18_BOLD.pbf'
_, gl, wl = lade(L38); _, gg, wg = lade(G18B); _, g26, w26 = lade(L26)
def zahl_tinte(font, g, w, z):
    l, r, adv = tinte(font, z, 8)
    return r, adv
for karte, font, g, w in [('Night', L38, gl, wl), ('Today', L26, g26, w26)]:
    for urteil in ['Well rested', 'Short night', 'Very short night']:
        ub = breite(gg, wg, urteil)
        ul, ur, _ = tinte(G18B, urteil, 192 - ub)
        faelle = []
        for m in range(1, 780):
            if m >= 60:
                z = '%d:%02d' % (m // 60, m % 60); e = 'h'
            else:
                z = '%d' % m; e = 'min'
            zr, zadv = zahl_tinte(font, g, w, z)
            el, er, _ = tinte(G18B, e, 8 + (zadv - 8) + 3)
            rechts = max(zr, er)
            if rechts >= ul:
                faelle.append((z + ' ' + e, rechts - ul + 1))
        print(karte, repr(urteil), 'Tinte ab', ul, '-> Ueberlappung in', len(faelle), 'von 779 Dauern; Beispiele', faelle[:2], faelle[len(faelle)//2: len(faelle)//2+1] if faelle else '')
