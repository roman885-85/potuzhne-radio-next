"""Пробний проєкт: ресурси меню (nxassets.py) + порожня сторінка ui, на яку команди малює симулятор."""
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
from hmi import Project
import nxassets
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
a = nxassets.build()
p = Project('m2test')
for f in a['fonts']: p.font(f)
for n, f in a['pics']: p.image(f)
p.program = "baud=921600\r\ndim=100\r\nrecmod=0\r\nbkcmd=0\r\npage 0\r\n"
pg = p.page('ui'); pg.set('ui', sta=1, bco=0)
print(p.write(os.path.join(ROOT, 'build', 'nx', 'm2test'), r'C:\Tools\work\out'))
if os.environ.get('NX_EV'):
    pg.add('timer', 'tm0', tim=50, en=0)
    pg.event('tm0', 'timer', 'printh 7E 4D\r\nprints tch0,2\r\nprints tch1,2')
    if os.environ.get('NX_EV') == '2':
        pg.event('ui', 'down', 'printh 7E 50\r\nprints tch0,2\r\nprints tch1,2\r\ntm0.en=1')
        pg.event('ui', 'up', 'tm0.en=0\r\nprinth 7E 52\r\nprints tch2,2\r\nprints tch3,2')
    print(p.write(os.path.join(ROOT, 'build', 'nx', 'm2test'), r'C:\Tools\work\out'))
