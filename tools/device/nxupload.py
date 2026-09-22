#!/usr/bin/env python3
"""nxupload.py <url> [бод] — сказати радіо (консоль UART) залити .tft в екран і дочекатися кінця."""
import sys, time, serial
url = sys.argv[1]; baud = sys.argv[2] if len(sys.argv) > 2 else '921600'
s = serial.Serial(); s.port, s.baudrate, s.timeout = '/dev/cu.usbserial-14610', 115200, 0.3
s.dtr = False; s.rts = False; s.open(); s.reset_input_buffer()
s.write(('nx upload %s %s\n' % (url, baud)).encode()); s.flush()
buf = ''; t = time.time(); ok = False
while time.time() - t < 400:
    d = s.read(4096).decode('utf-8', 'replace')
    if not d: continue
    buf += d
    for line in d.splitlines():
        if '##NX#' in line: print(time.strftime('%H:%M:%S'), line.split('##NX#', 1)[1].strip(), flush=True)
    if 'готово:' in buf: ok = True; time.sleep(3); break
    if any(k in buf for k in ('HTTP -', 'не відповідає', 'не прийняв', 'обрив', 'не підтвердив', "немає пам'яті", 'не вдалося')): break
s.close(); sys.exit(0 if ok else 1)
