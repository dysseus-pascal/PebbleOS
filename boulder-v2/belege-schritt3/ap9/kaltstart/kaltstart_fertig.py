"""AP9 Diagnose: Laeuft die Pruefnacht im Zeitraffer noch, wenn die App zum ersten Mal startet?
Wie kaltstart_kal.py; dazu, ob "Boulder Pruefbau <n> fertig" vor dem ersten Start im Protokoll
stand. Mit `warten` wartet es vor dem ersten Start auf diese Zeile.
Aufruf: python3 kaltstart_fertig.py <bau> [warten]"""
import pathlib
import re
import sys
import tempfile
import time

sys.path.insert(0, '/home/user/pebbleos-v2/tools/boulder')
import qemu_health as q  # noqa: E402

FERTIG = re.compile(r'Boulder Pruefbau \d+ fertig.*')


def stempel(st):
    st = st or []
    t0 = next((t for t, s in st if s == 'main'), None)
    return {s.split()[0]: q.ms(t - t0) for t, s in st if t0 is not None}


bau = pathlib.Path(sys.argv[1])
warten = len(sys.argv) > 2 and sys.argv[2] == 'warten'
with tempfile.TemporaryDirectory() as tmp:
    emu = q.Emulator(bau, pathlib.Path(tmp))
    emu.starten()
    k = None
    try:
        time.sleep(20)
        k = q.Konsole(bau)
        m = q.Messung(emu, k, pathlib.Path(tmp))
        t_vor = time.time()
        m.vorbereiten()
        t_vorbereitet = time.time() - t_vor
        gewartet = 0.0
        if warten:
            t = time.time()
            k.warte(FERTIG.pattern, 0, 300)
            gewartet = time.time() - t
        vorher = [z for z in k.ab(0) if FERTIG.search(z)]
        st1, stand = m.app_auf(f'app launch {q.APP_HEALTH}')
        time.sleep(1)
        nachher = [z for z in k.ab(stand) if FERTIG.search(z)]
        m.app_zu()
        if not vorher and not nachher:
            fertig = k.warte(FERTIG.pattern, stand, 300)
            nachher = [fertig] if fertig else []
        a = stempel(st1)
        lauf = (vorher or nachher or ['-'])[0]
        print(f'ERGEBNIS vorbereiten {t_vorbereitet:.0f} s, gewartet {gewartet:.0f} s, '
              f'fertig {"VOR" if vorher else "NACH"} dem Start; kalt start {a.get("start", -1):.1f} '
              f'daten {a.get("daten", -1):.1f} bild {a.get("bild", -1):.1f} | '
              f'{FERTIG.search(lauf).group(0)[:90] if lauf != "-" else "-"}', flush=True)
    finally:
        if k:
            k.close()
        emu.stoppen()
