"""Welche Naechte eine Variante anders rechnet als V2, mit Einschlafen, Schlaf und Zahl der anderen Minuten.

Aufruf: gegen_v2.py <Variante ...>. Die Programme liegen wie bei bewerten.py in NACHTLABOR_BIN, sonst labor/bin;
V2 ist dort v_o2h4w30 (rechnet alle Naechte wie schlafanalyse.c in V2 seit 14e34d2e)."""
import os
import subprocess
import sys

HIER = os.path.dirname(os.path.abspath(__file__)) + '/'
N = os.path.dirname(HIER.rstrip('/')) + '/'
BIN = os.environ.get('NACHTLABOR_BIN', HIER + 'bin') + '/'
NAECHTE = [('29.9.', HIER + 'ref-09-29.csv'), ('30.9.', HIER + 'ref-09-30.csv'), ('3.10.', HIER + 'ref-10-03.csv'),
           ('5.10.', HIER + 'ref-10-05.csv'), ('5.10.m', HIER + 'ref-10-05-morgen.csv'),
           ('7.10.', N + 'nacht-2026-10-07.csv'), ('8.10.', N + 'nacht-2026-10-08.csv'),
           ('9.10.', N + 'nacht-2026-10-09.csv'), ('12.10.', N + 'nacht-2026-10-12.csv'),
           ('13.10.', N + 'nacht-2026-10-13.csv'), ('16.10.', N + 'nacht-2026-10-16.csv')]


def uhr(m):
    return f'{(20 + m // 60) % 24:02d}:{m % 60:02d}'


def rechnen(var, csv):
    aus = subprocess.run([BIN + var, csv], capture_output=True, text=True, check=True).stdout.splitlines()
    kopf = dict(t.split('=') for t in aus[0].split()[1:])
    ph = next(z for z in aus if z.startswith('phasen ') and not z.startswith('phasen anders')).split(' ', 1)[1]
    return {k: int(v) for k, v in kopf.items()}, ph


basis = {n: rechnen('v_o2h4w30', c) for n, c in NAECHTE}
print('V2: ' + '  '.join(f"{n} {uhr(k['einschlafen'])}-{uhr(k['aufwachen'])} {k['schlaf']}"
                         for n, (k, _) in basis.items()))
for var in sys.argv[1:]:
    teile = []
    for n, c in NAECHTE:
        k, ph = rechnen(var, c)
        anders = sum(x != y for x, y in zip(ph, basis[n][1]))
        if anders:
            teile.append(f"{n} ein {uhr(k['einschlafen'])} schlaf {k['schlaf']} ({anders} min anders)")
    print(f'{var:14} ' + ('; '.join(teile) if teile else 'wie V2'))
