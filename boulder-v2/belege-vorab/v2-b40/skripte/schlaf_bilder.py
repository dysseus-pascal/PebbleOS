#!/usr/bin/env python3
"""b40: der Schlafmodus in QEMU (Pruefbau build-pruef), mit Bildern.

Die Health-App auf Night, Select lange (Pebbles Schalter zeigt "Sleep Mode Started"), die App
wieder auf (sie oeffnet auf Night: "Sleeping since ..."), Select ("Sleep Mode Ended"), dann der
Schalter aus Quick Launch direkt (app launch -105) an und aus. Nach jedem Schritt die Lage
("boulder lage": schlafmodus <start>).

Aufruf: schlaf_bilder.py <ausgabeordner> [<arbeitsordner fuer QEMU>]
"""
import pathlib
import re
import sys
import time

REPO = pathlib.Path('/home/user/pebbleos-v2')
sys.path.insert(0, str(REPO / 'tools' / 'boulder'))
import qemu_health as q  # noqa: E402

APP_HEALTH = -82
APP_SCHALTER = -105
aus = pathlib.Path(sys.argv[1])
aus.mkdir(parents=True, exist_ok=True)
arbeit = pathlib.Path(sys.argv[2]) if len(sys.argv) > 2 else aus / 'qemu'
arbeit.mkdir(parents=True, exist_ok=True)
bau = REPO / 'build-pruef'
emu = q.Emulator(bau, arbeit)
emu.starten()
protokoll = []
fehler = []


def notiz(text):
    zeile = f'{time.strftime("%H:%M:%S")} {text}'
    print(zeile, flush=True)
    protokoll.append(zeile)


def pruefe(was, ok):
    notiz(f'{"OK  " if ok else "FEHL"} {was}')
    if not ok:
        fehler.append(was)


try:
    time.sleep(20)
    k = q.Konsole(bau)

    def befehl(c):
        antwort = k.befehl(c)
        notiz(f'{c}: ' + ' | '.join(antwort))
        return antwort

    def seit():
        for z in befehl('boulder lage'):
            m = re.search(r'schlafmodus (\d+)', z)
            if m:
                return int(m.group(1))
        return None

    def bild(name, warten=0.6):
        time.sleep(warten)
        k.befehl('backlight test')
        emu.bild(aus / name)
        notiz(f'{name} aufgenommen')

    befehl('boulder tracking')
    befehl(f'boulder zeit {q.UHR_UTC}')
    time.sleep(70)  # init, "Laden" und die erste Minute
    pruefe('vorher kein Schlafmodus', seit() == 0)

    befehl(f'app launch {APP_HEALTH}')
    time.sleep(2.0)
    bild('01_today.png')
    emu.taste('u')
    time.sleep(1.0)
    bild('02_night.png')
    # Select lange ueber Pebbles Befehl (remote_input_shell.c): QEMUs sendkey haelt die Taste nicht.
    befehl('button hold 2 1000')
    time.sleep(1.2)
    bild('03_lang_gestartet.png', warten=0.5)
    time.sleep(3.0)
    s = seit()
    pruefe(f'Select lange startet ihn ({s})', bool(s))
    bild('04_zifferblatt_danach.png')

    befehl(f'app launch {APP_HEALTH}')
    time.sleep(2.0)
    bild('05_health_im_schlafmodus.png')
    emu.taste('s')
    bild('06_select_beendet.png', warten=0.5)
    time.sleep(3.0)
    pruefe('Select beendet ihn', seit() == 0)

    befehl(f'app launch {APP_SCHALTER}')
    bild('07_quick_launch_an.png', warten=0.5)
    time.sleep(3.0)
    pruefe('der Schalter startet ihn', bool(seit()))
    befehl(f'app launch {APP_SCHALTER}')
    bild('08_quick_launch_aus.png', warten=0.5)
    time.sleep(3.0)
    pruefe('der Schalter beendet ihn', seit() == 0)
    k.close()
finally:
    emu.stoppen()
    (aus / 'protokoll.txt').write_text('\n'.join(protokoll) + '\n')
print(f'{len(fehler)} Fehler')
sys.exit(1 if fehler else 0)
