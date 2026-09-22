import os
import serial
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import port, time, sys
S = '/Users/admin/Documents/radio_potughne_next/build/device-work'
s = serial.Serial(); s.port, s.baudrate, s.timeout = port.find(), 115200, 0.3
s.dtr = False; s.rts = False; s.open()
log = open(S + '/serial_capture.log', 'ab')
buf = b''; t = time.time(); boots = 0
while time.time() - t < 1800:
    d = s.read(4096)
    if not d: continue
    log.write(d); log.flush(); buf += d
    if b'rst:' in d or b'ets ' in d:
        boots += 1; print(f'[{time.strftime("%H:%M:%S")}] boot #{boots} seen', flush=True)
    if b'waiting for download' in buf or b'DOWNLOAD_BOOT' in buf:
        time.sleep(0.5); log.write(s.read(4096)); s.close(); log.close()
        print('DOWNLOAD MODE', flush=True); sys.exit(0)
    buf = buf[-300:]
s.close(); print('TIMEOUT'); sys.exit(1)
