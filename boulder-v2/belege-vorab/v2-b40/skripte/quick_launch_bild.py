#!/usr/bin/env python3
"""b40: Pebbles Quick-Launch-Auswahl in QEMU (Pruefbau): Steht "Toggle Sleep Mode" darin?

Einstellungen oeffnen, Bild je Schritt; der Weg steht in `wege` (o/u/s/b).
Aufruf: quick_launch_bild.py <ausgabeordner> <weg> [...]
"""
import pathlib
import sys
import time

REPO = pathlib.Path('/home/user/pebbleos-v2')
sys.path.insert(0, str(REPO / 'tools' / 'boulder'))
import qemu_health as q  # noqa: E402

aus = pathlib.Path(sys.argv[1])
aus.mkdir(parents=True, exist_ok=True)
emu = q.Emulator(REPO / 'build-pruef', aus / 'qemu')
(aus / 'qemu').mkdir(exist_ok=True)
emu.starten()
try:
    time.sleep(20)
    k = q.Konsole(REPO / 'build-pruef')
    k.befehl('app launch -7')
    time.sleep(2)
    n = 0
    def bild():
        global n
        time.sleep(0.6)
        k.befehl('backlight test')
        emu.bild(aus / f'{n:02d}.png')
        n += 1
    bild()
    for weg in sys.argv[2:]:
        for t in weg:
            emu.taste(t)
            time.sleep(0.4)
        bild()
    k.close()
finally:
    emu.stoppen()
