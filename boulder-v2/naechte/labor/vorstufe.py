"""Die Velo-Vorstufe (kontext.c) auf den wachen Minuten einer V2-Nacht nachgerechnet: velo-artig ist eine Minute
mit unter 40 Schritten, VMC ab 250 und keinem Puls unter Ruhepuls + 15; ab der dritten in Folge misst die Uhr
den Puls jede Sekunde. Ohne den Fahrtzustand (Frage, Nein), der sie frueher abschaltet - die Zahl ist eine
Obergrenze; ob sie lief, zeigt der Puls in jeder Minute. Aufruf: vorstufe.py <csv> <ruhepuls>"""
import sys

pfad, ruhe = sys.argv[1], int(sys.argv[2])
kopf, mi, hrv, teil = None, [], [], 1
for z in open(pfad):
    if z.startswith('# nacht'):
        kopf = dict(t.split('=') for t in z.split()[2:])
    if z.startswith('hrv_minute'):
        teil = 2
        continue
    t = z.strip().split(',')
    if teil == 1 and z[0] not in '#m' and len(t) == 7:
        mi.append(dict(m=int(t[0]), u=t[1], s=int(t[2]), v=int(t[3]), p=int(t[4])))
    elif teil == 2 and len(t) == 4:
        hrv.append(int(t[0]))
fenster = {m for h in hrv for m in range(h, h + 5)}
ein, auf = int(kopf['einschlafen']), int(kopf['aufwachen'])
for name, (a, b) in (('Abend', (0, ein)), ('Morgen', (auf, len(mi)))):
    folge, vorher, lief = 0, 0, []
    for x in mi:
        if not a <= x['m'] < b:
            continue
        if vorher >= 3:
            lief.append(x)
        velo = x['s'] < 40 and x['v'] >= 250 and not (0 < x['p'] < ruhe + 15)
        folge = folge + 1 if velo else 0
        vorher = folge
    mit_puls = [x for x in lief if x['p'] > 0 and x['m'] not in fenster]
    print(f'{name}: {b - a} min wach, Vorstufe nachgerechnet {len(lief)} min, davon mit Puls ausserhalb der '
          f'HRV-Fenster {len(mit_puls)} min')
    gruppen = []
    for x in lief:
        if gruppen and x['m'] == gruppen[-1][-1]['m'] + 1:
            gruppen[-1].append(x)
        else:
            gruppen.append([x])
    for g in gruppen:
        v = sorted(x['v'] for x in g)
        p = [x['p'] for x in g if x['p'] > 0]
        print(f"  {g[0]['u']}-{g[-1]['u']} {len(g):2} min  Schritte {sum(x['s'] for x in g):3}  VMC-Median "
              f"{v[len(v) // 2]:5}  Puls in {len(p)}/{len(g)} Minuten")
