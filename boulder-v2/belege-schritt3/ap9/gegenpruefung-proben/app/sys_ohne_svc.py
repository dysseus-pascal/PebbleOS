import re, bisect, collections
funcs = []
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
svc = set()
ins = re.compile(r'^\s*([0-9a-f]+):\s+(\S+)\s*(.*)$')
for z in open('dis.txt'):
    m = ins.match(z)
    if not m: continue
    if m.group(2).startswith('svc') and m.group(3).strip().startswith('2'):
        f = fn_at(int(m.group(1), 16))
        if f: svc.add(f[0])
ohne = [f for f in funcs if f[2].startswith('sys_') and f[0] not in svc]
print(len([f for f in funcs if f[2].startswith('sys_')]), 'sys_*-Funktionen,', len(ohne), 'ohne svc 2:')
for f in ohne:
    print('  ', f[2], f[3])
