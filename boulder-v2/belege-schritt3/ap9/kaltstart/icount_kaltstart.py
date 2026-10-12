"""AP9 Diagnose: Kaltstart mit -icount (Gastzeit nach ausgefuehrten Befehlen statt Host-Uhr).
Ist der erste Start dann stabil, kommt seine Schwankung aus QEMU (Uebersetzen beim ersten Lauf),
nicht aus der App. Aufruf: python3 icount_kaltstart.py <bau> [shift]"""
import pathlib
import shutil
import socket
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, '/home/user/pebbleos-v2/tools/boulder')
import qemu_health as q  # noqa: E402

SHIFT = sys.argv[2] if len(sys.argv) > 2 else '3'


class IcountEmulator(q.Emulator):
    def starten(self):
        flash = self.arbeit / 'qemu_spi_flash.bin'
        shutil.copy(self.bau / 'qemu_spi_flash.bin', flash)
        self.monitor = self.bau / 'qemu-mon.sock'
        befehl = [
            shutil.which('qemu-pebble'), '-rtc', 'base=localtime',
            '-icount', f'shift={SHIFT},align=off,sleep=on',
            '-monitor', f'unix:{self.monitor},server=on,wait=off',
            '-qmp', f'unix:{self.bau / "qmp.sock"},server=on,wait=off',
            '-serial', f'file:{self.arbeit / "uart1.log"}',
            '-serial', 'tcp::12344,server=on,wait=off',
            '-serial', f'tcp::{q.KONSOLE_PORT},server=on,wait=off',
            '-machine', 'pebble-emery',
            '-kernel', str(self.bau / 'pebbleos.elf'),
            '-drive', f'if=mtd,format=raw,file={flash}',
            '-display', 'none',
        ]
        self.prozess = subprocess.Popen(befehl, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        frist = time.time() + 60
        while time.time() < frist:
            if self.prozess.poll() is not None:
                raise SystemExit('QEMU beendet: ' + self.prozess.stderr.read().decode()[-500:])
            try:
                socket.create_connection(('127.0.0.1', q.KONSOLE_PORT), timeout=1).close()
                return
            except OSError:
                time.sleep(0.5)
        raise SystemExit('QEMU antwortet nicht')


def stempel(st):
    st = st or []
    t0 = next((t for t, s in st if s == 'main'), None)
    return {s.split()[0]: q.ms(t - t0) for t, s in st if t0 is not None}


bau = pathlib.Path(sys.argv[1])
with tempfile.TemporaryDirectory() as tmp:
    emu = IcountEmulator(bau, pathlib.Path(tmp))
    emu.starten()
    k = None
    try:
        time.sleep(20)
        k = q.Konsole(bau)
        m = q.Messung(emu, k, pathlib.Path(tmp))
        m.vorbereiten()
        erg = []
        for i in range(3):
            st, _ = m.app_auf(f'app launch {q.APP_HEALTH}')
            a = stempel(st)
            erg.append(f'{i + 1}: start {a.get("start", -1):.1f} daten {a.get("daten", -1):.1f} '
                       f'bild {a.get("bild", -1):.1f}')
            m.app_zu()
        print(f'ERGEBNIS shift {SHIFT} | ' + ' | '.join(erg), flush=True)
    finally:
        if k:
            k.close()
        emu.stoppen()
