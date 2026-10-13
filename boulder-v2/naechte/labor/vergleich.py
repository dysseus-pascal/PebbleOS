"""Boulder (Phasen der Uhr aus der CSV) gegen Fitbit (aus dem Bild, fitbit_lesen.py) je Minute."""
import json
import statistics
import sys

NAME = {0: 'wach', 1: 'leicht', 2: 'tief', 3: 'REM', -1: 'aus'}


def uhr(m):
    return f'{(20 + m // 60) % 24:02d}:{m % 60:02d}'


def csv_lesen(pfad):
    zeilen = open(pfad).read().splitlines()
    kopf = dict(t.split('=') for t in zeilen[0][2:].split()[1:])
    leer = zeilen.index('')
    minuten = []
    for z in zeilen[2:leer]:
        m, u, s, v, p, l, ph = z.split(',')
        minuten.append({'schritte': int(s), 'vmc': int(v), 'puls': int(p), 'phase': int(ph)})
    hrv = []
    for z in zeilen[leer + 2:]:
        m, u, r, p = z.split(',')
        hrv.append((int(m), int(r), int(p)))
    return {k: int(v) for k, v in kopf.items()}, minuten, hrv


def laeufe(folge, wert):
    out, i = [], 0
    while i < len(folge):
        if folge[i] == wert:
            j = i
            while j < len(folge) and folge[j] == wert:
                j += 1
            out.append((i, j))
            i = j
        else:
            i += 1
    return out


def kappa(a, b, klassen):
    n = len(a)
    po = sum(x == y for x, y in zip(a, b)) / n
    pe = sum((a.count(k) / n) * (b.count(k) / n) for k in klassen)
    return (po - pe) / (1 - pe) if pe < 1 else 1.0


def main(csv, fit_json, titel):
    kopf, mi, hrv = csv_lesen(csv)
    fit = json.load(open(fit_json))
    n = len(mi)
    b = [x['phase'] for x in mi]
    f = fit[:n] + [-1] * max(0, n - len(fit))
    f_schlaf = [i for i in range(n) if f[i] in (1, 2, 3)]
    b_schlaf = [i for i in range(n) if b[i] in (1, 2, 3)]
    print(f'## {titel}')
    fb = next(i for i in range(n) if f[i] != -1)
    fa = max(i for i in range(n) if f[i] != -1) + 1
    print(f'Boulder: Einschlafen {uhr(kopf["einschlafen"])}, Aufwachen {uhr(kopf["aufwachen"])}, '
          f'Schlaf {kopf["schlaf"]} min, Ruhepuls {kopf["ruhepuls"]}, HRV {kopf["hrv"]}')
    print(f'Fitbit:  im Bett {uhr(fb)}, erster Schlaf {uhr(f_schlaf[0])}, letzter Schlaf bis '
          f'{uhr(f_schlaf[-1] + 1)}, aus dem Bett {uhr(fa)}, Schlaf {len(f_schlaf)} min')
    for k, name in ((0, 'wach'), (3, 'REM'), (1, 'leicht'), (2, 'tief')):
        bo = sum(1 for i in range(kopf['einschlafen'], kopf['aufwachen']) if b[i] == k)
        fi = sum(1 for x in f if x == k)
        print(f'  {name:6} Boulder {bo:4} min  Fitbit {fi:4} min')
    # Schlaf/Wach je Minute ueber beide Naechte (ausserhalb = wach)
    von = min(kopf['einschlafen'], fb)
    bis = max(kopf['aufwachen'], fa)
    sb = [1 if b[i] in (1, 2, 3) else 0 for i in range(von, bis)]
    sf = [1 if f[i] in (1, 2, 3) else 0 for i in range(von, bis)]
    gleich = sum(x == y for x, y in zip(sb, sf))
    print(f'Schlaf/Wach gleich: {gleich}/{bis - von} min ({100 * gleich / (bis - von):.0f} %), '
          f'Kappa {kappa(sb, sf, (0, 1)):.2f}')
    beide = [i for i in range(n) if b[i] in (1, 2, 3) and f[i] in (1, 2, 3)]
    pb = [b[i] for i in beide]
    pf = [f[i] for i in beide]
    gleich = sum(x == y for x, y in zip(pb, pf))
    print(f'Phase gleich (wo beide Schlaf): {gleich}/{len(beide)} ({100 * gleich / len(beide):.0f} %), '
          f'Kappa {kappa(pb, pf, (1, 2, 3)):.2f}')
    print('  Boulder\\Fitbit  leicht  tief   REM')
    for k in (1, 2, 3):
        print(f'  {NAME[k]:14} ' + ' '.join(f'{sum(1 for i in beide if b[i] == k and f[i] == j):6}'
                                          for j in (1, 2, 3)))
    # Schlafpuls der Nacht: Median der gueltigen Pulse in Boulders Schlafminuten
    sp = statistics.median([mi[i]['puls'] for i in b_schlaf if mi[i]['puls'] > 0])
    print(f'Schlafpuls (Median in Boulders Schlaf): {sp}')

    def beschreibe(von, bis):
        p = [mi[i]['puls'] for i in range(von, bis) if mi[i]['puls'] > 0]
        v = [mi[i]['vmc'] for i in range(von, bis)]
        s = sum(mi[i]['schritte'] for i in range(von, bis))
        h = [r for m, r, _ in hrv if von - 5 < m < bis]
        fz = {NAME[k]: sum(1 for i in range(von, bis) if f[i] == k) for k in (-1, 0, 1, 2, 3)}
        fz = ', '.join(f'{k} {v}' for k, v in fz.items() if v)
        bz = {NAME[k]: sum(1 for i in range(von, bis) if b[i] == k) for k in (-1, 0, 1, 2, 3)}
        bz = ', '.join(f'{k} {v}' for k, v in bz.items() if v)
        return (f'Puls {min(p) if p else "-"}-{max(p) if p else "-"} (Median '
                f'{statistics.median(p) if p else "-"}), VMC max {max(v)}, Regungen>=450 '
                f'{sum(1 for x in v if x >= 450)}, Schritte {s}, HRV {h}', fz, bz)

    print('Boulders Wachphasen in der Nacht:')
    for a, e in laeufe(b, 0):
        if a < kopf['einschlafen'] or e > kopf['aufwachen']:
            continue
        txt, fz, _ = beschreibe(a, e)
        print(f'  {uhr(a)}-{uhr(e)} ({e - a:3} min) Fitbit: {fz} | {txt}')
    print('Fitbits Wachphasen:')
    for a, e in laeufe(f, 0):
        txt, _, bz = beschreibe(a, e)
        print(f'  {uhr(a)}-{uhr(e)} ({e - a:3} min) Boulder: {bz} | {txt}')
    zeichen = {-1: ' ', 0: 'W', 1: '-', 2: '_', 3: 'R'}
    for s in range((von // 60) * 60, bis, 120):
        print(f'  {uhr(s)} B ' + ''.join(zeichen[b[i]] for i in range(s, min(s + 120, n))))
        print(f'        F ' + ''.join(zeichen[f[i]] for i in range(s, min(s + 120, n))))


if __name__ == '__main__':
    main(*sys.argv[1:4])
