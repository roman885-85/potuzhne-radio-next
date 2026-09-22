import struct, subprocess, sys, collections, re, os
S = '/Users/admin/Documents/radio_potughne_next/build/device-work'
B = '/Users/admin/Library/Arduino15/packages/esp32/tools/esp-x32/2507/bin/'
def segs(path):
    d = open(path,'rb').read(); n = d[1]; off = 24; out = []
    for _ in range(n):
        a, l = struct.unpack('<II', d[off:off+8]); out.append((a, d[off+8:off+8+l])); off += 8 + l
    return out
def rd(sg, va, n):
    for a, b in sg:
        if a <= va < a + len(b): return b[va-a:va-a+n]
mine = segs(sys.argv[1]); dev = segs(sys.argv[2]); elf = sys.argv[3]; pats = sys.argv[4:]
syms = []
for line in subprocess.run([B+'xtensa-esp32-elf-nm', '-C', '-S', '--defined-only', elf], capture_output=True, text=True).stdout.splitlines():
    p = line.split(' ', 3)
    if len(p) == 4 and p[2] in 'tTW': syms.append((int(p[0],16), int(p[1],16), p[3]))
def locate(va, sz):
    b = rd(mine, va, sz); votes = collections.Counter()
    for k in range(0, sz - 6, 3):
        ch = b[k:k+6]
        if len(set(ch)) < 3: continue
        for a, seg in dev:
            lo = max(0, va - a - 0x30000); hi = min(len(seg), va - a + 0x30000)
            if lo >= hi: continue
            i = seg.find(ch, lo, hi); cnt = 0
            while i != -1 and cnt < 5:
                votes[(a + i - k) - va] += 1; cnt += 1; i = seg.find(ch, i + 1, hi)
    return votes.most_common(2)
def dis(data, va):
    fn = S + '/_slice.bin'; open(fn, 'wb').write(data)
    out = subprocess.run([B+'xtensa-esp32-elf-objdump', '-D', '-b', 'binary', '-m', 'xtensa', f'--adjust-vma={va:#x}', fn], capture_output=True, text=True).stdout
    ins = []
    for l in out.splitlines():
        m = re.match(r'\s*([0-9a-f]+):\s+([0-9a-f ]+?)\s{2,}(\S+)\s*(.*)', l)
        if m: ins.append((m.group(3), m.group(4)))
    return ins
for pat in pats:
    for va, sz, name in syms:
        if not re.search(pat, name): continue
        loc = locate(va, sz)
        if not loc: print(f'### {name}: NOT FOUND'); continue
        d, votes = loc[0]
        a = dis(rd(mine, va, sz), va); b = dis(rd(dev, va + d, sz + 16), va + d)
        print(f'### {name}  mine@{va:#x} dev@{va+d:#x} size={sz} votes={votes} alt={loc[1:] }')
        for (m1, o1), (m2, o2) in zip(a, b):
            if m1.startswith('movi') or m2.startswith('movi') or m1 != m2:
                mark = '   ' if (m1, o1) == (m2, o2) else '>>>'
                if m1.startswith('movi') or m2.startswith('movi') or m1 != m2: print(f'  {mark} mine: {m1:8} {o1:28} | dev: {m2:8} {o2}')
