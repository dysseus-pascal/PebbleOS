"""Die Aufwach-Schuebe einer Nacht mit ihren Merkmalen und Fitbits Urteil.

Liest auf stdin die Ausgaben eines Pruefbaus von schlafanalyse.c (V2 mit `schub_ausgaben.patch`, gebaut wie
nacht_rechnen.c) und dazu die CSV der Nacht. Aufruf je Nacht:
    <pruefbau> <csv> 2>&1 >/dev/null | python3 schuebe.py <csv> <fitbit.json oder -> <name>
Die HRV steht als Verhaeltnis zum Median der Nacht, den die Schub-Regel nimmt; 'Gang' heisst, ein Gang macht den
Schub ohne HRV-Vergleich wach. 'Fitbit wach' zaehlt auch Minuten ausserhalb von Fitbits Aufzeichnung."""
import json
import re
import sys


def uhr(m):
    return f'{(20 + m // 60) % 24:02d}:{m % 60:02d}'


csv, fitbit, name = sys.argv[1:4]
f = json.load(open(fitbit)) if fitbit != '-' else None
# Minute -> (VMC, Lage)
mi = {}
for z in open(csv):
    t = z.strip().split(',')
    if z[0] not in '#mh' and len(t) == 7:
        mi[int(t[0])] = (int(t[3]), int(t[5]))
puls = {}
for z in sys.stdin:
    m = re.match(r'Puls B(\d+)-B(\d+) grund ([\d.]+) hoch (\d+)', z)
    if m:
        puls[(int(m[1]), int(m[2]))] = (float(m[3]), int(m[4]))
        continue
    m = re.match(r'  B(\d+)-B(\d+) (bleibt Schlaf|wach)(.*)', z)
    if not m:
        continue
    a, b = int(m[1]), int(m[2])
    grund, hoch = puls.get((a, b), (0, 0))
    hv = re.findall(r'HRV (\d+)\D+Nacht (\d+)', m[4])
    hrv = f'{int(hv[0][0]) / int(hv[0][1]):.2f}' if hv else ('Gang' if 'Gang' in m[4] else '-')
    vmc = [mi[i][0] for i in range(a, b) if i in mi]
    still = sum(x == 0 for x in vmc) / len(vmc)
    lage = sum(1 for i in range(a + 1, b) if i in mi and i - 1 in mi and mi[i][1] != mi[i - 1][1])
    regungen = sum(x >= 250 for x in vmc)
    fw = f'{sum(1 for i in range(a, b) if f[i] in (0, -1))}/{b - a}' if f else '?'
    print(f'{name} {uhr(a)}-{uhr(b)} {b - a:3}m {m[3][:6]:6} HRV {hrv:5} Puls +{hoch - grund:2.0f} '
          f'still {100 * still:3.0f}% Lage {60 * lage / (b - a):4.0f}/h Reg {60 * regungen / (b - a):3.0f}/h '
          f'Fitbit wach {fw}')
