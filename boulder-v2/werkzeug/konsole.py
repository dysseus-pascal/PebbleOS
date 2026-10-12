"""Pruefbefehle an QEMU ueber PULSE (wie pbl console) und das Protokoll mitlesen."""
import sys, time, threading
sys.path.insert(0, "/home/user/pebbleos-v2/tools")
from pebble.commander.commander import PebbleCommander
from log_hashing.logdehash import LogDehash

class Konsole:
    def __init__(self, bau='/home/user/pebbleos-v2/build-pruef', host='localhost:12345'):
        self.cmdr = PebbleCommander(tty=f'socket://{host}')
        self.dehash = LogDehash(f'{bau}/fw/loghash_dict.json')
        self.zeilen = []
        self.lock = threading.Lock()
        self.cmdr.attach_log_listener(self._log)
    def _log(self, msg):
        d = self.dehash.dehash(msg)
        z = self.dehash.commander_format_line(d)
        with self.lock:
            self.zeilen.append((time.time(), z))
    def befehl(self, cmd):
        return self.cmdr.send_prompt_command(cmd)
    def protokoll(self, ab=0.0):
        with self.lock:
            return [z for t, z in self.zeilen if t >= ab]
    def close(self):
        self.cmdr.close()

if __name__ == '__main__':
    k = Konsole()
    for c in sys.argv[1:]:
        print('>>', c)
        for z in k.befehl(c) or []:
            print(z)
    time.sleep(0.5)
    k.close()
