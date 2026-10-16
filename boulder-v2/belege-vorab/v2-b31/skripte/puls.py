import csv, glob, sys, collections
diffs = []; vmcs = []
for p in sorted(glob.glob(sys.argv[1] + '/nacht-*.csv')):
    zeilen = open(p).read().split('\n\n')[0].splitlines()
    if not zeilen[0].startswith('# nacht'): continue
    k = dict(t.split('=') for t in zeilen[0][2:].split()[1:])
    ein, auf, ruhe = int(k['einschlafen']), int(k['aufwachen']), int(k['ruhepuls'])
    for x in csv.DictReader(zeilen[1:]):
        m = int(x['minute'])
        if not (m < ein or m >= auf): continue
        s, v, pu = int(x['schritte']), int(x['vmc']), int(x['puls'])
        if s < 40 and v >= 250:
            vmcs.append(v)
            if pu: diffs.append(pu - ruhe)
diffs.sort(); vmcs.sort()
q = lambda a, f: a[min(len(a)-1, int(f*len(a)))]
print('velo-artige Wachminuten mit Puls:', len(diffs), 'Puls ueber Ruhe: min', diffs[0], 'p25', q(diffs,.25), 'median', q(diffs,.5), 'p75', q(diffs,.75), 'p90', q(diffs,.9), 'max', diffs[-1])
print('Anteil >= +20:', round(sum(d >= 20 for d in diffs)/len(diffs),2), '>= +30:', round(sum(d >= 30 for d in diffs)/len(diffs),2), '>= +35:', round(sum(d >= 35 for d in diffs)/len(diffs),2))
print('vmc dieser Minuten: p25', q(vmcs,.25), 'median', q(vmcs,.5), 'p75', q(vmcs,.75), 'n', len(vmcs))
