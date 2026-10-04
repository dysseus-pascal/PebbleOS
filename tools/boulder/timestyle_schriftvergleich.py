#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0
"""Boulder: welche freie Schrift kommt den Avenir-Next-Ziffern von TimeStyle am naechsten?

So wurde die Ersatzschrift fuer TimeStyle Bedrock gewaehlt (BOULDER.md,
Abschnitt Schriften; Bau: timestyle_schriften.sh).

Bezug: die Umrisse aus AvenirNextRegular.ffont / AvenirNextDemiBold.ffont des
Originals (github.com/freakified/TimeStylePebble, e2be725, resources/fonts/ -
nur zum Vergleich gelesen, nicht uebernommen). Jede Ziffer 0-9 wird auf
dieselbe Ziffernhoehe (Oberkante der "1") skaliert, an der Grundlinie und
mittig ausgerichtet und gerastert (300 px hoch); Mass = mittlere Ueberdeckung
(IoU) der gefuellten Flaechen. Variable Schriften werden ueber die Staerke
(wght, Schritt 25) abgesucht, damit die Form entscheidet und nicht die
Strichstaerke; bei festen Schnitten zaehlt der eine Schnitt fuer beide.
Gibt es tabellarische Ziffern (tnum), werden diese genommen.

Aufruf:
  timestyle_schriftvergleich.py <TimeStylePebble/resources/fonts> <kandidaten.txt> <ergebnis.json>
kandidaten.txt: je Zeile "name|pfad/zur/schrift.ttf". Der Vergleich vom
4.10.2026 nahm 19 Schriften, alle aus github.com/google/fonts (ofl/<name>/):
nunitosans, mulish, figtree, jost, outfit, albertsans, urbanist,
plusjakartasans, manrope, hankengrotesk, redhatdisplay, montserrat, worksans,
kumbhsans, opensans, sourcesans3, lexend, inter (je die variable Schrift
"<Name>[...].ttf") und lato (Lato-Regular.ttf, nicht variabel).
Braucht fontTools, numpy und Pillow.
"""
import sys, json
import numpy as np
from PIL import Image, ImageDraw
from fontTools.ttLib import TTFont
from fontTools.pens.recordingPen import RecordingPen, DecomposingRecordingPen
from fontTools.pens.basePen import decomposeQuadraticSegment
import struct

def _bez2(p0,p1,p2,n=16):
    return [((1-t)**2*p0[0]+2*(1-t)*t*p1[0]+t*t*p2[0],(1-t)**2*p0[1]+2*(1-t)*t*p1[1]+t*t*p2[1]) for t in [i/n for i in range(1,n+1)]]
def _bez3(p0,p1,p2,p3,n=16):
    out=[]
    for i in range(1,n+1):
        t=i/n;u=1-t
        out.append((u**3*p0[0]+3*u*u*t*p1[0]+3*u*t*t*p2[0]+t**3*p3[0],u**3*p0[1]+3*u*u*t*p1[1]+3*u*t*t*p2[1]+t**3*p3[1]))
    return out

class FFont:
    def __init__(self, path=None, data=None):
        F=data if data is not None else open(path,'rb').read()
        self.F=F
        self.upem,self.asc,self.desc,self.cap,ni,nt=struct.unpack_from('<hhhhHH',F,0)
        self.ranges=[struct.unpack_from('<HH',F,12+4*i) for i in range(ni)]
        gt=12+4*ni; self.pd=gt+6*nt
        self.glyphs=[struct.unpack_from('<HHh',F,gt+6*i) for i in range(nt)]
    def glyph(self,cp):
        off=0
        for b,e in self.ranges:
            if cp<b: return None
            if cp<e: return self.glyphs[off+cp-b]
            off+=e-b
    def commands(self,cp):
        g=self.glyph(cp)
        if not g: return []
        o,l,a=g; p=self.pd+o; e=p+l; out=[]
        while p<e:
            c=chr(struct.unpack_from('<H',self.F,p)[0]); p+=2
            n={'M':2,'L':2,'H':1,'V':1,'C':6,'S':4,'Q':4,'T':2,'Z':0}[c]
            out.append((c,struct.unpack_from('<'+'h'*n,self.F,p))); p+=2*n
        return out
    def advance(self,cp):
        g=self.glyph(cp); return g[2] if g else 0
    def polygons(self,cp):
        polys=[];cur=None;init=None;ctrl=None;poly=None
        for c,a in self.commands(cp):
            if c=='M':
                if poly: polys.append(poly)
                cur=(a[0],a[1]);init=cur;poly=[cur];ctrl=cur
            elif c=='L': cur=(a[0],a[1]);poly.append(cur);ctrl=cur
            elif c=='H': cur=(a[0],cur[1]);poly.append(cur);ctrl=cur
            elif c=='V': cur=(cur[0],a[0]);poly.append(cur);ctrl=cur
            elif c=='Q':
                ctrl=(a[0],a[1]);end=(a[2],a[3]);poly+= _bez2(cur,ctrl,end);cur=end
            elif c=='T':
                ctrl=(2*cur[0]-ctrl[0],2*cur[1]-ctrl[1]);end=(a[0],a[1]);poly+=_bez2(cur,ctrl,end);cur=end
            elif c=='C':
                poly+=_bez3(cur,(a[0],a[1]),(a[2],a[3]),(a[4],a[5]));ctrl=(a[2],a[3]);cur=(a[4],a[5])
            elif c=='Z':
                poly.append(init);cur=init
        if poly: polys.append(poly)
        return polys  # Einheiten: upem (y nach oben)


H = 300
ZIFFERN = '0123456789'

def raster(polys, s, xoff, yoff, W=420, Hh=420):
    # NICHT-NULL-REGEL wie die Schriften selbst: variable Schriften haben sich
    # ueberlappende Umrisse, ein XOR wuerde die Ueberlappung ausstanzen.
    acc = np.zeros((Hh, W), np.int32)
    for poly in polys:
        pts = [(xoff + x * s, yoff - y * s) for x, y in poly]
        if len(pts) < 3:
            continue
        n = len(pts)
        flaeche = sum(pts[i][0] * pts[(i + 1) % n][1] - pts[(i + 1) % n][0] * pts[i][1] for i in range(n))
        im = Image.new('1', (W, Hh), 0)
        ImageDraw.Draw(im).polygon(pts, fill=1)
        acc += np.array(im, np.int32) * (1 if flaeche > 0 else -1)
    return acc != 0

def tnum_map(f):
    m = {}
    if 'GSUB' not in f:
        return m
    g = f['GSUB'].table
    if not g.FeatureList:
        return m
    for fr in g.FeatureList.FeatureRecord:
        if fr.FeatureTag != 'tnum':
            continue
        for li in fr.Feature.LookupListIndex:
            lk = g.LookupList.Lookup[li]
            for st in lk.SubTable:
                if lk.LookupType == 7:
                    st = st.ExtSubTable
                if hasattr(st, 'mapping'):
                    m.update(st.mapping)
    return m

class TT:
    def __init__(self, path):
        self.f = TTFont(path)
        self.cmap = self.f.getBestCmap()
        self.tnum = tnum_map(self.f)
        self.var = 'fvar' in self.f
        self.achsen = {a.axisTag: (a.minValue, a.defaultValue, a.maxValue) for a in self.f['fvar'].axes} if self.var else {}
    def glyphs(self, wght=None, tab=True):
        loc = {'wght': wght} if (self.var and wght is not None) else None
        gs = self.f.getGlyphSet(location=loc) if loc else self.f.getGlyphSet()
        out = {}
        for d in ZIFFERN:
            n = self.cmap[ord(d)]
            if tab and n in self.tnum:
                n = self.tnum[n]
            p = DecomposingRecordingPen(gs); gs[n].draw(p)
            out[d] = (self.polys(p.value), gs[n].width)
        return out
    @staticmethod
    def polys(rec):
        polys = []; poly = None; cur = None
        for op, args in rec:
            if op == 'moveTo':
                if poly: polys.append(poly)
                cur = args[0]; poly = [cur]
            elif op == 'lineTo':
                cur = args[0]; poly.append(cur)
            elif op == 'qCurveTo':
                for (c, e) in decomposeQuadraticSegment(args):
                    poly += _bez2(cur, c, e); cur = e
            elif op == 'curveTo':
                poly += _bez3(cur, args[0], args[1], args[2]); cur = args[2]
            elif op in ('closePath', 'endPath'):
                if poly: polys.append(poly)
                poly = None
        if poly: polys.append(poly)
        return polys

def bilder(gl):
    hoehe = max(p[1] for poly in gl['1'][0] for p in poly)
    s = H / hoehe
    res = {}
    for d, (polys, adv) in gl.items():
        xs = [p[0] for poly in polys for p in poly]
        cx = (min(xs) + max(xs)) / 2
        res[d] = raster(polys, s, 210 - cx * s, 360)
    breiten = [adv * s for d, (polys, adv) in gl.items()]
    return res, breiten

def ffont_glyphs(ff):
    return {d: (ff.polygons(ord(d)), ff.advance(ord(d))) for d in ZIFFERN}

def mass(ref, b):
    ious = [(ref[d] & b[d]).sum() / (ref[d] | b[d]).sum() for d in ZIFFERN]
    fl = sum(b[d].sum() for d in ZIFFERN) / sum(ref[d].sum() for d in ZIFFERN)
    return float(np.mean(ious)), ious, fl

O = sys.argv[1].rstrip('/') + '/'
refR, refRB = bilder(ffont_glyphs(FFont(O + 'AvenirNextRegular.ffont')))
refB, refBB = bilder(ffont_glyphs(FFont(O + 'AvenirNextDemiBold.ffont')))
refBreite = refRB[0]

kandidaten = [l.split('|') for l in open(sys.argv[2]).read().strip().split('\n') if l and not l.startswith('#')]
ergebnis = []
for name, pfad in kandidaten:
    t = TT(pfad)
    if t.var and 'wght' in t.achsen:
        lo, de, hi = t.achsen['wght']
        stufen = [w for w in range(int(lo), int(hi) + 1, 25)]
    else:
        stufen = [None]
    best = {}
    for ziel, ref in (('regular', refR), ('fett', refB)):
        bw = None
        for w in stufen:
            b, br = bilder(t.glyphs(w))
            m, ious, fl = mass(ref, b)
            if bw is None or m > bw[0]:
                bw = (m, w, ious, fl, br)
        best[ziel] = bw
    tab = len(set(round(x, 3) for x in best['regular'][4])) == 1
    ergebnis.append((name, best, tab, bool(t.tnum)))
    print(f"{name:22s} regular {best['regular'][0]:.3f} @wght {best['regular'][1]} Flaeche x{best['regular'][3]:.2f} | fett {best['fett'][0]:.3f} @wght {best['fett'][1]} Flaeche x{best['fett'][3]:.2f} | Breite x{best['regular'][4][0]/refBreite:.2f} tabellarisch={tab} tnum={bool(t.tnum)} | 6:{best['regular'][2][6]:.2f} 9:{best['regular'][2][9]:.2f}", flush=True)

json.dump([(n, {k: [v[0], v[1], [round(x, 3) for x in v[2]], v[3]] for k, v in b.items()}, tab, tn) for n, b, tab, tn in ergebnis],
          open(sys.argv[3], 'w'), indent=1)
