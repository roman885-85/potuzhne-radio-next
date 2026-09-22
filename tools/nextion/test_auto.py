"""Пробна автозбірка: одна сторінка, фон-картинка, текст українською, подія."""
import os, sys
from PIL import Image, ImageDraw
sys.path.insert(0, os.path.dirname(__file__))
from hmi import Project, rgb565
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
out = os.path.join(ROOT, 'build', 'nx', 'test')
os.makedirs(out, exist_ok=True)
bg = Image.new('RGB', (480, 320), (16, 20, 28))
d = ImageDraw.Draw(bg)
d.rounded_rectangle((16, 60, 464, 180), radius=18, fill=(28, 33, 43))
bg.save(os.path.join(out, 'bg_test.png'))
p = Project('autotest')
f0 = p.font(os.path.join(ROOT, 'nextion', 'fonts-zi', 'roboto24.zi'))
i0 = p.image(os.path.join(out, 'bg_test.png'))
p.program = "baud=115200\r\ndim=100\r\nrecmod=0\r\nbkcmd=0\r\npage 0\r\n"
pg = p.page('main')
pg.set('main', sta=2, pic=i0)
pg.add('text', 'title', x=40, y=90, w=400, h=40, font=f0, pco=rgb565('#e8c84a'), sta=0, picc=i0, txt_maxl=60,
       txt='ПОТУЖНЕ РАДІО — ґєії Ґ€Ї', xcen=0)
pg.event('title', 'up', 'prints "^tap=title$",0')
print(p.write(os.path.join(ROOT, 'build', 'nx', 'auto-test'), r'C:\Tools\work\out'))
