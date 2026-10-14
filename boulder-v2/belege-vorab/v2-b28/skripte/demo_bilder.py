#!/usr/bin/env python3
"""Kieselstrasse-Demo in QEMU: App oeffnen und die 20 Demo-Staende aufnehmen (Taste Unten).

Aufruf: demo_bilder.py <bauordner> <app-id> <qemu-bin-ordner> <ausgabe>
"""
import os
import pathlib
import socket
import sys
import tempfile
import time

sys.path.insert(0, '/home/user/pebbleos-v2/tools/boulder')
import qemu_health as q  # noqa: E402


def monitor(bau, befehl):
    s = socket.socket(socket.AF_UNIX)
    s.connect(str(bau / 'qemu-mon.sock'))
    s.settimeout(1.0)
    try:
        s.recv(4096)
    except socket.timeout:
        pass
    s.sendall((befehl + '\n').encode())
    time.sleep(0.8)
    s.close()


def main():
    bau = pathlib.Path(sys.argv[1]).resolve()
    app_id = sys.argv[2]
    os.environ['PATH'] = sys.argv[3] + ':' + os.environ['PATH']
    aus = pathlib.Path(sys.argv[4]).resolve()
    aus.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        emu = q.Emulator(bau, pathlib.Path(tmp))
        emu.starten()
        k = None
        try:
            time.sleep(25)  # Boot bis zum Zifferblatt
            k = q.Konsole(bau)
            stand = k.stand()
            k.befehl(f'app launch {app_id}')
            time.sleep(3)
            for i in range(20):
                emu.taste('o')  # Licht an; Oben schaltet die Demo nicht weiter
                time.sleep(0.8)
                monitor(bau, f'screendump {aus}/demo_{i:02d}.png -f png')
                emu.taste('u')
                time.sleep(1.5)
            (aus / 'protokoll.txt').write_text('\n'.join(k.ab(stand)) + '\n')
            print('laeuft am Ende:', emu.laeuft())
        finally:
            if k:
                k.close()
            emu.stoppen()


if __name__ == '__main__':
    main()
