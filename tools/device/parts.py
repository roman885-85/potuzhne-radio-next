import struct, sys
d = open(sys.argv[1], 'rb').read()
print('partition table @0x8000:')
parts = []
for i in range(0x8000, 0x9000, 32):
    e = d[i:i+32]
    if e[:2] != b'\xaa\x50': break
    t, st, off, size = struct.unpack('<BBII', e[2:12]); name = e[12:28].split(b'\0')[0].decode()
    parts.append((name, t, st, off, size))
    print(f'  {name:10} type={t} sub=0x{st:02x} off=0x{off:06x} size=0x{size:06x} ({size//1024} KB)')
# app description (esp_app_desc_t) sits right after image header + first segment header
for name, t, st, off, size in parts:
    if t == 0:
        hdr = d[off:off+24]
        if hdr[0] != 0xE9: print(f'  {name}: empty'); continue
        desc = d[off+24+8: off+24+8+256]
        magic, sec, _, _, ver, proj, tm, dt, idf = struct.unpack('<IIII32s32s16s16s32s', desc[:144])
        c = lambda b: b.split(b'\0')[0].decode(errors='replace')
        print(f'  {name}: magic=0x{magic:08x} ver={c(ver)} proj={c(proj)} built={c(dt)} {c(tm)} idf={c(idf)}')
    if t == 1 and st == 2: print(f'  NVS at 0x{off:x}')
