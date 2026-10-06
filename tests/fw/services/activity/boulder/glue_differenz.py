#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0
"""Boulder: T2.2b Glue-Differenztest (ctest test_boulder_glue_differenz).

Aufruf: glue_differenz.py <runme Original> <runme Kopie>

Beide Programme sind test_boulder_glue_differenz.c, einmal mit Pebbles Glue und Kern,
einmal mit der Kopie. Verglichen werden ihre "#B "-Zeilen (DLS 81, Minutendatei,
Schrittsumme je Minute, Sitzungen ausser Schlaf), je Lauf:

  ohne Historie-Lesen: muss gleich sein (Kriterium 2). Einzige erlaubte Abweichung: eine
      Walk/Run-Sitzung, die der Gehfilter (D1) unterdrueckt - sie fehlt in der Kopie ganz
      und lag bei ihrer ersten Meldung unter 40 Schritten/min.
  mit Historie-Lesen: zeigt die Wirkung von D7 (Spezifikation V2); die Zahlen stehen in
      der Ausgabe. Gefordert ist nur: Die Kopie rechnet mit und ohne Lesen gleich.
"""
import re
import subprocess
import sys

SITZUNG = re.compile(r'^SITZUNG \w+ typ=(\d+) start=(\d+) len=(\d+) laufend=\d schritte=(\d+)')


def laeufe(programm):
    r = subprocess.run([programm], capture_output=True, text=True, errors='replace')
    aus, lauf = {}, None
    for z in r.stdout.splitlines():
        if not z.startswith('#B '):
            continue
        z = z[3:]
        if z.startswith('LAUF '):
            lauf = z[5:]
            aus[lauf] = []
        elif lauf:
            aus[lauf].append(z)
    return r.returncode, aus


def sitzung(z):
    m = SITZUNG.match(z)
    return (int(m.group(1)), int(m.group(2))) if m else None


def gefiltert(original, kopie):
    """Sitzungen, die nur das Original hat, mit ihrer ersten Meldung."""
    in_kopie = {sitzung(z) for z in kopie if sitzung(z)}
    erste = {}
    for z in original:
        s = sitzung(z)
        if s and s not in in_kopie and s not in erste:
            erste[s] = z
    return erste


def zaehle(art, a, b):
    za = [z for z in a if z.startswith(art)]
    zb = [z for z in b if z.startswith(art)]
    return sum(1 for x, y in zip(za, zb) if x != y) + abs(len(za) - len(zb)), len(za)


def main(argv):
    if len(argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    rc_o, original = laeufe(argv[1])
    rc_k, kopie = laeufe(argv[2])
    print(f'Original rc={rc_o}, Kopie rc={rc_k}')
    if rc_o or rc_k or set(original) != {'ohne', 'mit'} or set(kopie) != {'ohne', 'mit'}:
        print('Ein Lauf ist gescheitert.')
        return 1
    fehler = 0

    o, k = original['ohne'], kopie['ohne']
    unterdrueckt = gefiltert(o, k)
    for s, z in unterdrueckt.items():
        m = SITZUNG.match(z)
        laenge, schritte = int(m.group(3)), int(m.group(4))
        if schritte >= 40 * laenge:
            print(f'Sitzung fehlt in der Kopie, aber nicht vom Gehfilter: {z}')
            fehler += 1
        else:
            print(f'Gehfilter (D1): {z}')
    o_ohne = [z for z in o if sitzung(z) not in unterdrueckt]
    if o_ohne != k:
        fehler += 1
        print('Ohne Historie-Lesen rechnet die Kopie anders:')
        n = 0
        for i, (x, y) in enumerate(zip(o_ohne, k)):
            if x != y and n < 10:
                print(f'  {i}: original {x[:120]}\n  {i}: kopie    {y[:120]}')
                n += 1
        if len(o_ohne) != len(k):
            print(f'  Zeilen: original {len(o_ohne)}, kopie {len(k)}')
    zeilen = {art: sum(1 for z in o if z.startswith(art)) for art in ('DLS', 'DATEI', 'SCHRITTE')}
    sitz = sum(1 for z in o if sitzung(z))
    print(f'Ohne Historie-Lesen: {zeilen["DLS"]} DLS-Saetze, {zeilen["DATEI"]} Dateisaetze, '
          f'{zeilen["SCHRITTE"]} Minuten, {sitz} Sitzungsmeldungen; '
          f'{len(unterdrueckt)} vom Gehfilter unterdrueckt; ' +
          ('gleich.' if o_ohne == k else 'VERSCHIEDEN.'))

    if kopie['mit'] != k:
        fehler += 1
        print('Die Kopie rechnet mit Historie-Lesen anders als ohne (D7 verletzt).')
    om = [z for z in original['mit'] if sitzung(z) not in unterdrueckt]
    teile = []
    for art in ('DLS', 'DATEI', 'SCHRITTE', 'SITZUNG'):
        n, gesamt = zaehle(art, om, kopie['mit'])
        teile.append(f'{art} {n}/{gesamt}')
    letzte = lambda zs: int([z for z in zs if z.startswith('SCHRITTE')][-1].split()[-1])
    print('Mit Historie-Lesen (V2), Original gegen Kopie verschieden: ' + ', '.join(teile) +
          f'; Schrittsumme am Ende Original {letzte(om)}, Kopie {letzte(kopie["mit"])}.')
    return 1 if fehler else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
