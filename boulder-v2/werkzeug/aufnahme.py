"""Pruefdaten einspielen und alle Karten und Details der Health-App aufnehmen."""
import datetime, sys, time
sys.path.insert(0, '/tmp/claude-0/-home-user/7e362f6d-34dc-553d-8345-69bd6588c9e5/scratchpad/werkzeug')
from konsole import Konsole
from taste import taste, bild
from PIL import Image
W = '/tmp/claude-0/-home-user/7e362f6d-34dc-553d-8345-69bd6588c9e5/scratchpad/bilder'
import os; os.makedirs(W, exist_ok=True)

def b(k, name):
    k.befehl('backlight test'); time.sleep(0.8); bild(f'{W}/{name}.png'); return f'{W}/{name}.png'

def tafel(namen, ziel):
    bs = [Image.open(n) for n in namen]
    o = Image.new('RGB', (sum(x.width for x in bs) + 8 * (len(bs) - 1), max(x.height for x in bs)), 'white')
    x = 0
    for i in bs:
        o.paste(i, (x, 0)); x += i.width + 8
    o.resize((o.width * 2, o.height * 2), Image.NEAREST).save(ziel)

k = Konsole()
if '--daten' in sys.argv:
    t = int(datetime.datetime(2026, 10, 11, 20, 0, tzinfo=datetime.timezone.utc).timestamp())
    for c in [f'boulder zeit {t}', 'boulder prueflauf 0']:
        print(c, k.befehl(c))
    time.sleep(40)
    for c in ['boulder spo2 40', 'boulder schritte 6230', 'boulder lage', 'boulder naechte']:
        print(c, k.befehl(c))
k.befehl('app launch -82'); time.sleep(1.5)
karten = []
for name in ['today', 'night', 'heart', 'activity', 'settings']:
    karten.append(b(k, 'k_' + name)); taste('o'); time.sleep(0.8)
tafel(karten, f'{W}/karten.png')
# Details in einem Lauf: von Karte zu Karte, Select, Bild, rollen, Bild, Zurueck zur Karte.
k.befehl('app launch -82'); time.sleep(1.5)
details = []
for name in ['night', 'heart', 'activity', 'settings']:
    taste('o'); time.sleep(0.6)
    taste('s'); time.sleep(1.0)
    details.append(b(k, 'd_' + name))
    if name != 'settings':
        for _ in range(4):
            taste('u')
        time.sleep(0.8)
        details.append(b(k, 'd_' + name + '_unten'))
        taste('b'); time.sleep(0.8)
    else:
        taste('u'); time.sleep(0.5)
        details.append(b(k, 'd_settings_auswahl'))
        taste('s'); time.sleep(1.0)
        details.append(b(k, 'k_settings_neu'))
tafel(details, f'{W}/details.png')
k.close()
print('fertig')
