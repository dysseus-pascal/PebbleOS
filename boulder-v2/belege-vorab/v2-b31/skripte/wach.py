import csv, glob, sys, collections
# Wie kontext.c prv_art und prv_kandidatin, an den Wachminuten echter Naechte (Abend vor dem
# Einschlafen, Morgen nach dem Aufwachen). Puls fehlt in den meisten Minuten: wie in der Firmware
# zaehlt er nur, wenn er da ist.
def art(x, ruhe):
    s, v, p = int(x['schritte']), int(x['vmc']), int(x['puls'])
    tief = p and p < ruhe + 15
    return s < 40 and v >= 250 and not tief
summe = collections.Counter()
for p in sorted(glob.glob(sys.argv[1] + '/nacht-*.csv')):
    zeilen = open(p).read().split('\n\n')[0].splitlines()
    if not zeilen[0].startswith('# nacht'): continue
    k = dict(t.split('=') for t in zeilen[0][2:].split()[1:])
    ein, auf, ruhe = int(k['einschlafen']), int(k['aufwachen']), int(k['ruhepuls'])
    r = list(csv.DictReader(zeilen[1:]))
    for teil, m in (('abend', [x for x in r if int(x['minute']) < ein]), ('morgen', [x for x in r if int(x['minute']) >= auf])):
        bewegt = [x for x in m if int(x['vmc']) >= 250]
        velo = [x for x in m if art(x, ruhe)]
        kand = []
        for i in range(len(m) - 9):
            w = m[i:i+10]
            if any(int(w[j+1]['minute']) != int(w[j]['minute']) + 1 for j in range(9)): continue
            v = sum(art(x, ruhe) for x in w)
            lage = max(collections.Counter(x['lage'] for x in w).values())
            erste = next((j for j, x in enumerate(w) if art(x, ruhe)), None)
            if v >= 8 and lage >= 6 and erste == 0: kand.append(w[-1]['uhrzeit'])
        summe[teil, 'min'] += len(m); summe[teil, 'bewegt'] += len(bewegt); summe[teil, 'velo'] += len(velo); summe[teil, 'kand'] += len(kand)
        print(f"{p.split('/')[-1][6:16]} {teil:6} {len(m):3} min, vmc>=250: {len(bewegt):3}, velo-artig: {len(velo):3}, Kandidatin-Fenster: {len(kand)} {kand[:3]}")
print(dict(summe))
