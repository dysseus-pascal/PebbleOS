"""Aus protokoll.txt eines qemu_health-Laufs: L1 des ersten Starts nach dem Boot und der weiteren."""
import re
import statistics
import sys

zeilen = open(sys.argv[1], encoding='utf-8').read().splitlines()
st = [(int(m.group(1)), m.group(2).strip()) for z in zeilen if (m := re.search(r'bh t=(\d+) (.*)$', z))]
spannen = []
for i, (t, s) in enumerate(st):
    if s == 'main':
        for t2, s2 in st[i + 1:]:
            if s2 == 'main':
                break
            if s2 == 'bild':
                spannen.append((t2 - t) * 1000 / 1024)
                break
# L1 sind die ersten 60 Starts (3 Einstiege zu 20); danach folgen L2, L4, Bilder, Heap-Starts.
l1 = spannen[:60]
print(f'erster {l1[0]:.1f} ms; weitere {len(l1) - 1}: hoechstens {max(l1[1:]):.1f}, '
      f'Median {statistics.median(l1[1:]):.1f} ms')
