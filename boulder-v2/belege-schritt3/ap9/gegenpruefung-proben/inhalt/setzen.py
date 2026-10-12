# Setzt Text mit den Glyphen aus Pebbles .pbf (ohne RLE4), um Ueberlappungen zu zeigen.
import struct, sys
from PIL import Image
def lade_voll(pfad):
    d = open(pfad,'rb').read()
    ver, maxh, n, wild = struct.unpack_from('<BBHH', d, 0)
    table_size, cpb = struct.unpack_from('<BB', d, 6)
    size, feat = struct.unpack_from('<BB', d, 8)
    off = size
    offs = 2 if (feat & 1) else 4
    hashes = [struct.unpack_from('<BBH', d, off + 4*i) for i in range(table_size)]
    tabellen = off + 4*table_size
    total = sum(b for _,b,_ in hashes)
    base = tabellen + total*(offs+cpb)
    fmt = '<' + ('L' if cpb==4 else 'H') + ('L' if offs==4 else 'H')
    g = {}
    for h, bs, acc in hashes:
        for k in range(bs):
            cp, o = struct.unpack_from(fmt, d, tabellen + acc + k*(offs+cpb))
            w, hh, left, top, adv = struct.unpack_from('<BBbbb', d, base + o)
            bits = []
            nwords = (w*hh + 31)//32
            for i in range(nwords):
                word, = struct.unpack_from('<I', d, base + o + 5 + 4*i)
                for b in range(32):
                    bits.append((word >> b) & 1)
            g[cp] = (w, hh, left, top, adv, bits[:w*hh])
    return g, wild
def breite(g, wild, text):
    return sum((g.get(ord(c)) or g[wild])[4] for c in text)
def setze(img, g, wild, text, x, y, farbe):
    for c in text:
        w, hh, left, top, adv, bits = g.get(ord(c)) or g[wild]
        for i in range(hh):
            for j in range(w):
                if bits[i*w + j]:
                    px, py = x + left + j, y + top + i
                    if 0 <= px < img.width and 0 <= py < img.height:
                        img.putpixel((px, py), farbe)
        x += adv
    return x
