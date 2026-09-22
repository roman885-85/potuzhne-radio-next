"""Програвач команд Nextion на Mac: що покаже екран, якщо виконати ці команди.

Бере ті самі картинки й шрифти, що вшиваються в .tft (build/nx/assets, build/nx/fonts), і малює
fill / xpic / xstr / cls у картинку 480×320. Так перевіряється малювання меню без віртуальної
машини, а в парі з «nx shot» прошивки — ще й знімок того, що зараз на екрані радіо.

  nxrender.py <команди.txt> <знімок.png> [--from <знімок.png>]
"""
import os, re, sys
from PIL import Image
sys.path.insert(0, os.path.dirname(__file__))
import zifont, gfx

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
ASSETS = os.path.join(ROOT, 'build', 'nx', 'assets')
FONTS = os.path.join(ROOT, 'build', 'nx', 'fonts')


class Screen:
    def __init__(self, assets=ASSETS, fonts=FONTS, base=None):
        self.im = base.copy().convert('RGB') if base else Image.new('RGB', (480, 320), (0, 0, 0))
        self.px = self.im.load()
        self.pics = [Image.open(os.path.join(assets, f)).convert('RGB') for f in sorted(os.listdir(assets)) if f.endswith('.png')]
        self.fonts = []
        for f in sorted(os.listdir(fonts)):
            if not f.endswith('.zi'): continue
            h, name, gl, ms = zifont.read(os.path.join(fonts, f))
            self.fonts.append((h['h'], {g['code']: g for g in gl}))
        self.errors = []

    def fill(self, x, y, w, h, c):
        rgb = gfx.from565(c)
        self.im.paste(rgb, (x, y, x + w, y + h))

    def xpic(self, x, y, w, h, sx, sy, pic):
        if pic >= len(self.pics): self.errors.append('немає картинки %d' % pic); return
        self.im.paste(self.pics[pic].crop((sx, sy, sx + w, sy + h)), (x, y))

    def xstr(self, x, y, w, h, font, pco, bco, xcen, ycen, sta, text):
        if sta == 0: self.xpic(x, y, w, h, x, y, bco)
        elif sta == 1: self.fill(x, y, w, h, bco)
        elif sta == 2: self.xpic(x, y, w, h, 0, 0, bco)
        if font >= len(self.fonts): self.errors.append('немає шрифту %d' % font); return
        fh, gl = self.fonts[font]
        tw = sum(gl[ord(ch)]['w'] for ch in text if ord(ch) in gl)
        pen = x + (0 if xcen == 0 else (w - tw) // 2 if xcen == 1 else w - tw)
        top = y + (0 if ycen == 0 else (h - fh) // 2 if ycen == 1 else h - fh)
        fg = gfx.from565(pco)
        for ch in text:
            g = gl.get(ord(ch))
            if not g: self.errors.append('немає літери %r у шрифті %d' % (ch, font)); continue
            lv = zifont.decode_glyph(g['data'], g['bw'], fh)
            gx0 = pen - g['a']
            for yy in range(fh):
                Y = top + yy
                if Y < y or Y >= y + h or Y < 0 or Y >= 320: continue
                row = lv[yy * g['bw']:(yy + 1) * g['bw']]
                for xx, v in enumerate(row):
                    if not v: continue
                    X = gx0 + xx
                    if X < x or X >= x + w or X < 0 or X >= 480: continue
                    a = v / 7; o = self.px[X, Y]
                    self.px[X, Y] = tuple(int(o[i] * (1 - a) + fg[i] * a + 0.5) for i in range(3))
            pen += g['w']

    def run(self, line):
        line = line.strip()
        if not line: return
        m = re.match(r'(\w+)[ =]?(.*)$', line)
        cmd, rest = m.group(1), m.group(2)
        if cmd == 'xstr':
            q = rest.index('"')
            nums = [int(v) for v in rest[:q].rstrip(',').split(',')]
            text = rest[q + 1:rest.rindex('"')].replace('\\"', '"').replace('\\\\', '\\')
            self.xstr(*nums, text)
        elif cmd == 'fill': self.fill(*[int(v) for v in rest.split(',')])
        elif cmd == 'xpic': self.xpic(*[int(v) for v in rest.split(',')])
        elif cmd == 'cls': self.fill(0, 0, 480, 320, int(rest))
        # page, dim, bkcmd, vis … на картинку не впливають

    def play(self, lines):
        for l in lines: self.run(l)
        return self.im


if __name__ == '__main__':
    a = sys.argv[1:]
    base = None
    if '--from' in a:
        i = a.index('--from'); base = Image.open(a[i + 1]); del a[i:i + 2]
    sc = Screen(base=base)
    sc.play(open(a[0], encoding='utf-8'))
    sc.im.save(a[1])
    for e in sorted(set(sc.errors)): print('УВАГА:', e)
    print('знімок', a[1])
