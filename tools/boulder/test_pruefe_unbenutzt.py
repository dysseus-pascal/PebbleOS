#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0
"""Boulder: Gegenproben fuer pruefe_unbenutzt.py (ctest).

Jede Probe legt eine kleine App in einem leeren Ordner an und ruft das Skript wie ctest. Die roten
Proben muessen rot sein, und zwar mit der genannten Meldung; die gruenen gruen.
"""
import pathlib
import subprocess
import sys
import tempfile

SKRIPT = pathlib.Path(__file__).with_name('pruefe_unbenutzt.py')


def lauf(app, weitere=None):
    """(Rueckgabe, Ausgabe) fuer eine App aus {dateiname: inhalt}, dazu ein weiterer Ordner."""
    with tempfile.TemporaryDirectory() as tmp:
        ordner = []
        for name, dateien in (('app', app), ('weitere', weitere)):
            if dateien is None:
                continue
            o = pathlib.Path(tmp, name)
            o.mkdir()
            for datei, inhalt in dateien.items():
                (o / datei).write_text(inhalt)
            ordner.append(str(o))
        r = subprocess.run([sys.executable, str(SKRIPT), *ordner], capture_output=True, text=True,
                           check=False)
        return r.returncode, r.stdout


def rot(app, meldung, weitere=None):
    rc, aus = lauf(app, weitere)
    return rc == 1 and meldung in aus


def gruen(app, weitere=None):
    rc, aus = lauf(app, weitere)
    if rc:
        print('      ' + aus.strip().replace('\n', '; '))
    return rc == 0


# Die Tabelle der Health-App: TEXT(id, englisch), eingebunden als X-Makro.
TABELLE_TEXT = 'TEXT(BhTEins, "One")\nTEXT(BhTZwei, "Two")\n'
TEXTE_H = '#define TEXT(id, en) id,\ntypedef enum {\n#include "texte_tabelle.h"\n} BhText;\n'


def proben():
    yield 'TEXT: alle benutzt', gruen({
        'texte_tabelle.h': TABELLE_TEXT, 'texte.h': TEXTE_H,
        'a.c': 'int f(void) { return BhTEins + BhTZwei; }\n'})
    yield 'TEXT: eine ID nirgends benutzt', rot({
        'texte_tabelle.h': TABELLE_TEXT, 'texte.h': TEXTE_H,
        'a.c': 'int f(void) { return BhTEins; }\n'}, 'BhTZwei wird nirgends benutzt')
    # Erst gerechnet (BhTEins + karte), dann nur im Kommentar: Beides zaehlt nicht als Gebrauch.
    yield 'TEXT: nur im Kommentar', rot({
        'texte_tabelle.h': TABELLE_TEXT, 'texte.h': TEXTE_H,
        'a.c': 'int f(int k) { return BhTEins + k; } // BhTZwei\n'}, 'BhTZwei wird nirgends benutzt')
    yield 'STR: eine ID nirgends benutzt', rot({
        'kieselsport_texte_tabelle.h': 'STR(STR_EINS, "One")\nSTR(STR_ZWEI, "Two")\n',
        'a.c': 'int f(void) { return STR_EINS; }\n'}, 'STR_ZWEI wird nirgends benutzt')
    yield 'Funktion: nur deklariert und definiert', rot({
        'a.h': 'int bh_hilfe(int x);\n',
        'a.c': '#include "a.h"\nint bh_hilfe(int x) { return x; }\n'}, 'bh_hilfe() wird nirgends')
    yield 'Funktion: in einem weiteren Ordner benutzt', gruen({
        'a.h': 'int bh_hilfe(int x);\n',
        'a.c': '#include "a.h"\nint bh_hilfe(int x) { return x; }\n'},
        weitere={'b.c': 'int g(void) { return bh_hilfe(1); }\n'})


def main():
    rot_ = 0
    for name, ok in proben():
        print(('ok    ' if ok else 'FEHLT ') + name)
        rot_ += not ok
    return 1 if rot_ else 0


if __name__ == '__main__':
    sys.exit(main())
