"""Готові картинки (спрайти) для меню ПОТУЖНОГО РАДІО на Nextion — за ключами, які просить прошивка.

Ключ описує фігуру в пікселях Nextion і кольори RGB565 (шістнадцятково):
  R<r>.<колір>.<тло>                 квадрат 2r×2r із заокругленням r — чотири кути карток/кнопок
  C<r·4>.<колір>.<тло>               коло радіуса r (у чвертях пікселя)
  D<r·4>.<колір>.<R·4>.<колір2>.<тло>  коло на більшому колі з тим самим центром (обідок)
  A<r·4>.<w·4>.<a0>.<a1>.<колір>.<тло>  дуга: радіус, товщина, кути (0° — вгору, за годинниковою)
  I<номер>.<колір>.<тло>             значок m2icons (номер — з enum Icon), масштаб 4/3, поле 32×32
  S<рівень>.<увімк>.<вимк>.<тло>     рівень сигналу — чотири риски
Тло — колір RGB565, або «P<картинка>_<x>_<y>»: точний шматок картинки-тла з лівого верхнього кута
(x, y) — для нерухомих елементів на градієнті чи «світінні» (шапка меню, плеєр), або «K…» — колір
з однією фігурою (плашка під значком, рамка довкола квадрата) — див. synth_bg.
Прошивка шукає картинку за хешем FNV-1a ключа (m2gfx.cpp), тож ключ має збігатися до символу.
"""
import math, os, re
from PIL import Image
import gfx

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
K = 4 / 3


def icon_names():
    src = open(os.path.join(ROOT, 'source', 'port', 'm2', 'm2icons.h')).read()
    body = src[src.index('enum Icon'):]
    body = body[:body.index('};')]
    names = [n[3:] for n in re.findall(r'IC_[A-Z]+', body)]
    return names[:names.index('N')]


ICONS = None


def c565(h):
    return gfx.from565(int(h, 16))


# ---------------------------------------------------------------- фігури за назвою (Gfx::shape), поле 40×40
BLUE = gfx.C['BLUE']; ACC = gfx.C['ACC']; CLOUD = (205, 210, 220); CLOUD2 = (140, 148, 160)


def _sunny(cv, x, y, s, c, k):
    cv.circle(20 + x * k, 20 + y * k, 3.6 * s * k, c)
    for i in range(8):
        a = i * 0.7854
        cv.line(20 + (x + 5.9 * s * math.cos(a)) * k, 20 + (y + 5.9 * s * math.sin(a)) * k,
                20 + (x + 8.0 * s * math.cos(a)) * k, 20 + (y + 8.0 * s * math.sin(a)) * k, 1.7 * k, c)


def _cloud(cv, x, y, s, c, k):
    cv.circle(20 + (x - 4 * s) * k, 20 + (y + 1 * s) * k, 4.2 * s * k, c)
    cv.circle(20 + (x + 2 * s) * k, 20 + (y - 1.5 * s) * k, 5.5 * s * k, c)
    cv.box(20 + int(x - 8 * s) * k, 20 + int(y + 1 * s) * k, int(16 * s) * k, int(5 * s) * k, int(2.5 * s) * k, c)


def shape(cv, name, c, bg):
    """Малюнки з m2player ПОТУЖНОГО, яких нема серед значків (poly і похилі лінії). Центр (20,20), масштаб 4/3."""
    k = K
    L = lambda x0, y0, x1, y1, w, col=c: cv.line(20 + x0 * k, 20 + y0 * k, 20 + x1 * k, 20 + y1 * k, w * k, col)
    P = lambda pts, col=c: cv.poly([(20 + x * k, 20 + y * k) for x, y in pts], col)
    O = lambda x, y, r, col=c: cv.circle(20 + x * k, 20 + y * k, r * k, col)
    if name == 'bell':                     # stBell: дзвоник у рядку стану
        O(0, -2.4, 4.2); P([(-4.2, -2.4), (4.2, -2.4), (5.2, 2.6), (-5.2, 2.6)]); L(-5.8, 2.9, 5.8, 2.9, 1.6); O(0, 5.2, 1.5)
    elif name == 'bt':                     # знак Bluetooth (бездротова колонка)
        L(-3.6, -3.2, 3.6, 3.4, 1.6); L(3.6, 3.4, 0, 6.8, 1.6); L(0, 6.8, 0, -6.8, 1.6); L(0, -6.8, 3.6, -3.4, 1.6); L(3.6, -3.4, -3.6, 3.2, 1.6)
    elif name == 'prev':                   # ⏮ у пульті картки/проповіді
        L(-6, -6, -6, 6, 2); P([(6, -6), (6, 6), (-4, 0)])
    elif name == 'next':
        L(6, -6, 6, 6, 2); P([(-6, -6), (-6, 6), (4, 0)])
    elif name == 'tick':                   # «Готово» на сторінці підключення — вирізом
        L(-13, 1, -4, 10, 5); L(-4, 10, 14, -9, 5)
    elif name == 'cross':
        L(-11, -11, 11, 11, 5); L(-11, 11, 11, -11, 5)
    elif name == 'strike':                 # перекреслене око (пароль сховано)
        L(-9, 9, 9, -9, 1.8)
    elif name[0] == 'w':                   # погода: m2player _drawClock, S = 0.8
        ic = int(name[1:]); S = 0.8
        if ic == 0: _sunny(cv, 0, -1, S, ACC, k)
        elif ic == 1: _sunny(cv, 3 * S, -5 * S, S * 0.85, ACC, k); _cloud(cv, -2 * S, 1 * S, 0.9 * S, CLOUD, k)
        elif ic in (2, 3):
            if ic == 3: _cloud(cv, 5 * S, -4 * S, 0.7 * S, CLOUD2, k)
            _cloud(cv, 0, 0, S, CLOUD, k)
        elif ic in (4, 5):
            _cloud(cv, 0, -3 * S, S, CLOUD, k)
            for j in range(3): L((-5 + j * 5) * S, 5 * S, (-7 + j * 5) * S, 10 * S, 1.6, BLUE)
        elif ic == 6:
            _cloud(cv, 0, -3 * S, S, CLOUD, k)
            pts = [(1 * S, 3 * S), (-4 * S, 10 * S), (-1 * S, 10 * S), (-3 * S, 15 * S)]
            for a, b in zip(pts, pts[1:]): L(a[0], a[1], b[0], b[1], 2, ACC)
        elif ic == 7:
            _cloud(cv, 0, -3 * S, S, CLOUD, k)
            for j in range(3): O((-5 + j * 5) * S, 8 * S, 1.5, (255, 255, 255))
        elif ic == 8:
            for j in range(3):
                o = (j & 1) * 3
                L((-8 + o) * S, (-4 + j * 5) * S, (8 - o) * S, (-4 + j * 5) * S, 2, CLOUD)
    else:
        raise KeyError(name)


def ink_box(key):
    """Де на полі спрайта є малюнок (для значків: картинка лише по контуру, щоб її квадрат не виліз за
    кнопку чи плашку, на якій значок лежить). → (x0, y0, x1, y1) або None — усе поле."""
    t = key[0]
    if t not in 'IN': return None
    f = key[1:].split('.')
    a = _render(t + '.'.join(f[:2] + ['0000'])).convert('L')
    bb = a.point(lambda v: 255 if v > 3 else 0).getbbox()
    if not bb: return (15, 15, 17, 17)
    x0, y0, x1, y1 = bb
    return (max(0, x0 - 1), max(0, y0 - 1), min(a.width, x1 + 1), min(a.height, y1 + 1))


def render_ofs(key, pics=None):
    """→ (Image, ox, oy): картинка й зсув її лівого верхнього кута від центру поля."""
    im = render(key, pics)
    bb = ink_box(key)
    if bb:
        return im.crop(bb), bb[0] - im.width // 2, bb[1] - im.height // 2
    return im, -(im.width // 2), -(im.height // 2)


def synth_bg(spec, w, h):
    """«K<тло>_R_<x>_<y>_<w>_<h>_<r>_<колір>» чи «K<тло>_O_<cx·4>_<cy·4>_<r·4>_<колір>»: тло поля з фігурою."""
    p = spec[1:].split('_')
    cv = gfx.Canvas(w, h, c565(p[0]))
    if p[1] == 'R':
        x, y, ww, hh, r = (int(v) for v in p[2:7])
        if r > 0: cv.box(x, y, ww, hh, r, c565(p[7]))
        else: cv.fill(x, y, ww, hh, c565(p[7]))
    elif p[1] == 'O':
        cx, cy, r = (int(v) / 4 for v in p[2:5])
        cv.circle(cx, cy, r, c565(p[5]))
    return cv.image()


def render(key, pics=None):
    """→ (Image RGB) для ключа. pics — {номер: Image} картинок-тла для ключів «…P<n>_<x>_<y>»."""
    t, rest = key[0], key[1:]
    f = rest.split('.')
    if f[-1][0] in 'PK':
        # На картинці чи на складеному тлі: малюємо двічі — на чорному й на білому, звідти
        # прозорість кожного пікселя, і кладемо на точний шматок тла.
        k0 = t + '.'.join(f[:-1] + ['0000']); k1 = t + '.'.join(f[:-1] + ['FFFF'])
        a, b = _render(k0), _render(k1)
        w, h = a.size
        if f[-1][0] == 'P':
            n, x, y = (int(v) for v in f[-1][1:].split('_'))
            bg = pics[n].crop((x, y, x + w, y + h)).convert('RGB')
        else:
            bg = synth_bg(f[-1], w, h)
        pa, pb, pg = a.load(), b.load(), bg.load()
        out = Image.new('RGB', (w, h)); po = out.load()
        for yy in range(h):
            for xx in range(w):
                po[xx, yy] = tuple(min(255, int(round(pa[xx, yy][c] + (pb[xx, yy][c] - pa[xx, yy][c]) / 255 * pg[xx, yy][c]))) for c in range(3))
        return out
    return _render(key)


def _render(key):
    global ICONS
    t, rest = key[0], key[1:]
    f = rest.split('.')
    if t == 'R':
        r = int(f[0]); fg, bg = c565(f[1]), c565(f[2])
        cv = gfx.Canvas(2 * r, 2 * r, bg); cv.box(0, 0, 2 * r, 2 * r, r, fg)
        return cv.image()
    if t == 'C':
        rd = int(f[0]) / 4; fg, bg = c565(f[1]), c565(f[2])
        S = 2 * math.ceil(rd) + 2
        cv = gfx.Canvas(S, S, bg); cv.circle(S / 2, S / 2, rd, fg)
        return cv.image()
    if t == 'D':                      # коло на більшому колі з тим самим центром
        rd, fg, Rd, rc, bg = int(f[0]) / 4, c565(f[1]), int(f[2]) / 4, c565(f[3]), c565(f[4])
        S = 2 * math.ceil(Rd) + 2
        cv = gfx.Canvas(S, S, bg); cv.circle(S / 2, S / 2, Rd, rc); cv.circle(S / 2, S / 2, rd, fg)
        return cv.image()
    if t == 'A':
        rd, wd = int(f[0]) / 4, int(f[1]) / 4; a0, a1 = int(f[2]), int(f[3]); fg, bg = c565(f[4]), c565(f[5])
        S = 2 * math.ceil(rd + wd / 2) + 2
        cv = gfx.Canvas(S, S, bg); cv.arc(S / 2, S / 2, rd, wd, fg, a0, a1)
        return cv.image()
    if t == 'I':
        if ICONS is None: ICONS = icon_names()
        name = ICONS[int(f[0])]; fg, bg = c565(f[1]), c565(f[2])
        cv = gfx.Canvas(32, 32, bg); gfx.icon(cv, name, 16, 16, fg, bg, K)
        return cv.image()
    if t == 'N':
        fg, bg = c565(f[1]), c565(f[2])
        cv = gfx.Canvas(40, 40, bg); shape(cv, f[0], fg, bg)
        return cv.image()
    if t == 'S':
        lv = int(f[0]); on, off, bg = c565(f[1]), c565(f[2]), c565(f[3])
        cv = gfx.Canvas(24, 18, bg)
        # m2icons signalBars: риска k — центр x+1.2+4.2k, висота 3+2.3k, товщина 2.4; тут центр групи — (x+7.5, bottom−5)
        for k in range(4):
            h = 3 + 2.3 * k; bx = -7.5 + 1.2 + 4.2 * k
            cv.line(12 + bx * K, 9 + (5 - 1.2) * K, 12 + bx * K, 9 + (5 - h + 1.2) * K, 2.4 * K, on if k < lv else off)
        return cv.image()
    raise KeyError(key)


def fnv(s):
    h = 2166136261
    for b in s.encode('utf-8'):
        h = ((h ^ b) * 16777619) & 0xFFFFFFFF
    return h


def pack(keys, width=480, max_h=960, pics=None):
    """Спрайти → атласи (картинки width×до max_h). Повертає (атласи [Image], таблиця {key: (атлас, x, y, w, h, ox, oy)});
    ox, oy — де лівий верхній кут відносно центру поля (для Gfx::sprite, що ставить картинку за центром)."""
    items, ofs = [], {}
    for k in sorted(set(keys)):
        im, ox, oy = render_ofs(k, pics)
        items.append((k, im.convert('RGB'))); ofs[k] = (ox, oy)
    items.sort(key=lambda kv: (-kv[1].height, -kv[1].width, kv[0]))
    atlases, table = [], {}
    x = y = shelf = 0; cur = []
    def close():
        nonlocal cur, x, y, shelf
        if not cur: return
        h = y + shelf
        a = Image.new('RGB', (width, max(1, h)), (255, 0, 255))
        for k, im, px, py in cur: a.paste(im, (px, py))
        atlases.append(a); cur = []; x = y = shelf = 0
    for k, im in items:
        w, h = im.size
        if x + w > width: x = 0; y += shelf; shelf = 0
        if y + h > max_h: close()
        cur.append((k, im, x, y)); table[k] = (len(atlases), x, y, w, h) + ofs[k]
        x += w; shelf = max(shelf, h)
    close()
    return atlases, table


if __name__ == '__main__':
    import sys
    ks = sys.argv[1:] or ['R16.1905.0862', 'C48.FFDF.2987', 'I7.E68B.1905', 'S3.8CB4.2987.10C4', 'A64.12.0.90.E68B.1905']
    for k in ks:
        im = render(k); print(k, im.size)
        im.resize((im.width * 6, im.height * 6), Image.NEAREST).save(os.path.join(ROOT, 'build', 'nx', 'spr_%s.png' % k.replace('.', '_')))
