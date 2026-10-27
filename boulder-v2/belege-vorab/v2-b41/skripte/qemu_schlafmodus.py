import pathlib, re, sys, time
sys.path.insert(0, '/home/user/pebbleos-v2/tools/boulder')
import qemu_health as q

bau = pathlib.Path('/home/user/pebbleos-v2/build-pruef')
aus = pathlib.Path(sys.argv[1])
emu = q.Emulator(bau, aus)
emu.starten()
time.sleep(20)
k = q.Konsole(bau)

def seit():
    for z in k.befehl('boulder lage'):
        m = re.search(r'schlafmodus (\d+)', z)
        if m:
            return int(m.group(1))

try:
    k.befehl('boulder tracking')
    k.befehl(f'boulder zeit {q.UHR_UTC}')
    time.sleep(70)
    k.befehl('boulder schritte 4120')
    print('vorher', seit())
    k.befehl('app launch -106')
    time.sleep(3)
    emu.bild(aus / '1_vorher.png')
    k.befehl('app launch -105')
    time.sleep(4)
    print('nach dem Schalter', seit())
    k.befehl('app launch -106')
    time.sleep(3)
    emu.bild(aus / '2_schlafmodus.png')
    time.sleep(62)
    emu.bild(aus / '3_schlafmodus_minute.png')
    k.befehl('app launch -105')
    time.sleep(4)
    print('wieder aus', seit())
    k.befehl('app launch -106')
    time.sleep(3)
    emu.bild(aus / '4_wieder_aus.png')
    print('laeuft', emu.laeuft())
finally:
    (aus / 'konsole.log').write_text('\n'.join(k.ab(0)))
    k.close()
    emu.stoppen()
