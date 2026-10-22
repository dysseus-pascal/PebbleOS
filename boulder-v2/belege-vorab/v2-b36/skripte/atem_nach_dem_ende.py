#!/usr/bin/env python3
"""b36: Atmen aus der Shell gestartet (1-0-1-0, 31 Runden = 62 s), die App erst nach 66 s geoeffnet:
Die Uebung ist um, die App beendet das Training gleich wie mit Pause und Speichern.

Aufruf: atem_nach_dem_ende.py <ausgabeordner>
"""
import pathlib
import sys
import time

REPO = pathlib.Path('/home/user/pebbleos-v2')
sys.path.insert(0, str(REPO / 'tools' / 'boulder'))
import qemu_health as q  # noqa: E402

APP_KIESELSPORT = -103
aus = pathlib.Path(sys.argv[1])
aus.mkdir(parents=True, exist_ok=True)
arbeit = aus / 'qemu'
arbeit.mkdir(exist_ok=True)
bau = REPO / 'build-pruef'
emu = q.Emulator(bau, arbeit)
emu.starten()
try:
    time.sleep(20)
    k = q.Konsole(bau)
    for c in ['kieselsport vorbereiten', 'kieselsport puls 64', 'kieselsport atem 1 0 1 0 31',
              'kieselsport start 7']:
        print(c, k.befehl(c), flush=True)
    time.sleep(66)
    print('vor dem Oeffnen:', k.befehl('kieselsport stand')[0], flush=True)
    k.befehl(f'app launch {APP_KIESELSPORT}')
    time.sleep(3.0)
    k.befehl('backlight test')
    emu.bild(aus / 'b_geoeffnet.png')
    for z in k.befehl('kieselsport stand'):
        print('danach:', z, flush=True)
    k.close()
finally:
    emu.stoppen()
