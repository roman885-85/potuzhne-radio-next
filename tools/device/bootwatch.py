"""Чекати перезавантаження радіо й записати журнал запуску (до 15 хв), потім спитати модель екрана."""
import serial, time, sys
port, out = sys.argv[1], sys.argv[2]
s = serial.Serial(); s.port, s.baudrate, s.timeout = port, 115200, 0.3
s.dtr = False; s.rts = False; s.open()
buf = b''; t = time.time(); started = None
while time.time() - t < 3600:
    d = s.read(4096)
    if d:
        buf += d
        if started is None and (b'rst:' in buf or b'ets ' in buf): started = time.time()
    if started and time.time() - started > 25: break
if started:
    s.write(b'nx info\n'); s.flush(); t2 = time.time()
    while time.time() - t2 < 12: buf += s.read(4096)
s.close(); open(out, 'wb').write(buf)
print(buf.decode('utf-8', 'replace') if buf else 'NO DATA')
