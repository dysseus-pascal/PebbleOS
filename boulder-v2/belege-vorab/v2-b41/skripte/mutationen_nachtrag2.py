#!/usr/bin/env python3
"""b41: zweiter Nachtrag zu mutationen.py (B8c). Jede muss mindestens einen Test rot machen.

Je Mutation: eine Stelle genau einmal ersetzen, die drei Testziele bauen und laufen lassen, die
Datei zuruecksetzen. Rot = getoetet. Aufruf: mutationen.py <ausgabe.txt>
"""
import pathlib
import subprocess
import sys

R = pathlib.Path('/home/user/pebbleos-v2')
B = R / 'build-test'
BAND = 'fw/apps/system/boulder_tagesband/band.c'
BILD = 'fw/apps/system/boulder_tagesband/bild.c'
APP = 'fw/apps/system/boulder_tagesband/tagesband.c'
ZIELE = {
    'test_tagesband_band': B / 'fw/apps/system_apps/boulder_tagesband/test_tagesband_band/runme',
    'test_tagesband_bild_emery':
        B / 'fw/apps/system_apps/boulder_tagesband/test_tagesband_bild_emery/runme_emery',
    'test_tagesband_app': B / 'fw/apps/system_apps/boulder_tagesband/test_tagesband_app/runme',
}

# Nachtrag 2: B8b baute nicht (anzahl++ und anzahl im selben Ausdruck, -Werror=sequence-point).
# tief steht bis zum ersten Punkt auf UINT8_MAX: nur der erste Punkt setzt neu an.
M = [
    ('B8c', BAND, '.neu = luecke}', '.neu = luecke && tief == UINT8_MAX}',
     'Luecke im Puls ueberbrueckt'),
]


def lauf():
    rot = []
    bau = subprocess.run(['cmake', '--build', str(B), '--target', *ZIELE], capture_output=True,
                         text=True)
    if bau.returncode:
        return ['Bau']
    for name, pfad in ZIELE.items():
        if subprocess.run([str(pfad)], cwd=pfad.parent, capture_output=True).returncode:
            rot.append(name)
    return rot


def main():
    aus = open(sys.argv[1], 'w')
    def schreib(t):
        print(t, flush=True)
        aus.write(t + '\n')
        aus.flush()
    vorher = lauf()
    schreib(f'Ausgangslage: {"rot " + ", ".join(vorher) if vorher else "gruen"}')
    if vorher:
        return 1
    ueberlebt = 0
    for mid, datei, alt, neu, was in M:
        p = R / datei
        text = p.read_text()
        if text.count(alt) != 1:
            schreib(f'{mid} STELLE {text.count(alt)}x: {was}')
            ueberlebt += 1
            continue
        try:
            p.write_text(text.replace(alt, neu))
            rot = lauf()
        finally:
            p.write_text(text)
        if rot:
            schreib(f'{mid} rot ({", ".join(rot)}): {was}')
        else:
            ueberlebt += 1
            schreib(f'{mid} UEBERLEBT: {was}')
    nachher = lauf()
    schreib(f'Danach: {"rot " + ", ".join(nachher) if nachher else "gruen"}')
    schreib(f'{len(M) - ueberlebt}/{len(M)} rot')
    return 1 if ueberlebt or nachher else 0


sys.exit(main())
