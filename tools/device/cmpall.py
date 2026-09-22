import struct, subprocess, sys, collections, re
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
mine = segs(sys.argv[1]); dev = segs(sys.argv[2])
syms = []
for line in open(S + '/sketch_syms.txt'):
    head, src = line.rstrip('\n').split('\t', 1)
    p = head.split(' ', 3)
    if len(p) == 4 and int(p[1],16) >= 8: syms.append((int(p[0],16), int(p[1],16), p[3], src.split('/b1/yoRadio/')[1]))
syms = sorted(set(syms))
def locate(va, sz, guess):
    b = rd(mine, va, sz); votes = collections.Counter()
    if b is None: return None
    for k in range(0, max(1, sz - 6), 3):
        ch = b[k:k+6]
        if len(set(ch)) < 3: continue
        for a, seg in dev:
            lo = max(0, va + guess - a - 0x8000); hi = min(len(seg), va + guess - a + 0x8000)
            if lo >= hi: continue
            i = seg.find(ch, lo, hi); c = 0
            while i != -1 and c < 4:
                votes[(a + i - k) - va] += 1; c += 1; i = seg.find(ch, i + 1, hi)
    return votes.most_common(1)[0] if votes else None
def dis(data, va):
    fn = S + '/_s.bin'; open(fn, 'wb').write(data)
    out = subprocess.run([B+'xtensa-esp32-elf-objdump', '-D', '-b', 'binary', '-m', 'xtensa', f'--adjust-vma={va:#x}', fn], capture_output=True, text=True).stdout
    r = []
    for l in out.splitlines():
        m = re.match(r'\s*[0-9a-f]+:\s+[0-9a-f ]+?\s{2,}(\S+)\s*(.*)', l)
        if m:
            op = re.sub(r'0x4[0-9a-f]{7}|0x3f[0-9a-f]{6}', 'ADDR', m.group(2))
            r.append(m.group(1) + ' ' + op)
    return r
guess = 0; bad = []; nf = 0
for va, sz, name, src in syms:
    loc = locate(va, sz, guess)
    if not loc or loc[1] < 2: nf += 1; bad.append((name, src, 'NOT FOUND', [])); continue
    d = loc[0]; guess = d
    a = dis(rd(mine, va, sz), va); b = dis(rd(dev, va + d, sz), va + d)
    diffs = [(x, y) for x, y in zip(a, b) if x != y]
    if diffs: bad.append((name, src, f'{len(diffs)} diffs / {len(a)}', diffs[:6]))
print(f'functions: {len(syms)}, not found: {nf}, with diffs: {len(bad)-nf}')
for name, src, st, ds in bad:
    print(f'## {name}  [{src}]  {st}')
    for x, y in ds: print(f'     mine: {x:40} | dev: {y}')
