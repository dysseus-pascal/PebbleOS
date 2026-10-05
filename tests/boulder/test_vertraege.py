#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0
"""Boulder: schlaegt tools/boulder/vertraege.py an, wo es soll?

Aufruf: test_vertraege.py <repo>

Die vier Koepfe, die vertraege.py liest, kommen in einen Ordner auf Zeit -
einmal unveraendert (gruen), dann je mit einer Aenderung, wie ein
Upstream-Merge sie bringen koennte (rot). Gerufen wird das echte Werkzeug.
"""
import pathlib
import shutil
import subprocess
import sys
import tempfile

KOEPFE = [
    'include/pbl/services/activity/activity.h',
    'include/pbl/services/activity/activity_private.h',
    'include/pbl/services/data_logging/data_logging_service.h',
    'include/pbl/services/activity/boulder/nacht.h',
]

# (Name, Kopf, alt, neu, erwartete Rueckgabe)
FAELLE = [
    ('unveraendert', None, None, None, 0),
    ('neuer Typ mit Wert 8 vor der Bewegung', KOEPFE[0],
     '  ActivitySessionType_Movement = 8,',
     '  ActivitySessionType_Swim = 8,\n  ActivitySessionType_Movement = 8,', 1),
    ('Bewegung verschoben', KOEPFE[0],
     '  ActivitySessionType_Movement = 8,', '  ActivitySessionType_Movement = 9,', 1),
    ('Tag 84 in Fassung 8', KOEPFE[1],
     '#define ACTIVITY_SESSION_LOGGING_VERSION 7', '#define ACTIVITY_SESSION_LOGGING_VERSION 8', 1),
    ('Upstream vergibt 88', KOEPFE[2],
     '  DlsSystemTagAnalyticsNativeHeartbeat = 87,',
     '  DlsSystemTagAnalyticsNativeHeartbeat = 87,\n  DlsSystemTagNeu,', 1),
    ('Upstream vergibt 0x4001', KOEPFE[2],
     '  DlsSystemTagAnalyticsNativeHeartbeat = 87,',
     '  DlsSystemTagAnalyticsNativeHeartbeat = 87,\n  DlsSystemTagNeu = 0x4001,', 1),
    ('Boulder-Tag geaendert', KOEPFE[3],
     '#define BOULDER_DLS_TAG_NACHT 0x4001u', '#define BOULDER_DLS_TAG_NACHT 0x58u', 1),
]


def main():
    repo = pathlib.Path(sys.argv[1])
    werkzeug = repo / 'tools/boulder/vertraege.py'
    fehler = 0
    for name, kopf, alt, neu, soll in FAELLE:
        with tempfile.TemporaryDirectory() as ordner:
            ziel = pathlib.Path(ordner)
            for k in KOEPFE:
                (ziel / k).parent.mkdir(parents=True, exist_ok=True)
                shutil.copy(repo / k, ziel / k)
            if kopf:
                text = (ziel / kopf).read_text(encoding='utf-8')
                if alt not in text:
                    print(f'FAIL {name}: "{alt}" steht nicht in {kopf}')
                    fehler = 1
                    continue
                (ziel / kopf).write_text(text.replace(alt, neu, 1), encoding='utf-8')
            lauf = subprocess.run([sys.executable, str(werkzeug), str(ziel)],
                                  capture_output=True, text=True)
            ok = lauf.returncode == soll
            print(f'{"ok  " if ok else "FAIL"} {name}: Rueckgabe {lauf.returncode}'
                  + (f' - {lauf.stderr.strip().splitlines()[-1].strip()}' if lauf.stderr else ''))
            fehler |= 0 if ok else 1
    return fehler


if __name__ == '__main__':
    sys.exit(main())
