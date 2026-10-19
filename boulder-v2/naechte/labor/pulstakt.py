"""Wie dicht die Uhr den Puls misst: Anteil der Minuten mit Puls am Abend, in der Nacht und am Morgen, und die
Abstaende zwischen zwei Pulswerten im Schlaf. Bis 13.10. lief b26, ab 16.10. V2 (beide Messstufe 4)."""
import collections
import glob
import os

N = os.path.dirname(os.path.dirname(os.path.abspath(__file__))) + '/'


def lesen(pfad):
    kopf, mi = None, []
    for z in open(pfad):
        if z.startswith('# nacht'):
            kopf = dict(t.split('=') for t in z.split()[2:])
        t = z.strip().split(',')
        if z[0] not in '#mh' and len(t) == 7:
            mi.append((int(t[0]), int(t[2]), int(t[3]), int(t[4])))
    return kopf, mi


for pfad in sorted(glob.glob(N + 'nacht-2026-10-*.csv')):
    if 'morgen' in pfad:
        continue
    kopf, mi = lesen(pfad)
    ein, auf = int(kopf['einschlafen']), int(kopf['aufwachen'])
    name = os.path.basename(pfad)[6:16]
    teile = []
    for teil, (a, b) in (('Abend', (0, ein)), ('Nacht', (ein, auf)), ('Morgen', (auf, len(mi)))):
        minuten = [m for m in mi if a <= m[0] < b]
        if minuten:
            teile.append(f'{teil} {100 * sum(1 for m in minuten if m[3] > 0) / len(minuten):3.0f} %')
    puls = [m[0] for m in mi if ein <= m[0] < auf and m[3] > 0]
    abstaende = collections.Counter(b - a for a, b in zip(puls, puls[1:]))
    alle = sum(abstaende.values())
    takt = ', '.join(f'{g} min {100 * c / alle:.0f} %' for g, c in sorted(abstaende.items()) if c / alle >= 0.03)
    print(f'{name}  Minuten mit Puls: {", ".join(teile)}  |  Abstaende im Schlaf: {takt}')
