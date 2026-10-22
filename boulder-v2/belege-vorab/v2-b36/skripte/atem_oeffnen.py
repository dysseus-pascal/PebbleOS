#!/usr/bin/env python3
"""b36: Atmen ueber die Shell starten (kieselsport start 7), spaeter die App oeffnen: Sie setzt an der
aktiven Zeit des Dienstes an. Bilder in Abstaenden, dazu der Stand des Dienstes.

Aufruf: atem_oeffnen.py <ausgabeordner> [<sekunden bis zum Oeffnen>]
"""
import pathlib
import sys
import time

REPO = pathlib.Path('/home/user/pebbleos-v2')
sys.path.insert(0, str(REPO / 'tools' / 'boulder'))
import qemu_health as q  # noqa: E402

APP_KIESELSPORT = -103
aus = pathlib.Path(sys.argv[1])
warten = float(sys.argv[2]) if len(sys.argv) > 2 else 9.0
aus.mkdir(parents=True, exist_ok=True)
arbeit = aus / 'qemu'
arbeit.mkdir(exist_ok=True)
bau = REPO / 'build-pruef'
emu = q.Emulator(bau, arbeit)
emu.starten()
try:
    time.sleep(20)
    k = q.Konsole(bau)
    print(k.befehl('kieselsport vorbereiten'), k.befehl('kieselsport puls 64'), flush=True)
    print(k.befehl('kieselsport start 7'), flush=True)
    t0 = time.time()
    time.sleep(warten)
    print('stand vor dem Oeffnen:', k.befehl('kieselsport stand')[0], flush=True)
    k.befehl(f'app launch {APP_KIESELSPORT}')
    t_auf = time.time() - t0
    print(f'App auf bei {t_auf:.2f} s', flush=True)
    for i in range(8):
        time.sleep(0.6)
        k.befehl('backlight test')
        t = time.time() - t0
        emu.bild(aus / f'o_{i}.png')
        print(f'o_{i}.png bei {t:.2f} s', flush=True)
    print('stand:', k.befehl('kieselsport stand')[0], flush=True)
    k.close()
finally:
    emu.stoppen()
