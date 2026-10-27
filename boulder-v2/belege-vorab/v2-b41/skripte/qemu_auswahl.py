import pathlib, sys, time
sys.path.insert(0, '/home/user/pebbleos-v2/tools/boulder')
import qemu_health as q

bau = pathlib.Path('/home/user/pebbleos-v2/build-pruef')
aus = pathlib.Path(sys.argv[1])
emu = q.Emulator(bau, aus)
emu.starten()
k = q.Konsole(bau)
try:
    stand = k.stand()
    for c in ['boulder tracking', f'boulder zeit {q.UHR_UTC}', 'boulder prueflauf 0']:
        k.befehl(c)
    gestellt = time.time()
    time.sleep(30)
    frist = time.time() + 150
    while time.time() < frist:
        if q.zahl(r'anzahl (\d+)', k.befehl('boulder naechte')):
            break
        time.sleep(3)
    k.warte(r'resetting metrics for new day', stand, max(0.0, gestellt + 90 - time.time()))
    k.befehl('boulder schritte 6230')
    # Tagesband als Zifferblatt waehlen: Liste, zweimal Unten, Select.
    k.befehl('app launch -6')
    time.sleep(3)
    emu.taste('u'); time.sleep(1)
    emu.taste('u'); time.sleep(1)
    emu.taste('s'); time.sleep(4)
    emu.bild(aus / 'gewaehlt.png')
    # Schlafmodus wie am Arm: Health, Night, Select lang.
    k.befehl('app launch -82')
    time.sleep(3)
    emu.taste('u'); time.sleep(2)
    emu.bild(aus / 'night.png')
    k.befehl('button hold 2 1000')
    time.sleep(1)
    emu.bild(aus / 'schalter.png')
    time.sleep(5)
    emu.bild(aus / 'nach_schalter.png')
    print('\n'.join(k.befehl('boulder lage')))
    time.sleep(65)
    emu.bild(aus / 'schlafmodus_minute.png')
    print('\n'.join(k.befehl('boulder lage')))
    print('laeuft', emu.laeuft())
finally:
    (aus / 'konsole.log').write_text('\n'.join(k.ab(0)))
    k.close()
    emu.stoppen()
