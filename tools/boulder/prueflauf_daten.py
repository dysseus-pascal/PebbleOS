#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0
"""Boulder V2, AP9: die Fixture-Naechte als C-Felder fuer den QEMU-Pruefbau (Spezifikation 7.5).

Aufruf: prueflauf_daten.py <repo>
Liest tests/fixtures/activity/boulder/referenz-b26/nacht-<datum>.txt (Eingabe je Minute und
HRV-Fenster, AP-G) und schreibt fw/services/activity/boulder/prueflauf_daten.c. Nur Daten,
keine Rechnung: der Pruefbau spielt sie durch den echten Minutenteil.
"""
import os
import sys

NAECHTE = ['2026-09-26', '2026-09-29', '2026-09-30', '2026-10-03']
MINUTEN = 960
UNGUELTIG = 0xFFFF


def lesen(pfad):
    vmc = [UNGUELTIG] * MINUTEN
    puls = [0] * MINUTEN
    hrv = []
    for zeile in open(pfad):
        t = zeile.split()
        if len(t) == 4 and t[0] == 'eingabe':
            m = int(t[1])
            vmc[m], puls[m] = int(t[2]), int(t[3])
        elif len(t) == 4 and t[0] == 'hrv':
            hrv.append((int(t[1]), int(t[2]), int(t[3])))
    return vmc, puls, hrv


def feld(typ, name, werte):
    zeilen = [', '.join(str(w) for w in werte[i:i + 24]) for i in range(0, len(werte), 24)]
    return 'static const %s %s[] = {\n  %s,\n};\n' % (typ, name, ',\n  '.join(zeilen))


def main():
    repo = sys.argv[1]
    aus = ['/* SPDX-FileCopyrightText: 2026 dysseus-pascal */',
           '/* SPDX-License-Identifier: Apache-2.0 */', '',
           '// ERZEUGT von tools/boulder/prueflauf_daten.py aus den Referenznaechten (AP-G).',
           '// Nur im QEMU-Pruefbau (CONFIG_BOULDER_PRUEFLAUF).', '',
           '#include "pbl/services/activity/boulder/prueflauf.h"', '']
    eintraege = []
    for k, datum in enumerate(NAECHTE):
        pfad = os.path.join(repo, 'tests/fixtures/activity/boulder/referenz-b26',
                            'nacht-%s.txt' % datum)
        vmc, puls, hrv = lesen(pfad)
        aus.append(feld('uint16_t', 's_vmc_%d' % k, vmc))
        aus.append(feld('uint8_t', 's_puls_%d' % k, puls))
        aus.append('static const PrueflaufHrv s_hrv_%d[] = {\n%s\n};\n' % (
            k, '\n'.join('  {%d, %d, %d},' % h for h in hrv) or '  {0, 0, 0},'))
        eintraege.append('  {"%s", s_vmc_%d, s_puls_%d, s_hrv_%d, %d},' % (
            datum, k, k, k, len(hrv)))
    aus.append('const PrueflaufNacht g_boulder_prueflauf_naechte[PRUEFLAUF_NAECHTE] = {')
    aus.extend(eintraege)
    aus.append('};')
    ziel = os.path.join(repo, 'fw/services/activity/boulder/prueflauf_daten.c')
    open(ziel, 'w').write('\n'.join(aus) + '\n')


if __name__ == '__main__':
    main()
