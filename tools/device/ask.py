import serial, sys, time
port = sys.argv[1]
for baud in [int(b) for b in sys.argv[2].split(',')]:
    s = serial.Serial(); s.port, s.baudrate, s.timeout = port, baud, 0.2
    s.dtr = False; s.rts = False; s.open()
    s.reset_input_buffer()
    out = b''
    for cmd in sys.argv[3:]:
        s.write(cmd.encode() + b'\n'); s.flush()
        t = time.time()
        while time.time() - t < 1.5: out += s.read(4096)
    s.close()
    print(f'== baud {baud}: {len(out)} bytes')
    if out: print(out[:3000].decode('utf-8', 'replace'))
