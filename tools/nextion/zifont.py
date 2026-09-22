"""Шрифти Nextion .zi (версія 6, згладжені, змінна ширина) — читання й створення на Mac.

Формат розібрано за зразком із генератора Nextion Editor 1.68.1 (nextion/fonts-zi/roboto24.zi) і
описом UNUF/nextion-font-editor (ZI v5):
  заголовок 44 байти (поля — hmitype FontziV5.zimoxinxi_V6):
    0 Password=04  1 codelT0=FF  2 codelDec=00  3 qumo=0A  4 encode  5 state=02  6 w=0 (змінна)  7 h
    8 codeh_star 9 codeh_end 10 codel_star 11 codel_end  12 u32 qyt (кількість символів)
    16 fontver=6  17 zimoascbeg (довжина імені+кодування)  18 u16 zimobinbeg  20 u32 datasize (усе після заголовка)
    24 u32 dataaddr=44  28 codehT0 29 codehDec 30 Anti=1 31 unequal_res=1 32 encodenamebeg (довжина імені)
    33 fontdataadd8byte 34 u16 res1  36 u32 trueziqty  40 u32 res3
  потім ім'я шрифту + назва кодування (без нулів), потім таблиця символів по 10 байт:
    u16 код, u8 ширина (крок), u8 виступ ліворуч, u8 виступ праворуч, u24 зсув гліфа від початку таблиці, u16 довжина
    (картинка гліфа має ширину крок+виступи і малюється від x−виступ_ліворуч: j, Ї, ї)
  потім гліфи: байт 0x03 (згладжування 3 біти) і коди RLE по рядках w×h:
    00 0xxxxx — xxxxx прозорих       00 1xxxxx — xxxxx непрозорих
    01 0xxxxx — xxxxx прозорих + 1 непрозорий   01 1xxxxx — xxxxx прозорих + 2 непрозорі
    10 xxxccc — xxx прозорих + 1 піксель прозорості ccc    11 cccddd — 2 пікселі прозорості
"""
import struct, os
from PIL import Image, ImageDraw, ImageFont


def read(path):
    d = open(path, 'rb').read()
    h = dict(zip('pw codelT0 codelDec qumo encode state w h cs ce ls le qyt ver namelen binbeg datasize dataaddr chT0 chDec anti uneq encbeg add8 res1 true res3'.split(),
                 struct.unpack_from('<BBBBBBBBBBBBIBBHIIBBBBBBHII', d, 0)))
    name = d[44:44 + h['namelen']]
    mstart = 44 + h['namelen']
    glyphs = []
    for i in range(h['qyt']):
        code, w, a, b, o0, o1, o2, ln = struct.unpack_from('<HBBBBBBH', d, mstart + i * 10)
        off = o0 | (o1 << 8) | (o2 << 16)
        glyphs.append(dict(code=code, w=w, a=a, b=b, bw=w + a + b, off=off, len=ln, data=d[mstart + off: mstart + off + ln]))
    return h, name, glyphs, mstart


def decode_glyph(data, w, h):
    """→ список рівнів 0..7 довжиною w*h (0 — прозорий, 7 — повний)."""
    assert data[0] in (1, 3), 'невідомий режим гліфа: %r' % data[:1]   # 1 і 3 — ті самі коди RLE
    out = []
    for c in data[1:]:
        m, v = c >> 6, c & 0x3F
        if m == 0:
            n = v & 0x1F
            out += ([7] * n) if v & 0x20 else ([0] * n)
        elif m == 1:
            out += [0] * (v & 0x1F) + [7] * (2 if v & 0x20 else 1)
        elif m == 2:
            out += [0] * (v >> 3) + [v & 7]
        else:
            out += [v >> 3, v & 7]
    out = out[:w * h]
    return out + [0] * (w * h - len(out))


def encode_glyph(levels):
    """Рівні 0..7 → байти формату 0x03 (жадібне кодування, як розбирає декодер)."""
    out = [3]; i = 0; n = len(levels)
    while i < n:
        v = levels[i]
        if v == 0:
            j = i
            while j < n and levels[j] == 0 and j - i < 31: j += 1
            run = j - i
            # «прозорі + 1/2 непрозорі» чи «прозорі + рівень» — компактніше
            if j < n and levels[j] == 7:
                two = j + 1 < n and levels[j + 1] == 7
                out.append(0x40 | (0x20 if two else 0) | run); i = j + (2 if two else 1); continue
            if j < n and 0 < levels[j] < 7 and run <= 7:
                out.append(0x80 | (run << 3) | levels[j]); i = j + 1; continue
            out.append(run); i = j; continue
        if v == 7:
            j = i
            while j < n and levels[j] == 7 and j - i < 31: j += 1
            if j - i == 1 and j < n and 0 < levels[j] < 7:      # одиночний повний + проміжний — однією парою
                out.append(0xC0 | (7 << 3) | levels[j]); i += 2; continue
            out.append(0x20 | (j - i)); i = j; continue
        # проміжний рівень: два рівні однією парою, якщо наступний теж піксель
        nxt = levels[i + 1] if i + 1 < n else 0
        out.append(0xC0 | (v << 3) | nxt); i += 2
    return bytes(out)


def glyph_png(levels, w, h, path, scale=6):
    im = Image.new('L', (w, h))
    im.putdata([v * 255 // 7 for v in levels])
    im.resize((w * scale, h * scale), Image.NEAREST).save(path)


# ------------------------------------------------------------------ створення
UTF8_ENC = 0x18     # код кодування utf-8 у генераторі 1.68.1 (зі зразка)
GAMMA = 0.85        # як у ПОТУЖНЕ (make_aafonts.py): дрібний світлий текст на темному інакше «худне»


def cap_size(ttf, cap_px):
    """Кегль (px), за якого «H» має задану висоту — як cap_size() у ПОТУЖНЕ."""
    lo, hi = 4.0, 120.0
    for _ in range(30):
        mid = (lo + hi) / 2
        l, t, r, b = ImageFont.truetype(ttf, mid).getbbox("H", anchor="ls")
        lo, hi = (mid, hi) if (b - t) < cap_px else (lo, mid)
    return (lo + hi) / 2


def metrics(ttf, size, chars):
    """→ (asc, desc): скільки рядків над і під базовою лінією займають символи набору."""
    f = ImageFont.truetype(ttf, size)
    top = bot = 0
    for ch in chars:
        if not ch.strip(): continue
        l, t, r, b = f.getbbox(ch, anchor="ls")
        top, bot = min(top, t), max(bot, b)
    return -top, bot


def build(path, ttf, chars, name, cap=None, size=None, asc=None, desc=None, gamma=GAMMA):
    """Створити .zi. Розмір — за висотою «H» (cap) або кеглем (size).
    Висота клітинки h = asc + desc; базова лінія — на рядку asc (зверху клітинки).
    Повертає dict(h, asc, desc, size) — щоб розкладка знала, де базова лінія."""
    size = size or cap_size(ttf, cap)
    chars = sorted(set(chars), key=ord)
    a0, d0 = metrics(ttf, size, chars)
    asc = a0 if asc is None else asc
    desc = d0 if desc is None else desc
    height = asc + desc
    f = ImageFont.truetype(ttf, size)
    table, blobs = [], []
    for ch in chars:
        adv = max(1, int(round(f.getlength(ch))))
        l, t, r, b = f.getbbox(ch, anchor="ls")
        left = max(0, -l); right = max(0, r - adv)        # виступи за межі кроку
        bw = left + adv + right
        im = Image.new('L', (bw, height), 0)
        if ch.strip():
            ImageDraw.Draw(im).text((left, asc), ch, font=f, fill=255, anchor='ls')
        levels = [min(7, int(round(((p / 255.0) ** gamma) * 7))) for p in im.getdata()]
        table.append((ord(ch), adv, left, right)); blobs.append(encode_glyph(levels))
    nm = name.encode('ascii') + b'utf-8'
    n = len(chars)
    mapsize = n * 10
    first = mapsize + 4                                   # як у зразку: 4 нульові байти після таблиці
    offs, cur = [], first
    for b in blobs: offs.append(cur); cur += len(b)
    body = bytearray()
    for (code, w, left, right), o, b in zip(table, offs, blobs):
        assert w < 256 and left < 256 and right < 256 and len(b) < 65536, (chr(code), w, left, right)
        body += struct.pack('<HBBB', code, w, left, right) + bytes((o & 0xFF, (o >> 8) & 0xFF, (o >> 16) & 0xFF)) + struct.pack('<H', len(b))
    body += b'\x00' * (first - mapsize)
    for b in blobs: body += b
    datasize = len(nm) + len(body)
    hdr = struct.pack('<BBBBBBBBBBBBIBBHIIBBBBBBHII',
                      4, 0xFF, 0, 0x0A, UTF8_ENC, 2, 0, height, 0xFF, 0xFF, 0, 0xFF, n, 6, len(nm), 0,
                      datasize, 44, 0xFF, 0, 1, 1, len(name), 0, 0, n, 0)
    assert len(hdr) == 44
    open(path, 'wb').write(hdr + nm + body)
    return dict(h=height, asc=asc, desc=desc, size=size)


def preview(path, text, out, fg=(255, 255, 255), bg=(8, 12, 16), scale=3):
    """Намалювати рядок шрифтом .zi так, як це робитиме Nextion (для перевірки на Mac)."""
    h, name, gl, ms = read(path)
    by = {g['code']: g for g in gl}
    W = sum(by[ord(c)]['w'] for c in text if ord(c) in by) + 8
    im = Image.new('RGB', (W, h['h'] + 4), bg); px = im.load(); x = 4
    for c in text:
        g = by.get(ord(c))
        if not g: continue
        lv = decode_glyph(g['data'], g['bw'], h['h'])
        for yy in range(h['h']):
            for xx in range(g['bw']):
                a = lv[yy * g['bw'] + xx] / 7
                X = x - g['a'] + xx
                if a and 0 <= X < W:
                    o = px[X, yy + 2]
                    px[X, yy + 2] = tuple(int(o[k] * (1 - a) + fg[k] * a) for k in range(3))
        x += g['w']
    im.resize((im.width * scale, im.height * scale), Image.NEAREST).save(out)


if __name__ == '__main__':
    import sys
    h, name, gl, ms = read(sys.argv[1])
    print({k: h[k] for k in ('encode', 'h', 'qyt', 'ver', 'namelen', 'datasize', 'dataaddr', 'encbeg', 'add8', 'true')}, name)
    print('перший зсув', gl[0]['off'], 'таблиця', h['qyt'] * 10)
    out = os.path.join(os.path.dirname(sys.argv[1]), 'dump'); os.makedirs(out, exist_ok=True)
    bad = 0
    for g in gl:
        lv = decode_glyph(g['data'], g['bw'], h['h'])
        if g['data'][0] == 3 and decode_glyph(encode_glyph(lv), g['bw'], h['h']) != lv: bad += 1
        if chr(g['code']) in 'AgҐї0Жj':
            glyph_png(lv, g['bw'], h['h'], os.path.join(out, 'U%04X.png' % g['code']))
    print('гліфів', len(gl), 'не збіглося після перекодування:', bad)
