#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0
"""Boulder: zaehlt die Kopie wie das Original? (ctest test_boulder_kraepelin_gleich)

Aufruf: gleich_zaehlen.py <runme Original> <runme Kopie>

Beide Programme sind derselbe Pebble-Test (test_kraepelin_algorithm.c), einmal
mit dem Original-Kern, einmal mit der Kopie. Ihre Ausgabe nennt fuer jede Probe
des Pruefkorpus die Schritte, die Minutenwerte und die Schlaf- und
Aktivitaetssitzungen. Rot, wenn sich eine Zeile unterscheidet oder ein Lauf
scheitert. Die Zeilennummern in den Logzeilen (datei.c:123>) bleiben aussen
vor: die markierten Bloecke der Kopie verschieben sie. Ebenso die Kalorien aus
Pebbles altem Referenz-Worker (ProjectK_worker.c, nicht Kraepelin): er liest in
einer Minute Ungesetztes, zwei Laeufe des Originals unterscheiden sich dort
(geprueft 06.10.2026). Vergleichen laesst sich nur, solange die Kopie wie das Original
rechnen soll; jede gewollte Abweichung (Dn) muss hier begruendet ausgenommen
werden.
"""
import difflib
import os
import re
import subprocess
import sys

ZEILENNUMMER = re.compile(r'([A-Za-z0-9_]+\.[ch]):\d+>')
REFERENZ_KALORIEN = re.compile(r'^(ProjectK_worker\.c:#> minute_stats .*calories: )-?\d+$')


def lauf(programm):
    r = subprocess.run([programm], cwd=os.path.dirname(programm), capture_output=True,
                       text=True, errors='replace')
    # Logzeilen tragen die Zeilennummer der Quelle; die Kopie hat mehr Zeilen.
    zeilen = [ZEILENNUMMER.sub(r'\1:#>', z) for z in r.stdout.splitlines()]
    return r.returncode, [REFERENZ_KALORIEN.sub(r'\1?', z) for z in zeilen]


def main(argv):
    if len(argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    rc_o, original = lauf(argv[1])
    rc_k, kopie = lauf(argv[2])
    schritte = sum(1 for z in original if z.startswith('RESULTS: exp_steps'))
    print(f'Original rc={rc_o}, Kopie rc={rc_k}, {len(original)} Zeilen, '
          f'{schritte} Schrittproben')
    if rc_o or rc_k:
        return 1
    if schritte == 0:
        print('Keine Schrittproben in der Ausgabe - der Vergleich saehe nichts.')
        return 1
    if original != kopie:
        print('Kopie rechnet anders als das Original:')
        sys.stdout.writelines(difflib.unified_diff(
            [z + '\n' for z in original], [z + '\n' for z in kopie], 'original', 'kopie', n=1))
        return 1
    print('Kopie und Original: Ausgabe Zeile fuer Zeile gleich.')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
