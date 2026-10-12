"""AP9 Diagnose: nur die ersten Starts nach dem Boot im Pruefbau, mit allen Stempeln.

Wie qemu_health.py bis einschliesslich vorbereiten(), dann `n` Starts aus dem Startmenue; gibt je
Start alle Stempel relativ zu "main" aus (ms). Aufruf: python3 erster_start.py <bau> [n]
"""
import pathlib
import sys
import tempfile
import time

sys.path.insert(0, '/home/user/pebbleos-v2/tools/boulder')
import qemu_health as q  # noqa: E402

bau = pathlib.Path(sys.argv[1])
n = int(sys.argv[2]) if len(sys.argv) > 2 else 3
pause = float(sys.argv[3]) if len(sys.argv) > 3 else 0.0
with tempfile.TemporaryDirectory() as tmp:
    emu = q.Emulator(bau, pathlib.Path(tmp))
    emu.starten()
    k = None
    try:
        time.sleep(20)
        k = q.Konsole(bau)
        m = q.Messung(emu, k, pathlib.Path(tmp))
        m.vorbereiten()
        if pause:
            print(f'Pause {pause} s', flush=True)
            print('vorher: ' + ' | '.join(z for z in k.befehl('boulder zeiten') if 'Boulder' in z)[:900], flush=True)
            time.sleep(pause)
        for i in range(n):
            st, _ = m.app_auf(f'app launch {q.APP_HEALTH}')
            st = st or []
            t0 = next((t for t, s in st if s == 'main'), None)
            print(f'Start {i + 1}: ' + ', '.join(f'{s.split()[0]} {q.ms(t - t0):.1f}'
                                                 for t, s in st if t0 is not None), flush=True)
            m.app_zu()
    finally:
        if k:
            k.close()
        emu.stoppen()
