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
    yield 'Nachholen in alarm_init', not pruefe(hunk('fw/services/alarms/alarm.c', ['+  x();  // Boulder: x'],
                                                     'void alarm_init(void) {'), [])
    yield 'Geklungen in prv_record_alarm_op', not pruefe(
        hunk('fw/services/alarms/alarm.c', ['+  x();  // Boulder: x'],
             'static bool prv_record_alarm_op(AlarmId id, AlarmConfig *config, void *context)'), [])
    yield 'aehnlicher Name', pruefe(hunk('fw/services/alarms/alarm.c', ['+  x();  // Boulder: x'],
                                         'void alarm_init_spaeter(void) {'), [])
    yield 'alarm.c zu viele Zeilen', pruefe(hunk('fw/services/alarms/alarm.c', ['+  x();  // Boulder: x'] * 21,
                                                 'void alarm_init(void) {'), [])
    yield 'neue Datei ausserhalb', pruefe('', ['fw/services/activity/neu.c'])
    yield from proben_kieselsport(pruefe)


def hunks(datei, *teile):
    """Ein Diff mit mehreren Hunks: teile = (kopf, zeilen), ..."""
    kopf = f'diff --git a/{datei} b/{datei}\n--- a/{datei}\n+++ b/{datei}\n'
    return kopf + ''.join(f'@@ -{10 * i},1 +{10 * i},{len(z)} @@ {k}\n' + ''.join(x + '\n' for x in z)
                          for i, (k, z) in enumerate(teile, 1))


# Schritt 2, P1-P4 (Spezifikation 2.3, AP0).
WORKOUT_C = 'fw/apps/system/workout/workout.c'
WORKOUT_INFO = 'const PebbleProcessMd *workout_app_get_info(void) {'
P2 = ['+#ifdef CONFIG_BOULDER_KIESELSPORT', '+  .visibility = ProcessVisibilityHidden,  // Boulder: P2',
      '+#endif']
DIENST = 'fw/services/activity/workout_service.c'
STOPP = 'bool workout_service_stop_workout(void) {'
P1 = ['+#ifdef CONFIG_BOULDER_KIESELSPORT', '+  save_session &= !boulder_x();  // Boulder: F3', '+#endif']
REGISTRY = 'fw/shell/normal/system_app_registry_list.json'
P3 = ['+    {', '+      "kommentar": "Boulder: P3",', '+      "id": -103,',
      '+      "ifdefs": ["CONFIG_BOULDER_KIESELSPORT"]', '+    },']


def proben_kieselsport(pruefe):
    yield 'P2 im Prozess-Eintrag', not pruefe(hunk(WORKOUT_C, P2, WORKOUT_INFO), [])
    # Zusammen 4 Zeilen, also nur wegen des zweiten Hunks rot.
    zwei = hunks(WORKOUT_C, (WORKOUT_INFO, P2), (WORKOUT_INFO, ['+    // Boulder: zweiter Hunk']))
    yield 'zweiter Hunk in workout.c', pruefe(zwei, [], zwei)
    yield 'P2 ohne Schalter', pruefe(hunk(WORKOUT_C, ['+  .visibility = ProcessVisibilityHidden,  '
                                                     '// Boulder: P2'], WORKOUT_INFO), [])
    yield 'P2 in anderer Funktion', pruefe(hunk(WORKOUT_C, P2, 'static void prv_init(void) {'), [])
    yield 'P2 zu viele Zeilen', pruefe(hunk(WORKOUT_C, P2 + ['+  x;', '+  y;'], WORKOUT_INFO), [])
    yield 'P1 im Stopp', not pruefe(hunk(DIENST, P1, STOPP), [])
    yield 'P1 Include ohne Schalter', not pruefe(
        hunk(DIENST, ['+#include "pbl/services/activity/boulder/kalorien.h"  // Boulder: P1']), [])
    yield 'P1 Include mit Schalter', not pruefe(
        hunk(DIENST, ['+#ifdef CONFIG_BOULDER_KIESELSPORT',
                      '+#include "pbl/services/activity/boulder/kalorien.h"  // Boulder: P1',
                      '+#endif']), [])
    yield 'P1 #if 0 ausserhalb', pruefe(hunk(DIENST, ['+#ifdef CONFIG_BOULDER_KIESELSPORT  // Boulder: x',
                                                      '+#if 0', '+#endif', '+#endif']), [])
    yield 'P1 in Pause', pruefe(hunk(DIENST, P1, 'bool workout_service_pause_workout(bool should_be_paused) {'),
                                [])
    yield 'P1 ohne Schalter', pruefe(hunk(DIENST, ['+  boulder_x();  // Boulder: P1'], STOPP), [])
    yield 'P1 zu viele Zeilen', pruefe(hunk(DIENST, P1 * 9, STOPP), [])
    yield 'P3 Registry', not pruefe(hunk(REGISTRY, P3), [])
    zwei = hunks(REGISTRY, ('', P3), ('', P3))
    yield 'P3 zweiter Eintrag', pruefe(zwei, [], zwei)
    yield 'P4 Satz', not pruefe(hunk(
        'fw/services/activity/activity_sessions.c',
        ['+#ifdef CONFIG_BOULDER_KIESELSPORT', '+    .manual = session->manual,  // Boulder: P4', '+#endif'],
        'void activity_sessions_prv_send_activity_session_to_data_logging(ActivitySession *session) {'), [])
    yield 'P4 andere Funktion', pruefe(hunk(
        'fw/services/activity/activity_sessions.c',
        ['+#ifdef CONFIG_BOULDER_KIESELSPORT', '+  x();  // Boulder: P4', '+#endif'],
        'void activity_sessions_prv_add_activity_session(ActivitySession *session) {'), [])
    yield 'App-Ordner frei', not pruefe('', ['fw/apps/system/kieselsport/lauf_window.c'])
    yield 'Testordner der App frei', not pruefe('', ['tests/fw/apps/system_apps/kieselsport/test_x.c'])
    yield 'anderer App-Ordner', pruefe('', ['fw/apps/system/kieselstrasse/x.c'])


def main():
    rot = 0
    for name, gruen in list(proben_kopie()) + list(proben_k1()):
        print(('ok    ' if gruen else 'FEHLT ') + name)
        rot += not gruen
    return 1 if rot else 0


if __name__ == '__main__':
    sys.exit(main())
