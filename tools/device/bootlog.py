import os
import serial
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import port, time, sys
S='/Users/admin/Documents/radio_potughne_next/build/device-work'
s=serial.Serial(); s.port,s.baudrate,s.timeout=port.find(),115200,0.3
s.dtr=False; s.rts=False; s.open()
buf=b''; t=time.time(); seen=None
while time.time()-t<900:
    buf+=s.read(4096)
    if seen is None and b'##[BOOT]#\tSD:' in buf or (seen is None and buf.count(b'------------------------------------------------')>=2):
        seen=time.time()
    if seen and time.time()-seen>8: break
s.close(); open(S+'/boot_normal.log','wb').write(buf)
print(buf.decode('utf-8','replace') if buf else 'NO DATA')
