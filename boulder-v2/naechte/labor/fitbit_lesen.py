"""Liest aus einem Fitbit-Schlafbild die Stufe je Minute (Pixel der vier Zeilen).

Fitbits Zeitachse geht linear von "went to bed" (linker Rand der ersten Stufe) bis "out of bed"
(rechter Rand der letzten); die Summen der Stufen ergeben genau diese Spanne. Ausgabe je Minute:
0 wach, 1 leicht, 2 tief, 3 REM, -1 ausserhalb. Minute 0 ist 20:00 wie in der CSV.
"""
import json
import sys
from PIL import Image

THEMEN = {
    'hell': {'wach': (212, 89, 135), 'rem': (189, 233, 255), 'leicht': (67, 126, 248), 'tief': (75, 31, 129)},
    'dunkel': {'wach': (255, 155, 187), 'rem': (189, 233, 255), 'leicht': (144, 186, 255), 'tief': (141, 98, 197)},
}
CODE = {'wach': 0, 'leicht': 1, 'tief': 2, 'rem': 3}
# y der Zeilenmitte (aus fitbit_baender.py), Bettzeit und Aufstehen (Minuten ab 20:00)
NAECHTE = {
    '08': ('hell', {'wach': (788, 854), 'rem': (1182, 1244), 'leicht': (1371, 1439), 'tief': (1566, 1633)}, 152, 582),
    '09': ('hell', {'wach': (940, 999), 'rem': (1330, 1392), 'leicht': (1519, 1587), 'tief': (1714, 1782)}, 192, 657),
    '12': ('dunkel', {'wach': (872, 940), 'rem': (1262, 1330), 'leicht': (1457, 1525), 'tief': (1652, 1720)}, 98, 586),
}


# Der Rand der REM-Kuppeln im hellen Thema ist kraeftiger blau; er gehoert zur Kuppel.
REM_RAND = {'hell': (103, 212, 255), 'dunkel': (189, 233, 255)}
# Leicht ist der Untergrund: Seine Baender laufen mit runden Enden unter die Nachbarn. Wo zwei
# Zeilen dieselbe Spalte belegen, gilt die andere.
VORRANG = ['wach', 'rem', 'tief', 'leicht']


def nah(a, b, tol=40):
    return all(abs(x - y) <= tol for x, y in zip(a, b))


def lesen(pfad, nacht):
    thema, ys, bett, auf = NAECHTE[nacht]
    farben = THEMEN[thema]
    im = Image.open(pfad).convert('RGB')
    px = im.load()
    w = im.size[0]
    stufe_x = {}
    for stufe, (oben, unten) in ys.items():
        hoehe = unten - oben - 4
        for x in range(w):
            treffer = sum(nah(px[x, y], farben[stufe]) or (stufe == 'rem' and nah(px[x, y], REM_RAND[thema]))
                          for y in range(oben + 2, unten - 2))
            if treffer >= 0.3 * hoehe:
                stufe_x.setdefault(x, []).append(stufe)
    xs = sorted(stufe_x)
    links, rechts = xs[0], xs[-1] + 1
    dauer = auf - bett
    minuten = []
    for m in range(dauer):
        a = links + (rechts - links) * m / dauer
        b = links + (rechts - links) * (m + 1) / dauer
        zaehl = {}
        for x in range(int(a), max(int(a) + 1, int(b + 0.5))):
            for s in stufe_x.get(x, []):
                zaehl[s] = zaehl.get(s, 0) + 1
        if not zaehl:
            minuten.append(None)
            continue
        beste = max(zaehl.values())
        minuten.append(CODE[next(v for v in VORRANG if zaehl.get(v, 0) == beste)])
    # Luecken (kein Pixel) vom Nachbarn fuellen
    for i, s in enumerate(minuten):
        if s is None:
            minuten[i] = minuten[i - 1] if i else next(t for t in minuten if t is not None)
    return bett, auf, links, rechts, minuten


def angleichen(minuten, soll):
    """Die gezeichneten Breiten weichen um runde Enden und Raender ab. Jeder Abschnitt von wach,
    REM und tief wird um seine Mitte auf Fitbits Summe gestreckt oder gestaucht; leicht fuellt."""
    m = list(minuten)
    for stufe in ('wach', 'rem', 'tief'):
        c = CODE[stufe]
        laeufe, i = [], 0
        while i < len(m):
            if m[i] == c:
                j = i
                while j < len(m) and m[j] == c:
                    j += 1
                laeufe.append((i, j))
                i = j
            else:
                i += 1
        ist = sum(b - a for a, b in laeufe)
        if not ist:
            continue
        f = soll[stufe] / ist
        for a, b in laeufe:
            for k in range(a, b):
                m[k] = CODE['leicht']
        rest = soll[stufe]
        for n, (a, b) in enumerate(laeufe):
            laenge = round((b - a) * f) if n < len(laeufe) - 1 else rest
            laenge = max(1, min(laenge, rest))
            rest -= laenge
            mitte = (a + b) / 2
            von = max(0, int(round(mitte - laenge / 2)))
            for k in range(von, min(len(m), von + laenge)):
                if m[k] == CODE['leicht']:
                    m[k] = c
    return m


SOLL = {'08': {'wach': 17, 'leicht': 228, 'tief': 75, 'rem': 110},
        '09': {'wach': 16, 'leicht': 291, 'tief': 70, 'rem': 88},
        '12': {'wach': 79, 'leicht': 268, 'tief': 61, 'rem': 80}}

if __name__ == '__main__':
    nacht = sys.argv[2]
    bett, auf, links, rechts, roh = lesen(sys.argv[1], nacht)
    minuten = angleichen(roh, SOLL[nacht])
    print(f'Nacht {nacht}: x {links}..{rechts}, {len(minuten)} min, gelesen '
          f'{ {k: roh.count(v) for k, v in CODE.items()} }, angeglichen '
          f'{ {k: minuten.count(v) for k, v in CODE.items()} }')
    if len(sys.argv) > 3:
        voll = [-1] * 960
        voll[bett:auf] = minuten
        json.dump(voll, open(sys.argv[3], 'w'))
