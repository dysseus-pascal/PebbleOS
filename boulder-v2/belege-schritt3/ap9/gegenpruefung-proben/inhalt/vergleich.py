# Vergleicht die nachgebaute Kopfzeile der Night-Karte mit dem Referenzbild (gleiche Lage?).
import sys
sys.path.insert(0, sys.argv[1])
from setzen import lade_voll, breite, setze
from PIL import Image
fonts, ref = sys.argv[2], sys.argv[3]
L38, wl = lade_voll(fonts + '/LECO_38_BOLD_NUMBERS.pbf')
G18B, wg = lade_voll(fonts + '/GOTHIC_18_BOLD.pbf')
r = Image.open(ref).convert('RGB')
best = None
for dy in range(-6, 7):
    for dx in range(-3, 4):
        img = Image.new('RGB', (200, 228), (255, 255, 255))
        zb = breite(L38, wl, '7:52')
        setze(img, L38, wl, '7:52', 8 + dx, 30 + dy, (0, 0, 0))
        setze(img, G18B, wg, 'h', 8 + zb + 3 + dx, 30 + 38 - 17 + dy, (0, 0, 0))
        ub = breite(G18B, wg, 'Short night')
        setze(img, G18B, wg, 'Short night', 192 - ub + dx, 50 + dy, (0, 0, 0))
        diff = 0
        for y in range(28, 80):
            for x in range(0, 200):
                a = img.getpixel((x, y)) != (255, 255, 255)
                b = r.getpixel((x, y)) != (255, 255, 255)
                diff += a != b
        if best is None or diff < best[0]:
            best = (diff, dx, dy)
print('beste Deckung: abweichende Pixel %d bei dx=%d dy=%d' % best)
