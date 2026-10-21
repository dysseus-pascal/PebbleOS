#!/usr/bin/env python3
"""b35: die Probe der Bausteine ohne Systemrechte (AP1) in QEMU, mit dem Gleiten (F40).

Startet den Pruefbau, oeffnet "boulder probe", wartet auf "bh probe fertig", nimmt ein Bild,
schliesst die App mit Zurueck und liest "bh probe ende". Rueckgabe 1, wenn ein Baustein fehlt
oder die App an einem Fehler endet.
"""
import pathlib
import re
import sys
import time

REPO = pathlib.Path('/home/user/pebbleos-v2')
sys.path.insert(0, str(REPO / 'tools' / 'boulder'))
import qemu_health as q  # noqa: E402

aus = pathlib.Path(sys.argv[1])
aus.mkdir(parents=True, exist_ok=True)
bau = REPO / 'build-pruef'
emu = q.Emulator(bau, aus)
emu.starten()
rc = 1
try:
    time.sleep(20)
    k = q.Konsole(bau)
    stand = k.stand()
    k.befehl('boulder probe')
    fertig = k.warte(r'bh probe fertig', stand, 20)
    time.sleep(1.0)
    emu.bild(aus / 'probe.png')
    emu.taste('b')
    ende = k.warte(r'bh probe ende', stand, 10)
    time.sleep(1.0)
    zeilen = [z for z in k.ab(stand) if 'bh probe' in z or re.search(r'(?i)fault|memmanage|DACCVIOL', z)]
    (aus / 'probe.txt').write_text('\n'.join(zeilen) + '\n')
    fehler = [z for z in zeilen if re.search(r'(?i)fault|memmanage|DACCVIOL', z)]
    namen = [re.search(r'bh probe (\S+)', z).group(1) for z in zeilen if re.search(r'bh probe (\S+) ', z)]
    print('fertig:', (fertig or '').strip()[-40:])
    print('ende:', (ende or '').strip()[-40:])
    print('neu benutzt:', [n for n in namen if n in ('animation', 'animation_set_custom_interpolation',
                                                  'interpolate_moook_soft',
                                                  'interpolate_moook_soft_duration')])
    print('Fehler:', fehler)
    rc = 0 if fertig and ende and 'vollstaendig' in ende and 'UNVOLL' not in ende and not fehler else 1
    k.close()
finally:
    emu.stoppen()
print('rc', rc)
sys.exit(rc)
