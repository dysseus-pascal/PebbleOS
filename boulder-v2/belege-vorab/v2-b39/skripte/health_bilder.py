#!/usr/bin/env python3
"""b39: Pebbles Settings > Health in QEMU (Pruefbau build-pruef), mit Bildern.

Seit b39 zeigen zwei Zeilen von Pebbles settings/health.c Boulders Inhalt ueber Haken
(boulder_health_zeilen.c): "HR Monitoring" die Messstufe (Precision), "SpO2 Monitoring" den
Schalter SpO2 im Schlaf. Das Skript oeffnet die Einstellungen, geht zu Health, schaltet Health
Tracking ein (ohne zeigt Pebble nur diese Zeile), nimmt jede Zeile gewaehlt auf, schaltet SpO2 im
Schlaf ein und aus und oeffnet die Auswahl der Messstufe.

Aufruf: health_bilder.py <ausgabeordner> [<arbeitsordner fuer QEMU>]
"""
import pathlib
import sys
import time

REPO = pathlib.Path('/home/user/pebbleos-v2')
sys.path.insert(0, str(REPO / 'tools' / 'boulder'))
import qemu_health as q  # noqa: E402

APP_EINSTELLUNGEN = -7
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
    notiz('app launch: ' + ' | '.join(k.befehl(f'app launch {APP_EINSTELLUNGEN}')))
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
            time.sleep(0.3)

    bild('01_einstellungen.png')
    tasten('u', 8)
    bild('02_einstellungen_health.png')
    tasten('s')
    time.sleep(1.0)
    # Ohne Health Tracking zeigt Pebble nur diese eine Zeile; Select schaltet es ein.
    bild('03_health_tracking_aus.png')
    tasten('s')
    bild('04_health_tracking_an.png')
    for name in ('05_distanz', '06_precision', '07_hr_aktivitaet', '08_spo2', '09_spo2_schlaf',
                 '10_spo2_aktivitaet'):
        tasten('u')
        bild(f'{name}.png')
    # SpO2 im Schlaf: eine Zeile hinauf, Select schaltet ein, noch einmal aus.
    tasten('o')
    tasten('s')
    bild('11_spo2_schlaf_an.png')
    tasten('s')
    bild('12_spo2_schlaf_aus.png')
    # Die Messstufe: drei Zeilen hinauf zu Precision, Select oeffnet die Auswahl.
    tasten('o', 3)
    bild('13_precision_gewaehlt.png')
    tasten('s')
    time.sleep(1.0)
    bild('14_precision_auswahl.png')
    tasten('b')
    bild('15_zurueck_health.png')
    notiz('QEMU laeuft noch: ' + str(emu.laeuft()))
finally:
    emu.stoppen()
    (aus / 'protokoll.txt').write_text('\n'.join(protokoll) + '\n')
