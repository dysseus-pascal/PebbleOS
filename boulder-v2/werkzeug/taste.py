"""Tasten an QEMU (Monitor sendkey) und Bilder (pbl screenshot)."""
import socket, subprocess, sys, time
MON = '/home/user/pebbleos-v2/build-pruef/qemu-mon.sock'
NAMEN = {'b': 'left', 's': 'right', 'o': 'up', 'u': 'down'}
def taste(k, halten_ms=None):
    s = socket.socket(socket.AF_UNIX); s.connect(MON)
    s.settimeout(1.0)
    try:
        s.recv(4096)  # Begruessung des Monitors abwarten, sonst geht die Taste verloren
    except socket.timeout:
        pass
    cmd = f'sendkey {NAMEN.get(k, k)} {halten_ms or 100}'
    s.sendall(cmd.encode() + b'\n'); time.sleep(0.3); s.close()
def bild(pfad):
    subprocess.run(['/home/user/pebbleos-v2/.venv/bin/pbl', 'screenshot', '-b', '/home/user/pebbleos-v2/build-pruef',
                    '--output', pfad], check=True, capture_output=True, cwd='/home/user/pebbleos-v2')
if __name__ == '__main__':
    # Aufruf: taste.py <tasten> [bild] ; tasten z.B. "ssou", Pause 0.6 s je Taste
    for k in sys.argv[1]:
        taste(k); time.sleep(0.6)
    if len(sys.argv) > 2:
        time.sleep(0.5); bild(sys.argv[2])
