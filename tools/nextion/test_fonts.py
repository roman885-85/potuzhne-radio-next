"""Пробна збірка: усі шрифти набору (fonts.py) на одній сторінці — перевірити, що редактор їх приймає."""
import json, os, sys
from PIL import Image
sys.path.insert(0, os.path.dirname(__file__))
from hmi import Project, rgb565
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
FD = os.path.join(ROOT, 'build', 'nx', 'fonts')
meta = json.load(open(os.path.join(FD, 'fonts.json')))
work = os.path.join(ROOT, 'build', 'nx', 'fonttest-src'); os.makedirs(work, exist_ok=True)
Image.new('RGB', (480, 320), (8, 12, 16)).save(os.path.join(work, 'bg.png'))
p = Project('fonttest')
ids = {}
for role, m in sorted(meta.items(), key=lambda kv: kv[1]['id']):
    ids[role] = p.font(os.path.join(FD, m['file']))
bg = p.image(os.path.join(work, 'bg.png'))
p.program = "baud=115200\r\ndim=100\r\nrecmod=0\r\nbkcmd=0\r\npage 0\r\n"
pg = p.page('main'); pg.set('main', sta=2, pic=bg)
y = 2
for role in ['title', 'row', 'rowb', 'sm', 'smb', 'mid', 'key', 'tiny', 'clock']:
    m = meta[role]
    txt = '12:45' if role == 'clock' else '%s Ґґ Єє Іі Її «№» — ПОТУЖНЕ' % role
    pg.add('text', 't_' + role, x=6, y=y, w=468, h=m['h'], font=ids[role], pco=rgb565('#f0f0f0'),
           sta=0, picc=bg, txt_maxl=60, txt=txt, xcen=0, ycen=0)
    y += m['h'] + 2
print(p.write(os.path.join(ROOT, 'build', 'nx', 'fonttest'), r'C:\Tools\work\out'))
