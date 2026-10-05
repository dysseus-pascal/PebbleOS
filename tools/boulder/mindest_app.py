#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0
"""Boulder: die Zeile "Mindest-App: X.Y.Z" in den Release-Notizen pruefen.

Aufruf: mindest_app.py <notizen.md> <untergrenze-datei>

Gibt die Fassung aus (Rueckgabe 0) oder sagt, was fehlt (Rueckgabe 1).

WARUM: Boulder bietet eine Firmware nur an, wenn die App mindestens die
genannte Fassung hat. Eine Firmware, die etwas schickt, das eine alte App
nicht kennt (etwa neue Datalogging-Tags, die sie quittiert und verwirft),
verloere sonst Daten. Die App liest den Text des GitHub-Releases mit genau
dem Muster unten, Zeile fuer Zeile.

- Die Zeile steht genau einmal da, allein, ohne Leerzeichen davor oder
  dahinter, und nicht in der ersten Zeile: die ist der Titel und kommt nicht
  in den Text des Releases (release.sh, tail -n +2).
- Keine CR: mit Windows-Zeilenenden traefe das Muster der App nicht.
- Die Fassung ist mindestens die aus <untergrenze-datei>. Dort steht, welche
  App die Firmware im Repo mindestens braucht; wer etwas einbaut, das eine
  neuere App verlangt, zieht sie dort nach.
"""
import re
import sys

# Wie in der App ("^Mindest-App: (\\d+\\.\\d+\\.\\d+)$"); [0-9] statt \d, weil
# Pythons \d auch andere Ziffern nimmt, die der App nicht.
MUSTER = re.compile(r'^Mindest-App: ([0-9]+\.[0-9]+\.[0-9]+)$')


def fassung(text):
    return tuple(int(t) for t in text.split('.'))


def pruefe(notizen, untergrenze):
    if '\r' in notizen:
        return None, 'Mindest-App nicht pruefbar: die Notizen haben Windows-Zeilenenden (CR)'
    zeilen = notizen.split('\n')
    treffer = [(i, m.group(1)) for i, z in enumerate(zeilen) if (m := MUSTER.match(z))]
    if not treffer:
        fast = [z for z in zeilen if 'mindest-app' in z.lower()]
        hinweis = f' (gefunden, aber nicht genau so: {fast[0]!r})' if fast else ''
        return None, f'Es fehlt die Zeile "Mindest-App: X.Y.Z"{hinweis}'
    if len(treffer) > 1:
        return None, 'Die Zeile "Mindest-App:" steht mehr als einmal da'
    zeile, wert = treffer[0]
    if zeile == 0:
        return None, 'Die Zeile "Mindest-App:" ist der Titel - sie gehoert in den Text darunter'
    if fassung(wert) < fassung(untergrenze):
        return None, f'Mindest-App {wert} liegt unter {untergrenze}, die diese Firmware braucht'
    return wert, None


def main():
    if len(sys.argv) != 3:
        print(__doc__.strip().split('\n\n')[1], file=sys.stderr)
        return 2
    with open(sys.argv[1], encoding='utf-8', newline='') as f:
        notizen = f.read()
    with open(sys.argv[2], encoding='utf-8') as f:
        untergrenze = f.read().strip()
    if not MUSTER.match('Mindest-App: ' + untergrenze):
        print(f'{sys.argv[2]}: keine Mindest-App X.Y.Z: {untergrenze!r}', file=sys.stderr)
        return 1
    wert, fehler = pruefe(notizen, untergrenze)
    if fehler:
        print(f'{sys.argv[1]}: {fehler}', file=sys.stderr)
        return 1
    print(wert)
    return 0


if __name__ == '__main__':
    sys.exit(main())
