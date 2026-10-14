"""Schreibt eine Boulder-CSV mit Kopf und Phasen einer Nachrechnung (nacht_rechnen) statt denen der
Uhr, damit vergleich.py auch eine andere Fassung der Schlafrechnung gegen Fitbit stellen kann.
Aufruf: mit_rechnung.py <nacht_rechnen> <csv> <ausgabe.csv>"""
import subprocess
import sys

rechner, quelle, ziel = sys.argv[1:4]
aus = subprocess.run([rechner, quelle], capture_output=True, text=True, check=True).stdout.splitlines()
kopf = dict(t.split('=') for t in aus[0].split()[1:])
phasen = next(z for z in aus if z.startswith('phasen ') and not z.startswith('phasen anders')).split(' ', 1)[1]
zeilen = open(quelle).read().splitlines()
neu, teil = [], 0
for z in zeilen:
    if z.startswith('# nacht'):
        felder = z.split()
        felder = [f'{k}={kopf[k]}' if '=' in f and (k := f.split('=')[0]) in
                  ('einschlafen', 'aufwachen', 'schlaf') else f for f in felder]
        z = ' '.join(felder)
    elif z.startswith('minute,'):
        teil = 1
    elif z.startswith('hrv_minute,') or not z:
        teil = 2
    elif teil == 1:
        t = z.split(',')
        p = phasen[int(t[0])]
        t[-1] = '-1' if p == '.' else p
        z = ','.join(t)
    neu.append(z)
open(ziel, 'w').write('\n'.join(neu) + '\n')
