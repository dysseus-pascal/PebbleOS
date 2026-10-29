import pathlib, sys, time
sys.path.insert(0, '/home/user/pebbleos-v2/tools/boulder')
import qemu_health as q

bau = pathlib.Path('/home/user/pebbleos-v2/build-pruef')
aus = pathlib.Path(sys.argv[1])
emu = q.Emulator(bau, aus)
emu.starten()
k = q.Konsole(bau)
try:
    m = q.Messung(emu, k, aus) if hasattr(q, 'Messung') else None
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
    print('\n'.join(k.befehl('boulder lage')))
    print('\n'.join(k.befehl('boulder naechte'))[:400])
    k.befehl('app launch -106')
    time.sleep(4)
    emu.bild(aus / 'tag.png')
    k.befehl('app launch -105')
    time.sleep(5)
    k.befehl('app launch -106')
    time.sleep(4)
    emu.bild(aus / 'schlafmodus.png')
    print('\n'.join(k.befehl('boulder lage')))
    # eine Minute weiter: der Takt frischt auf, das Zifferblatt bleibt
    time.sleep(65)
    emu.bild(aus / 'schlafmodus_minute.png')
    print('laeuft', emu.laeuft())
finally:
    zeilen = k.ab(0)
    (aus / 'konsole.log').write_text('\n'.join(zeilen))
    k.close()
    emu.stoppen()
