#!/usr/bin/env python3
"""b37: SpO2 im Schlaf in QEMU (Pruefbau build-pruef), mit Bildern.

1. Menue: Settings -> Health. Die Zeile "Blood O2 During Sleep" steht nach "Blood Oxygen"; Select
   schaltet sie (Bilder: aus, an, aus, an).
2. Pebbles SpO2-Plan mit "Blood Oxygen" aus und "Schlaf" an, Stufe 4 (SpO2-Takt 10 min). Die Uhr
   liegt nicht flach: eine Beschleunigung (0, -1000, 0) ueber QEMUs Sensorkanal (Port 12344, wie
   "pebble emu-accel"), sonst misst Pebble nie. Tags (14:00 UTC) bleibt das SpO2-Abo des Plans ohne
   Merkmale, auch nach dem Takt; nachts (23:00 UTC, ruhig: QEMU bewegt sich nicht) misst es
   (Merkmal 0x4, Takt 1 s). Abgefragt mit dem Pruefbefehl "kieselsport abo" (Pebbles HRM-Abos).
3. Gegenprobe nachts: "Schlaf" aus, ein ganzer Takt ohne Messung. Am Ende Pebbles Maske:
   "hrm spo2" ohne Schalter abgelehnt, mit "Schlaf" angenommen.
QEMUs Pulssensor ist eine Attrappe ohne Werte: Gezeigt wird, dass der Plan misst (das Abo mit
SpO2), nicht ein Wert.

Aufruf: qemu_spo2.py <ausgabeordner> <arbeitsordner fuer QEMU>
"""
import datetime
import pathlib
import re
import socket
import sys
import time

REPO = pathlib.Path('/home/user/pebbleos-v2')
sys.path.insert(0, str(REPO / 'tools' / 'boulder'))
import qemu_health as q  # noqa: E402
from libpebble2.communication.transports.qemu.protocol import (  # noqa: E402
    QemuAccel, QemuAccelSample, QemuPacket)

APP_SETTINGS = -7
TAG = int(datetime.datetime(2026, 10, 22, 14, 0, tzinfo=datetime.timezone.utc).timestamp())
NACHT = int(datetime.datetime(2026, 10, 22, 23, 0, tzinfo=datetime.timezone.utc).timestamp())
ABO = re.compile(r'ABO ref (\d+) app (-?\d+) takt (\d+) rest (\d+) merkmale (0x[0-9a-f]+)')

aus = pathlib.Path(sys.argv[1])
arbeit = pathlib.Path(sys.argv[2])
aus.mkdir(parents=True, exist_ok=True)
arbeit.mkdir(parents=True, exist_ok=True)
bau = REPO / 'build-pruef'
protokoll = []
t_start = time.time()


def notiz(text):
    zeile = f'{time.strftime("%H:%M:%S")} +{time.time() - t_start:6.0f}s {text}'
    print(zeile, flush=True)
    protokoll.append(zeile)


def beschleunigung(x, y, z):
    paket = QemuPacket(data=QemuAccel(count=1, samples=[QemuAccelSample(x=x, y=y, z=z)]))
    with socket.create_connection(('127.0.0.1', 12344), timeout=5) as s:
        s.sendall(paket.serialise())
        time.sleep(0.5)


def abos(k):
    """{ref: (takt, merkmale)} aus "kieselsport abo"."""
    zeilen = k.befehl('kieselsport abo')
    return {int(m.group(1)): (int(m.group(3)), int(m.group(5), 16))
            for m in (ABO.search(z) for z in zeilen) if m}


def spo2_misst(liste):
    return sorted(ref for ref, (_, merkmale) in liste.items() if merkmale & 0x4)


emu = q.Emulator(bau, arbeit)
emu.starten()
try:
    time.sleep(20)
    k = q.Konsole(bau)

    def bild(name):
        k.befehl('backlight test')  # ohne Licht nimmt QEMU den Schirm abgedunkelt auf
        time.sleep(0.4)
        emu.bild(aus / name)
        notiz(f'Bild {name}')

    def menue_bis_zur_zeile():
        """Settings -> Health (Zeile 8), dann die Zeile nach "Blood Oxygen" (Zeile 5)."""
        for _ in range(3):  # erst zum Zifferblatt: Settings beginnt dann im Hauptmenue
            emu.taste('b')
        time.sleep(1.0)
        k.befehl(f'app launch {APP_SETTINGS}')
        time.sleep(2.0)
        for _ in range(8):
            emu.taste('u')
        emu.taste('s')
        time.sleep(1.0)
        for _ in range(5):
            emu.taste('u')
        time.sleep(0.6)

    notiz('tracking: ' + ' | '.join(k.befehl('boulder tracking')))
    notiz('zeit: ' + ' | '.join(k.befehl(f'boulder zeit {TAG}')))
    notiz('stufe: ' + ' | '.join(k.befehl('boulder stufe 4')))
    time.sleep(4)
    beschleunigung(0, -1000, 0)
    notiz('Beschleunigung (0, -1000, 0) gesendet: die Uhr steht auf der Kante, nicht flach')
    notiz('lage: ' + ' | '.join(k.befehl('boulder lage')))
    t_tracking = time.time()

    # 1. Das Menue
    menue_bis_zur_zeile()
    bild('menue_schlaf_aus.png')
    emu.taste('o')  # Blood Oxygen
    time.sleep(0.5)
    bild('menue_blood_oxygen.png')
    emu.taste('u')
    time.sleep(0.5)
    emu.taste('s')
    time.sleep(0.8)
    bild('menue_schlaf_an.png')
    emu.taste('s')
    time.sleep(0.8)
    bild('menue_schlaf_wieder_aus.png')
    emu.taste('s')
    time.sleep(0.8)
    bild('menue_schlaf_wieder_an.png')
    emu.taste('u')
    time.sleep(0.5)
    bild('menue_danach_activity.png')
    time.sleep(2)
    notiz('lage mit Schlaf: ' + ' | '.join(k.befehl('boulder lage')))
    notiz('abos am Anfang: ' + str(abos(k)))

    # 2a. Tags: kein Fenster, auch ueber den Takt (10 min nach dem Start des Trackings) hinaus.
    tags_gemessen = []
    while time.time() - t_tracking < 13 * 60:
        liste = abos(k)
        if spo2_misst(liste):
            tags_gemessen.append((round(time.time() - t_tracking), liste))
        time.sleep(10)
    notiz(f'TAGS (14:00, nur Schlaf an): {len(tags_gemessen)} Abfragen mit SpO2 in 13 min: '
          f'{tags_gemessen[:3]}')
    notiz('abos tags: ' + str(abos(k)))

    # 2b. Nachts: 23:00, ruhig.
    notiz('zeit: ' + ' | '.join(k.befehl(f'boulder zeit {NACHT}')))
    t_nacht = time.time()
    nachts = None
    while time.time() - t_nacht < 6 * 60:
        liste = abos(k)
        if spo2_misst(liste):
            nachts = (round(time.time() - t_nacht), liste)
            break
        time.sleep(5)
    notiz(f'NACHTS (23:00, Schlaf an): SpO2-Abo misst nach {nachts and nachts[0]} s: '
          f'{nachts and nachts[1]}')
    notiz('lage nachts: ' + ' | '.join(k.befehl('boulder lage')))
    plan_ref = spo2_misst(nachts[1])[0] if nachts else None
    # Bis das Fenster vorbei ist (Pebble bricht ohne Wert nach 45 s ab).
    ende = None
    while nachts and time.time() - t_nacht < 10 * 60:
        liste = abos(k)
        if not spo2_misst(liste):
            ende = round(time.time() - t_nacht)
            break
        time.sleep(5)
    notiz(f'Fenster vorbei nach {ende} s seit 23:00: {abos(k)}')

    # 3. Gegenprobe nachts: Schlaf aus, ein ganzer Takt (10 min) und mehr ohne Messung.
    menue_bis_zur_zeile()
    emu.taste('s')
    time.sleep(0.8)
    bild('menue_schlaf_aus_nachts.png')
    t_aus = time.time()
    aus_gemessen = []
    while time.time() - t_aus < 12 * 60:
        liste = abos(k)
        if spo2_misst(liste):
            aus_gemessen.append((round(time.time() - t_aus), liste))
        time.sleep(10)
    notiz(f'NACHTS OHNE SCHLAF: {len(aus_gemessen)} Abfragen mit SpO2 in 12 min: {aus_gemessen[:3]}')
    notiz('lage ohne Schlaf: ' + ' | '.join(k.befehl('boulder lage')))
    # Pebbles Maske: die Konsole bittet um SpO2 wie ein Abo im Hintergrund.
    notiz('hrm spo2 ohne Schalter: ' + ' | '.join(k.befehl('hrm spo2')))
    emu.taste('s')
    time.sleep(3)
    bild('menue_schlaf_an_am_ende.png')
    notiz('hrm spo2 mit Schlaf: ' + ' | '.join(k.befehl('hrm spo2')))
    time.sleep(2)
    notiz('abos am Ende: ' + str(abos(k)))
    zeilen = [z for z in k.ab(0) if re.search(r'(?i)spo2|messstufe|fault|assert|crash|reboot', z)]
    (aus / 'konsole.txt').write_text('\n'.join(zeilen) + '\n')
    notiz(f'Plan-Abo: ref {plan_ref}')
    k.close()
finally:
    emu.stoppen()
    (aus / 'ablauf.txt').write_text('\n'.join(protokoll) + '\n')
