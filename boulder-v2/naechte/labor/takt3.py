"""Duennt den Nachtpuls einer V2-Nacht auf den Takt von b26 aus: ausserhalb der HRV-Fenster bleibt je drei
Minuten ein Pulswert (der erste). So rechnet die Schlafanalyse die Nacht, als haette die Uhr wie b26 gemessen.
Aufruf: takt3.py <csv> <ausgabe.csv>"""
import sys

quelle, ziel = sys.argv[1:3]
zeilen = open(quelle).read().splitlines()
hrv, teil = [], 1
for z in zeilen:
    if z.startswith('hrv_minute'):
        teil = 2
    elif teil == 2 and z.count(',') == 3:
        hrv.append(int(z.split(',')[0]))
fenster = {m for h in hrv for m in range(h, h + 5)}
belegt, neu, weg = set(), [], 0
for z in zeilen:
    t = z.split(',')
    if z and z[0] not in '#mh' and len(t) == 7 and int(t[4]) > 0 and int(t[0]) not in fenster:
        if int(t[0]) // 3 in belegt:
            t[4] = '0'
            weg += 1
            z = ','.join(t)
        else:
            belegt.add(int(t[0]) // 3)
    neu.append(z)
open(ziel, 'w').write('\n'.join(neu) + '\n')
print(f'{quelle}: {weg} Pulswerte entfernt, {len(belegt)} ausserhalb der HRV-Fenster behalten')
