"""QEMU mit offener PULSE-Konsole: Protokoll nach log.txt, Befehle aus befehle.txt.

Aufruf (V2-venv, SDK-Umgebung): python3 dienst.py <elf> <flash.bin> <loghash.json> <arbeit>
Das Flash-Abbild wird NICHT kopiert: was die Uhr schreibt, bleibt darin.

Befehle, je Zeile an <arbeit>/befehle.txt angehaengt:
  <konsolenbefehl>        an die Uhr; Antwort nach antworten.txt
  !bild <name>            Bildschirm nach <arbeit>/<name>.png
  !taste <b|s|o|u> [ms]   Taste ueber den QEMU-Monitor (b zurueck, s Select, o oben, u unten)
  !ende                   QEMU beenden
"""
import pathlib
import re
import shutil
import socket
import subprocess
import sys
import threading
import time

sys.path.insert(0, '/home/user/pebbleos-v2/tools')
from log_hashing.logdehash import LogDehash  # noqa: E402
from pebble.commander.commander import PebbleCommander  # noqa: E402
from pebble.commander.exceptions import CommandTimedOut  # noqa: E402

FARBE = re.compile(r'\x1b\[[0-9;]*m')
PORT = 12345


def monitor(sock, befehl):
    s = socket.socket(socket.AF_UNIX)
    s.connect(str(sock))
    s.settimeout(2.0)
    try:
        s.recv(4096)
    except socket.timeout:
        pass
    s.sendall((befehl + '\n').encode())
    time.sleep(0.5)
    try:
        antwort = s.recv(65536).decode(errors='replace')
    except socket.timeout:
        antwort = ''
    s.close()
    return antwort


def main():
    elf, flash, loghash, arbeit = (pathlib.Path(a) for a in sys.argv[1:5])
    arbeit.mkdir(parents=True, exist_ok=True)
    log = open(arbeit / 'log.txt', 'a', buffering=1)
    antworten = open(arbeit / 'antworten.txt', 'a', buffering=1)
    befehle = arbeit / 'befehle.txt'
    befehle.touch()
    mon = arbeit / 'mon.sock'
    qemu = [shutil.which('qemu-pebble'), '-rtc', 'base=localtime',
            '-monitor', f'unix:{mon},server=on,wait=off',
            '-serial', f'file:{arbeit / "uart1.log"}',
            '-serial', 'tcp::12344,server=on,wait=off',
            '-serial', f'tcp::{PORT},server=on,wait=off',
            '-machine', 'pebble-emery', '-kernel', str(elf),
            '-drive', f'if=mtd,format=raw,file={flash}', '-display', 'none']
    proz = subprocess.Popen(qemu, stdout=subprocess.DEVNULL, stderr=open(arbeit / 'qemu.err', 'w'))
    (arbeit / 'qemu.pid').write_text(str(proz.pid))
    for _ in range(120):
        try:
            socket.create_connection(('127.0.0.1', PORT), timeout=1).close()
            break
        except OSError:
            time.sleep(0.5)
    time.sleep(15)
    dehash = LogDehash(str(loghash))
    sperre = threading.Lock()

    def mitschreiben(msg):
        zeile = FARBE.sub('', dehash.commander_format_line(dehash.dehash(msg)))
        with sperre:
            log.write(f'[{time.strftime("%H:%M:%S")}] {zeile}\n')

    cmdr = PebbleCommander(tty=f'socket://localhost:{PORT}')
    cmdr.attach_log_listener(mitschreiben)
    antworten.write(f'== bereit {time.strftime("%H:%M:%S")}\n')
    gelesen = 0
    while proz.poll() is None:
        zeilen = befehle.read_text().splitlines()
        for b in zeilen[gelesen:]:
            gelesen += 1
            b = b.strip()
            if not b:
                continue
            antworten.write(f'>> {b}\n')
            if b == '!ende':
                cmdr.close()
                proz.terminate()
                proz.wait(10)
                antworten.write('== beendet\n')
                return
            if b.startswith('!bild '):
                ziel = arbeit / f'{b.split()[1]}.png'
                antwort = monitor(mon, f'screendump {ziel} -f png')
                if 'rror' in antwort or not ziel.exists():
                    antworten.write(f'   Bild fehlt: {antwort[-200:]}\n')
            elif b.startswith('!taste '):
                teile = b.split()
                namen = {'b': 'left', 's': 'right', 'o': 'up', 'u': 'down'}
                ms = teile[2] if len(teile) > 2 else '100'
                monitor(mon, f'sendkey {namen[teile[1]]} {ms}')
            else:
                try:
                    for z in cmdr.send_prompt_command(b) or []:
                        antworten.write(f'   {z}\n')
                except CommandTimedOut:
                    antworten.write('   (keine Antwort)\n')
            antworten.write(f'== ok {time.strftime("%H:%M:%S")}\n')
        time.sleep(0.3)
    antworten.write(f'== QEMU beendet sich selbst ({proz.returncode})\n')


if __name__ == '__main__':
    main()
