#!/usr/bin/env python3
"""b36: zwei weitere Laeufe der Atemuebung in QEMU (Pruefbau build-pruef).

A  Rhythmus 3-0-3-0, 12 Runden (72 s) aus der Shell (kieselsport atem), wie vom Telefon: Der
   Startschirm nennt ihn, im Ablauf kommt kein Halten vor (Bilder alle 0,7 s), gespeichert wird
   mit der ganzen Dauer.
B  Rhythmus 1-0-1-0, 31 Runden (62 s), Start aus der Shell (kieselsport start 7), die App erst nach
   66 s geoeffnet: Die Uebung ist um, die App beendet sie gleich.

Aufruf: atem_varianten.py <ausgabeordner>
"""
import pathlib
import re
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
protokoll = []


def notiz(text):
    zeile = f'{time.strftime("%H:%M:%S")} {text}'
    print(zeile, flush=True)
    protokoll.append(zeile)


def bild(k, name):
    k.befehl('backlight test')
    emu.bild(aus / name)


try:
    time.sleep(20)
    k = q.Konsole(bau)
    for c in ['kieselsport vorbereiten', 'kieselsport puls 64', 'kieselsport atem 3 0 3 0 12']:
        notiz(f'{c}: {k.befehl(c)}')
    # A: ueber die App
    k.befehl(f'app launch {APP_KIESELSPORT}')
    time.sleep(2.0)
    for _ in range(7):
        emu.taste('u')
    emu.taste('s')
    time.sleep(1.0)
    bild(k, 'a_bereit.png')
    t_select = time.time()
    emu.taste('s')
    t0 = t_select + 3.0
    time.sleep(max(0.0, t0 + 0.4 - time.time()))
    for i in range(12):
        t = time.time() - t0
        bild(k, f'a_{i:02d}.png')
        notiz(f'a_{i:02d}.png bei {t:.2f} s')
        time.sleep(0.35)
    time.sleep(max(0.0, t0 + 76 - time.time()))
    bild(k, 'a_gespeichert.png')
    notiz('A stand: ' + ' | '.join(k.befehl('kieselsport stand')))
    emu.taste('s')  # Endschirm zu
    time.sleep(2.0)
    # B: Start aus der Shell, die App erst nach dem Ende
    notiz(f"{k.befehl('kieselsport atem 1 0 1 0 31')}")
    notiz(f"{k.befehl('kieselsport start 7')}")
    time.sleep(66)
    notiz('B vor dem Oeffnen: ' + k.befehl('kieselsport stand')[0])
    k.befehl(f'app launch {APP_KIESELSPORT}')
    time.sleep(3.0)
    bild(k, 'b_geoeffnet.png')
    notiz('B stand: ' + ' | '.join(k.befehl('kieselsport stand')))
    zeilen = [z for z in k.ab(0) if re.search(r'(?i)kieselsport|fault|assert|crash', z)]
    (aus / 'konsole.txt').write_text('\n'.join(zeilen) + '\n')
    k.close()
finally:
    emu.stoppen()
    (aus / 'ablauf.txt').write_text('\n'.join(protokoll) + '\n')
