#!/usr/bin/env python3
# Erreichbare Funktionen ab der App (ohne Syscall-Grenze zu ueberschreiten), aus objdump -d.
import re, sys, bisect, collections
funcs = []  # (addr, size, name, quelle)
for z in open('funcs_quellen.txt'):
    t = z.split()
    if len(t) < 4: continue
    funcs.append((int(t[0], 16), int(t[1], 16), t[3], t[4] if len(t) > 4 else '?'))
funcs.sort()
starts = [f[0] for f in funcs]
def fn_at(a):
    i = bisect.bisect_right(starts, a) - 1
    if i >= 0 and funcs[i][0] <= a < funcs[i][0] + max(funcs[i][1], 2):
        return funcs[i]
    return None
# Disassembly: je Funktion die Sprungziele und ob svc 2 vorkommt
calls = collections.defaultdict(set)
svc = set()
lits = collections.defaultdict(set)
cur = None
kopf = re.compile(r'^([0-9a-f]{8}) <([^>]+)>:')
ins = re.compile(r'^\s*([0-9a-f]+):\s+(\S+)\s*(.*)$')
for z in open('dis.txt'):
    m = kopf.match(z)
    if m:
        cur = int(m.group(1), 16); continue
    m = ins.match(z)
    if not m or cur is None: continue
    adr = int(m.group(1), 16); op = m.group(2); rest = m.group(3)
    f = fn_at(adr)
    if not f: continue
    if op.startswith('svc') and rest.strip().startswith('2'):
        svc.add(f[0])
    if op.startswith(('bl', 'b.', 'b\t', 'blx', 'b')) and re.match(r'^b(l|lx|\.w|\.n|eq|ne|cs|cc|mi|pl|vs|vc|hi|ls|ge|lt|gt|le|al)?(\.w|\.n)?$', op):
        mm = re.match(r'^([0-9a-f]+)\s', rest)
        if mm:
            ziel = fn_at(int(mm.group(1), 16))
            if ziel and ziel[0] != f[0]:
                calls[f[0]].add(ziel[0])
    # Literale (ldr rX, [pc, #..] ; (adresse)) -> Datenadressen
    if op == '.word':
        mm = re.match(r'^0x([0-9a-f]+)', rest.strip())
        if mm:
            lits[f[0]].add(int(mm.group(1), 16))
byaddr = {f[0]: f for f in funcs}
app = [f[0] for f in funcs if 'boulder_health/' in f[3]]
seen = set(app); q = list(app); parent = {}
while q:
    a = q.pop()
    if a in svc or re.match(r"^(passert_|croak|wtf|trigger_fault|kernel_fault|app_crashed|reboot_|system_reset|__assert|abort|util_assertion_failed|util_assert)", byaddr[a][2]): continue
    for b in calls[a]:
        if b not in seen:
            seen.add(b); parent[b] = a; q.append(b)
print('erreichbar:', len(seen), 'davon Syscalls:', len([a for a in seen if a in svc]))
def kette(a):
    k = []
    while a in parent:
        k.append(byaddr[a][2]); a = parent[a]
    k.append(byaddr[a][2])
    return ' <- '.join(k)
out = open('erreichbar.txt', 'w')
for a in sorted(seen, key=lambda a: byaddr[a][3]):
    f = byaddr[a]
    out.write(f"{'SVC ' if a in svc else '    '}{f[2]:50s} {f[3]}\n")
out.close()
# Funktionen ohne svc, die nicht aus applib/ oder libc/ oder der App stammen
verd = []
for a in seen:
    if a in svc or re.match(r"^(passert_|croak|wtf|trigger_fault|kernel_fault|app_crashed|reboot_|system_reset|__assert|abort|util_assertion_failed|util_assert)", byaddr[a][2]): continue
    f = byaddr[a]
    q_ = f[3]
    if any(s in q_ for s in ('/fw/applib/', 'boulder_health/', '/lib/c/', 'libc', 'picolibc', 'libgcc', '/lib/util/')):
        continue
    verd.append(f)
print('ausserhalb applib/libc, ohne Syscall:', len(verd))
for f in sorted(verd, key=lambda f: f[3]):
    print('  ', f[2], f[3], '|', kette(f[0])[:300])
# Datenadressen im Kernel-RAM, die erreichbare Funktionen ohne Syscall direkt laden
kram = []
for a in seen:
    if a in svc or re.match(r"^(passert_|croak|wtf|trigger_fault|kernel_fault|app_crashed|reboot_|system_reset|__assert|abort|util_assertion_failed|util_assert)", byaddr[a][2]): continue
    for l in lits[a]:
        if 0x20000000 <= l < 0x2003ca00:
            kram.append((byaddr[a], l))
print('Literale mit Kernel-RAM-Adresse:', len(kram))
for f, l in sorted(kram, key=lambda x: x[0][3]):
    print('   %08x %s %s | %s' % (l, f[2], f[3], kette(f[0])[:250]))
