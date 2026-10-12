# Zaehlt Pixel, die Zahl/Einheit und Urteil gemeinsam setzen (Night: y=30/50, Today: y+12/y+18).
import sys
sys.path.insert(0, sys.argv[1])
from setzen import lade_voll, breite, setze
from PIL import Image
fonts, ziel = sys.argv[2], sys.argv[3]
L38, wl = lade_voll(fonts + '/LECO_38_BOLD_NUMBERS.pbf')
L26, w26 = lade_voll(fonts + '/LECO_26_BOLD_NUMBERS_AM_PM.pbf')
G18B, wg = lade_voll(fonts + '/GOTHIC_18_BOLD.pbf')
def maske(text, font, w, x, y):
    img = Image.new('1', (200, 228), 0)
    setze(img, font, w, text, x, y, 1)
    return img
def fall(karte, zahl, urteil, speichern=None):
    if karte == 'Night':
        gl, wgl, y, gl_h, uy = L38, wl, 30, 38, 50
    else:
        gl, wgl, y, gl_h, uy = L26, w26, 32 + 12, 26, 32 + 18
    zb = breite(gl, wgl, zahl)
    a = maske(zahl, gl, wgl, 8, y)
    e = maske('h', G18B, wg, 8 + zb + 3, y + gl_h - 17)
    u = maske(urteil, G18B, wg, 192 - breite(G18B, wg, urteil), uy)
    n = sum(1 for yy in range(228) for xx in range(200) if (a.getpixel((xx, yy)) or e.getpixel((xx, yy))) and u.getpixel((xx, yy)))
    if speichern:
        img = Image.new('RGB', (200, 228), (255, 255, 255))
        setze(img, gl, wgl, zahl, 8, y, (0, 0, 0))
        setze(img, G18B, wg, 'h', 8 + zb + 3, y + gl_h - 17, (0, 0, 0))
        setze(img, G18B, wg, urteil, 192 - breite(G18B, wg, urteil), uy, (170, 85, 0))
        img.crop((0, 20, 200, 90)).resize((600, 210), Image.NEAREST).save(speichern)
    return n
for karte, zahl, urteil in [('Night', '7:52', 'Short night'), ('Night', '6:30', 'Very short night'), ('Night', '4:12', 'Very short night'), ('Night', '10:05', 'Well rested'), ('Today', '6:30', 'Very short night'), ('Today', '10:30', 'Very short night')]:
    print(karte, zahl, urteil, '-> gemeinsame Pixel:', fall(karte, zahl, urteil, ziel + '/sim_%s_%s.png' % (karte, zahl.replace(':', ''))))
