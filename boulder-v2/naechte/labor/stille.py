"""Anteil stiller Minuten in 30-Minuten-Fenstern: ab dem Einschlafen (Fitbit oder Notizen), in den 90 Minuten
davor und, wo Fitbit-Minuten da sind, ueberall in Fitbits Schlaf. Still heisst VMC unter S, fuer S = 1, 50, 100
und 250. Dazu fuer den 16.10. das gleitende Fenster von 21:30 bis 02:00."""
import json
import os

HIER = os.path.dirname(os.path.abspath(__file__)) + '/'
N = os.path.dirname(HIER.rstrip('/')) + '/'
W = 30
# Nacht, CSV, Einschlafen (Minute ab 20:00; Quelle wie in bewerten.py, am 16.10. Fitbits erster Schlaf), Fitbit
NAECHTE = [
    ('30.9.', HIER + 'ref-09-30.csv', 118, None),
    ('3.10.', HIER + 'ref-10-03.csv', 149, None),
    ('5.10.', HIER + 'ref-10-05.csv', 135, None),
    ('7.10.', N + 'nacht-2026-10-07.csv', 77, None),
    ('8.10.', N + 'nacht-2026-10-08.csv', 159, HIER + 'fitbit-08.json'),
    ('9.10.', N + 'nacht-2026-10-09.csv', 208, HIER + 'fitbit-09.json'),
    ('12.10.', N + 'nacht-2026-10-12.csv', 104, HIER + 'fitbit-12.json'),
    ('13.10.', N + 'nacht-2026-10-13.csv', 99, HIER + 'fitbit-13.json'),
    ('16.10.', N + 'nacht-2026-10-16.csv', 346, HIER + 'fitbit-16.json'),
]


def uhr(m):
    return f'{(20 + m // 60) % 24:02d}:{m % 60:02d}'


def lies(csv):
    v = {}
    for z in open(csv):
        t = z.strip().split(',')
        if z[0] not in '#mh' and len(t) == 7:
            v[int(t[0])] = int(t[3])
    return v


def anteil(v, a, s):
    ms = [v[i] for i in range(a, a + W) if i in v]
    return sum(x < s for x in ms) / len(ms) if len(ms) >= 0.8 * W else None


for s in (1, 50, 100, 250):
    print(f'== still: VMC < {s}')
    for name, csv, ein, fj in NAECHTE:
        v = lies(csv)
        vor = [x for x in (anteil(v, a, s) for a in range(max(0, ein - 90), ein - W + 1)) if x is not None]
        zeile = f'  {name:7} ab Einschlafen {100 * anteil(v, ein, s):4.0f} %  davor hoechstens {100 * max(vor):4.0f} %'
        if fj:
            f = json.load(open(fj))
            schlaf = [i for i, x in enumerate(f) if x in (1, 2, 3)]
            im = [x for x in (anteil(v, a, s) for a in range(schlaf[0], schlaf[-1] - W + 1)) if x is not None]
            zeile += f'  in Fitbits Schlaf mindestens {100 * min(im):4.0f} % (unter 50 %: {sum(x < 0.5 for x in im)} von {len(im)})'
        print(zeile)
print('== 16.10., gleitend: Fensterbeginn, still bei VMC 0 / < 50 / < 100 / < 250')
v = lies(N + 'nacht-2026-10-16.csv')
for a in range(90, 360, 5):
    print(f'  {uhr(a)} ' + '  '.join(f'{100 * anteil(v, a, s):3.0f} %' for s in (1, 50, 100, 250)))
