import struct, subprocess, sys, collections
S = '/Users/admin/Documents/radio_potughne_next/build/device-work'
NM = '/Users/admin/Library/Arduino15/packages/esp32/tools/esp-x32/2507/bin/xtensa-esp32-elf-nm'
def segs(path):
    d = open(path,'rb').read(); n = d[1]; off = 24; out = []
    for _ in range(n):
        a, l = struct.unpack('<II', d[off:off+8]); out.append((a, d[off+8:off+8+l])); off += 8 + l
    return out
def reader(sg):
    def rd(va, n):
        for a, b in sg:
            if a <= va < a + len(b): return b[va-a:va-a+n]
        return None
    return rd
mine = segs(sys.argv[1]); dev = segs(sys.argv[2])
rm, rdv = reader(mine), reader(dev)
devseg = {a: b for a, b in dev}
syms = []
for line in subprocess.run([NM, '-C', '-S', '--defined-only', sys.argv[3]], capture_output=True, text=True).stdout.splitlines():
    p = line.split(' ', 3)
    if len(p) < 4 or p[2] not in 'tTW': continue
    a, s = int(p[0],16), int(p[1],16)
    if s >= 24 and (0x400d0000 <= a < 0x40400000 or 0x40080000 <= a < 0x400a0000): syms.append((a, s, p[3]))
syms.sort()
def find(va, sz, guess):
    b = rm(va, sz)
    # candidate deltas from several 8-byte chunks
    votes = collections.Counter()
    for k in range(0, min(sz, 64) - 8 + 1, 4):
        chunk = b[k:k+8]
        for a, seg in dev:
            if not (a - 0x100000 <= va <= a + len(seg) + 0x100000): continue
            lo = max(0, va + guess - a - 0x2000); hi = min(len(seg), va + guess - a + 0x2000)
            i = seg.find(chunk, lo, hi)
            while i != -1:
                votes[(a + i - k) - va] += 1
                i = seg.find(chunk, i + 1, hi)
    return votes.most_common(1)[0] if votes else (None, 0)
res = []; guess = 0
for va, sz, name in syms:
    d, v = find(va, sz, guess)
    if d is None or v < 2: res.append((va, sz, name, None, None)); continue
    guess = d
    a = rm(va, sz); b = rdv(va + d, sz)
    diff = sum(1 for x, y in zip(a, b) if x != y) if b else None
    res.append((va, sz, name, d, diff))
import json; json.dump(res, open(S + '/align.json', 'w'))
prev = None
for va, sz, name, d, diff in res:
    if d is not None and d != prev:
        print(f'delta change -> {d:+#x} at {va:#x} {name[:70]}'); prev = d
print('unmatched:', sum(1 for r in res if r[3] is None), 'of', len(res))
