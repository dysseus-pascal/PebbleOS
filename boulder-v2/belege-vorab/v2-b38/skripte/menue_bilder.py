#!/usr/bin/env python3
"""b38: die Untermenues der Raeder und des Schwimmens in QEMU (Pruefbau build-pruef), mit Bildern.

Oeffnet Kieselsport, nimmt das Hauptmenue auf (erste Zeile, "Bike", letzte Zeile), oeffnet mit
Select auf "Bike" das Untermenue, nimmt Strasse/Gravel, MTB und Indoor gewaehlt auf, oeffnet
Indoors Startschirm und geht mit Zurueck ins Untermenue und ins Hauptmenue. Dasselbe mit
"Swimming" (Pool, Open water, Open water GPS), dann das offene Training (Custom, Custom GPS) aus
dem Hauptmenue; zuletzt noch einmal "Bike", weil beide Gruppen ein Untermenue teilen.

Aufruf: menue_bilder.py <ausgabeordner> [<arbeitsordner fuer QEMU>]
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
    notiz('app launch: ' + ' | '.join(k.befehl(f'app launch {APP_KIESELSPORT}')))
    time.sleep(2.0)

    def bild(name):
        # Ohne Licht nimmt QEMU den Schirm abgedunkelt auf.
        time.sleep(0.6)
        k.befehl('backlight test')
        emu.bild(aus / name)
        notiz(f'{name} aufgenommen')

    def tasten(t, n=1):
        for _ in range(n):
            emu.taste(t)

    # Das Hauptmenue: oben, "Bike", das Ende mit dem offenen Training.
    bild('01_haupt_laufen.png')
    tasten('u')
    bild('02_haupt_bike.png')
    tasten('u', 7)
    bild('03_haupt_ende.png')
    # Die Raeder.
    tasten('o', 7)
    tasten('s')
    bild('04_rad_strasse.png')
    tasten('u')
    bild('05_rad_mtb.png')
    tasten('u')
    bild('06_rad_indoor.png')
    tasten('s')
    bild('07_indoor_bereit.png')
    tasten('b')
    bild('08_zurueck_untermenue.png')
    tasten('b')
    bild('09_zurueck_hauptmenue.png')
    # Das Schwimmen: vier Zeilen unter Bike, dasselbe Untermenue mit der anderen Gruppe.
    tasten('u', 4)
    bild('10_haupt_schwimmen.png')
    tasten('s')
    bild('11_schwimmen_pool.png')
    tasten('u')
    bild('12_schwimmen_freiwasser.png')
    tasten('u')
    bild('13_schwimmen_freiwasser_gps.png')
    tasten('s')
    bild('14_freiwasser_gps_bereit.png')
    tasten('b')
    bild('15_zurueck_schwimmen.png')
    tasten('b')
    bild('16_zurueck_hauptmenue.png')
    # Das offene Training, ohne und mit GPS, direkt aus dem Hauptmenue.
    tasten('u', 2)
    bild('17_haupt_offen.png')
    tasten('s')
    bild('18_offen_bereit.png')
    tasten('b')
    tasten('u')
    bild('19_haupt_offen_gps.png')
    tasten('s')
    bild('20_offen_gps_bereit.png')
    tasten('b')
    # Wieder zu Bike: das geteilte Untermenue zeigt die Raeder, von der ersten Zeile an.
    tasten('o', 7)
    tasten('s')
    bild('21_wieder_bike.png')
    notiz('QEMU laeuft noch: ' + str(emu.laeuft()))
finally:
    emu.stoppen()
    (aus / 'protokoll.txt').write_text('\n'.join(protokoll) + '\n')
