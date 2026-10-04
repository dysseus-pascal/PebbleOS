#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0
"""Boulder: globale Namen des Zifferblatts TimeStyle ohne eigenes Praefix.

Aufruf: pruefe_ts_symbole.py <nm> <objekt> [weitere Objekte ...]

Rot (Rueckgabe 1), wenn eines der Objekte einen globalen Namen definiert, der
nicht mit ts_, fctx_, ffont_ oder timestyle_ beginnt. Laeuft im Firmware-Bau
ueber die Objekte von src/fw/apps/watch/timestyle.

WARUM: TimeStyle teilt den Gesamtlink mit der ganzen Firmware. Ein Name wie
settings oder mod stiesse mit dem naechsten Upstream-Stand zusammen, der ihn
auch benutzt. ts_namen.h gibt den Namen das Praefix; diese Pruefung sagt,
wenn ein neuer dazukommt, der dort noch fehlt.
"""
import subprocess
import sys

ERLAUBT = ('ts_', 'fctx_', 'ffont_', 'timestyle_')


def globale_namen(nm, objekt):
    # --defined-only: nur was das Objekt selbst anbietet, nicht was es braucht.
    aus = subprocess.run([nm, '-g', '--defined-only', objekt], capture_output=True, text=True)
    if aus.returncode != 0:
        print(f'{objekt}: nm scheiterte: {aus.stderr.strip()}', file=sys.stderr)
        sys.exit(2)
    for zeile in aus.stdout.splitlines():
        teile = zeile.split()
        if len(teile) == 3:
            yield teile[2]


def main():
    if len(sys.argv) < 3:
        print(__doc__, file=sys.stderr)
        return 2
    nm, objekte = sys.argv[1], sys.argv[2:]
    fehler = []
    for objekt in objekte:
        for name in globale_namen(nm, objekt):
            if not name.startswith(ERLAUBT):
                fehler.append((name, objekt))
    for name, objekt in sorted(fehler):
        print(f'TimeStyle: globaler Name ohne Praefix: {name} ({objekt}) - '
              f'in src/fw/apps/watch/timestyle/ts_namen.h eintragen', file=sys.stderr)
    return 1 if fehler else 0


if __name__ == '__main__':
    sys.exit(main())
