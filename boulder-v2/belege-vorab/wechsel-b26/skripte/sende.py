"""Befehle an dienst.py geben und die Antworten zeigen.

Aufruf: python3 sende.py <arbeit> <befehl> [<befehl> ...]
Je Befehl wartet er auf "== ok" (hoechstens 60 s); "!pause <s>" wartet nur hier.
"""
import pathlib
import sys
import time


def main():
    arbeit = pathlib.Path(sys.argv[1])
    antworten = arbeit / 'antworten.txt'
    for b in sys.argv[2:]:
        if b.startswith('!pause '):
            time.sleep(float(b.split()[1]))
            continue
        vorher = antworten.read_text() if antworten.exists() else ''
        with open(arbeit / 'befehle.txt', 'a') as f:
            f.write(b + '\n')
        ende = time.time() + 60
        while time.time() < ende:
            neu = antworten.read_text()[len(vorher):]
            if '== ok' in neu or '== beendet' in neu:
                print(neu.rstrip())
                break
            time.sleep(0.3)
        else:
            print(f'(kein "== ok" fuer {b!r})')


if __name__ == '__main__':
    main()
