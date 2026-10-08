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
  - eine Datei mehr Hunks (ohne Kontext gezaehlt) hat als erlaubt,
  - ein Hunk einer Kieselsport-Stelle den Schalter CONFIG_BOULDER_KIESELSPORT nicht nennt,
  - eine eigene Datei in Pebbles Ordnern (EIGENE_DATEIEN) Pebbles Zeilen aendert, kein
    "Boulder:" traegt oder als .c den Schalter nicht nennt,
  - der Remote `upstream` fehlt.
Die eigenen Ordner der Variante und von Kieselsport sind frei.
"""
import collections
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
    # Kieselsport (Schritt 2): Dienst und Header liegen in den Ordnern der Variante (sport/).
    'fw/apps/system/kieselsport/',
    'tests/fw/apps/system_apps/kieselsport/',
)

# Eigene Dateien in Pebbles Ordnern: ganz von Boulder, neu (upstream gibt es sie nicht). Die Kachel
# im Startmenue (P6) steht neben Pebbles Kacheln, weil sie deren Koepfe braucht; die .c baut jedes
# Board und muss darum ganz unter dem Schalter stehen.
EIGENE_DATEIEN = (
    'fw/apps/system/launcher/default/app_glance_kieselsport.c',
    'fw/apps/system/launcher/default/app_glance_kieselsport.h',
)

# Pebble-Datei: hoechstens geaenderte Zeilen, Funktion der Hunks als Regex, hoechstens so
# viele Hunks (ohne Kontext), und ob jeder Hunk den Schalter nennen muss.
# Die Liste folgt den Entscheidungen (entscheidungen.md; Schritt 1: Spezifikation 2.1, F2, F3;
# Schritt 2: Spezifikation 2.3, P1-P6).
Stelle = collections.namedtuple('Stelle', 'zeilen funktion hunks schalter',
                                defaults=(None, None, None))
SCHALTER = 'CONFIG_BOULDER_KIESELSPORT'

PEBBLE = {
    'fw/services/activity/Kconfig': Stelle(16),                      # E1 Algorithmus-Wahl
    'fw/services/activity/CMakeLists.txt': Stelle(6),                # E2 Quellen
    'fw/services/blob_db/settings_blob_db.c': Stelle(1),             # E3 Whitelist Messstufe
    # E4 Weckregel, dazu das Nachholen nach einem Neustart (Entscheid 07.10.2026, wecklauf.h).
    # Nach einem Boot fragt alarm.c die Weckregel nie, deshalb kamen mit c1c7b2eb0
    # prv_record_alarm_op (geklungen) und alarm_init (Nachholen) dazu. Vom Nutzer freigegeben am
    # 08.10.2026 (Entscheid 1 = A): diese drei Funktionen, zusammen hoechstens 20 Zeilen.
    'fw/services/alarms/alarm.c': Stelle(
        20, r'prv_should_smart_alarm_trigger\(|prv_record_alarm_op\(|alarm_init\('),
    'fw/services/hrm/hrm_manager.c': Stelle(10),                     # E5 HRV-Fix (F2)
    'fw/apps/system/settings/health.c': Stelle(40),                  # F3 Menue Messstufe
    # Kieselsport. Der Schalter haelt die vier Boards ohne Variante bei Pebble (Spezifikation G1).
    # P1 Pulskalorien (Start, Pulswert, Schritt-kcal ohne Puls, Stopp) und Verwerfen (F3 = A).
    'fw/services/activity/workout_service.c': Stelle(
        24, r'workout_service_start_workout\(|prv_handle_heart_rate_update\('
            r'|prv_handle_movement_update\(|workout_service_stop_workout\(', schalter=True),
    # P2 Workout-App ausgeblendet: eine Stelle im Prozess-Eintrag.
    'fw/apps/system/workout/workout.c': Stelle(
        4, r'workout_app_get_info\(', hunks=1, schalter=True),
    # P3 Registry-Eintrag (JSON ohne Kommentare: "Boulder:" steht im Schluessel "kommentar").
    'fw/shell/normal/system_app_registry_list.json': Stelle(10, hunks=1, schalter=True),
    # P4 DLS 84 Fassung 7 mit `manual` (F4 = A).
    'include/pbl/services/activity/activity_private.h': Stelle(10, schalter=True),
    'fw/services/activity/activity_sessions.c': Stelle(
        4, r'activity_sessions_prv_send_activity_session_to_data_logging\(', hunks=1,
        schalter=True),
    # P5 Kieselsport rueckt im Training im Startmenue nach oben (Entscheid 3 vom 08.10.2026): eine
    # Zeile in der Liste s_override_table, dem einzigen "static const struct {" der Datei.
    'fw/process_management/app_menu_data_source.c': Stelle(
        3, r'^static const struct \{$', hunks=1, schalter=True),
    # P6 die Kachel mit der Laufzeit: Include und ein Eintrag in der Liste der System-Kacheln.
    'fw/apps/system/launcher/default/app_glance_service.c': Stelle(
        8, r'prv_load_glance_for_node\(', hunks=2, schalter=True),
}

# Unversioniertes, das nicht zum Quellbaum gehoert (Bauordner im Worktree).
UNVERSIONIERT_FREI = re.compile(r'^build[^/]*/')

HUNK = re.compile(r'^@@ -\d+(?:,\d+)? \+\d+(?:,\d+)? @@ ?(.*)$')


def eigen(pfad):
    return pfad.startswith(EIGENE_ORDNER)


def pruefe_eigene_datei(pfad, hunks):
    """Eine eigene Datei in Pebbles Ordner: nur neue Zeilen, "Boulder:", als .c mit Schalter."""
    fehler = []
    zeilen = [x for _, z in hunks for x in z]
    if any(x.startswith('-') for x in zeilen):
        fehler.append(f'{pfad}: eigene Datei aendert Zeilen, die es schon gab')
    if not any('Boulder:' in x for x in zeilen):
        fehler.append(f'{pfad}: eigene Datei ohne "Boulder:"')
    if pfad.endswith('.c') and not any(x.startswith('+#ifdef ' + SCHALTER) for x in zeilen):
        fehler.append(f'{pfad}: eigene Datei nicht unter #ifdef {SCHALTER}')
    return fehler


def als_neue_datei(pfad, text):
    """Eine unversionierte Datei als Diff gegen nichts - so prueft der Waechter sie wie committet."""
    zeilen = text.splitlines()
    return (f'diff --git a/{pfad} b/{pfad}\n--- /dev/null\n+++ b/{pfad}\n'
            f'@@ -0,0 +1,{len(zeilen)} @@\n' + ''.join('+' + z + '\n' for z in zeilen))


def nur_includes(zeilen):
    """Nur Includes und Kommentare geaendert: an keine Funktion und keinen Schalter gebunden.

    Ein #ifdef um ein Include zaehlt mit; ohne Include waere ein #if 0 frei.
    """
    geaendert = [z[1:].strip() for z in zeilen if z[:1] in '+-']
    bedingt = ('#if', '#else', '#elif', '#endif')
    return (all(z.startswith(('#include', '/*', '//') + bedingt) for z in geaendert) and
            (not any(z.startswith(bedingt) for z in geaendert) or
             any(z.startswith('#include') for z in geaendert)))


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
        if pfad in EIGENE_DATEIEN:
            fehler += pruefe_eigene_datei(pfad, hunks)
            continue
        if pfad not in PEBBLE:
            fehler.append(f'{pfad}: Pebble-Datei ausserhalb der erlaubten Stellen geaendert')
            continue
        stelle = PEBBLE[pfad]
        plus = sum(1 for _, z in hunks for x in z if x.startswith('+'))
        minus = sum(1 for _, z in hunks for x in z if x.startswith('-'))
        if max(plus, minus) > stelle.zeilen:
            fehler.append(f'{pfad}: {plus} Zeilen neu, {minus} weg - hoechstens {stelle.zeilen}')
        for kopf, zeilen in hunks:
            if not any('Boulder:' in z for z in zeilen):
                fehler.append(f'{pfad}: Hunk "{kopf}" ohne "Boulder:"')
            # Mit Kontext: ein #endif zwei Zeilen unter dem eigenen #ifdef gehoert dazu.
            if stelle.schalter and not nur_includes(zeilen) and not any(SCHALTER in z for z in zeilen):
                fehler.append(f'{pfad}: Hunk "{kopf}" ohne {SCHALTER}')
        u0 = ohne_kontext.get(pfad, [])
        if stelle.hunks is not None and len(u0) > stelle.hunks:
            fehler.append(f'{pfad}: {len(u0)} Hunks - hoechstens {stelle.hunks}')
        for kopf, zeilen in u0:
            if stelle.funktion and not nur_includes(zeilen) and not re.search(stelle.funktion, kopf):
                fehler.append(f'{pfad}: Hunk in "{kopf}", erlaubt nur {stelle.funktion}')
    for pfad in unversioniert:
        if not eigen(pfad) and pfad not in EIGENE_DATEIEN and not UNVERSIONIERT_FREI.match(pfad):
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
    # Eine eigene Datei, noch nicht im Index: git diff zeigt sie nicht, geprueft wird sie trotzdem.
    for pfad in unversioniert:
        if pfad in EIGENE_DATEIEN:
            neu = als_neue_datei(pfad, (pathlib.Path(repo) / pfad).read_text())
            diff += neu
            diff_u0 += neu
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
