"""AP9 Diagnose: Kaltstart mit Host-Kalibrierung. Vor und nach dem ersten Start eine feste
Rechenarbeit (Python, ein Kern) und der Steal-Anteil aus /proc/stat; dazu die Stempel des ersten
und zweiten Starts. Aufruf: python3 kaltstart_kal.py <bau>"""
import pathlib
import sys
import tempfile
import time

sys.path.insert(0, '/home/user/pebbleos-v2/tools/boulder')
import qemu_health as q  # noqa: E402


def kalibrieren():
    # Feste Arbeit; die Dauer in ms ist das Mass fuer die Host-Geschwindigkeit in diesem Moment.
    t = time.perf_counter()
    s = 0
    for i in range(2_000_000):
        s += i * i
    return (time.perf_counter() - t) * 1000


def cpu():
    w = [int(x) for x in open('/proc/stat').readline().split()[1:]]
    return sum(w), w[7]


def stempel(st):
    st = st or []
    t0 = next((t for t, s in st if s == 'main'), None)
    return {s.split()[0]: q.ms(t - t0) for t, s in st if t0 is not None}


bau = pathlib.Path(sys.argv[1])
with tempfile.TemporaryDirectory() as tmp:
    emu = q.Emulator(bau, pathlib.Path(tmp))
    emu.starten()
    k = None
    try:
        time.sleep(20)
        k = q.Konsole(bau)
        m = q.Messung(emu, k, pathlib.Path(tmp))
        m.vorbereiten()
        kal_vor = min(kalibrieren() for _ in range(3))
        ges0, steal0 = cpu()
        st1, _ = m.app_auf(f'app launch {q.APP_HEALTH}')
        ges1, steal1 = cpu()
        kal_nach = min(kalibrieren() for _ in range(3))
        m.app_zu()
        st2, _ = m.app_auf(f'app launch {q.APP_HEALTH}')
        m.app_zu()
        a, b = stempel(st1), stempel(st2)
        steal = 100.0 * (steal1 - steal0) / max(1, ges1 - ges0)
        print(f'ERGEBNIS kal_vor {kal_vor:.0f} kal_nach {kal_nach:.0f} steal {steal:.1f}% '
              f'kalt start {a.get("start", -1):.1f} daten {a.get("daten", -1):.1f} '
              f'bild {a.get("bild", -1):.1f} warm bild {b.get("bild", -1):.1f}', flush=True)
    finally:
        if k:
            k.close()
        emu.stoppen()
