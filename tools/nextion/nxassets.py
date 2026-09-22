"""Картинки й шрифти меню ПОТУЖНОГО РАДІО для Nextion + таблиці для прошивки (src/m2/nxassets.{h,cpp}).

  фон меню (градієнт 0..90 → 0..120 px), фон екрана оновлення (0..120 → 0..160 px),
  атласи спрайтів за ключами (sprites.py), шрифти (fonts.py) з ширинами літер CP1251.

Номери картинок у .tft — порядок у списку pictures(); прошивка знає їх з nxassets.h.
"""
import json, os, sys
from PIL import Image
sys.path.insert(0, os.path.dirname(__file__))
import gfx, sprites, zifont, fonts

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
OUT = os.path.join(ROOT, 'build', 'nx', 'assets')
SRC = os.path.join(ROOT, 'source', 'yoRadio', 'src', 'm2')
KEYS = os.path.join(ROOT, 'nextion', 'sprite-keys.txt')      # ключі, які просить прошивка (поповнює nxhost)


def grad_bg(h_virtual):
    """Фон сторінки: вертикальний перехід C_BGTOP → C_BG на 0..h (у координатах 320×240), нижче — C_BG."""
    gh = h_virtual * 4 / 3
    im = Image.new('RGB', (480, 320)); px = im.load()
    top, bg = gfx.C['BGTOP'], gfx.C['BG']
    for y in range(320):
        k = y / gh if y < gh else 1
        c = tuple(t + (b - t) * k for t, b in zip(top, bg))
        for x in range(480): px[x, y] = tuple(int(round(v)) for v in c)
    return gfx.dither565(im)


def player_bg(glow565):
    """Тло плеєра з «світінням» — формула ПОТУЖНОГО (m2player _makeBg) у координатах 320×240:
    точка Nextion (x, y) ↔ (x/1,5, y·3/4). Дизеринг 4×4, як там."""
    BAY = [0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5]
    def ch(v): return (((v >> 11) & 31) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31)
    bg, top, gl = ch(0x0862), ch(0x10C4), ch(glow565)
    im = Image.new('RGB', (480, 320)); px = im.load()
    for Y in range(320):
        y = Y * 0.75
        ky = 1 - y / 110 if y < 110 else 0
        for X in range(480):
            x = X / 1.5
            dx = (x - 50) / 190; dy = (y - 30) / 150
            d = max(0.0, 1 - (dx * dx + dy * dy)); a = d * d * 0.32
            c = [b + (t - b) * ky for b, t in zip(bg, top)]
            c = [v + (g - v) * a for v, g in zip(c, gl)]
            dd = BAY[((Y & 3) << 2) | (X & 3)]
            r = min(31, (int(c[0]) * 31 + dd * 16) // 255); g = min(63, (int(c[1]) * 63 + dd * 16) // 255); b = min(31, (int(c[2]) * 31 + dd * 16) // 255)
            px[X, Y] = ((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2))
    return im


def rgb565(r, g, b): return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
PAL = [0x3A8D, 0x5A4B, 0x2C6A, 0x6A28, 0x2B0F, 0x7A6C, 0x4B09, 0x31CC]


def font_tables(meta):
    """Ширини літер (пікселі Nextion) у порядку спільної таблиці символів fonts.CHARS і де в клітинці
    починається й кінчається малюнок літери (перший і останній рядки з фарбою) — щоб поле тексту
    на екрані було не вище за самі літери."""
    out, ink = {}, {}
    for role, m in meta.items():
        h, name, gl, ms = zifont.read(os.path.join(fonts.OUT, m['file']))
        by = {g['code']: g for g in gl}
        out[role] = [by[ord(ch)]['w'] if ord(ch) in by else 0 for ch in fonts.CHARS]
        rows = []
        for ch in fonts.CHARS:
            g = by.get(ord(ch))
            if not g: rows.append((255, 0)); continue
            lv = zifont.decode_glyph(g['data'], g['bw'], h['h'])
            used = [y for y in range(h['h']) if any(lv[y * g['bw']:(y + 1) * g['bw']])]
            rows.append((used[0], used[-1]) if used else (255, 0))
        ink[role] = rows
    return out, ink


def load_keys():
    if not os.path.exists(KEYS): return []
    return [l.strip() for l in open(KEYS, encoding='utf-8') if l.strip() and not l.startswith('#')]


def build():
    os.makedirs(OUT, exist_ok=True)
    for f in os.listdir(OUT): os.remove(os.path.join(OUT, f))
    meta = fonts.main()
    pics = []                                  # (ім'я для enum, файл)
    def pic(name, im):
        path = os.path.join(OUT, '%03d_%s.png' % (len(pics), name.lower())); im.save(path); pics.append((name, path))
    pic('BG_MENU', grad_bg(90))
    pic('BG_OTA', grad_bg(120))
    for i, g in enumerate(PAL): pic('PL_%d' % i, player_bg(g))
    pic('PL_DEF', player_bg(rgb565(40, 70, 110)))
    pic('PL_SD', player_bg(rgb565(160, 100, 40)))
    pic('PL_SERMON', player_bg(rgb565(110, 70, 160)))
    keys = load_keys()
    atl, table = sprites.pack(keys, pics={i: Image.open(f) for i, (n, f) in enumerate(pics)}) if keys else ([], {})
    first_atlas = len(pics)
    for i, a in enumerate(atl): pic('ATLAS%d' % i, a)

    # ---- nxassets.h / .cpp
    adv, ink = font_tables(meta)
    order = sorted(meta.items(), key=lambda kv: kv[1]['id'])
    h = ['/*  Створює tools/nextion/nxassets.py — вручну не правити.  */',
         '#ifndef nxassets_h', '#define nxassets_h', '#include <stdint.h>', '#include "m2gfx.h"', '',
         'namespace m2 {', '',
         '/*  спрайт в атласі: хеш FNV-1a ключа (sprites.py), картинка, де в ній і розмір  */',
         'struct NxSpr { uint32_t hash; uint16_t x, y, w, h; int8_t ox, oy; uint8_t pic; };   /* ox, oy — лівий верхній кут від центру */',
         'extern const NxSpr NX_SPR[];', 'extern const uint16_t NX_SPR_N;',
         'extern const uint16_t NX_CP[];', 'extern const uint16_t NX_CP_N;', '',
         '/*  номери картинок у .tft  */', 'enum : uint8_t {']
    for i, (n, _) in enumerate(pics): h.append('  NXP_%s = %d,' % (n, i))
    h.append('  NXP_N = %d' % len(pics))
    h += ['};', '', '}  // namespace m2', '#endif', '']
    open(os.path.join(SRC, 'nxassets.h'), 'w', encoding='utf-8').write('\n'.join(h))

    c = ['/*  Створює tools/nextion/nxassets.py — вручну не правити.  */', '#include "nxassets.h"', '#include "m2theme.h"', '',
         'namespace m2 {', '']
    rows = sorted(((sprites.fnv(k), v, k) for k, v in table.items()), key=lambda r: r[0])
    hashes = [r[0] for r in rows]
    assert len(set(hashes)) == len(hashes), 'збіг хешів ключів спрайтів'
    c.append('const NxSpr NX_SPR[] = {')
    for hs, (ai, x, y, w, hh, ox, oy), k in rows:
        c.append('  { 0x%08XUL, %d, %d, %d, %d, %d, %d, %d },   // %s' % (hs, x, y, w, hh, ox, oy, first_atlas + ai, k))
    if not rows: c.append('  { 0, 0, 0, 0, 0, 0, 0, 0 }')
    c.append('};')
    c.append('const uint16_t NX_SPR_N = %d;' % len(rows))
    c.append('')
    c.append('/*  символи шрифтів (коди Unicode за зростанням) — ширини нижче в тому ж порядку  */')
    c.append('const uint16_t NX_CP[] = { %s };' % ', '.join('0x%04X' % ord(ch) for ch in fonts.CHARS))
    c.append('const uint16_t NX_CP_N = %d;' % len(fonts.CHARS))
    for role, m in order:
        c.append('static const uint8_t ADV_%s[%d] = { %s };' % (role, len(fonts.CHARS), ', '.join(str(v) for v in adv[role])))
        c.append('static const uint8_t INK_%s[%d] = { %s };' % (role, 2 * len(fonts.CHARS), ', '.join('%d, %d' % r for r in ink[role])))
        c.append('const NxFont NXF_%s = { %d, %d, %d, ADV_%s, INK_%s };   // %s, H=%d' % (role, m['id'], m['h'], m['asc'], role, role, m['file'], m['cap']))
    c += ['', '}  // namespace m2', '']
    open(os.path.join(SRC, 'nxassets.cpp'), 'w', encoding='utf-8').write('\n'.join(c))
    print('картинок %d (атласів %d), спрайтів %d, шрифтів %d' % (len(pics), len(atl), len(rows), len(meta)))
    return dict(pics=pics, fonts=[os.path.join(fonts.OUT, m['file']) for r, m in order], meta=meta)


if __name__ == '__main__':
    build()
