#!/usr/bin/env python3
"""b37: Pebbles Maske im HRM-Manager in QEMU (Pruefbau build-pruef). "hrm spo2" bittet wie ein Abo im
Hintergrund um SpO2 und prueft vorher Pebbles Maske (hrm_manager.c, prv_cmd_spo2_read): ohne Schalter
"blood oxygen monitoring is disabled"; mit dem Schalter "Schlaf" (im Menue an, "Blood Oxygen" und
"Blood O2 During Activity" aus) keine Fehlermeldung - der Befehl wartet dann auf einen Wert, den
QEMUs Sensor-Attrappe nie liefert (darum zuletzt).

Aufruf: qemu_maske.py <ausgabeordner> <arbeitsordner fuer QEMU>
"""
import pathlib
import sys
import time

REPO = pathlib.Path('/home/user/pebbleos-v2')
sys.path.insert(0, str(REPO / 'tools' / 'boulder'))
import qemu_health as q  # noqa: E402
from pebble.commander.exceptions import CommandTimedOut  # noqa: E402

aus = pathlib.Path(sys.argv[1])
arbeit = pathlib.Path(sys.argv[2])
aus.mkdir(parents=True, exist_ok=True)
arbeit.mkdir(parents=True, exist_ok=True)
bau = REPO / 'build-pruef'
protokoll = []


def notiz(text):
    zeile = f'{time.strftime("%H:%M:%S")} {text}'
    print(zeile, flush=True)
    protokoll.append(zeile)


emu = q.Emulator(bau, arbeit)
emu.starten()
try:
    time.sleep(20)
    k = q.Konsole(bau)
    notiz('tracking: ' + ' | '.join(k.befehl('boulder tracking')))
    notiz('hrm spo2 ohne Schalter: ' + ' | '.join(k.befehl('hrm spo2')))
    k.befehl('app launch -7')
    time.sleep(2.0)
    for _ in range(8):
        emu.taste('u')
    emu.taste('s')
    time.sleep(1.0)
    for _ in range(5):
        emu.taste('u')
    time.sleep(0.6)
    emu.taste('s')
    time.sleep(1.0)
    k.befehl('backlight test')
    time.sleep(0.4)
    emu.bild(aus / 'maske_schlaf_an.png')
    notiz('Bild maske_schlaf_an.png')
    try:
        antwort = k.befehl('hrm spo2', versuche=1)
        notiz('hrm spo2 mit Schlaf: ' + ' | '.join(antwort))
    except CommandTimedOut:
        notiz('hrm spo2 mit Schlaf: keine Fehlermeldung; der Befehl wartet auf einen Wert (Zeitlimit '
              'der Konsole), QEMUs Sensor liefert keinen')
    k.close()
finally:
    emu.stoppen()
    (aus / 'maske.txt').write_text('\n'.join(protokoll) + '\n')
