import csv, glob, sys, collections
def art(x, ruhe):
    s, v, p = int(x['schritte']), int(x['vmc']), int(x['puls'])
    return s < 40 and v >= 250 and not (p and p < ruhe + 15)
for p in sorted(glob.glob(sys.argv[1] + '/nacht-*.csv')):
    zeilen = open(p).read().split('\n\n')[0].splitlines()
    if not zeilen[0].startswith('# nacht'): continue
    k = dict(t.split('=') for t in zeilen[0][2:].split()[1:])
    ein, auf, ruhe = int(k['einschlafen']), int(k['aufwachen']), int(k['ruhepuls'])
    r = list(csv.DictReader(zeilen[1:]))
    wach = [x for x in r if int(x['minute']) < ein or int(x['minute']) >= auf]
    for i in range(len(wach) - 9):
        w = wach[i:i+10]
        if any(int(w[j+1]['minute']) != int(w[j]['minute']) + 1 for j in range(9)): continue
        v = sum(art(x, ruhe) for x in w)
        lage = max(collections.Counter(x['lage'] for x in w).values())
        if v >= 8 and lage >= 6 and art(w[0], ruhe):
            s = [int(x['schritte']) for x in w]; vm = [int(x['vmc']) for x in w]; pu = [int(x['puls']) for x in w if int(x['puls'])]
            print(f"{p.split('/')[-1][6:16]} {w[0]['uhrzeit']}-{w[-1]['uhrzeit']} ruhe {ruhe}: schritte {s} summe {sum(s)}; vmc median {sorted(vm)[5]}; lage {[x['lage'] for x in w]}; puls {pu}")
