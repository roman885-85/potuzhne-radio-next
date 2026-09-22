#!/bin/bash
# check.sh — повний прогін системи одним рухом: самоперевірка радіо, стан екрана,
# чи йдуть секунди, чи ворушиться спектр. Друкує підсумок і повертає 1, якщо щось зламано.
#
# Сенс: не «подивись на екран», а перевірка числами, яку можна повторити будь-коли.
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; PY="$ROOT/build/venv/bin/python"
cd "$ROOT"
bad=0

echo "=== 1. самоперевірка радіо ==="
OUT="$($PY - <<'PY'
import serial, time, glob, sys
try: p = sorted(glob.glob('/dev/cu.usbserial-*'))[0]
except IndexError: print('ПОРТУ НЕМАЄ'); sys.exit(2)
s = serial.Serial(); s.port, s.baudrate, s.timeout = p, 115200, 0.4
s.dtr=False; s.rts=False; s.open(); s.reset_input_buffer()
s.write(b'nx test\n'); s.flush(); time.sleep(16)
for l in s.read(60000).decode('utf-8','replace').splitlines():
    if '##NXT#' in l: print(l.split('##NXT#',1)[1].strip())
s.close()
PY
)"
echo "$OUT"
echo "$OUT" | grep -q "ЗЛАМАНО" && bad=1

echo
echo "=== 2. секунди йдуть? ==="
A="$($PY tools/device/nxdump.py 2>/dev/null | awk '/^sc/{print $2}')"
sleep 4
B="$($PY tools/device/nxdump.py 2>/dev/null | awk '/^sc/{print $2}')"
if [ -n "$A" ] && [ "$A" != "$B" ]; then echo "ГАРАЗД: $A → $B"; else echo "ЗЛАМАНО: $A → $B (стоять)"; bad=1; fi

echo
echo "=== 3. спектр ворушиться? ==="
$PY - <<'PY'
import serial, time, glob
s = serial.Serial(); s.port, s.baudrate, s.timeout = sorted(glob.glob('/dev/cu.usbserial-*'))[0], 115200, 0.4
s.dtr=False; s.rts=False; s.open(); s.reset_input_buffer()
vals=[]
for i in range(3):
    s.reset_input_buffer(); s.write(b'nx spec\n'); s.flush(); time.sleep(1.4)
    for l in s.read(9000).decode('utf-8','replace').splitlines():
        if 'смуг' in l: vals.append(l.split(':',1)[1].strip())
s.close()
for v in vals: print(' ', v)
print('ГАРАЗД: значення різні' if len(set(vals)) > 1 else 'УВАГА: значення однакові (тиша чи не читається)')
PY

echo
[ $bad -eq 0 ] && echo "ПІДСУМОК: усе гаразд" || echo "ПІДСУМОК: є зламане"
exit $bad
