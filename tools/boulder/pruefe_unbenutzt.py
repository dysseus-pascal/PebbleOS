#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0
"""Boulder: Texte und Funktionen einer System-App, die niemand benutzt.

Aufruf: pruefe_unbenutzt.py <app-verzeichnis> [weitere Verzeichnisse ...]

Rot (Rueckgabe 1), wenn eine Text-ID aus texte_tabelle.h ausserhalb der
Tabelle nirgends vorkommt (STR(STR_...) wie Kieselsport, TEXT(BhT...) wie
die Health-App), oder eine in den Koepfen der App deklarierte
Funktion ausser ihrer Definition nirgends. Gesucht wird in allen .c und .h
der genannten Verzeichnisse - die weiteren sind die, die Teile der App
mitbenutzen (etwa die Kachel im Startmenue).

WARUM: tote Texte liegen im Abbild der Firmware, und eine tote Funktion
sieht aus, als zeichne sie etwas. Beides fiel bisher erst beim Lesen auf.
"""
import pathlib
import re
import sys


def quellen(verzeichnisse):
    for v in verzeichnisse:
        for p in sorted(pathlib.Path(v).rglob('*')):
            if p.suffix in ('.c', '.h'):
                yield p


def ohne_kommentare(text):
    text = re.sub(r'/\*.*?\*/', ' ', text, flags=re.S)
    return re.sub(r'//[^\n]*', ' ', text)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    app = pathlib.Path(sys.argv[1])
    alle = {p: ohne_kommentare(p.read_text(errors='replace')) for p in quellen(sys.argv[1:])}
    fehler = []

    # Die Texte: jede ID muss ausserhalb der Tabelle vorkommen.
    for tabelle in app.glob('*texte_tabelle.h'):
        ids = re.findall(r'^(?:STR|TEXT)\((\w+)', alle[tabelle], flags=re.M)
        for i in ids:
            if not any(re.search(r'\b%s\b' % i, t) for p, t in alle.items() if p != tabelle):
                fehler.append(f'{tabelle.name}: {i} wird nirgends benutzt')

    # Die Funktionen aus den Koepfen der App: ausser Deklaration und
    # Definition muss es mindestens einen Aufruf oder Verweis geben.
    for kopf in sorted(app.glob('*.h')):
        for name in re.findall(r'^[A-Za-z_][\w \t\*]*?\b([a-z_][a-z0-9_]*)\s*\([^;{()]*\)\s*;',
                               alle[kopf], flags=re.M):
            treffer = sum(len(re.findall(r'\b%s\b' % name, t)) for t in alle.values())
            # Eine Deklaration und eine Definition sind kein Gebrauch.
            definiert = any(re.search(r'\b%s\s*\([^;]*\)\s*\{' % name, t)
                            for p, t in alle.items() if p.suffix == '.c')
            if treffer - 1 - (1 if definiert else 0) <= 0:
                fehler.append(f'{kopf.name}: {name}() wird nirgends benutzt')

    for f in fehler:
        print(f)
    return 1 if fehler else 0


sys.exit(main())
