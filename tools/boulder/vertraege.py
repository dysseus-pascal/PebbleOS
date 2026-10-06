#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0
"""Boulder: die mit Upstream geteilten Vertraege gegen eine eingefrorene Liste.

Aufruf: vertraege.py <repo> [--stempel <datei>]

Rot (Rueckgabe 1), wenn sich Namen oder Werte von ActivitySessionType oder
DlsSystemTag, die Fassung von Tag 84 oder Boulders eigene Datalogging-Tags
von der Liste unten unterscheiden, oder ein Upstream-Tag einen von Boulder
belegten oder gesperrten Wert traegt. Mit --stempel wird die Datei bei Erfolg
geschrieben (Bauschritt).

V2 (aus b26 uebernommen): Pebbles Sitzungstypen 0 bis 7 ohne eigenen Typ, Tag 84
in Pebbles Fassung 3, die Formate der Nacht in boulder/nacht_satz.h.

WARUM NEBEN boulder/vertraege.c: der Compiler bricht bei einem neuen Namen in
einem switch ohne default, aber nicht, wenn ein neuer Name einen schon
vergebenen Wert traegt (`Swim = 8` vor `Movement = 8`): dann laesen Uhr und
App eine Sitzung des einen als die des anderen. Hier zaehlt jeder Name mit
seinem Wert.

Aendert ein Upstream-Merge etwas davon, ist das gewollt laut: erst klaeren,
was die App dann liest (Boulder: HealthDataProcessor, Datalogging), dann hier
und in fw/services/activity/boulder/vertraege.c nachziehen.
"""
import pathlib
import re
import sys

SOLL_TYPEN = {
    'ActivitySessionType_None': 0,
    'ActivitySessionType_Sleep': 1,
    'ActivitySessionType_RestfulSleep': 2,
    'ActivitySessionType_Nap': 3,
    'ActivitySessionType_RestfulNap': 4,
    'ActivitySessionType_Walk': 5,
    'ActivitySessionType_Run': 6,
    'ActivitySessionType_Open': 7,
    'ActivitySessionTypeCount': 8,
    'ActivitySessionType_Invalid': 8,
}

SOLL_TAGS = {
    'DlsSystemTagAnalyticsDeviceHeartbeat': 78,
    'DlsSystemTagAnalyticsAppHeartbeat': 79,
    'DlsSystemTagAnalyticsEvent': 80,
    'DlsSystemTagActivityMinuteData': 81,
    'DlsSystemTagActivityAccelSamples': 82,
    'DlsSystemTagActivitySession': 84,
    'DlsSystemTagProtobufLogSession': 85,
    'DlsSystemTagAnalyticsNativeHeartbeat': 87,
}

# Boulders eigene Tags (boulder/nacht_satz.h) und die bis b24 belegten.
SOLL_BOULDER = {
    'BOULDER_DLS_TAG_NACHT': 0x4001,
    'BOULDER_DLS_TAG_NACHT_HRV': 0x4002,
    'BOULDER_DLS_TAG_NACHT_ALT': 88,
    'BOULDER_DLS_TAG_NACHT_HRV_ALT': 89,
}

# Werte, die kein Upstream-Tag tragen darf: 83 (alter Schlaf, die App liest ihn
# wie 84), 86 (bei Pebble ausser Dienst) und alles von Boulder.
GESPERRT = {83, 86} | set(SOLL_BOULDER.values())

SOLL_FASSUNG_84 = 3

ACTIVITY_H = 'include/pbl/services/activity/activity.h'
PRIVATE_H = 'include/pbl/services/activity/activity_private.h'
DLS_H = 'include/pbl/services/data_logging/data_logging_service.h'
NACHT_H = 'include/pbl/services/activity/boulder/nacht_satz.h'
MESSSTUFE_H = 'include/pbl/services/activity/boulder/messstufe.h'
SETTINGS_BLOB_DB_C = 'fw/services/blob_db/settings_blob_db.c'
# Schluessel der Messstufe, wie Boulder 0.50 ihn schreibt (B: WatchPrefEntity.kt:571-583).
SOLL_MESSSTUFE = '"boulderMessstufe"'


def ohne_kommentare(text):
    text = re.sub(r'/\*.*?\*/', ' ', text, flags=re.S)
    return re.sub(r'//[^\n]*', '', text)


def zahl(ausdruck, bekannt):
    ausdruck = ausdruck.strip()
    if ausdruck in bekannt:
        return bekannt[ausdruck]
    return int(re.sub(r'[uUlL]+$', '', ausdruck), 0)


def enum(text, name):
    """Namen und Werte eines `typedef enum { ... } name;` wie der Compiler sie zaehlt."""
    treffer = re.findall(r'typedef\s+enum\s*\{([^{}]*)\}\s*' + name + r'\s*;', text)
    if len(treffer) != 1:
        raise ValueError(f'enum {name}: {len(treffer)}-mal gefunden')
    werte, naechster = {}, 0
    for eintrag in (e.strip() for e in treffer[0].split(',')):
        if not eintrag:
            continue
        schluessel, _, ausdruck = eintrag.partition('=')
        schluessel = schluessel.strip()
        wert = zahl(ausdruck, werte) if ausdruck.strip() else naechster
        if schluessel in werte:
            raise ValueError(f'enum {name}: {schluessel} doppelt')
        werte[schluessel] = wert
        naechster = wert + 1
    return werte


def defines(text, namen):
    werte = {}
    for n in namen:
        m = re.search(r'^\s*#define\s+' + n + r'\s+\(?([0-9xXa-fA-FuUlL]+)\)?\s*$', text, re.M)
        if m:
            werte[n] = zahl(m.group(1), {})
    return werte


def vergleiche(was, ist, soll):
    fehler = []
    for k in sorted(set(ist) | set(soll)):
        if k not in ist:
            fehler.append(f'{was}: {k} fehlt (soll {soll[k]})')
        elif k not in soll:
            fehler.append(f'{was}: {k} = {ist[k]} ist neu')
        elif ist[k] != soll[k]:
            fehler.append(f'{was}: {k} = {ist[k]}, soll {soll[k]}')
    return fehler


def pruefe(repo):
    lies = lambda p: ohne_kommentare((repo / p).read_text(encoding='utf-8'))
    fehler = []
    typen = enum(lies(ACTIVITY_H), 'ActivitySessionType')
    fehler += vergleiche('ActivitySessionType', typen, SOLL_TYPEN)
    gezaehlt = [w for k, w in typen.items() if k not in ('ActivitySessionTypeCount',
                                                          'ActivitySessionType_Invalid')]
    if len(gezaehlt) != len(set(gezaehlt)):
        fehler.append('ActivitySessionType: zwei Typen mit demselben Wert')

    tags = enum(lies(DLS_H), 'DlsSystemTag')
    fehler += vergleiche('DlsSystemTag', tags, SOLL_TAGS)
    if len(tags.values()) != len(set(tags.values())):
        fehler.append('DlsSystemTag: zwei Tags mit demselben Wert')
    for k, w in tags.items():
        if w in GESPERRT:
            fehler.append(f'DlsSystemTag: {k} = {w} ist von Boulder belegt oder gesperrt')

    fassung = defines(lies(PRIVATE_H), ['ACTIVITY_SESSION_LOGGING_VERSION'])
    if fassung.get('ACTIVITY_SESSION_LOGGING_VERSION') != SOLL_FASSUNG_84:
        fehler.append(f'ACTIVITY_SESSION_LOGGING_VERSION = '
                      f'{fassung.get("ACTIVITY_SESSION_LOGGING_VERSION")}, soll {SOLL_FASSUNG_84}')

    boulder = defines(lies(NACHT_H), SOLL_BOULDER.keys())
    fehler += vergleiche('Boulder-Tags', boulder, SOLL_BOULDER)

    # AP5/E3: Boulder 0.50 schreibt die Messstufe unter diesem Schluessel (WatchPrefEntity.kt).
    # Fehlt er in der Whitelist, lehnt die Uhr ihn still ab ("Rejecting non-whitelisted").
    # Hier statt im Host-Test: settings_blob_db.c braucht verschachtelte Funktionen (gcc).
    m = re.search(r'^\s*#define\s+BOULDER_MESSSTUFE_SCHLUESSEL\s+("[^"]*")', lies(MESSSTUFE_H), re.M)
    if not m or m.group(1) != SOLL_MESSSTUFE:
        fehler.append(f'BOULDER_MESSSTUFE_SCHLUESSEL = {m and m.group(1)}, soll {SOLL_MESSSTUFE}')
    liste = re.search(r's_syncable_settings\[\]\s*=\s*\{(.*?)\};', lies(SETTINGS_BLOB_DB_C), re.S)
    if not liste or SOLL_MESSSTUFE not in re.findall(r'"[^"]*"', liste.group(1)):
        fehler.append(f'settings_blob_db.c: {SOLL_MESSSTUFE} fehlt in s_syncable_settings (E3)')
    return fehler


def main(argv):
    if len(argv) not in (2, 4) or (len(argv) == 4 and argv[2] != '--stempel'):
        print(__doc__.strip().split('\n\n')[1], file=sys.stderr)
        return 2
    repo = pathlib.Path(argv[1])
    try:
        fehler = pruefe(repo)
    except (OSError, ValueError) as e:
        fehler = [str(e)]
    if fehler:
        print('Boulder-Vertraege verletzt (tools/boulder/vertraege.py):', file=sys.stderr)
        for f in fehler:
            print('  ' + f, file=sys.stderr)
        return 1
    if len(argv) == 4:
        pathlib.Path(argv[3]).write_text('ok\n', encoding='utf-8')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
