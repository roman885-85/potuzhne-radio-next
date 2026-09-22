#!/usr/bin/env python3
"""nxshot.py [знімок.png] — що зараз на екрані радіо.

Радіо (консоль «nx shot») перемальовує весь екран і дублює команди Nextion у консоль між
«##NXC# BEGIN» і «##NXC# END»; тут вони програються тими самими картинками й шрифтами, що
вшиті в екран (tools/nextion/nxrender.py). Виходить точна копія намальованого — без фото.
"""
import os, sys, time, serial
import os, sys as _s; _s.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import port
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'nextion'))
import nxrender
out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'build', 'nx', 'shots', 'radio.png')
s = serial.Serial(port.find(), 115200, timeout=0.2); s.dtr = False; s.rts = False
s.reset_input_buffer(); s.write(b'nx shot\n'); s.flush()
buf = b''; t0 = time.time(); cmds = []; began = None; ended = None
while time.time() - t0 < 20:
    buf += s.read(8192)
    while b'\n' in buf:
        line, buf = buf.split(b'\n', 1)
        l = line.decode('utf-8', 'replace').rstrip('\r')
        if '##NXC#\t' not in l: continue
        body = l.split('##NXC#\t', 1)[1]
        if body == 'BEGIN': began = time.time(); cmds = []
        elif body == 'END': ended = time.time(); break
        elif began: cmds.append(body)
    if ended: break
s.close()
if not ended or not began: print('знімка не дочекався (команд %d)' % len(cmds)); sys.exit(1)
open(out.replace('.png', '.txt'), 'w', encoding='utf-8').write('\n'.join(cmds) + '\n')
sc = nxrender.Screen(); sc.play(cmds); sc.im.save(out)
print('команд %d, %.2f с; знімок %s' % (len(cmds), ended - began, out))
for e in sorted(set(sc.errors)): print('УВАГА:', e)
