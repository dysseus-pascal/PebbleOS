#!/usr/bin/env python3
"""Bilder mitten im Gleiten: Taste und sofort ein Bild."""
import pathlib
import socket
import subprocess
import sys
import time

REPO = pathlib.Path('/home/user/pebbleos-v2')
sys.path.insert(0, str(REPO / 'tools' / 'boulder'))
import qemu_health as q  # noqa: E402

aus = pathlib.Path(sys.argv[1])
aus.mkdir(parents=True, exist_ok=True)
bau = REPO / 'build-pruef'
emu = q.Emulator(bau, aus)
emu.starten()


def taste_schnell(k):
    s = socket.socket(socket.AF_UNIX)
    s.connect(str(emu.monitor))
    s.settimeout(1.0)
    try:
        s.recv(4096)
    except socket.timeout:
        pass
    s.sendall(f'sendkey {k} 50\n'.encode())
    s.close()


try:
    time.sleep(20)
    k = q.Konsole(bau)
    k.befehl('boulder tracking')
    stand = k.stand()
    k.befehl(f'app launch {q.APP_HEALTH}')
    k.warte(r'bh t=\d+ bild', stand, 5)
    time.sleep(1)
    for i, (taste, verz) in enumerate([('down', 0.0), ('down', 0.0), ('up', 0.0), ('up', 0.03),
                                       ('down', 0.06), ('down', 0.1)]):
        stand = k.stand()
        taste_schnell(taste)
        time.sleep(verz)
        t0 = time.time()
        r = subprocess.run(['pbl', 'screenshot', '-b', str(bau), '--output', str(aus / f'm_{i}_{taste}.png')],
                           capture_output=True, cwd=REPO)
        dauer = time.time() - t0
        k.warte(r'bh t=\d+ ruhe', stand, 3)
        print(i, taste, verz, 'rc', r.returncode, f'{dauer:.2f}s', q.stempel(k.ab(stand)), r.stderr[-200:])
        time.sleep(0.8)
    k.close()
finally:
    emu.stoppen()
