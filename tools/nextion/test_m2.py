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
p.program = "baud=115200\r\ndim=100\r\nrecmod=0\r\nbkcmd=0\r\npage 0\r\n"
pg = p.page('ui'); pg.set('ui', sta=1, bco=0)
print(p.write(os.path.join(ROOT, 'build', 'nx', 'm2test'), r'C:\Tools\work\out'))
