"""Findet in einem Fitbit-Schlafbild die Zeilen der Stufen (y-Baender) und zeigt die Farblaeufe."""
import sys
from PIL import Image

FARBEN = {
    'hell': {'wach': (212, 89, 135), 'rem': (189, 233, 255), 'leicht': (67, 126, 248), 'tief': (75, 31, 129)},
    'dunkel': {'wach': (255, 155, 187), 'rem': (189, 233, 255), 'leicht': (144, 186, 255), 'tief': (141, 98, 197)},
}


def nah(a, b, tol=18):
    return all(abs(x - y) <= tol for x, y in zip(a, b))


im = Image.open(sys.argv[1]).convert('RGB')
thema = sys.argv[2]
w, h = im.size
px = im.load()
for stufe, farbe in FARBEN[thema].items():
    zeilen = [y for y in range(h) if sum(nah(px[x, y], farbe) for x in range(0, w, 2)) >= 6]
    # zusammenhaengende Baender
    baender, start = [], None
    for i, y in enumerate(zeilen):
        if start is None:
            start = y
        if i + 1 == len(zeilen) or zeilen[i + 1] != y + 1:
            baender.append((start, y))
            start = None
    print(stufe, farbe, 'Baender:', [(a, b) for a, b in baender if b - a >= 8])
