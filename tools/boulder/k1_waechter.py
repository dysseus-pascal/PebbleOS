#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0
"""Boulder: Waechter fuer Abnahmekriterium 1 - Pebble-Code nur an den erlaubten Stellen.

Aufruf: k1_waechter.py <repo>

Liest `git diff <merge-base HEAD upstream/main>` mit dem Arbeitsbaum (auch
Uncommittetes) und die unversionierten Dateien. Die Funktion eines Hunks kommt aus
`git diff -U0`: Ohne Kontextzeilen nennt der Kopf die Funktion der ersten geaenderten
Zeile, mit Kontext die Funktion drei Zeilen davor. Rot (Rueckgabe 1), wenn
  - eine Pebble-Datei ausserhalb der Liste unten geaendert ist,
  - eine erlaubte Pebble-Datei mehr Zeilen aendert als ihr Hoechstwert,
  - ein Hunk in einer Pebble-Datei kein "Boulder:" traegt,
  - ein Hunk in einer anderen Funktion liegt als erlaubt,
  - der Remote `upstream` fehlt.
Die eigenen Ordner der Variante sind frei.
"""
import pathlib
import re
import subprocess
import sys

EIGENE_ORDNER = (
    'fw/services/activity/boulder/',
    'include/pbl/services/activity/boulder/',
    'tests/fw/services/activity/boulder/',
    'tests/fixtures/activity/boulder/',
    'tools/boulder/',
)

# Pebble-Datei: (hoechstens geaenderte Zeilen, Funktion der Hunks als Regex oder None).
# Die Liste folgt den Entscheidungen (entscheidungen.md, Spezifikation 2.1, F2, F3).
PEBBLE = {
    'fw/services/activity/Kconfig': (16, None),                      # E1 Algorithmus-Wahl
    'fw/services/activity/CMakeLists.txt': (6, None),                # E2 Quellen
    'fw/services/blob_db/settings_blob_db.c': (1, None),             # E3 Whitelist Messstufe
    # E4 Weckregel, dazu das Nachholen nach einem Neustart (Entscheid 07.10.2026, wecklauf.h)
    'fw/services/alarms/alarm.c': (
        20, r'prv_should_smart_alarm_trigger\(|prv_record_alarm_op\(|alarm_init\('),
    'fw/services/hrm/hrm_manager.c': (10, None),                     # E5 HRV-Fix (F2)
    'fw/apps/system/settings/health.c': (40, None),                  # F3 Menue Messstufe
}

# Unversioniertes, das nicht zum Quellbaum gehoert (Bauordner im Worktree).
UNVERSIONIERT_FREI = re.compile(r'^build[^/]*/')

HUNK = re.compile(r'^@@ -\d+(?:,\d+)? \+\d+(?:,\d+)? @@ ?(.*)$')


def eigen(pfad):
    return pfad.startswith(EIGENE_ORDNER)


def dateien(diff):
    """{pfad: [hunk, ...]}, ein Hunk = (kopf, [zeilen])."""
    aus, pfad, hunk = {}, None, None
    for zeile in diff.splitlines():
        if zeile.startswith('diff --git '):
            pfad = zeile.split(' b/', 1)[1]
            aus.setdefault(pfad, [])
            hunk = None
        elif zeile.startswith(('--- ', '+++ ')) and hunk is None:
            continue
        elif HUNK.match(zeile):
            hunk = (HUNK.match(zeile).group(1), [])
            aus[pfad].append(hunk)
        elif hunk is not None:
            hunk[1].append(zeile)
    return aus


def pruefe_diff(diff, unversioniert, diff_u0=None):
    """`diff_u0`: derselbe Diff ohne Kontext, fuer die Funktion der Hunks (sonst `diff`)."""
    fehler = []
    ohne_kontext = dateien(diff_u0 if diff_u0 is not None else diff)
    for pfad, hunks in dateien(diff).items():
        if eigen(pfad):
            continue
        if pfad not in PEBBLE:
            fehler.append(f'{pfad}: Pebble-Datei ausserhalb der erlaubten Stellen geaendert')
            continue
        hoechstens, funktion = PEBBLE[pfad]
        plus = sum(1 for _, z in hunks for x in z if x.startswith('+'))
        minus = sum(1 for _, z in hunks for x in z if x.startswith('-'))
        if max(plus, minus) > hoechstens:
            fehler.append(f'{pfad}: {plus} Zeilen neu, {minus} weg - hoechstens {hoechstens}')
        for kopf, zeilen in hunks:
            if not any('Boulder:' in z for z in zeilen):
                fehler.append(f'{pfad}: Hunk "{kopf}" ohne "Boulder:"')
        for kopf, zeilen in ohne_kontext.get(pfad, []):
            nur_includes = all(z[1:].startswith('#include') or z[1:].strip().startswith(('/*', '//'))
                               for z in zeilen if z[:1] in '+-')
            if funktion and not nur_includes and not re.search(funktion, kopf):
                fehler.append(f'{pfad}: Hunk in "{kopf}", erlaubt nur {funktion}')
    for pfad in unversioniert:
        if not eigen(pfad) and not UNVERSIONIERT_FREI.match(pfad):
            fehler.append(f'{pfad}: neue Datei ausserhalb der eigenen Ordner')
    return fehler


def git(repo, *args):
    return subprocess.run(['git', '-C', str(repo), *args], check=True, capture_output=True,
                          text=True).stdout


def pruefe(repo):
    if 'upstream' not in git(repo, 'remote').split():
        return ['Remote "upstream" fehlt (git remote add upstream '
                'https://github.com/coredevices/PebbleOS.git)']
    basis = git(repo, 'merge-base', 'HEAD', 'upstream/main').strip()
    diff = git(repo, 'diff', basis)
    diff_u0 = git(repo, 'diff', '-U0', basis)
    unversioniert = [z[3:] for z in git(repo, 'status', '--porcelain', '-z').split('\0')
                     if z.startswith('?? ')]
    return pruefe_diff(diff, unversioniert, diff_u0)


def main(argv):
    if len(argv) != 2:
        print(__doc__, file=sys.stderr)
        return 2
    try:
        fehler = pruefe(pathlib.Path(argv[1]))
    except (OSError, subprocess.CalledProcessError) as e:
        fehler = [f'{type(e).__name__}: {e}']
    if fehler:
        print('Kriterium 1 verletzt (tools/boulder/k1_waechter.py):')
        for f in fehler:
            print('  ' + f)
        return 1
    print('Kriterium 1: Pebble-Code nur an den erlaubten Stellen.')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
