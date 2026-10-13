"""Einmal mit der PULSE-Konsole verbinden und Befehle senden. Aufruf: einmal.py <befehl> ..."""
import sys
sys.path.insert(0, '/home/user/pebbleos-v2/tools')
from pebble.commander.commander import PebbleCommander  # noqa: E402

cmdr = PebbleCommander(tty='socket://localhost:12345')
try:
    for b in sys.argv[1:]:
        print(f'>> {b}')
        for z in cmdr.send_prompt_command(b) or []:
            print(f'   {z}')
finally:
    cmdr.close()
