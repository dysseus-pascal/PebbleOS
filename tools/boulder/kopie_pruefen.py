#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0
"""Boulder: Pflege der Kraepelin-Kopie (fw/services/activity/boulder/HERKUNFT).

Aufruf:
  kopie_pruefen.py <repo>                      Kopie gegen das Original zum Commit
  kopie_pruefen.py <repo> --nachziehen [ref]   Original zum Commit gegen ref
                                               (Vorgabe upstream/main)

Pruefen (rot = Rueckgabe 1):
  - Das Original zum Commit aus HERKUNFT hat die dort genannte SHA-256.
  - Jede geaenderte, hinzugefuegte oder geloeschte Zeile der Kopie liegt in einem
    Block "Boulder: Dn" ... "Boulder: Ende Dn", und Dn steht in HERKUNFT.

Nachziehen: rot, sobald sich ein Original oder eine Datei, auf die sich die
Variante stuetzt, upstream geaendert hat; der Diff steht in der Ausgabe. Dann:
git merge-file <kopie> <original alt> <original neu>, HERKUNFT nachfuehren, die
Tests der Variante laufen lassen.
"""
import difflib
import hashlib
import pathlib
import re
import subprocess
import sys

HERKUNFT = 'fw/services/activity/boulder/HERKUNFT'

# Dateien, auf deren Verhalten sich die Variante stuetzt (Spezifikation 2.3).
VERTRAGSDATEIEN = [
    'fw/services/activity/activity.c',
    'fw/services/activity/activity_sessions.c',
    'fw/services/activity/activity_metrics.c',
    'fw/services/activity/workout_service.c',
    'fw/services/hrm/hrm_manager.c',
    'fw/services/alarms/alarm.c',
    'fw/services/blob_db/settings_blob_db.c',
    'include/pbl/services/activity/activity_algorithm.h',
]

ANFANG = re.compile(r'Boulder: (D\d+)\b')
ENDE = re.compile(r'Boulder: Ende (D\d+)\b')


def lies_herkunft(text):
    commit, kopien, abweichungen = None, [], set()
    for zeile in text.splitlines():
        teile = zeile.split()
        if not teile or teile[0].startswith('#'):
            continue
        if teile[0] == 'commit' and len(teile) == 2:
            commit = teile[1]
        elif teile[0] == 'kopie' and len(teile) == 4:
            kopien.append(tuple(teile[1:]))
        elif teile[0] == 'abweichung' and len(teile) >= 2:
            abweichungen.add(teile[1])
        else:
            raise ValueError(f'HERKUNFT: Zeile nicht lesbar: {zeile!r}')
    if not commit or not kopien:
        raise ValueError('HERKUNFT: commit oder kopie fehlt')
    return commit, kopien, abweichungen


def bloecke(zeilen, abweichungen, name):
    """Je Zeile der Kopie: liegt sie in einem markierten Block (Marken eingeschlossen)?"""
    fehler, drin, offen = [], [], None
    for nr, zeile in enumerate(zeilen, 1):
        ende = ENDE.search(zeile)
        anfang = None if ende else ANFANG.search(zeile)
        if anfang:
            if offen:
                fehler.append(f'{name}:{nr}: Block {anfang.group(1)} in offenem Block {offen}')
            offen = anfang.group(1)
            if offen not in abweichungen:
                fehler.append(f'{name}:{nr}: {offen} steht nicht in HERKUNFT')
            drin.append(True)
        elif ende:
            if ende.group(1) != offen:
                fehler.append(f'{name}:{nr}: Ende {ende.group(1)} ohne passenden Anfang')
            drin.append(True)
            offen = None
        else:
            drin.append(offen is not None)
    if offen:
        fehler.append(f'{name}: Block {offen} nicht geschlossen')
    return drin, fehler


def pruefe_text(original, kopie, abweichungen, name='kopie'):
    """Fehlerliste fuer eine Kopie; leer, wenn jede Abweichung markiert ist."""
    a, b = original.splitlines(), kopie.splitlines()
    drin, fehler = bloecke(b, abweichungen, name)
    matcher = difflib.SequenceMatcher(None, a, b, autojunk=False)
    for art, i1, i2, j1, j2 in matcher.get_opcodes():
        if art == 'equal':
            continue
        if j2 > j1:
            for j in range(j1, j2):
                if not drin[j]:
                    fehler.append(f'{name}:{j + 1}: Abweichung ohne Boulder-Block: {b[j]!r}')
        else:
            # Geloeschte Zeilen: die Stelle muss zwischen zwei Zeilen eines Blocks liegen.
            vor = drin[j1 - 1] if j1 > 0 else False
            nach = drin[j1] if j1 < len(drin) else False
            if not (vor and nach):
                fehler.append(f'{name}:{j1 + 1}: {i2 - i1} Zeile(n) des Originals ohne '
                              f'Boulder-Block geloescht: {a[i1]!r}')
    return fehler


def git(repo, *args):
    return subprocess.run(['git', '-C', str(repo), *args], check=True,
                          capture_output=True).stdout


def pruefe(repo, herkunft=None):
    if herkunft is None:
        herkunft = (repo / HERKUNFT).read_text(encoding='utf-8')
    commit, kopien, abweichungen = lies_herkunft(herkunft)
    fehler = []
    for kopie, original, sha in kopien:
        alt = git(repo, 'show', f'{commit}:{original}')
        if hashlib.sha256(alt).hexdigest() != sha:
            fehler.append(f'{original}@{commit[:9]}: SHA-256 stimmt nicht mit HERKUNFT')
            continue
        fehler += pruefe_text(alt.decode('utf-8'), (repo / kopie).read_text(encoding='utf-8'),
                              abweichungen, kopie)
    return fehler


def nachziehen(repo, ref):
    commit, kopien, _ = lies_herkunft((repo / HERKUNFT).read_text(encoding='utf-8'))
    pfade = [o for _, o, _ in kopien] + VERTRAGSDATEIEN
    diff = git(repo, 'diff', commit, ref, '--', *pfade).decode('utf-8', 'replace')
    return diff


def main(argv):
    if len(argv) < 2:
        print(__doc__, file=sys.stderr)
        return 2
    repo = pathlib.Path(argv[1])
    try:
        if len(argv) >= 3 and argv[2] == '--nachziehen':
            ref = argv[3] if len(argv) > 3 else 'upstream/main'
            diff = nachziehen(repo, ref)
            if diff:
                print(f'Upstream hat das Original oder einen Vertrag geaendert ({ref}):')
                print(diff)
                return 1
            print(f'Original und Vertragsdateien unveraendert bis {ref}.')
            return 0
        fehler = pruefe(repo)
    except (OSError, ValueError, subprocess.CalledProcessError) as e:
        fehler = [f'{type(e).__name__}: {e}']
    if fehler:
        print('Kraepelin-Kopie nicht in Ordnung (tools/boulder/kopie_pruefen.py):')
        for f in fehler:
            print('  ' + f)
        return 1
    print('Kraepelin-Kopie: alle Abweichungen markiert, Originale wie in HERKUNFT.')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
