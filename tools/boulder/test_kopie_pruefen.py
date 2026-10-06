#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0
"""Boulder: Gegenproben fuer kopie_pruefen.py und k1_waechter.py (ctest).

Jede Probe muss den Waechter rot faerben; die markierte Fassung gruen.
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).parent))
import k1_waechter  # noqa: E402
import kopie_pruefen  # noqa: E402

ORIGINAL = 'a\nb\nc\nd\n'
D = {'D0', 'D1'}


def proben_kopie():
    ok = kopie_pruefen.pruefe_text
    yield 'markiert', not ok(ORIGINAL, 'a\n/* Boulder: D1 x */\nB\n/* Boulder: Ende D1 */\nc\nd\n', D)
    yield 'geloescht im Block', not ok(ORIGINAL, 'a\n/* Boulder: D1 */\n/* Boulder: Ende D1 */\nc\nd\n', D)
    yield 'unmarkiert geaendert', ok(ORIGINAL, 'a\nB\nc\nd\n', D)
    yield 'unmarkiert geloescht', ok(ORIGINAL, 'a\nc\nd\n', D)
    yield 'unmarkiert angefuegt', ok(ORIGINAL, 'a\nb\nc\nd\ne\n', D)
    yield 'Dn nicht in HERKUNFT', ok(ORIGINAL, 'a\n/* Boulder: D7 */\nB\n/* Boulder: Ende D7 */\nc\nd\n', D)
    yield 'Block offen', ok(ORIGINAL, 'a\n/* Boulder: D1 */\nB\nc\nd\n', D)
    # Falsche Pruefsumme: dieselbe HERKUNFT mit einer anderen SHA-256 ist rot.
    repo = pathlib.Path(__file__).resolve().parents[2]
    herkunft = (repo / kopie_pruefen.HERKUNFT).read_text()
    _, kopien, _ = kopie_pruefen.lies_herkunft(herkunft)
    falsch = herkunft.replace(kopien[0][2], '0' * 64)
    yield 'falsche Pruefsumme', any('SHA-256' in f for f in kopie_pruefen.pruefe(repo, falsch))


def hunk(datei, zeilen, kopf=''):
    return (f'diff --git a/{datei} b/{datei}\n--- a/{datei}\n+++ b/{datei}\n'
            f'@@ -1,1 +1,{len(zeilen)} @@ {kopf}\n' + ''.join(z + '\n' for z in zeilen))


def proben_k1():
    pruefe = k1_waechter.pruefe_diff
    gut = hunk('fw/services/activity/Kconfig', [' x', '+# Boulder: Wahl', '+choice A'])
    yield 'erlaubt', not pruefe(gut, [])
    yield 'eigener Ordner', not pruefe(hunk('fw/services/activity/boulder/x.c', ['+a'] * 500), [])
    yield 'activity.c', pruefe(hunk('fw/services/activity/activity.c', ['+// Boulder: x']), [])
    yield 'ohne Marke', pruefe(hunk('fw/services/activity/Kconfig', ['+choice A']), [])
    yield 'zu viele Zeilen', pruefe(hunk('fw/services/activity/CMakeLists.txt',
                                         ['+# Boulder: x'] + ['+y'] * 6), [])
    yield 'falsche Funktion', pruefe(hunk('fw/services/alarms/alarm.c', ['+  return 1;  // Boulder: x'],
                                          'static void prv_andere(void) {'), [])
    yield 'richtige Funktion', not pruefe(hunk('fw/services/alarms/alarm.c', ['+  return 1;  // Boulder: x'],
                                               'static bool prv_should_smart_alarm_trigger('), [])
    yield 'neue Datei ausserhalb', pruefe('', ['fw/services/activity/neu.c'])


def main():
    rot = 0
    for name, gruen in list(proben_kopie()) + list(proben_k1()):
        print(('ok    ' if gruen else 'FEHLT ') + name)
        rot += not gruen
    return 1 if rot else 0


if __name__ == '__main__':
    sys.exit(main())
