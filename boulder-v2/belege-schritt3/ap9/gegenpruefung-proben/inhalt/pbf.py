# Liest Vorschubbreiten aus Pebbles .pbf (fontgen.py), nur lesend.
import struct, sys
def lade(pfad):
    d = open(pfad,'rb').read()
    ver, maxh, n, wild = struct.unpack_from('<BBHH', d, 0)
    if ver == 1:
        off = 6; table_size = 0; cpb = 2; feat = 0
    else:
        table_size, cpb = struct.unpack_from('<BB', d, 6)
        if ver == 2:
            off = 8; feat = 0
        else:
            size, feat = struct.unpack_from('<BB', d, 8)
            off = size
    offs = 2 if (feat & 1) else 4
    hashes = []
    for i in range(table_size):
        h, bs, acc = struct.unpack_from('<BBH', d, off + 4*i)
        hashes.append((h, bs, acc))
    tabellen = off + 4*table_size
    total_entries = sum(b for _,b,_ in hashes)
    glyph_base = tabellen + total_entries*(offs+cpb)
    fmt = '<' + ('L' if cpb==4 else 'H') + ('L' if offs==4 else 'H')
    glyphs = {}
    for h, bs, acc in hashes:
        for k in range(bs):
            cp, o = struct.unpack_from(fmt, d, tabellen + acc + k*(offs+cpb))
            w, hh, left, top, adv = struct.unpack_from('<BBbbb', d, glyph_base + o)
            glyphs[cp] = (w, hh, left, top, adv)
    return maxh, glyphs, wild
def breite(glyphs, wild, text):
    s = 0
    for ch in text:
        g = glyphs.get(ord(ch)) or glyphs.get(wild)
        s += g[4]
    return s
if __name__ == '__main__':
    pfad = sys.argv[1]
    maxh, g, wild = lade(pfad)
    for t in sys.argv[2:]:
        print(repr(t), breite(g, wild, t), 'fehlend:', [c for c in t if ord(c) not in g])
