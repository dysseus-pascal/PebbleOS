"""Rechnet alle Naechte mit jeder Variante und vergleicht mit Fitbit (soweit bekannt).

Aufruf: bewerten.py [--wecker] [Variante ...]. Die Varianten sind Programme aus nacht_rechnen.c und
schlafanalyse_var.c (siehe LIESMICH.md) im Verzeichnis NACHTLABOR_BIN, sonst labor/bin. Mit --wecker
zaehlen bei Fitbit die Minuten bis zum Aufwachen als Schlaf, das der Nutzer angegeben hat: am 8. und
12.10. bis zum Wecker um 06:00 (Fitbit endete 05:42 bzw. 05:46), am 13.10. bis 05:45 (Fitbit endete
05:23). Die Uhr zeigt dort Schlafpuls und kaum Bewegung."""
import json
import os
import subprocess
import sys

HIER = os.path.dirname(os.path.abspath(__file__)) + '/'
NAECHTE_DIR = os.path.dirname(HIER.rstrip('/')) + '/'
BIN = os.environ.get('NACHTLABOR_BIN', HIER + 'bin') + '/'
WECKER = '--wecker' in sys.argv
VARIANTEN = [a for a in sys.argv[1:] if a != '--wecker'] or [
    'v_b26', 'v_w30', 'v_o2', 'v_h2', 'v_o2h2', 'v_o2w30', 'v_o2h2w30', 'v_h3', 'v_o2h3w30', 'v_g20', 'v_o2g20w30',
    'v_o2g25w30', 'v_o2hnw30', 'v_h4', 'v_o2h4w30']
# Fitbit je Nacht: Schlafbeginn (Minute ab 20:00), Schlaf gesamt, wach im Bett; Quelle in Klammern
NAECHTE = [
    ('29.9.', HIER + 'ref-09-29.csv', None, None),
    ('30.9.', HIER + 'ref-09-30.csv', {'ein': 118, 'schlaf': 414, 'wach': 66}, None),  # notizen.md, Bett 21:58
    ('3.10.', HIER + 'ref-10-03.csv', {'ein': 149, 'schlaf': 347, 'wach': 131}, None),  # notizen.md, Bett 22:29
    ('5.10.', HIER + 'ref-10-05.csv', {'ein': 135}, None),  # schlafanalyse.c: Fitbit sah den Schlaf um 22:15
    ('7.10.', NAECHTE_DIR + 'nacht-2026-10-07.csv', {'ein': 77, 'wach': 27}, None),  # notizen.md, Bett 21:17
    ('8.10.', NAECHTE_DIR + 'nacht-2026-10-08.csv', {'ein': 159, 'schlaf': 431 if WECKER else 413, 'wach': 17},
     HIER + ('fitbit-08k.json' if WECKER else 'fitbit-08.json')),
    ('9.10.', NAECHTE_DIR + 'nacht-2026-10-09.csv', {'ein': 208, 'schlaf': 449, 'wach': 16}, HIER + 'fitbit-09.json'),
    ('12.10.', NAECHTE_DIR + 'nacht-2026-10-12.csv', {'ein': 104, 'schlaf': 423 if WECKER else 409, 'wach': 79},
     HIER + ('fitbit-12k.json' if WECKER else 'fitbit-12.json')),
    # 13.10.: erster Schlaf 21:39 aus dem Bild; Schlaf = Fitbits Summe ohne wach (458), Ende 05:23
    ('13.10.', NAECHTE_DIR + 'nacht-2026-10-13.csv', {'ein': 99, 'schlaf': 481 if WECKER else 458, 'wach': 9},
     HIER + ('fitbit-13k.json' if WECKER else 'fitbit-13.json')),
]


def uhr(m):
    return f'{(20 + m // 60) % 24:02d}:{m % 60:02d}'


def rechnen(var, csv):
    aus = subprocess.run([BIN + var, csv], capture_output=True, text=True, check=True).stdout.splitlines()
    kopf = dict(t.split('=') for t in aus[0].split()[1:])
    phasen = next(z for z in aus if z.startswith('phasen ') and not z.startswith('phasen anders')).split(' ', 1)[1]
    return {k: int(v) for k, v in kopf.items()}, phasen


basis = {name: rechnen('v_b26', csv)[1] for name, csv, _, _ in NAECHTE}
for var in VARIANTEN:
    print(f'### {var}')
    summe_ein, summe_schlaf, summe_wach, n_ein, n_schlaf, n_wach, summe_sw = 0, 0, 0, 0, 0, 0, 0
    for name, csv, fit, fjson in NAECHTE:
        k, ph = rechnen(var, csv)
        wach = sum(1 for i in range(k['einschlafen'], k['aufwachen']) if ph[i] == '0')
        anders = sum(a != b for a, b in zip(ph, basis[name]))
        zeile = (f'  {name:7} ein {uhr(k["einschlafen"])} auf {uhr(k["aufwachen"])} schlaf {k["schlaf"]:3} '
                 f'wach {wach:3} | anders als b26: {anders:3} min')
        if fit:
            if 'ein' in fit:
                d = k['einschlafen'] - fit['ein']
                zeile += f' | ein {d:+4}'
                summe_ein += abs(d)
                n_ein += 1
            if 'schlaf' in fit:
                d = k['schlaf'] - fit['schlaf']
                zeile += f' schlaf {d:+4}'
                summe_schlaf += abs(d)
                n_schlaf += 1
            if 'wach' in fit:
                d = wach - fit['wach']
                zeile += f' wach {d:+4}'
                summe_wach += abs(d)
                n_wach += 1
        if fjson:
            f = json.load(open(fjson))
            # festes Fenster je Nacht: 2 h vor Fitbits Bettzeit (frühestens 20:00) bis 1 h nach dem Aufstehen
            von = max(0, next(i for i, x in enumerate(f) if x != -1) - 120)
            bis = min(len(ph), max(i for i, x in enumerate(f) if x != -1) + 61)
            sb = [1 if ph[i] in '123' else 0 for i in range(von, bis)]
            sf = [1 if f[i] in (1, 2, 3) else 0 for i in range(von, bis)]
            gl = sum(x == y for x, y in zip(sb, sf))
            zeile += f' | S/W anders {len(sb) - gl:3} min'
            summe_sw += len(sb) - gl
        print(zeile)
    print(f'  Mittel |Abweichung|: Einschlafen {summe_ein / n_ein:.0f} min ({n_ein} Naechte), Schlaf '
          f'{summe_schlaf / n_schlaf:.0f} min ({n_schlaf}), wach {summe_wach / n_wach:.0f} min ({n_wach}); '
          f'Schlaf/Wach anders als Fitbit (8.-13.10.): {summe_sw} min')
