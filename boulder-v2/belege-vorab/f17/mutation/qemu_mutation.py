"""F17 (13.10.): die QEMU-Mutation aus AP9 fuer K6 (L1) und K7 (Kernel-Heap), dazu der erste Start
unter -icount. Zwei Fehler in einem Lauf des Pruefbaus: 60 ms Warten vor dem ersten Bild und 16 B
Kernel-Heap je sys_boulder_lage, die nie frei werden. qemu_health.py muss rot sein, und zwar an L1
in Echtzeit, am ersten Start unter -icount und am Kernel-Heap. Danach die Quellen zurueck und den
Pruefbau neu."""
import json
import os
import pathlib
import subprocess
import sys

R = pathlib.Path('/home/user/pebbleos-v2')
AUS = pathlib.Path(sys.argv[1])
ENV = dict(os.environ)
ENV['PATH'] = f'{R}/.venv/bin:/root/pebbleos-sdk-0.1.10/arm-none-eabi/bin:' + ENV['PATH']
M = [
    (R / 'fw/apps/system/boulder_health/boulder_health.c',
     '  BH_ZEIT("daten");\n',
     '  BH_ZEIT("daten");\n  { extern void psleep(int); psleep(60); }\n'),
    (R / 'fw/services/activity/boulder/schnittstelle.c',
     '  pbl_mutex_lock(&s_lage_sperre, PBL_FOREVER);\n  // MEMCPY STATT ZUWEISUNG',
     '  { extern void *kernel_malloc(size_t); (void)kernel_malloc(16); }\n'
     '  pbl_mutex_lock(&s_lage_sperre, PBL_FOREVER);\n  // MEMCPY STATT ZUWEISUNG'),
]


def bauen():
    for ziel in ([], ['--target', 'qemu_image_spi']):
        subprocess.run(['cmake', '--build', str(R / 'build-pruef'), *ziel], env=ENV, check=True,
                       capture_output=True)


alt = {}
for pfad, vorher, nachher in M:
    s = pfad.read_text()
    assert s.count(vorher) == 1, pfad
    alt[pfad] = s
    pfad.write_text(s.replace(vorher, nachher))
try:
    bauen()
    subprocess.run('pgrep -x qemu-pebble | xargs -r kill', shell=True, check=False)
    r = subprocess.run([sys.executable, str(R / 'tools/boulder/qemu_health.py'), str(R / 'build-pruef'),
                        '--aus', str(AUS)], env=ENV, cwd=R, capture_output=True, text=True)
    (AUS / 'ausgabe.txt').write_text(r.stdout + r.stderr)
finally:
    for pfad, s in alt.items():
        pfad.write_text(s)
    subprocess.run('pgrep -x qemu-pebble | xargs -r kill', shell=True, check=False)
    bauen()
m = json.loads((AUS / 'messung.json').read_text())
l1 = next(z for z in m['ziele'] if z['ziel'] == 'L1')
l1i = next(z for z in m['ziele'] if z['ziel'] == 'L1 erster Start (icount)')
heap = next(p for p in m['pruefungen'] if p['pruefung'].startswith('Kernel-Heap'))
print(f'qemu_health rc={r.returncode}; L1 {l1["hoechstwert_ms"]} ms ok={l1["ok"]}; '
      f'L1 erster Start (icount) {l1i["hoechstwert_ms"]} ms ok={l1i["ok"]}; '
      f'Kernel-Heap ok={heap["ok"]} ({heap["wert"]})')
sys.exit(0 if r.returncode == 1 and not l1['ok'] and not l1i['ok'] and not heap['ok'] else 1)
