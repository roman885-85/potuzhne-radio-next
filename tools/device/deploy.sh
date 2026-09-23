#!/bin/bash
# deploy.sh — одна команда на весь цикл: прошивка → екран → перевірка.
#   deploy.sh          — залити прошивку й екран, потім перевірити
#   deploy.sh fw       — лише прошивку
#   deploy.sh tft      — лише екран
# Екран радіо качає сам із GitHub (LAN до Mac буває недоступний), тому файл спершу туди.
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
PY="$ROOT/build/venv/bin/python"
what="${1:-all}"
ls /dev/cu.usbserial-* >/dev/null 2>&1 || { echo "адаптера радіо немає — під'єднайте кабель"; exit 1; }

if [ "$what" = "all" ] || [ "$what" = "fw" ]; then
  echo "=== прошивка ==="
  tools/device/ota_dev.sh 2>&1 | grep -E "прошивка:|Ready!|помилка" || true
fi

if [ "$what" = "all" ] || [ "$what" = "tft" ]; then
  echo "=== екран ==="
  gh release upload dev-screen build/nextion-out/potuzhne.tft --clobber >/dev/null 2>&1 || true
  URL=https://github.com/roman885-85/potuzhne-radio-next/releases/download/dev-screen/potuzhne.tft
  $PY - "$URL" <<'PY'
import serial, time, glob, sys
s=serial.Serial(); s.port,s.baudrate,s.timeout=sorted(glob.glob('/dev/cu.usbserial-*'))[0],115200,0.4
s.dtr=False;s.rts=False;s.open();s.reset_input_buffer()
#  старий протокол (v1): новий зривався на 8 КБ після пошкодження пам'яті екрана
s.write(('nx upload %s 115200 v1\n' % sys.argv[1]).encode()); s.flush(); time.sleep(8)
for l in s.read(15000).decode('utf-8','replace').splitlines():
    if '##NX#' in l: print(' ', l.split('##NX#',1)[1].strip()[:110])
s.close()
PY
  echo "  (заливка йде в радіо; чекаю…)"
  for i in $(seq 1 90); do
    sleep 20
    st=$($PY - <<'PY' 2>/dev/null
import serial, time, glob
m=glob.glob('/dev/cu.usbserial-*')
if not m: raise SystemExit
s=serial.Serial(); s.port,s.baudrate,s.timeout=sorted(m)[0],115200,0.4
s.dtr=False;s.rts=False;s.open();s.reset_input_buffer()
s.write(b'nx status\n'); s.flush(); time.sleep(2.5)
o=s.read(12000).decode('utf-8','replace'); s.close()
for l in o.splitlines():
    if '##NX#' in l: print(l.split('##NX#',1)[1].strip()[:60])
PY
)
    echo "  $st"
    echo "$st" | grep -q "зайнято" || break
  done
fi

echo "=== перевірка ==="
sleep 20
tools/device/check.sh
