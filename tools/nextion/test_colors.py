"""Таблиця для перевірки кольорів панелі NX4832F035 (TN): шкала сірого й зразки палітри ПОТУЖНОГО РАДІО."""
import os, sys
from PIL import Image, ImageDraw, ImageFont
sys.path.insert(0, os.path.dirname(__file__))
from hmi import Project
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
out = os.path.join(ROOT, 'build', 'nx', 'colors'); os.makedirs(out, exist_ok=True)
F = ImageFont.truetype(os.path.expanduser('~/Library/Fonts/Roboto-Bold.ttf'), 13)
im = Image.new('RGB', (480, 320), (0, 0, 0)); d = ImageDraw.Draw(im)
# 1) шкала сірого: 16 кроків
for i in range(16):
    v = i * 17
    d.rectangle((i * 30, 0, i * 30 + 29, 60), fill=(v, v, v))
    d.text((i * 30 + 3, 44), str(v), font=F, fill=(255, 0, 0) if v > 128 else (255, 255, 0))
# 2) темні відтінки ПОТУЖНОГО РАДІО (фон, картки) та варіанти
dark = [('фон', (16, 20, 28)), ('картка', (28, 33, 43)), ('рядок', (38, 44, 56)), ('0x20', (32, 32, 32)),
        ('0x40', (64, 64, 64)), ('синій т.', (20, 30, 60)), ('сірий т.', (40, 40, 48)), ('чорний', (0, 0, 0))]
for i, (n, c) in enumerate(dark):
    x = i * 60; d.rectangle((x, 64, x + 59, 150), fill=c); d.text((x + 4, 70), n, font=F, fill=(255, 255, 255))
# 3) акценти
acc = [('жовтий', (232, 200, 74)), ('білий', (255, 255, 255)), ('сірий', (140, 146, 158)), ('зелений', (60, 200, 90)),
       ('червоний', (230, 60, 60)), ('помар.', (240, 140, 40)), ('фіолет', (160, 110, 230)), ('блак.', (70, 160, 240))]
for i, (n, c) in enumerate(acc):
    x = i * 60; d.rectangle((x, 154, x + 59, 240), fill=c); d.text((x + 4, 160), n, font=F, fill=(0, 0, 0))
# 4) текст на темному фоні
d.rectangle((0, 244, 479, 319), fill=(16, 20, 28))
for i, (n, c) in enumerate(acc):
    d.text((8 + (i % 4) * 118, 252 + (i // 4) * 32), 'Текст ' + n, font=ImageFont.truetype(os.path.expanduser('~/Library/Fonts/Roboto-Bold.ttf'), 18), fill=c)
im.save(os.path.join(out, 'colors.png'))
p = Project('colors'); i0 = p.image(os.path.join(out, 'colors.png'))
p.program = "baud=115200\r\ndim=100\r\nrecmod=0\r\nbkcmd=0\r\npage 0\r\n"
p.page('colors').set('colors', sta=2, pic=i0)
print(p.write(os.path.join(ROOT, 'build', 'nx', 'auto-colors'), r'C:\Tools\work\out'))
