import sys
sys.path.insert(0, sys.argv[1])
from setzen import lade_voll, breite, setze
from PIL import Image
fonts, ziel = sys.argv[2], sys.argv[3]
L38, wl = lade_voll(fonts + '/LECO_38_BOLD_NUMBERS.pbf')
G18B, wg = lade_voll(fonts + '/GOTHIC_18_BOLD.pbf')
L26, w26 = lade_voll(fonts + '/LECO_26_BOLD_NUMBERS_AM_PM.pbf')
def karte(zahl, urteil, name, y_zahl=30, y_urteil=50, gl=L38, wgl=wl, grundlinie=38):
    img = Image.new('RGB', (200, 100), (255, 255, 255))
    # zeichnen.c:47-57: Zahl bei (8, y), Einheit bei 8+breite+3, y + grundlinie - 17
    zb = breite(gl, wgl, zahl)
    setze(img, gl, wgl, zahl, 8, y_zahl - 20, (0, 0, 0))
    setze(img, G18B, wg, 'h', 8 + zb + 3, y_zahl - 20 + grundlinie - 17, (0, 0, 0))
    ub = breite(G18B, wg, urteil)
    setze(img, G18B, wg, urteil, 192 - ub, y_urteil - 20, (170, 85, 0))
    img.resize((600, 300), Image.NEAREST).save(ziel + '/' + name)
karte('6:30', 'Very short night', 'night_6_30_very_short.png')
karte('7:52', 'Short night', 'night_7_52_short.png')
karte('10:05', 'Well rested', 'night_10_05_well_rested.png')
