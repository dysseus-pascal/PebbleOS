#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0
"""Boulder V2, T8.4: was b25 auf dem Flash hinterlaesst, liest V2 mit demselben Aufbau.

Aufruf: umzug_pruefen.py <repo> [--b25 <commit>] [--gegenprobe]

Vergleicht die Definitionen von b25 (M: main 217428e90, ueber git aus demselben Repository)
mit denen im Arbeitsbaum (U:), ohne Kommentare und Leerraum:
- liegende Datalogging-Saetze (0x4001/0x4002 und Tag 84 im Spool): DLSFileHeader,
  DLSChunkHeader, die Fassung, die Chunk-Grenzen, der Dateiname, DataLoggingItemType;
- Pebbles Activity-Datei: Name, Groesse, ActivitySettingsKey (die Merker der zuletzt geloggten
  Enden und die Sitzungsliste muessen unter denselben Schluesseln stehen), die Sitzung selbst
  und die Laenge der Liste.

WARUM KEIN HOST-TEST DER GROESSEN: Die Satzkoepfe stehen privat in dls_storage.c, und b25s
Quellen lassen sich im V2-Baum nicht uebersetzen. Gleicher Text heisst gleicher Aufbau.

Erlaubt ist nur, was die Liste ERLAUBT nennt (b25s Union-Mitglied nicht_schlaf_min in der
Sitzung: kleiner als step_data, die Groesse bleibt). Rot (1) bei jedem anderen Unterschied.
--gegenprobe: prueft, dass ein veraenderter Kopf rot wird.
"""
import re
import subprocess
import sys

B25 = '217428e90'

# (Pfad in b25, Pfad in V2, Art, Name)
PRUEFEN = [
    ('src/fw/services/data_logging/dls_storage.c', 'fw/services/data_logging/dls_storage.c',
     'typedef', 'DLSFileHeaderVersion'),
    ('src/fw/services/data_logging/dls_storage.c', 'fw/services/data_logging/dls_storage.c',
     'typedef', 'DLSFileHeader'),
    ('src/fw/services/data_logging/dls_storage.c', 'fw/services/data_logging/dls_storage.c',
     'typedef', 'DLSChunkHeader'),
    ('src/fw/services/data_logging/dls_storage.c', 'fw/services/data_logging/dls_storage.c',
     'define', 'DLS_CHUNK_HDR_NUM_BYTES_UNINITIALIZED'),
    ('src/fw/services/data_logging/dls_storage.c', 'fw/services/data_logging/dls_storage.c',
     'define', 'DLS_MAX_CHUNK_SIZE_BYTES'),
    ('include/pbl/services/data_logging/dls_private.h',
     'include/pbl/services/data_logging/dls_private.h', 'define', 'DLS_FILE_NAME_PREFIX'),
    ('src/fw/applib/data_logging.h', 'fw/applib/data_logging.h', 'typedef',
     'DataLoggingItemType'),
    ('include/pbl/services/activity/activity_private.h',
     'include/pbl/services/activity/activity_private.h', 'define', 'ACTIVITY_SETTINGS_FILE_NAME'),
    ('include/pbl/services/activity/activity_private.h',
     'include/pbl/services/activity/activity_private.h', 'define', 'ACTIVITY_SETTINGS_FILE_LEN'),
    ('include/pbl/services/activity/activity_private.h',
     'include/pbl/services/activity/activity_private.h', 'typedef', 'ActivitySettingsKey'),
    ('include/pbl/services/activity/activity.h', 'include/pbl/services/activity/activity.h',
     'define', 'ACTIVITY_MAX_ACTIVITY_SESSIONS_COUNT'),
    ('include/pbl/services/activity/activity.h', 'include/pbl/services/activity/activity.h',
     'typedef', 'ActivitySessionDataStepping'),
    ('include/pbl/services/activity/activity.h', 'include/pbl/services/activity/activity.h',
     'typedef', 'ActivitySession'),
]

# b25s Mitglied in der Union der Sitzung (M: activity.h:240-244): 2 Byte neben 8 Byte step_data.
ERLAUBT = {'ActivitySession': [r'uint16_tnicht_schlaf_min;']}


def ohne_kommentare(text):
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.S)
    return re.sub(r'//[^\n]*', '', text)


def typedef(text, name):
    for m in re.finditer(r'typedef\s+(struct|enum)\b[^{;]*\{', text):
        tiefe, i = 1, m.end()
        while tiefe and i < len(text):
            tiefe += {'{': 1, '}': -1}.get(text[i], 0)
            i += 1
        ende = re.match(r'\s*(\w+)\s*;', text[i:])
        if ende and ende.group(1) == name:
            return text[m.start():i + ende.end()]
    return None


def define(text, name):
    m = re.search(r'^\s*#define\s+' + name + r'\b(.*)$', text, flags=re.M)
    return m.group(0) if m else None


def teil(text, art, name):
    text = ohne_kommentare(text)
    gefunden = typedef(text, name) if art == 'typedef' else define(text, name)
    return None if gefunden is None else re.sub(r'\s+', '', gefunden)


def b25_datei(repo, pfad, commit):
    return subprocess.run(['git', '-C', repo, 'show', f'{commit}:{pfad}'], check=True,
                          capture_output=True, text=True).stdout


def pruefe(repo, commit, aendern=None):
    fehler = []
    for alt_pfad, neu_pfad, art, name in PRUEFEN:
        alt = teil(b25_datei(repo, alt_pfad, commit), art, name)
        with open(f'{repo}/{neu_pfad}', encoding='utf-8') as f:
            neu_text = f.read()
        if aendern and aendern[0] == name:
            neu_text = neu_text.replace(*aendern[1:])
        neu = teil(neu_text, art, name)
        if alt is None or neu is None:
            fehler.append(f'{name}: nicht gefunden (b25 {alt is not None}, V2 {neu is not None})')
            continue
        for muster in ERLAUBT.get(name, []):
            alt = re.sub(muster, '', alt)
        if alt != neu:
            fehler.append(f'{name}: b25 {alt}\n   V2  {neu}')
    return fehler


def main(argv):
    repo = argv[1]
    commit = argv[argv.index('--b25') + 1] if '--b25' in argv else B25
    if '--gegenprobe' in argv:
        # Ein Feld des Dateikopfs anders breit: das muss auffallen.
        rot = pruefe(repo, commit, ('DLSFileHeader', 'uint16_t item_size;', 'uint32_t item_size;'))
        if not rot:
            print('Gegenprobe: veraenderter DLSFileHeader blieb gruen')
            return 1
        print('Gegenprobe rot wie erwartet')
        return 0
    fehler = pruefe(repo, commit)
    for f in fehler:
        print('ANDERS', f)
    print(f'{len(PRUEFEN)} Definitionen b25 = V2' if not fehler else f'{len(fehler)} anders')
    return 1 if fehler else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
