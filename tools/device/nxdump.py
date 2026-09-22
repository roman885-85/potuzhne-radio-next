#!/usr/bin/env python3
"""nxdump.py — що зараз на рідній сторінці плеєра: питаємо значення в самого екрана.

Знімок команд («nx shot», nxshot.py) годиться лише для сторінок, які малює прошивка.
Головний екран тепер малює сам Nextion, тому читаємо з нього значення компонентів.
"""
import sys, time, serial
import os, sys as _s; _s.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import port

s = serial.Serial(); s.port, s.baudrate, s.timeout = port.find(), 115200, 0.3
s.dtr = False; s.rts = False; s.open(); s.reset_input_buffer()
s.write(b'nx dump\n'); s.flush()

buf, name = '', None
t = time.time()
while time.time() - t < 25:
    d = s.read(4096).decode('utf-8', 'replace')
    if not d: continue
    buf += d
    while '\n' in buf:
        line, buf = buf.split('\n', 1)
        if '##NXD#' not in line: continue
        v = line.split('##NXD#', 1)[1].strip()
        if v in ('BEGIN', 'END'):
            if v == 'END': s.close(); sys.exit(0)
            continue
        if v.endswith('='):
            name = v[:-1].strip()
        else:
            print('%-10s %s' % (name or '?', v))
            name = None
s.close()
print('екран не відповів', file=sys.stderr); sys.exit(1)
