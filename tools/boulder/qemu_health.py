#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0
"""Boulder: die Health-App im QEMU-Pruefbau messen (Schritt 3, AP8).

Aufruf (aus dem venv, SDK auf dem PATH):
  pbl configure -b build-pruef --board qemu_emery -DCONFIG_BOULDER_PRUEFLAUF=y \\
      -DCONFIG_BOULDER_HEALTH_ZEITEN=y && pbl build -b build-pruef
  python3 tools/boulder/qemu_health.py build-pruef --aus <ordner>

Startet QEMU ohne Fenster auf einer Kopie des Flash-Abbilds, spielt Pruefdaten ein (Tracking an,
Pruefnacht, SpO2, Schritte) und misst:
  L1  Start bis zum ersten Bild mit Werten, je Einstieg 20-mal         <= 50 ms
      (der erste Start nach dem Boot in einem zweiten Boot unter -icount, siehe ICOUNT_SHIFT)
  L2  Taste bis zum Bild der naechsten Karte oder des Details          <= 30 ms
  L4  Auffrischen im Minutentakt bis zum Bild                          <= 30 ms
  L6  jeder Syscall der Daten (Mittel aus 100 Aufrufen)                <= 1 ms
dazu den Hoechststand des App-Heaps, den Kernel-Heap vor und nach 100 Starts, das Puls-Abo im
Sekundentakt auf Heart und nach dem Ende, den Tiefstand des Syscall-Stapels, Neustarts und die
Fehlerprobe am Endstand. Ergebnis: Tabelle auf stdout, messung.json und Bilder im Ordner --aus.

Die Zeiten kommen aus den Zeitstempeln der App ("bh t=<ticks> <stelle>", zeiten.h; 1024 Ticks je
Sekunde), gelesen ueber die Konsole (PULSE wie pbl console). Rueckgabe 1, wenn ein Ziel verfehlt
ist oder eine Pruefung scheitert.

QEMU ist nicht obelix: Die Zahlen belegen die Bauart (kein Lesen an der Taste, kleine Kopien statt
Minutenverlauf), nicht die Zeit auf der Uhr. Die misst der Geraetetest.
"""
import argparse
import datetime
import json
import os
import pathlib
import re
import shutil
import socket
import statistics
import subprocess
import sys
import tempfile
import threading
import time

REPO = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / 'tools'))

from log_hashing.logdehash import LogDehash  # noqa: E402
from pebble.commander.commander import PebbleCommander  # noqa: E402
from pebble.commander.exceptions import CommandTimedOut  # noqa: E402

TICKS_HZ = 1024
ZIELE_MS = {'L1': 50.0, 'L2': 30.0, 'L4': 30.0, 'L6': 1.0}
# DER ERSTE START NACH DEM BOOT ZAEHLT UNTER -icount (F17): In Echtzeit enthaelt er, dass QEMU jeden
# Codepfad beim ersten Lauf uebersetzt, und schwankt mit dem Host (30-54 ms in AP9). Unter -icount
# laeuft die Gastzeit nach ausgefuehrten Befehlen (2^3 ns je Befehl); es zaehlt nur, was die App tut.
ICOUNT_SHIFT = 3
APP_HEALTH = -82
KONSOLE_PORT = 12345
# Die Pruefnacht soll von heute sein: Die Uhr steht am Abend nach ihr (Wachtag = heute).
UHR_UTC = int(datetime.datetime(2026, 10, 11, 20, 0, tzinfo=datetime.timezone.utc).timestamp())
STEMPEL = re.compile(r'bh t=(\d+) (.*)$')
FARBE = re.compile(r'\x1b\[[0-9;]*m')


class Emulator:
    """QEMU ohne Fenster: Tasten ueber den Monitor, Bilder ueber pbl screenshot."""

    def __init__(self, bau, arbeit, icount=None):
        self.bau = bau
        self.arbeit = arbeit
        self.icount = icount
        self.prozess = None

    def starten(self):
        # Eine Kopie des Flash-Abbilds: Jeder Lauf beginnt gleich, das Abbild im Bauordner bleibt.
        flash = self.arbeit / 'qemu_spi_flash.bin'
        shutil.copy(self.bau / 'qemu_spi_flash.bin', flash)
        self.monitor = self.bau / 'qemu-mon.sock'
        qemu = shutil.which('qemu-pebble')
        if not qemu:
            raise SystemExit('qemu-pebble fehlt auf dem PATH (SDK: env.sh)')
        befehl = [
            qemu, '-rtc', 'base=localtime',
            '-monitor', f'unix:{self.monitor},server=on,wait=off',
            '-qmp', f'unix:{self.bau / "qmp.sock"},server=on,wait=off',
            '-serial', f'file:{self.arbeit / "uart1.log"}',
            '-serial', 'tcp::12344,server=on,wait=off',
            '-serial', f'tcp::{KONSOLE_PORT},server=on,wait=off',
            '-machine', 'pebble-emery',
            '-kernel', str(self.bau / 'pebbleos.elf'),
            '-drive', f'if=mtd,format=raw,file={flash}',
            '-display', 'none',
        ]
        if self.icount is not None:
            befehl[1:1] = ['-icount', f'shift={self.icount},align=off,sleep=on']
        self.prozess = subprocess.Popen(befehl, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        frist = time.time() + 60
        while time.time() < frist:
            try:
                socket.create_connection(('127.0.0.1', KONSOLE_PORT), timeout=1).close()
                return
            except OSError:
                time.sleep(0.5)
        raise SystemExit('QEMU antwortet nicht')

    def laeuft(self):
        return self.prozess and self.prozess.poll() is None

    def stoppen(self):
        if self.laeuft():
            self.prozess.terminate()
            self.prozess.wait(10)

    def taste(self, k, halten_ms=100):
        namen = {'b': 'left', 's': 'right', 'o': 'up', 'u': 'down'}
        s = socket.socket(socket.AF_UNIX)
        s.connect(str(self.monitor))
        s.settimeout(1.0)
        try:
            s.recv(4096)  # die Begruessung des Monitors; vorher geht eine Taste verloren
        except socket.timeout:
            pass
        s.sendall(f'sendkey {namen[k]} {halten_ms}\n'.encode())
        time.sleep(0.3)
        s.close()

    def bild(self, pfad):
        subprocess.run(['pbl', 'screenshot', '-b', str(self.bau), '--output', str(pfad)],
                       check=True, capture_output=True, cwd=REPO)


class Konsole:
    """Pruefbefehle und das Protokoll ueber PULSE, mit der Ankunftszeit jeder Zeile."""

    def __init__(self, bau):
        self.cmdr = PebbleCommander(tty=f'socket://localhost:{KONSOLE_PORT}')
        self.dehash = LogDehash(str(bau / 'fw' / 'loghash_dict.json'))
        self.zeilen = []
        self.lock = threading.Lock()
        self.cmdr.attach_log_listener(self._log)

    def _log(self, msg):
        zeile = self.dehash.commander_format_line(self.dehash.dehash(msg))
        zeile = FARBE.sub('', zeile)  # die Konsole faerbt Zeilen ein
        with self.lock:
            self.zeilen.append(zeile)

    def befehl(self, cmd, versuche=5):
        # Waehrend die Pruefnacht im Zeitraffer laeuft, antwortet die Konsole manchmal zu spaet.
        for versuch in range(versuche):
            try:
                return self.cmdr.send_prompt_command(cmd) or []
            except CommandTimedOut:
                if versuch == versuche - 1:
                    raise
                time.sleep(2)

    def stand(self):
        with self.lock:
            return len(self.zeilen)

    def ab(self, stand):
        with self.lock:
            return list(self.zeilen[stand:])

    def warte(self, muster, stand, frist=10.0):
        """Die erste Zeile ab `stand`, die `muster` trifft; None nach der Frist."""
        regex = re.compile(muster)
        ende = time.time() + frist
        while time.time() < ende:
            for z in self.ab(stand):
                if regex.search(z):
                    return z
            time.sleep(0.05)
        return None

    def close(self):
        self.cmdr.close()


def stempel(zeilen):
    """[(ticks, stelle)] aus den Zeilen der App."""
    aus = []
    for z in zeilen:
        m = STEMPEL.search(z)
        if m:
            aus.append((int(m.group(1)), m.group(2).strip()))
    return aus


def ms(ticks):
    return ticks * 1000.0 / TICKS_HZ


def spanne(stempel_liste, von, bis):
    """Zeit von der ersten Stelle, die mit `von` beginnt, bis zur naechsten `bis` danach."""
    for i, (t, stelle) in enumerate(stempel_liste):
        if stelle.startswith(von):
            for t2, stelle2 in stempel_liste[i + 1:]:
                if stelle2.startswith(bis):
                    return ms(t2 - t)
            return None
    return None


def zahl(muster, zeilen):
    for z in zeilen:
        m = re.search(muster, z)
        if m:
            return int(m.group(1))
    return None


class Messung:
    def __init__(self, emu, konsole, aus):
        self.emu = emu
        self.k = konsole
        self.aus = aus
        self.werte = {'L1': {}, 'L2': [], 'L4': [], 'L6': {}}
        self.pruefungen = []

    def pruefe(self, name, ok, wert=''):
        self.pruefungen.append({'pruefung': name, 'ok': bool(ok), 'wert': wert})

    def vorbereiten(self):
        stand = self.k.stand()
        for c in ['boulder tracking', f'boulder zeit {UHR_UTC}', 'boulder prueflauf 0']:
            self.k.befehl(c)
        gestellt = time.time()
        # Die Pruefnacht laeuft im Zeitraffer; fertig ist sie, wenn S5 sie fuehrt.
        time.sleep(30)
        frist = time.time() + 120
        while time.time() < frist:
            if zahl(r'anzahl (\d+)', self.k.befehl('boulder naechte')):
                break
            time.sleep(3)
        # PEBBLES TAGESWECHSEL ABWARTEN: Die gestellte Uhr ist fuer Pebble ein anderer Tag, an der
        # naechsten Minutengrenze schiebt es die Werte von heute nach gestern (activity_metrics.c).
        # Vorher gesetzte Schritte stuenden dann auf gestern. Am selben Datum kommt die Zeile nicht.
        self.k.warte(r'resetting metrics for new day', stand, max(0.0, gestellt + 90 - time.time()))
        for c in ['boulder spo2 40', 'boulder schritte 6230']:
            self.k.befehl(c)

    def app_auf(self, befehl):
        stand = self.k.stand()
        self.k.befehl(befehl)
        if not self.k.warte(r'bh t=\d+ bild', stand, 5):
            return None, stand
        time.sleep(0.3)
        return stempel(self.k.ab(stand)), stand

    def app_zu(self):
        self.emu.taste('b')
        time.sleep(0.8)

    def l1(self, je_einstieg=20):
        einstiege = {
            'Startmenue (app launch)': f'app launch {APP_HEALTH}',
            'Pin Night': 'boulder start 1',
            'Pin Activity': 'boulder start 0',
        }
        for name, befehl in einstiege.items():
            werte = []
            for _ in range(je_einstieg):
                st, _ = self.app_auf(befehl)
                w = spanne(st or [], 'main', 'bild')
                if w is not None:
                    werte.append(w)
                self.app_zu()
            self.pruefe(f'L1 {name}: {len(werte)} von {je_einstieg} Starts gemessen',
                        len(werte) == je_einstieg)
            if not self.werte['L1'] and werte:
                # Der erste Start nach dem Boot: hier nur zur Auskunft (ICOUNT_SHIFT).
                self.erster_start_echtzeit = werte.pop(0)
            self.werte['L1'][name] = werte

    def l2(self, runden=4):
        st, _ = self.app_auf(f'app launch {APP_HEALTH}')
        for _ in range(runden):
            for _karte in range(5):
                stand = self.k.stand()
                self.emu.taste('o')
                self.k.warte(r'bh t=\d+ bild', stand, 3)
                w = spanne(stempel(self.k.ab(stand)), 'taste', 'bild')
                if w is not None:
                    self.werte['L2'].append(('Karte', w))
                # Select: auf Today nichts, sonst das Detail
                stand = self.k.stand()
                self.emu.taste('s')
                if self.k.warte(r'bh t=\d+ (bild|nichts)', stand, 3):
                    time.sleep(0.2)
                    st = stempel(self.k.ab(stand))
                    w = spanne(st, 'taste', 'bild') if any(x.startswith('detail') for _, x in st) else None
                    if w is not None:
                        self.werte['L2'].append(('Detail', w))
                        # Getrennt: bis das Detail zu zeichnen beginnt (Fenster, Uebergang), und
                        # das Zeichnen selbst.
                        self.werte['L2'].append(('Detail bis Zeichnen', spanne(st, 'taste', 'zeichnen')))
                        self.werte['L2'].append(('Detail zeichnen', spanne(st, 'zeichnen', 'bild')))
                        self.emu.taste('b')
                        time.sleep(0.6)
        self.app_zu()

    def l4(self, minuten=3):
        self.app_auf(f'app launch {APP_HEALTH}')
        stand = self.k.stand()
        ende = time.time() + minuten * 60 + 20
        while time.time() < ende:
            time.sleep(5)
        st = stempel(self.k.ab(stand))
        for i, (t, stelle) in enumerate(st):
            if stelle == 'auffrischen':
                w = spanne(st[i:], 'auffrischen', 'bild')
                if w is not None:
                    self.werte['L4'].append(w)
        self.app_zu()
        self.pruefe(f'L4: {len(self.werte["L4"])} Minuten gemessen', len(self.werte['L4']) >= minuten)

    def l6(self):
        stand = self.k.stand()
        self.k.befehl('boulder syscalls')
        self.k.warte(r'bh syscall fertig', stand, 20)
        for z in self.k.ab(stand):
            m = re.search(r'bh syscall (\w+) (\d+)', z)
            if m and m.group(1) != 'fertig':
                self.werte['L6'][m.group(1)] = ms(int(m.group(2))) / 100.0
        self.pruefe('L6: neun Syscalls gemessen', len(self.werte['L6']) == 9)

    def bilder(self):
        """Jede Karte und jedes Detail, dazu der Hoechststand des App-Heaps danach."""
        ordner = self.aus / 'bilder'
        ordner.mkdir(parents=True, exist_ok=True)
        self.app_auf(f'app launch {APP_HEALTH}')
        for name in ['today', 'night', 'heart', 'activity', 'settings']:
            self.k.befehl('backlight test')
            time.sleep(0.5)
            self.emu.bild(ordner / f'k_{name}.png')
            if name != 'today':
                self.emu.taste('s')
                time.sleep(1.0)
                self.k.befehl('backlight test')
                self.emu.bild(ordner / f'd_{name}.png')
                for _ in range(4):
                    self.emu.taste('u')
                time.sleep(0.6)
                self.emu.bild(ordner / f'd_{name}_unten.png')
                self.emu.taste('b')
                time.sleep(0.6)
            self.emu.taste('o')
            time.sleep(0.6)
        app = self.k.befehl('boulder app')
        self.heap_hoch = zahl(r'Hoechststand (\d+)', app)
        self.heap_gross = zahl(r'App-Heap: (\d+)', app)
        self.app_zu()

    def puls(self):
        self.app_auf(f'app launch {APP_HEALTH}')
        self.emu.taste('o')
        self.emu.taste('o')  # Heart
        time.sleep(1.0)
        auf_heart = zahl(r'Sekundentakt: (\d)', self.k.befehl('boulder app'))
        self.emu.taste('o')  # Activity
        time.sleep(1.0)
        weg = zahl(r'Sekundentakt: (\d)', self.k.befehl('boulder app'))
        self.emu.taste('u')  # zurueck auf Heart, dann die App schliessen
        time.sleep(0.6)
        self.app_zu()
        time.sleep(1.0)
        nach_ende = zahl(r'Sekundentakt: (\d)', self.k.befehl('boulder app'))
        self.pruefe('Puls im Sekundentakt auf Heart', auf_heart == 1, auf_heart)
        self.pruefe('kein Puls im Sekundentakt nach Verlassen von Heart', weg == 0, weg)
        self.pruefe('kein Puls im Sekundentakt nach dem Ende auf Heart', nach_ende == 0, nach_ende)

    def kernel_heap(self, starts=100):
        """Freier Kernel-Heap vor, nach der Haelfte und nach allen Starts. Ein Leck der App waechst
        mit jedem Start; der Rest des Systems belegt nebenher auch etwas (Minutendaten, Takt).
        Darum zaehlt die zweite Haelfte: Sie muss so viel frei lassen wie die erste."""
        def frei():
            time.sleep(2)
            return zahl(r'Heap: \d+ B gesamt, (\d+) frei', self.k.befehl('boulder zeiten'))
        stufen = [frei()]
        for haelfte in range(2):
            for _ in range(starts // 2):
                self.app_auf(f'app launch {APP_HEALTH}')
                self.app_zu()
            stufen.append(frei())
        self.heap_kern = stufen
        erste, zweite = stufen[0] - stufen[1], stufen[1] - stufen[2]
        self.pruefe(f'Kernel-Heap: kein Wachstum mit den Starts ({starts})',
                    zweite <= 0 or zweite < starts // 2,
                    f'frei {stufen[0]} -> {stufen[1]} -> {stufen[2]} B')

    def stapel(self):
        frei = zahl(r'App (\d+) B', self.k.befehl('boulder stapel'))
        self.pruefe('Syscall-Stapel der App: Reserve', frei is not None and frei >= 512,
                    f'{frei} B nie benutzt')

    def fehlerprobe(self):
        stand = self.k.stand()
        self.k.befehl('boulder fehlerprobe')
        time.sleep(8)
        lebt = bool(self.k.befehl('boulder lage'))
        geendet = self.k.warte(r'(?i)(memmanage|app fault|DACCVIOL)', stand, 1)
        # Danach noch ein Start: Seine Ticks setzen die des Laufs fort, wenn die Uhr nicht neu
        # gestartet hat (ein Neustart setzt sie auf 0).
        self.app_auf(f'app launch {APP_HEALTH}')
        self.app_zu()
        self.pruefe('Fehlerprobe: die Uhr laeuft weiter', lebt and self.emu.laeuft())
        self.pruefe('Fehlerprobe: die App endet am Fehler', geendet is not None,
                    (geendet or '')[-60:])

    def kein_neustart(self):
        ticks = [t for t, _ in stempel(self.k.ab(0))]
        fallend = [(a, b) for a, b in zip(ticks, ticks[1:]) if b < a]
        self.pruefe('kein Neustart im ganzen Lauf (Ticks steigen)', not fallend and len(ticks) > 100,
                    f'{len(ticks)} Stempel, fallend: {fallend[:3]}')


def zusammenfassen(m, erster_start_icount):
    def kennzahl(werte):
        return {'n': len(werte), 'max': round(max(werte), 2) if werte else None,
                'median': round(statistics.median(werte), 2) if werte else None}
    l1_alle = [w for v in m.werte['L1'].values() for w in v]
    l2 = [w for a, w in m.werte['L2'] if a in ('Karte', 'Detail')]
    ergebnis = {
        'L1': {name: kennzahl(v) for name, v in m.werte['L1'].items()},
        'L2': {art: kennzahl([w for a, w in m.werte['L2'] if a == art and w is not None])
               for art in ('Karte', 'Detail', 'Detail bis Zeichnen', 'Detail zeichnen')},
        'L1_erster_start': {'echtzeit': getattr(m, 'erster_start_echtzeit', None),
                            f'icount_shift_{ICOUNT_SHIFT}': erster_start_icount},
        'L4': kennzahl(m.werte['L4']),
        'L6': {name: round(w, 3) for name, w in m.werte['L6'].items()},
        'app_heap': {'gross': getattr(m, 'heap_gross', None), 'hoechststand': getattr(m, 'heap_hoch', None)},
        'kernel_heap_frei': getattr(m, 'heap_kern', None),
        'laengster_callback_ms': round(max(l1_alle + l2 + m.werte['L4']), 2) if l1_alle else None,
        'pruefungen': m.pruefungen,
    }
    ziele = [
        ('L1', 'L1', max(l1_alle) if l1_alle else None),
        ('L1 erster Start (icount)', 'L1', erster_start_icount[0] if erster_start_icount else None),
        ('L2', 'L2', max(l2) if l2 else None),
        ('L4', 'L4', max(m.werte['L4']) if m.werte['L4'] else None),
        ('L6', 'L6', max(m.werte['L6'].values()) if m.werte['L6'] else None),
    ]
    ergebnis['ziele'] = [{'ziel': name, 'hoechstwert_ms': None if w is None else round(w, 3),
                          'grenze_ms': ZIELE_MS[z], 'ok': w is not None and w <= ZIELE_MS[z]}
                         for name, z, w in ziele]
    return ergebnis


def erster_start_icount(bau, starts=3):
    """Ein eigener Boot unter -icount: die ersten Starts danach (ms bis zum Bild, None ohne Bild)."""
    with tempfile.TemporaryDirectory() as tmp:
        emu = Emulator(bau, pathlib.Path(tmp), icount=ICOUNT_SHIFT)
        emu.starten()
        k = None
        try:
            time.sleep(20)  # Boot bis zum Zifferblatt
            k = Konsole(bau)
            m = Messung(emu, k, pathlib.Path(tmp))
            m.vorbereiten()
            werte = []
            for _ in range(starts):
                st, _ = m.app_auf(f'app launch {APP_HEALTH}')
                werte.append(spanne(st or [], 'main', 'bild'))
                m.app_zu()
            return werte
        finally:
            if k:
                k.close()
            emu.stoppen()


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('bau', type=pathlib.Path, help='Bauordner des Pruefbaus (qemu_emery)')
    p.add_argument('--aus', type=pathlib.Path, required=True, help='Ordner fuer messung.json und Bilder')
    p.add_argument('--starts', type=int, default=20, help='Starts je Einstieg fuer L1')
    p.add_argument('--heap-starts', type=int, default=100, help='Starts fuer den Kernel-Heap')
    args = p.parse_args()
    bau = args.bau.resolve()
    args.aus.mkdir(parents=True, exist_ok=True)
    konfig = (bau / 'autoconf.h').read_text() if (bau / 'autoconf.h').exists() else ''
    for schalter in ('CONFIG_BOULDER_PRUEFLAUF', 'CONFIG_BOULDER_HEALTH_ZEITEN'):
        if f'#define {schalter} 1' not in konfig:
            raise SystemExit(f'{bau} ist kein Pruefbau: {schalter} fehlt')

    with tempfile.TemporaryDirectory() as tmp:
        emu = Emulator(bau, pathlib.Path(tmp))
        emu.starten()
        k = None
        try:
            time.sleep(20)  # Boot bis zum Zifferblatt
            k = Konsole(bau)
            m = Messung(emu, k, args.aus)
            m.vorbereiten()
            m.l1(args.starts)
            m.l2()
            m.l4()
            m.l6()
            m.bilder()
            m.puls()
            m.kernel_heap(args.heap_starts)
            m.stapel()
            m.fehlerprobe()
            m.kein_neustart()
            m.pruefe('QEMU laeuft am Ende noch', emu.laeuft())
            protokoll = [z for z in k.ab(0) if 'bh ' in z]
            (args.aus / 'protokoll.txt').write_text('\n'.join(protokoll) + '\n')
        finally:
            if k:
                k.close()
            emu.stoppen()
    # Erst nach dem Ende des ersten: Beide Boots nutzen dieselben Ports und Sockets.
    ergebnis = zusammenfassen(m, erster_start_icount(bau))

    (args.aus / 'messung.json').write_text(json.dumps(ergebnis, indent=2, ensure_ascii=False) + '\n')
    for z in ergebnis['ziele']:
        print(f"{'ok  ' if z['ok'] else 'ROT '} {z['ziel']}: hoechstens {z['hoechstwert_ms']} ms "
              f"(Grenze {z['grenze_ms']} ms)")
    for pr in ergebnis['pruefungen']:
        print(f"{'ok  ' if pr['ok'] else 'ROT '} {pr['pruefung']} {pr['wert']}")
    print(json.dumps({k: ergebnis[k] for k in ('L1', 'L2', 'L4', 'L6', 'app_heap', 'kernel_heap_frei',
                                               'laengster_callback_ms')}, indent=2,
                     ensure_ascii=False))
    alles = all(z['ok'] for z in ergebnis['ziele']) and all(p['ok'] for p in ergebnis['pruefungen'])
    return 0 if alles else 1


if __name__ == '__main__':
    sys.exit(main())
