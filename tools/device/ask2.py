import serial, sys, time, re
s = serial.Serial(); s.port, s.baudrate, s.timeout = sys.argv[1], 115200, 0.2
s.dtr = False; s.rts = False; s.open(); s.reset_input_buffer()
out = b''
for cmd in sys.argv[3:]:
    s.write(cmd.encode() + b'\n'); s.flush()
    t = time.time()
    while time.time() - t < 2.0: out += s.read(8192)
s.close()
open(sys.argv[2], 'wb').write(out)
txt = out.decode('utf-8', 'replace')
# mask wifi passwords in "N: ssid, pass" lines inside WIFI.CON / WIFI.STATION blocks
def mask(block):
    return re.sub(r'^(\d+: [^,\n]+), (.+)$', lambda m: f'{m.group(1)}, <пароль скрыт, {len(m.group(2))} симв.>', block, flags=re.M)
txt = re.sub(r'#WIFI\.(CON|STATION)#\n.*?##WIFI\.\1#', lambda m: mask(m.group(0)), txt, flags=re.S)
print(txt)
