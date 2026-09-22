import serial, sys, time
port, baud, secs = sys.argv[1], int(sys.argv[2]), float(sys.argv[3])
reset = len(sys.argv) > 4 and sys.argv[4] == 'reset'
s = serial.Serial()
s.port, s.baudrate, s.timeout = port, baud, 0.2
s.dtr = False; s.rts = False
s.open()
if reset:
    s.dtr = False; s.rts = True; time.sleep(0.2); s.rts = False  # EN low pulse, GPIO0 high
buf = b''
t = time.time()
while time.time() - t < secs:
    buf += s.read(4096)
s.close()
open(sys.argv[5] if len(sys.argv) > 5 else '/dev/null', 'ab').write(buf)
print(f'bytes={len(buf)}')
sys.stdout.write(buf.decode('utf-8', 'replace'))
