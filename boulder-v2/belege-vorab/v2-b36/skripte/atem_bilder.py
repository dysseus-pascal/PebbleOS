#!/usr/bin/env python3
"""b36: die Atemuebung in QEMU (Pruefbau build-pruef), mit Bildern.

Startet den Pruefbau, oeffnet Kieselsport, waehlt Atmen (achte Zeile), nimmt den Startschirm auf,
startet mit Select und nimmt nach dem Countdown die Phasen auf: mitten im Einatmen, beim Halten,
mitten im Ausatmen, beim leeren Halten, die Pulsseite; nach 10 Runden den Endschirm. Danach liest
es den Stand des Dienstes (die gespeicherte Dauer).

Aufruf: atem_bilder.py <ausgabeordner> [<arbeitsordner fuer QEMU>]
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
# Die Kopie des Flash-Abbilds (32 MB) und das UART-Protokoll nicht zu den Bildern.
arbeit = pathlib.Path(sys.argv[2]) if len(sys.argv) > 2 else aus / 'qemu'
arbeit.mkdir(parents=True, exist_ok=True)
bau = REPO / 'build-pruef'
emu = q.Emulator(bau, arbeit)
emu.starten()
protokoll = []


def notiz(text):
    zeile = f'{time.strftime("%H:%M:%S")} {text}'
    print(zeile, flush=True)
    protokoll.append(zeile)


try:
    time.sleep(20)
    k = q.Konsole(bau)
    for c in ['kieselsport vorbereiten', 'kieselsport puls 64']:
        notiz(f'{c}: {k.befehl(c)}')
    k.befehl(f'app launch {APP_KIESELSPORT}')
    time.sleep(2.0)
    for _ in range(7):
        emu.taste('u')
    time.sleep(0.6)
    k.befehl('backlight test')
    emu.bild(aus / 'menue.png')
    emu.taste('s')
    time.sleep(1.0)
    k.befehl('backlight test')
    emu.bild(aus / 'bereit.png')
    t_select = time.time()
    emu.taste('s')
    time.sleep(1.2)
    emu.bild(aus / 'countdown.png')
    t0 = t_select + 3.0

    def bei(sek, name):
        rest = t0 + sek - time.time()
        if rest > 0:
            time.sleep(rest)
        # Ohne Licht nimmt QEMU den Schirm abgedunkelt auf.
        k.befehl('backlight test')
        t = time.time() - t0
        emu.bild(aus / name)
        notiz(f'{name}: aufgenommen bei {t:.2f} s nach dem ersten Einatmen (Soll {sek} s)')

    bei(1.8, 'einatmen.png')
    bei(5.8, 'halten.png')
    bei(9.8, 'ausatmen.png')
    bei(13.8, 'leer.png')
    bei(17.8, 'runde2_einatmen.png')
    emu.taste('u')
    bei(20.0, 'pulsseite.png')
    emu.taste('u')
    emu.taste('u')
    bei(25.8, 'runde2_ausatmen.png')
    notiz('stand: ' + ' | '.join(k.befehl('kieselsport stand')))
    bei(160.4, 'ende.png')
    bei(164.0, 'gespeichert.png')
    notiz('stand: ' + ' | '.join(k.befehl('kieselsport stand')))
    zeilen = [z for z in k.ab(0) if re.search(r'(?i)kieselsport|fault|assert|crash', z)]
    (aus / 'konsole.txt').write_text('\n'.join(zeilen) + '\n')
    k.close()
finally:
    emu.stoppen()
    (aus / 'ablauf.txt').write_text('\n'.join(protokoll) + '\n')
