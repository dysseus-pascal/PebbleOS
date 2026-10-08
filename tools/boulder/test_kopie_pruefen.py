#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0
"""Boulder: Gegenproben fuer kopie_pruefen.py und k1_waechter.py (ctest).

Jede Probe muss den Waechter rot faerben; die markierte Fassung gruen.
"""
import pathlib
import subprocess
import sys
import tempfile

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


def rot(fehler, wort):
    """Rot aus dem erwarteten Grund: eine Meldung enthaelt `wort`."""
    return any(wort in f for f in fehler)


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
    yield from proben_health(pruefe)


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
    # Ein Include ohne Schalter zieht den Kopf auf jedem Board herein (Befund nach a75d3156a).
    yield 'P1 Include ohne Schalter', rot(pruefe(
        hunk(DIENST, ['+#include "pbl/services/activity/boulder/kalorien.h"  // Boulder: P1']), []),
        'ausserhalb von #ifdef')
    yield 'P1 Include mit Schalter', not pruefe(
        hunk(DIENST, ['+#ifdef CONFIG_BOULDER_KIESELSPORT',
                      '+#include "pbl/services/activity/boulder/kalorien.h"  // Boulder: P1',
                      '+#endif']), [])
    yield 'P1 #if 0 ausserhalb', pruefe(hunk(DIENST, ['+#ifdef CONFIG_BOULDER_KIESELSPORT  // Boulder: x',
                                                      '+#if 0', '+#endif', '+#endif']), [])
    yield 'P1 in Pause', pruefe(hunk(DIENST, P1, 'bool workout_service_pause_workout(bool should_be_paused) {'),
                                [])
    yield 'P1 ohne Schalter', pruefe(hunk(DIENST, ['+  boulder_x();  // Boulder: P1'], STOPP), [])
    yield 'P1 Schalter nur im Kommentar', rot(pruefe(hunk(
        DIENST, ['+  boulder_x();  // Boulder: P1 wie CONFIG_BOULDER_KIESELSPORT'], STOPP), []),
        'ausserhalb von #ifdef')
    yield 'P1 Zeile nach dem #endif', rot(pruefe(hunk(DIENST, P1 + ['+  boulder_y();'], STOPP), []),
                                          'ausserhalb von #ifdef')
    # Pebbles Zeile bleibt im #else stehen (Schritt-kcal, F5): so ist es richtig ...
    bewegung = ['+#ifdef CONFIG_BOULDER_KIESELSPORT  // Boulder: Schritt-kcal',
                '+    kcal += boulder_kalorien_schritte(kcal_neu);', '+#else',
                '     kcal += kcal_neu;', '+#endif', '   }']
    yield 'P1 Pebbles Zeile im #else', not pruefe(hunk(DIENST, bewegung, STOPP), [])
    # ... geaendert oder neu im #else aendert sie jedes andere Board.
    geaendert = bewegung[:3] + ['-    kcal += kcal_neu;', '+    kcal += 2 * kcal_neu;'] + bewegung[4:]
    yield 'P1 Zeile im #else geaendert', rot(pruefe(hunk(DIENST, geaendert, STOPP), []),
                                             'ausserhalb von #ifdef')
    yield 'P1 neue Zeile im #else', rot(pruefe(hunk(DIENST, P1[:2] + ['+#else', '+  pebble_y();'] + P1[2:],
                                                    STOPP), []), 'ausserhalb von #ifdef')
    yield 'P1 Pebbles Zeile entfernt', rot(pruefe(hunk(DIENST, P1 + ['-  save_session = true;'], STOPP),
                                                  []), 'ausserhalb von #ifdef')
    yield 'P1 #if 0 ohne Schalter', rot(pruefe(hunk(DIENST, ['+#if 0  // Boulder: x', '   alt();', '+#endif'],
                                                    STOPP), []), 'ausserhalb von #ifdef')
    yield 'P1 Kommentar vor dem #ifdef', not pruefe(hunk(DIENST, ['+  // Boulder: P1, warum'] + P1, STOPP), [])
    yield 'P1 zu viele Zeilen', pruefe(hunk(DIENST, P1 * 9, STOPP), [])
    yield 'P3 Registry', not pruefe(hunk(REGISTRY, P3), [])
    yield 'P3 ohne Schalter', rot(pruefe(hunk(REGISTRY, [z for z in P3 if 'ifdefs' not in z]), []),
                                  'ohne CONFIG_BOULDER_KIESELSPORT')
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
    yield from proben_startmenue(pruefe)


# Schritt 2, P5/P6 und die Kacheldatei (Entscheid 3 vom 08.10.2026).
MENUE = 'fw/process_management/app_menu_data_source.c'
LISTE = 'static const struct {'
P5 = ['+#ifdef CONFIG_BOULDER_KIESELSPORT  // Boulder: P5', '+  {APP_ID_KIESELSPORT, true},', '+#endif']
GLANCE = 'fw/apps/system/launcher/default/app_glance_service.c'
LADEN = 'static LauncherAppGlance *prv_load_glance_for_node(const AppMenuNode *node,'
P6_INCLUDE = ['+#ifdef CONFIG_BOULDER_KIESELSPORT  // Boulder: P6', '+#include "app_glance_kieselsport.h"',
              '+#endif']
P6 = ['+#ifdef CONFIG_BOULDER_KIESELSPORT  // Boulder: P6',
      '+    {.uuid = KIESELSPORT_UUID_INIT, .constructor = launcher_app_glance_kieselsport_create},',
      '+#endif']
KACHEL = 'fw/apps/system/launcher/default/app_glance_kieselsport.c'
KACHEL_TEXT = '// Boulder: P6\n#ifdef CONFIG_BOULDER_KIESELSPORT\nint x;\n#endif\n'


def proben_startmenue(pruefe):
    yield 'P5 in der Liste', not pruefe(hunk(MENUE, P5, LISTE), [])
    yield 'P5 ohne Schalter', pruefe(hunk(MENUE, ['+  {APP_ID_KIESELSPORT, true},  // Boulder: P5'], LISTE), [])
    yield 'P5 zu viele Zeilen', pruefe(hunk(MENUE, P5 + ['+  {APP_ID_X, true},'], LISTE), [])
    yield 'P5 in einer Funktion', pruefe(hunk(MENUE, P5, 'static int prv_override_index(AppInstallId app_id) {'),
                                         [])
    zwei = hunks(MENUE, (LISTE, P5), (LISTE, ['+// Boulder: zweiter Hunk']))
    yield 'P5 zweiter Hunk', pruefe(zwei, [], zwei)
    # So zeigt git P5 wirklich: das eigene #endif vor dem eigenen #ifdef, Pebbles #endif danach.
    verschoben = ['  {APP_ID_GOLF, false},', ' #ifdef APP_ID_WORKOUT', '   {APP_ID_WORKOUT, true},',
                  '+#endif', '+#ifdef CONFIG_BOULDER_KIESELSPORT  // Boulder: P5',
                  '+  {APP_ID_KIESELSPORT, true},', ' #endif', '   {APP_ID_MUSIC, true},']
    yield 'P5 wie git es zeigt', not pruefe(hunk(MENUE, verschoben, LISTE), [])
    yield 'P5 Zeile nach dem #endif', rot(pruefe(hunk(MENUE, verschoben[:-1] + ['+  {APP_ID_GOLF, true},'] +
                                                      verschoben[-1:], LISTE), []), 'ausserhalb von #ifdef')
    beide = hunks(GLANCE, ('', P6_INCLUDE), (LADEN, P6))
    yield 'P6 Include und Eintrag', not pruefe(beide, [], beide)
    yield 'P6 in anderer Funktion', pruefe(hunk(GLANCE, P6, 'void launcher_app_glance_service_init('), [])
    yield 'P6 ohne Schalter', pruefe(hunk(GLANCE, ['+    {.uuid = X},  // Boulder: P6'], LADEN), [])
    # Das Include ohne Schalter: eine Zeile statt drei, der Kopf kaeme auf jedes Board.
    ohne = hunks(GLANCE, ('', ['+#include "app_glance_kieselsport.h"  // Boulder: P6']), (LADEN, P6))
    yield 'P6 Include ohne Schalter', rot(pruefe(ohne, [], ohne), 'ausserhalb von #ifdef')
    vorher = hunks(GLANCE, ('', ['+#include "app_glance_kieselsport.h"  // Boulder: P6',
                                 '+#ifdef CONFIG_BOULDER_KIESELSPORT', '+#endif']), (LADEN, P6))
    yield 'P6 Include vor dem #ifdef', rot(pruefe(vorher, [], vorher), 'ausserhalb von #ifdef')
    # Ein weiterer Eintrag in der Liste: unter dem Schalter, aber ueber der Grenze (6 Zeilen).
    mehr = hunks(GLANCE, ('', P6_INCLUDE), (LADEN, P6[:2] + P6[1:]))
    yield 'P6 weiterer Eintrag', rot(pruefe(mehr, [], mehr), 'hoechstens 6')
    # Drei Hunks mit 5 Zeilen: nur die Zahl der Hunks ist zu hoch.
    drei = hunks(GLANCE, ('', P6_INCLUDE), (LADEN, ['+// Boulder: a']), (LADEN, ['+// Boulder: b']))
    yield 'P6 dritter Hunk', rot(pruefe(drei, [], drei), '3 Hunks')
    # Die Kacheldatei: neu, ganz Boulder, unter dem Schalter - committet oder noch unversioniert.
    neu = k1_waechter.als_neue_datei(KACHEL, KACHEL_TEXT)
    yield 'Kacheldatei', not pruefe(neu, [])
    yield 'Kacheldatei unversioniert erlaubt', not pruefe('', [KACHEL])
    yield 'Kacheldatei ohne Schalter', pruefe(k1_waechter.als_neue_datei(KACHEL, '// Boulder: P6\nint x;\n'), [])
    # Ganz unter dem Schalter: fw/apps/system/ baut jede .c, Code davor oder danach jedes Board.
    for name, text in (('Zeile vor dem #ifdef', 'int y;\n' + KACHEL_TEXT),
                       ('Zeile nach dem #endif', KACHEL_TEXT + 'int y;\n'),
                       ('Zeile im #else', KACHEL_TEXT.replace('#endif', '#else\nint y;\n#endif')),
                       ('Code hinter einem Kommentar', '/* SPDX */ int y;\n' + KACHEL_TEXT)):
        yield f'Kacheldatei {name}', rot(pruefe(k1_waechter.als_neue_datei(KACHEL, text), []),
                                         'nicht ganz unter')
    kommentiert = ('/* SPDX-FileCopyrightText: x */\n/*\n * int nicht_code;\n */\n' + KACHEL_TEXT +
                   '// nach dem Schalter nur Kommentar\n')
    yield 'Kacheldatei mit Kommentaren aussen', not pruefe(k1_waechter.als_neue_datei(KACHEL, kommentiert), [])
    yield 'Kacheldatei ohne Marke', pruefe(
        k1_waechter.als_neue_datei(KACHEL, '#ifdef CONFIG_BOULDER_KIESELSPORT\nint x;\n#endif\n'), [])
    yield 'Kacheldatei aendert Pebbles Zeilen', pruefe(hunk(KACHEL, ['-alt', '+// Boulder: P6',
                                                                     '+#ifdef CONFIG_BOULDER_KIESELSPORT']), [])
    yield 'andere neue Datei im Startmenue', pruefe('', ['fw/apps/system/launcher/default/app_glance_x.c'])
    yield from proben_arbeitsbaum()


# Schritt 3, P7: Boulders Health-App auf emery (Spezifikation 4.1, 7).
CMAKE_APPS = 'fw/apps/system/CMakeLists.txt'
FILTER = '+    list(FILTER source EXCLUDE REGEX "/apps/system/health/")'
P7 = ['+  # Boulder: P7 eigene Health-App auf emery', '+  if(CONFIG_BOULDER_HEALTH_APP)', FILTER,
      '+  endif()']
# So zeigt git P7: Pebbles else()-Zweig als Kontext.
P7_KONTEXT = ['   file(GLOB_RECURSE source CONFIGURE_DEPENDS *.c)'] + P7 + [' endif()', ' pbl_library()']


def proben_health(pruefe):
    yield 'P7 im else()-Zweig', not pruefe(hunk(CMAKE_APPS, P7_KONTEXT), [])
    yield 'P7 mit Leerzeichen und Kommentar am Ende', not pruefe(hunk(CMAKE_APPS, [
        '+  # Boulder: P7', '+  if (CONFIG_BOULDER_HEALTH_APP)', FILTER + '  # Pebbles Ordner', '+  endif ()']), [])
    # Eine zweite Zeile im Block, in 4 Zeilen ("Boulder:" am if): rot nur wegen des Inhalts.
    zweite = ['+  if(CONFIG_BOULDER_HEALTH_APP)  # Boulder: P7', FILTER,
              '+    list(FILTER source EXCLUDE REGEX "/apps/system/settings/")', '+  endif()']
    yield 'P7 zweite Zeile', rot(pruefe(hunk(CMAKE_APPS, zweite), []), 'erlaubt ist genau')
    yield 'P7 zweite Zeile nach dem endif()', rot(pruefe(hunk(CMAKE_APPS, P7[1:] + [
        '+  set(x y)  # Boulder: P7']), []), 'erlaubt ist genau')
    yield 'P7 ohne Schalter', rot(pruefe(hunk(CMAKE_APPS, ['+  # Boulder: P7', FILTER]), []),
                                  'erlaubt ist genau')
    yield 'P7 anderer Schalter', rot(pruefe(hunk(CMAKE_APPS, [P7[0], '+  if(CONFIG_BOULDER_KIESELSPORT)',
                                                                FILTER, P7[3]]), []), 'erlaubt ist genau')
    yield 'P7 Schalter nur im Kommentar', rot(pruefe(hunk(CMAKE_APPS, [
        '+  # Boulder: P7 if(CONFIG_BOULDER_HEALTH_APP)', FILTER]), []), 'erlaubt ist genau')
    # "health" allein naehme settings/health.c und boulder_health/ mit: jeder andere Ausdruck ist rot.
    yield 'P7 anderer Ausdruck', rot(pruefe(hunk(CMAKE_APPS, P7[:2] + [
        '+    list(FILTER source EXCLUDE REGEX "health")'] + P7[3:]), []), 'erlaubt ist genau')
    yield 'P7 entfernt Pebbles Zeile', rot(pruefe(hunk(CMAKE_APPS, [
        '-  file(GLOB_RECURSE source CONFIGURE_DEPENDS *.c)'] + P7), []), 'entfernt Pebbles Zeile')
    yield 'P7 ohne Marke', rot(pruefe(hunk(CMAKE_APPS, P7[1:]), []), 'ohne "Boulder:"')
    # Ein weiterer Kommentar: der Block stimmt, nur die Grenze von 4 Zeilen nicht.
    yield 'P7 zu viele Zeilen', rot(pruefe(hunk(CMAKE_APPS, P7[:1] + ['+  # warum'] + P7[1:]), []),
                                    'hoechstens 4')
    zwei = hunks(CMAKE_APPS, ('', P7), ('', P7))
    yield 'P7 zweiter Hunk', rot(pruefe(zwei, [], zwei), '2 Hunks')
    yield 'Zeile in Pebbles Health-Ordner', rot(pruefe(hunk(
        'fw/apps/system/health/health.c', ['+  // Boulder: x']), []), 'ausserhalb der erlaubten Stellen')
    yield 'neue Datei in Pebbles Health-Ordner', rot(pruefe('', ['fw/apps/system/health/boulder.c']),
                                                     'ausserhalb der eigenen Ordner')
    yield 'Ordner der Health-App frei', not pruefe('', ['fw/apps/system/boulder_health/karten.c'])
    yield 'Testordner der Health-App frei', not pruefe(
        '', ['tests/fw/apps/system_apps/boulder_health/test_bh_app.c'])
    yield 'Pebbles Health-Tests nicht frei', pruefe('', ['tests/fw/apps/system_apps/health/test_x.c'])


def arbeitsbaum(unversioniert):
    """pruefe() an einem echten Git-Repo, upstream/main = HEAD, dazu unversionierte Dateien.

    Pebbles Kachel-Ordner ist versioniert, sonst zeigte git status nur "fw/".
    """
    with tempfile.TemporaryDirectory() as tmp:
        repo = pathlib.Path(tmp)

        def git(*args):
            subprocess.run(['git', '-C', tmp, '-c', 'user.name=k1', '-c', 'user.email=k1@test',
                            '-c', 'commit.gpgsign=false', '-c', 'core.hooksPath=/dev/null', *args],
                           check=True, capture_output=True)
        git('init', '-q')
        (repo / GLANCE).parent.mkdir(parents=True)
        (repo / GLANCE).write_text('// Pebble\n')
        git('add', '.')
        git('commit', '-q', '-m', 'Pebble')
        git('remote', 'add', 'upstream', tmp)
        git('update-ref', 'refs/remotes/upstream/main', 'HEAD')
        for pfad, text in unversioniert.items():
            (repo / pfad).write_text(text)
        return k1_waechter.pruefe(repo)


def proben_arbeitsbaum():
    # Eine Kacheldatei vor dem ersten git add: git diff zeigt sie nicht, ihr Inhalt zaehlt trotzdem.
    yield 'Arbeitsbaum: Kacheldatei unversioniert', not arbeitsbaum({KACHEL: KACHEL_TEXT})
    yield 'Arbeitsbaum: Kacheldatei unversioniert, Zeile vor dem #ifdef', rot(
        arbeitsbaum({KACHEL: 'int y;\n' + KACHEL_TEXT}), 'nicht ganz unter')
    yield 'Arbeitsbaum: Kacheldatei unversioniert ohne Marke', rot(
        arbeitsbaum({KACHEL: KACHEL_TEXT.replace('// Boulder: P6\n', '')}), 'ohne "Boulder:"')
    yield 'Arbeitsbaum: andere Datei unversioniert', rot(
        arbeitsbaum({'fw/apps/system/launcher/default/app_glance_x.c': KACHEL_TEXT}),
        'ausserhalb der eigenen Ordner')


def main():
    rot = 0
    for name, gruen in list(proben_kopie()) + list(proben_k1()):
        print(('ok    ' if gruen else 'FEHLT ') + name)
        rot += not gruen
    return 1 if rot else 0


if __name__ == '__main__':
    sys.exit(main())
