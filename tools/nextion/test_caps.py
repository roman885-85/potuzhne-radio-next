"""Пробна збірка: чи приймає редактор картинки вищі за екран, і як xstr малює текст на різному фоні."""
import json, os, sys
from PIL import Image, ImageDraw
sys.path.insert(0, os.path.dirname(__file__))
from hmi import Project, rgb565
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
FD = os.path.join(ROOT, 'build', 'nx', 'fonts')
meta = json.load(open(os.path.join(FD, 'fonts.json')))
work = os.path.join(ROOT, 'build', 'nx', 'caps-src'); os.makedirs(work, exist_ok=True)
# 0: фон — градієнт зверху, 1: висока картинка 480×1200 зі смугами й номерами
bg = Image.new('RGB', (480, 320), (8, 12, 16)); d = ImageDraw.Draw(bg)
for y in range(120): d.line((0, y, 479, y), fill=(int(16 - 8 * y / 120), int(24 - 12 * y / 120), int(32 - 16 * y / 120)))
d.rounded_rectangle((20, 180, 460, 300), radius=16, fill=(24, 32, 41))
bg.save(os.path.join(work, 'bg.png'))
tall = Image.new('RGB', (480, 1200), (8, 12, 16)); d = ImageDraw.Draw(tall)
for i in range(20):
    d.rounded_rectangle((15, 10 + i * 60, 465, 60 + i * 60), radius=12, fill=[(24, 32, 41), (230, 210, 90)][i % 2])
    d.text((30, 25 + i * 60), 'row %d' % i, fill=(240, 240, 240))
tall.save(os.path.join(work, 'tall.png'))
p = Project('caps')
for role, m in sorted(meta.items(), key=lambda kv: kv[1]['id']): p.font(os.path.join(FD, m['file']))
i_bg = p.image(os.path.join(work, 'bg.png')); i_tall = p.image(os.path.join(work, 'tall.png'))
p.program = "baud=115200\r\ndim=100\r\nrecmod=0\r\nbkcmd=0\r\npage 0\r\n"
pg = p.page('ui'); pg.set('ui', sta=2, pic=i_bg)
pg.add('timer', 'tm0', tim=50, en=0)
pg.event('tm0', 'timer', 'prints "M",1\r\nprints tch0,2\r\nprints tch1,2')
print(p.write(os.path.join(ROOT, 'build', 'nx', 'caps'), r'C:\Tools\work\out'))
