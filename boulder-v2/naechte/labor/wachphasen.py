"""Merkmale aller Wachphasen von Boulder mitten in der Nacht, mit Fitbits Urteil."""
import json
import statistics
import sys

sys.path.insert(0, '/tmp/claude-0/-home-user/7e362f6d-34dc-553d-8345-69bd6588c9e5/scratchpad/nachtlabor')
from vergleich import csv_lesen, laeufe, uhr  # noqa: E402

N = '/tmp/claude-0/-home-user/7e362f6d-34dc-553d-8345-69bd6588c9e5/scratchpad/'
NAECHTE = [
    ('7.10.', '/home/user/pebbleos-notizen/boulder-v2/naechte/nacht-2026-10-07.csv', None),
    ('8.10.', N + 'naechte-neu/nacht-2026-10-08.csv', N + 'nachtlabor/fitbit-08.json'),
    ('9.10.', N + 'naechte-neu/nacht-2026-10-09.csv', N + 'nachtlabor/fitbit-09.json'),
    ('12.10.', N + 'naechte-neu/nacht-2026-10-12.csv', N + 'nachtlabor/fitbit-12.json'),
]
# 7.10.: Fitbit nur aus den Notizen (naechte/notizen.md): diese drei Wachphasen sind Schlaf/REM.
SCHLAF_7 = [(57 + 4 * 60, 67 + 4 * 60), (6 * 60 + 43, 7 * 60 + 27), (8 * 60 + 43, 8 * 60 + 56)]

print('Nacht  Wachphase      min  Fitbit-wach  Puls-Median (Schlaf)  dPuls  Puls>+13  HRV (Nacht)  '
      'Regungen  je 10 min  Schritte')
for name, csv, fit in NAECHTE:
    kopf, mi, hrv = csv_lesen(csv)
    b = [x['phase'] for x in mi]
    f = json.load(open(fit)) if fit else None
    schlaf = [i for i in range(len(b)) if b[i] in (1, 2, 3)]
    sp = statistics.median([mi[i]['puls'] for i in schlaf if mi[i]['puls'] > 0])
    hrv_nacht = statistics.median([r for m, r, _ in hrv if kopf['einschlafen'] <= m < kopf['aufwachen']])
    for a, e in laeufe(b, 0):
        if a < kopf['einschlafen'] or e > kopf['aufwachen']:
            continue
        p = [mi[i]['puls'] for i in range(a, e) if mi[i]['puls'] > 0]
        pm = statistics.median(p) if p else 0
        hoch = sum(1 for x in p if x >= sp + 13)
        h = [r for m, r, _ in hrv if a - 5 < m < e]
        reg = sum(1 for i in range(a, e) if mi[i]['vmc'] >= 250)
        st = sum(mi[i]['schritte'] for i in range(a, e))
        if f:
            fw = sum(1 for i in range(a, e) if f[i] in (0, -1))
        else:
            fw = 0 if any(x <= a < y or x < e <= y for x, y in SCHLAF_7) else None
        fw_txt = '?' if fw is None else f'{fw}/{e - a}'
        print(f'{name:6} {uhr(a)}-{uhr(e)} {e - a:4}  {fw_txt:>11}  {pm:5} ({sp})          '
              f'{pm - sp:+5}  {hoch:8}  {str(h):>12} ({hrv_nacht})  {reg:8}  {10 * reg / (e - a):8.1f}  {st:8}')
