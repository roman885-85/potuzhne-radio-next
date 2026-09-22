"""Графіка екрана ПОТУЖНОГО РАДІО для Nextion 480×320 — повторює примітиви src/m2/m2gfx.cpp.

Усе малюється з 4-кратною надвибіркою (SS) і зменшується з LANCZOS — так само м'які краї, як у
прошивки ПОТУЖНОГО РАДІО. Кути дуг — у градусах, 0° = вгору, за годинниковою стрілкою (як m2gfx).
Кольори — RGB888; помічник c565() приводить до того, що реально покаже панель (RGB565).
Опис значків — docs/analysis/potuzhne-ui-inventory.md §7.
"""
import math, os
from PIL import Image, ImageDraw, ImageFont, ImageChops

SS = 4
FONTS = os.path.join(os.path.dirname(__file__), '..', '..', 'nextion', 'fonts-src')

# ------------------------------------------------------------------ тема (m2theme.h, §1.1)
def c565(rgb):
    r, g, b = rgb
    r5, g6, b5 = r >> 3, g >> 2, b >> 3
    return ((r5 << 3) | (r5 >> 2), (g6 << 2) | (g6 >> 4), (b5 << 3) | (b5 >> 2))

def to565(rgb):
    r, g, b = rgb
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

def from565(v):
    return c565((((v >> 11) & 31) << 3, ((v >> 5) & 63) << 2, (v & 31) << 3))

C = dict(
    BG=from565(0x0862), BGTOP=from565(0x10C4), SURF=from565(0x1905), SURF2=from565(0x2987),
    LINE=from565(0x29A8), TXT=from565(0xF79D), TXT2=from565(0x8CB4), TXT3=from565(0x5B2E),
    ACC=from565(0xE68B), ACCTXT=from565(0x2922), KNOB=from565(0xFFDF), ORANGE=from565(0xFC47),
    BLUE=from565(0x4D1F), TEAL=from565(0x3694), VIOLET=from565(0xB3FF), RED=from565(0xFAAA),
    GREY=from565(0x7C11), PINK=from565(0xFB14), GREEN=from565(0x464D), REC=from565(0xE9E7),
    WHITE=(255, 255, 255), PULTSUB=from565(0x5243),
)
PAL = [from565(v) for v in (0x3A8D, 0x5A4B, 0x2C6A, 0x6A28, 0x2B0F, 0x7A6C, 0x4B09, 0x31CC)]
GLOW_SERMON = (110, 70, 160); GLOW_SD = (160, 100, 40); GLOW_DEFAULT = (40, 70, 110)

def blend(bg, fg, a):
    return tuple(int((f * a + b * (255 - a) + 127) / 255) for b, f in zip(bg, fg))

BAYER = [0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5]

def dither565(img):
    """Упорядкований дизеринг 4×4 до RGB565 (як у ПОТУЖНОГО РАДІО — без смуг на градієнтах)."""
    px = img.load(); w, h = img.size
    for y in range(h):
        for x in range(w):
            t = (BAYER[(y & 3) * 4 + (x & 3)] + 0.5) / 16.0
            r, g, b = px[x, y][:3]
            r5 = min(31, int(r * 31 / 255 + t)); g6 = min(63, int(g * 63 / 255 + t)); b5 = min(31, int(b * 31 / 255 + t))
            px[x, y] = ((r5 << 3) | (r5 >> 2), (g6 << 2) | (g6 >> 4), (b5 << 3) | (b5 >> 2))
    return img

# ------------------------------------------------------------------ полотно з надвибіркою
class Canvas:
    def __init__(self, w, h, bg=(0, 0, 0)):
        self.w, self.h = w, h
        self.im = Image.new('RGB', (w * SS, h * SS), bg)
        self.d = ImageDraw.Draw(self.im)

    # --- примітиви m2gfx (координати й розміри — у пікселях екрана, дробові дозволені)
    def line(self, x0, y0, x1, y1, w, c):
        s = SS
        self.d.line((x0 * s, y0 * s, x1 * s, y1 * s), fill=c, width=max(1, round(w * s)))
        r = w * s / 2
        for x, y in ((x0, y0), (x1, y1)):
            self.d.ellipse((x * s - r, y * s - r, x * s + r, y * s + r), fill=c)

    def circle(self, cx, cy, r, c):
        s = SS; self.d.ellipse(((cx - r) * s, (cy - r) * s, (cx + r) * s, (cy + r) * s), fill=c)

    def arc(self, cx, cy, r, w, c, a0=0, a1=360):
        """Дуга шириною w по радіусу r; кути m2gfx: 0° — вгору, за годинниковою."""
        s = SS
        if a1 < a0: a1 += 360
        # PIL: 0° — праворуч, за годинниковою → m2 a ↦ a − 90
        box = ((cx - r - w / 2) * s, (cy - r - w / 2) * s, (cx + r + w / 2) * s, (cy + r + w / 2) * s)
        if a1 - a0 >= 360:
            self.d.ellipse(box, outline=c, width=max(1, round(w * s)))
        else:
            self.d.arc(box, a0 - 90, a1 - 90, fill=c, width=max(1, round(w * s)))
            for a in (a0, a1):                      # круглі кінці
                t = math.radians(a - 90)
                self.circle(cx + r * math.cos(t), cy + r * math.sin(t), w / 2, c)

    def poly(self, pts, c):
        s = SS; self.d.polygon([(x * s, y * s) for x, y in pts], fill=c)

    def box(self, x, y, w, h, r, c):
        s = SS; self.d.rounded_rectangle((x * s, y * s, (x + w) * s - 1, (y + h) * s - 1), radius=r * s, fill=c)

    def frame(self, x, y, w, h, r, t, c):
        s = SS; self.d.rounded_rectangle((x * s, y * s, (x + w) * s - 1, (y + h) * s - 1), radius=r * s, outline=c, width=max(1, round(t * s)))

    def fill(self, x, y, w, h, c):
        s = SS; self.d.rectangle((x * s, y * s, (x + w) * s - 1, (y + h) * s - 1), fill=c)

    def text(self, x, y, s, font, size, c, anchor='ls', maxw=None, spacing=0):
        """Текст за базовою лінією (anchor як у PIL: ls — ліворуч, ms — центр, rs — праворуч)."""
        f = ImageFont.truetype(os.path.join(FONTS, font), round(size * SS))
        if maxw is not None:
            while s and f.getlength(s) > maxw * SS:
                s = s[:-1].rstrip()
                if f.getlength(s + '…') <= maxw * SS: s = s + '…'; break
        self.d.text((x * SS, y * SS), s, font=f, fill=c, anchor=anchor)
        return f.getlength(s) / SS

    def paste(self, img, x, y):
        self.im.paste(img.resize((img.width * SS, img.height * SS), Image.NEAREST), (x * SS, y * SS))

    def image(self):
        return self.im.resize((self.w, self.h), Image.LANCZOS)

def textw(s, font, size):
    return ImageFont.truetype(os.path.join(FONTS, font), round(size * SS)).getlength(s) / SS

# ------------------------------------------------------------------ фон: вертикальний градієнт і «світіння»
def glow_bg(w, h, glow=None, grad_h=None, spot=True, scale=1.0):
    """Фон плеєра (§4.2) / сторінок меню (§1.6) у 480×320. scale — коефіцієнт відносно 320×240."""
    grad_h = grad_h if grad_h is not None else 110 * scale
    im = Image.new('RGB', (w, h)); px = im.load()
    bg, top = C['BG'], C['BGTOP']
    for y in range(h):
        ky = 1 - y / grad_h if y < grad_h else 0
        base = tuple(b + (t - b) * ky for b, t in zip(bg, top))
        for x in range(w):
            if glow is not None and spot:
                dx = (x / scale - 50) / 190; dy = (y / scale - 30) / 150
                d = max(0.0, 1 - dx * dx - dy * dy); a = d * d * 0.32
                p = tuple(bb + (g - bb) * a for bb, g in zip(base, glow))
            else:
                p = base
            px[x, y] = tuple(int(round(v)) for v in p)
    return dither565(im)

# ------------------------------------------------------------------ значки (§7), коробка ~20×20 навколо (cx, cy)
def icon(cv, name, cx, cy, c, bg, k=1.0):
    """Намалювати значок name з центром (cx, cy) кольором c; bg — колір «дірок»; k — масштаб."""
    L = lambda x0, y0, x1, y1, w: cv.line(cx + x0 * k, cy + y0 * k, cx + x1 * k, cy + y1 * k, w * k, c)
    Lb = lambda x0, y0, x1, y1, w: cv.line(cx + x0 * k, cy + y0 * k, cx + x1 * k, cy + y1 * k, w * k, bg)
    O = lambda x, y, r, col=c: cv.circle(cx + x * k, cy + y * k, r * k, col)
    A = lambda x, y, r, w, a0=0, a1=360, col=c: cv.arc(cx + x * k, cy + y * k, r * k, w * k, col, a0, a1)
    P = lambda pts, col=c: cv.poly([(cx + x * k, cy + y * k) for x, y in pts], col)
    B = lambda x, y, w, h, r, col=c: cv.box(cx + x * k, cy + y * k, w * k, h * k, r * k, col)
    F = lambda x, y, w, h, r, t: cv.frame(cx + x * k, cy + y * k, w * k, h * k, r * k, t * k, c)
    if name == 'BACK': L(3, -6, -3, 0, 2.3); L(-3, 0, 3, 6, 2.3)
    elif name == 'CHEV': L(-2, -4.5, 2.5, 0, 1.9); L(2.5, 0, -2, 4.5, 1.9)
    elif name == 'MOON': O(0, 0, 8); O(4.6, -3.6, 7, bg)
    elif name == 'ALARM':
        A(0, 1, 7.3, 2); L(0, 1, 0, -3, 1.9); L(0, 1, 3, 2.6, 1.9); L(-8.2, -5.5, -5.2, -8.3, 2.1); L(8.2, -5.5, 5.2, -8.3, 2.1)
    elif name == 'REC': A(0, 0, 8, 2); O(0, 0, 4.3)
    elif name == 'CARD':
        P([(-6.5, -8.5), (3, -8.5), (7, -4.5), (7, 8.5), (-6.5, 8.5)])
        for x in (-3.5, -0.5, 2.5): Lb(x, -6.3, x, -3, 1.3)
    elif name == 'RADIO':
        O(0, -1.5, 2.5); L(0, 1, 0, 8.5, 2)
        for r, a in ((6, (50, 130, 230, 310)), (10, (55, 125, 235, 305))):
            A(0, -1.5, r, 1.8, a[0], a[1]); A(0, -1.5, r, 1.8, a[2], a[3])
    elif name == 'STAR':
        R = 9; pts = []
        for i in range(10):
            rr = R if i % 2 == 0 else 0.45 * R; t = math.radians(-90 + i * 36)
            pts.append((rr * math.cos(t), 0.5 + rr * math.sin(t)))
        P(pts)
    elif name == 'CROSS': L(0, -8, 0, 8.5, 2.7); L(-6, -3, 6, -3, 2.7)
    elif name == 'EQ': L(-6, 7, -6, -1, 2.5); L(0, 7, 0, -8, 2.5); L(6, 7, 6, -3, 2.5)
    elif name == 'SUN':
        O(0, 0, 3.9)
        for i in range(8):
            t = math.radians(i * 45); L(6.4 * math.sin(t), -6.4 * math.cos(t), 8.6 * math.sin(t), -8.6 * math.cos(t), 1.8)
    elif name == 'GEAR':
        A(0, 0, 4.7, 2.3)
        for i in range(8):
            t = math.radians(i * 45); L(6.8 * math.sin(t), -6.8 * math.cos(t), 8.8 * math.sin(t), -8.8 * math.cos(t), 2.5)
    elif name == 'LIST':
        for k2 in range(3): O(-6.5, -5 + 5 * k2, 1.5); L(-2.5, -5 + 5 * k2, 7.5, -5 + 5 * k2, 2.1)
    elif name == 'WIFI':
        for r in (4.2, 8.4, 12.6): A(0, 6.5, r, 2, -45, 45)
        O(0, 6.5, 1.9)
    elif name == 'CLOCK': A(0, 0, 7.8, 2); L(0, 0, 0, -4.6, 1.9); L(0, 0, 3.6, 1.2, 1.9)
    elif name == 'INFO': A(0, 0, 7.8, 2); O(0, -3.7, 1.35); L(0, -0.6, 0, 4.3, 2.1)
    elif name == 'POWER': A(0, 0.8, 7.2, 2.1, 38, 322); L(0, -8.4, 0, -1.2, 2.1)
    elif name == 'CODE':
        L(-4, -5, -8, 0, 1.9); L(-8, 0, -4, 5, 1.9); L(4, -5, 8, 0, 1.9); L(8, 0, 4, 5, 1.9); L(1.6, -6.5, -1.6, 6.5, 1.7)
    elif name == 'SPEAKER':
        P([(-8.5, -3.2), (-4.2, -3.2), (0.5, -7.8), (0.5, 7.8), (-4.2, 3.2), (-8.5, 3.2)])
        A(0.5, 0, 4.8, 1.7, 50, 130); A(0.5, 0, 8.6, 1.7, 55, 125)
    elif name == 'LOCK': B(-4.5, -1, 9, 7, 2); A(0, -1.5, 3, 1.5, 270, 90)
    elif name == 'PLUS': L(-6.5, 0, 6.5, 0, 2.3); L(0, -6.5, 0, 6.5, 2.3)
    elif name == 'CLOSE': L(-5.5, -5.5, 5.5, 5.5, 2.2); L(-5.5, 5.5, 5.5, -5.5, 2.2)
    elif name == 'UP': L(0, 7, 0, -6, 2.2); L(-5.5, -1, 0, -6.5, 2.2); L(0, -6.5, 5.5, -1, 2.2)
    elif name == 'CHECK': L(-6, 0.5, -2, 4.5, 2.3); L(-2, 4.5, 6.5, -4.5, 2.3)
    elif name == 'REFRESH': A(0, 0, 7, 2, 30, 300); P([(2.5, -10.5), (2.5, -3.5), (8.5, -7)])
    elif name == 'KEYS':
        F(-9, -6, 18, 12, 3, 2)
        for x in (-4, 0, 4): O(x, -1.5, 1.1)
        L(-4, 2.5, 4, 2.5, 1.4)
    elif name == 'NOTE': O(-4, 5, 3.2); L(-1.4, 5, -1.4, -8, 1.9); L(-1.4, -8, 6, -5.5, 2.2)
    elif name == 'PERSON': O(0, -4.5, 3.8); A(0, 9, 8, 2.6, 300, 60)
    elif name == 'WAVE':
        for i, hh in enumerate((3, 7, 10, 6, 3)): L(-8 + 4 * i, -hh, -8 + 4 * i, hh, 2)
    elif name == 'CHIP':
        F(-6, -6, 12, 12, 2, 2)
        for v in (-3.5, 0, 3.5): L(-7, v, -9, v, 1.3); L(7, v, 9, v, 1.3); L(v, -7, v, -9, 1.3); L(v, 7, v, 9, 1.3)
    elif name == 'PLAY': P([(-4.5, -7), (-4.5, 7), (7, 0)])
    elif name == 'TRASH':
        L(-7.5, -5.5, 7.5, -5.5, 2); L(-2.5, -8, 2.5, -8, 2); P([(-6, -3.5), (6, -3.5), (5, 8.5), (-5, 8.5)])
    elif name == 'RESTART': A(0, 0, 7.2, 2.1, 60, 350); P([(-1.2, -11), (-1.2, -3.6), (4.6, -7.3)])
    elif name == 'MENU':
        for y in (-5, 0, 5): L(-7, y, 7, y, 2.1)
    elif name == 'SPLASH':
        O(0, -2, 2.6); L(0, 0, 0, 8, 2); A(0, -2, 6, 1.8, 50, 130); A(0, -2, 6, 1.8, 230, 310)
    elif name == 'BELL':
        O(0, -2.5, 6.2); P([(-6.2, -2.5), (6.2, -2.5), (8.5, 5), (-8.5, 5)]); L(-9, 5.4, 9, 5.4, 2); O(0, 8.3, 2.2)
    elif name == 'GLOBE':
        A(0, 0, 8, 1.8); L(-8, 0, 8, 0, 1.5); A(9, 0, 12, 1.5, 222, 318); A(-9, 0, 12, 1.5, 42, 138)
    elif name == 'START': A(0, 0, 8, 1.8); P([(-2.5, -4.5), (-2.5, 4.5), (5, 0)])
    elif name == 'DOWN': L(-5, -2.5, 0, 2.5, 2.2); L(0, 2.5, 5, -2.5, 2.2)
    elif name == 'BACKSPACE':
        P([(-10, 0), (-5, -6.5), (9, -6.5), (9, 6.5), (-5, 6.5)]); Lb(-1.5, -3, 4.5, 3, 1.8); Lb(-1.5, 3, 4.5, -3, 1.8)
    elif name == 'SHIFT': P([(0, -8), (8, 0), (3.5, 0), (3.5, 7), (-3.5, 7), (-3.5, 0), (-8, 0)])
    elif name == 'BATTERY': F(-9, -5, 16, 10, 2, 2); B(7, -2, 2, 4, 1); B(-6, -2, 7, 4, 1)
    elif name == 'MIC':
        B(-3.5, -9, 7, 12, 3); A(0, -1.5, 6.3, 1.8, 95, 265); L(0, 5, 0, 8.5, 1.8)
    else:
        raise KeyError(name)

def badge(cv, x, y, size, col, name, k=1.0):
    """Кольоровий квадрат із білим значком (темним — на жовтому C_ACC)."""
    cv.box(x, y, size, size, 7 * size / 28, col)
    ic = C['ACCTXT'] if col == C['ACC'] else C['WHITE']
    icon(cv, name, x + size / 2, y + size / 2, ic, col, k)

def signal_bars(cv, x, bottom, level, on, off, k=1.0):
    for i in range(4):
        h = (3 + 2.3 * i) * k; cx = x + (1.2 + 4.2 * i) * k
        cv.line(cx, bottom - h + 1.2 * k, cx, bottom - 1.2 * k, 2.4 * k, on if i < level else off)
