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
  - ein Hunk einer Kieselsport-Stelle Pebbles Code ausserhalb von
    #ifdef CONFIG_BOULDER_KIESELSPORT aendert (neue Zeile davor, danach oder im #else,
    entfernte Zeile; auch ein Include) - im Registry-JSON: den Schalter nicht nennt,
  - eine eigene Datei in Pebbles Ordnern (EIGENE_DATEIEN) Pebbles Zeilen aendert, kein
    "Boulder:" traegt oder als .c nicht ganz unter dem Schalter steht,
  - ein CMake-Hunk (P7) Pebbles Zeilen entfernt oder als Code etwas anderes bringt als genau
    if(<Schalter>), die erlaubten Zeilen und endif(),
  - der Remote `upstream` fehlt.
Die eigenen Ordner der Variante, von Kieselsport und der Health-App sind frei.
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
    # Boulders Health-App (Schritt 3); Pebbles fw/apps/system/health/ bleibt Pebble-Code.
    'fw/apps/system/boulder_health/',
    'tests/fw/apps/system_apps/boulder_health/',
    # Kein Firmware-Code: meldet tools/boulder/sitzung-start.sh als Start-Hook der Cloud-Sitzungen an.
    '.claude/settings.json',
)

# Eigene Dateien in Pebbles Ordnern: ganz von Boulder, neu (upstream gibt es sie nicht). Die Kachel
# im Startmenue (P6) steht neben Pebbles Kacheln, weil sie deren Koepfe braucht; die .c baut jedes
# Board und muss darum ganz unter dem Schalter stehen.
EIGENE_DATEIEN = (
    'fw/apps/system/launcher/default/app_glance_kieselsport.c',
    'fw/apps/system/launcher/default/app_glance_kieselsport.h',
)

# Pebble-Datei: hoechstens geaenderte Zeilen, Funktion der Hunks als Regex, hoechstens so
# viele Hunks (ohne Kontext), ob jeder Hunk den Schalter nennen muss, und fuer CMake der Block,
# den jeder Hunk genau bringen muss.
# Die Liste folgt den Entscheidungen (entscheidungen.md; Schritt 1: Spezifikation 2.1, F2, F3;
# Schritt 2: Spezifikation 2.3, P1-P6; Schritt 3: Spezifikation 7, P7).
Stelle = collections.namedtuple('Stelle', 'zeilen funktion hunks schalter cmake',
                                defaults=(None, None, None, None))
SCHALTER = 'CONFIG_BOULDER_KIESELSPORT'
# Ein CMake-Block: if(schalter), darin genau die Zeilen `inhalt`, endif().
CMakeBlock = collections.namedtuple('CMakeBlock', 'schalter inhalt')

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
    # P6 die Kachel mit der Laufzeit: Include und ein Eintrag in der Liste der System-Kacheln, je
    # #ifdef, Zeile, #endif. Die Grenze ist so eng wie bei P5, damit kein weiterer Eintrag durchgeht.
    'fw/apps/system/launcher/default/app_glance_service.c': Stelle(
        6, r'prv_load_glance_for_node\(', hunks=2, schalter=True),
    # P7 Boulders Health-App auf emery (Schritt 3): Sie traegt Pebbles health_app_get_info, also
    # darf Pebbles Ordner dort nicht mitgebaut werden. Ein Hunk, der unter dem Schalter nur diesen
    # Ordner aus der Quellliste nimmt - jede weitere Zeile koennte still andere Apps abschalten.
    'fw/apps/system/CMakeLists.txt': Stelle(4, hunks=1, cmake=CMakeBlock(
        'CONFIG_BOULDER_HEALTH_APP', ('list(FILTER source EXCLUDE REGEX "/apps/system/health/")',))),
}

# Unversioniertes, das nicht zum Quellbaum gehoert (Bauordner im Worktree).
UNVERSIONIERT_FREI = re.compile(r'^build[^/]*/')

HUNK = re.compile(r'^@@ -\d+(?:,\d+)? \+\d+(?:,\d+)? @@ ?(.*)$')
# Praeprozessor-Zeilen ohne Kommentar. Nur genau "#ifdef SCHALTER" oeffnet den eigenen Block.
SCHALTER_AUF = re.compile(r'#\s*ifdef\s+' + SCHALTER)
IF = re.compile(r'#\s*if')
ELSE = re.compile(r'#\s*(?:else|elif)\b')
ENDIF = re.compile(r'#\s*endif\b')


def eigen(pfad):
    return pfad.startswith(EIGENE_ORDNER)


def ohne_kommentar(text, kommentar):
    """(Code der Zeile ohne Kommentare, ob am Zeilenende ein /* offen ist).

    `kommentar`: ob die Zeile in einem /* beginnt. Zeichenketten kennt das nicht: ein "//" darin
    kuerzt nur Code, der davor schon steht, die Zeile bleibt Code; ein "/*" darin hielte die
    folgenden Zeilen faelschlich fuer Kommentar (in Pebbles Haken kommt das nicht vor).
    """
    code, rest = '', text
    while rest:
        if kommentar:
            ende = rest.find('*/')
            if ende < 0:
                break
            rest, kommentar = rest[ende + 2:], False
            continue
        auf, zeile = rest.find('/*'), rest.find('//')
        if zeile >= 0 and (auf < 0 or zeile < auf):
            code += rest[:zeile]
            break
        if auf < 0:
            code += rest
            break
        code, rest, kommentar = code + rest[:auf] + ' ', rest[auf + 2:], True
    return code.strip(), kommentar


def ausserhalb_des_schalters(zeilen):
    """Die Zeilen eines Hunks, mit denen Pebbles Code auf jedem Board anders wird.

    Das sind neue Zeilen, die nicht im ersten Zweig eines #ifdef SCHALTER stehen (davor, danach,
    im #else), und entfernte Zeilen. Kommentare und Leerzeilen aendern keinen Code und zaehlen
    nicht. Gelesen wird das Ergebnis (Kontext und neue Zeilen) mit den offenen #if; ein Hunk ohne
    das eigene #ifdef steht darum ausserhalb. Ein neues #endif ist ueberall erlaubt: git haengt
    das eigene #endif oft an ein altes an (P5), und ein #if, das es schliessen koennte, ist selbst
    neu und muss unter dem Schalter stehen.
    """
    aussen = []
    offen = []  # je offenes #if: (ist es #ifdef SCHALTER, steht man in seinem ersten Zweig)
    kommentar = False
    for zeile in zeilen:
        art, text = zeile[:1], zeile[1:]
        if art == '-':
            if ohne_kommentar(text, False)[0]:
                aussen.append(zeile)
            continue
        if art not in ' +':
            continue
        code, kommentar = ohne_kommentar(text, kommentar)
        if not code:
            continue
        neu = art == '+'
        unter = any(schalter and erster for schalter, erster in offen)
        if SCHALTER_AUF.fullmatch(code):
            offen.append((True, True))
        elif IF.match(code):
            if neu and not unter:
                aussen.append(zeile)
            offen.append((False, True))
        elif ELSE.match(code):
            schalter = bool(offen) and offen[-1][0]
            if neu and not (unter or schalter):
                aussen.append(zeile)
            if offen:
                offen[-1] = (schalter, False)
        elif ENDIF.match(code):
            if offen:
                offen.pop()
        elif neu and not unter:
            aussen.append(zeile)
    return aussen


def schalterfehler(pfad, kopf, zeilen):
    """Ein Hunk einer Kieselsport-Stelle: C ganz unter dem Schalter, JSON nennt ihn ("ifdefs")."""
    if pfad.endswith(('.c', '.h')):
        aussen = ausserhalb_des_schalters(zeilen)
        if aussen:
            return [f'{pfad}: Hunk "{kopf}" aendert ausserhalb von #ifdef {SCHALTER}: '
                    f'"{aussen[0].strip()}"']
        return []
    if not any(SCHALTER in z for z in zeilen):
        return [f'{pfad}: Hunk "{kopf}" ohne {SCHALTER}']
    return []


def cmake_code(text):
    """Der Code einer CMake-Zeile: ohne Kommentar ("#" ausserhalb von Anfuehrungszeichen), ohne
    Leerraum vor "(" und mit einfachem Leerraum - "if (X)" ist dasselbe wie "if(X)"."""
    code, in_text = '', False
    for zeichen in text:
        if zeichen == '"':
            in_text = not in_text
        elif zeichen == '#' and not in_text:
            break
        code += zeichen
    return re.sub(r'^(\w+)\s*\(', r'\1(', ' '.join(code.split()))


def cmake_fehler(pfad, kopf, zeilen, block):
    """Ein Hunk einer CMake-Stelle: als Code genau if(schalter), der Inhalt, endif(); keine Zeile
    von Pebble entfernt. Kommentare und Leerzeilen zaehlen nicht ("Boulder:" steht im Kommentar)."""
    soll = [f'if({block.schalter})', *block.inhalt, 'endif()']
    neu = [cmake_code(z[1:]) for z in zeilen if z.startswith('+')]
    weg = [cmake_code(z[1:]) for z in zeilen if z.startswith('-')]
    neu, weg = [c for c in neu if c], [c for c in weg if c]
    fehler = []
    if weg:
        fehler.append(f'{pfad}: Hunk "{kopf}" entfernt Pebbles Zeile "{weg[0]}"')
    if neu != soll:
        fehler.append(f'{pfad}: Hunk "{kopf}" bringt {neu}, erlaubt ist genau {soll}')
    return fehler


def pruefe_eigene_datei(pfad, hunks):
    """Eine eigene Datei in Pebbles Ordner: nur neue Zeilen, "Boulder:", als .c ganz unter dem
    Schalter - fw/apps/system/ sammelt jede .c ein, Code davor oder danach baute jedes Board."""
    fehler = []
    zeilen = [x for _, z in hunks for x in z]
    if any(x.startswith('-') for x in zeilen):
        fehler.append(f'{pfad}: eigene Datei aendert Zeilen, die es schon gab')
    if not any('Boulder:' in x for x in zeilen):
        fehler.append(f'{pfad}: eigene Datei ohne "Boulder:"')
    if pfad.endswith('.c'):
        aussen = ausserhalb_des_schalters([x for x in zeilen if x.startswith('+')])
        if aussen:
            fehler.append(f'{pfad}: eigene Datei nicht ganz unter #ifdef {SCHALTER}: '
                          f'"{aussen[0].strip()}"')
    return fehler


def als_neue_datei(pfad, text):
    """Eine unversionierte Datei als Diff gegen nichts - so prueft der Waechter sie wie committet."""
    zeilen = text.splitlines()
    return (f'diff --git a/{pfad} b/{pfad}\n--- /dev/null\n+++ b/{pfad}\n'
            f'@@ -0,0 +1,{len(zeilen)} @@\n' + ''.join('+' + z + '\n' for z in zeilen))


def nur_includes(zeilen):
    """Nur Includes und Kommentare geaendert: an keine Funktion gebunden (den Schalter brauchen sie
    trotzdem, siehe schalterfehler).

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
            # Mit Kontext: das #ifdef, unter dem eine Zeile steht, und sein #endif gehoeren dazu.
            if stelle.schalter:
                fehler += schalterfehler(pfad, kopf, zeilen)
            if stelle.cmake:
                fehler += cmake_fehler(pfad, kopf, zeilen, stelle.cmake)
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
