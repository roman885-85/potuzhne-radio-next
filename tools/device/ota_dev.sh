#!/bin/bash
# ota_dev.sh — залити свіжу основну прошивку в радіо через оновлювач, без USB-завантажувача:
#   firmware/PotuzhneRadio-Nextion-update.bin → тимчасовий випуск GitHub «dev-screen» (dev-fw.bin)
#   → консоль радіо: upd fw <адреса>; upd go → оновлювач качає, пише app0 і повертає радіо.
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
BIN="$ROOT/firmware/PotuzhneRadio-Nextion-update.bin"
[ -f "$BIN" ] || { echo "немає $BIN — спершу firmware/rebuild.sh"; exit 1; }
cp "$BIN" "$ROOT/build/nextion-out/dev-fw.bin"
gh release view dev-screen >/dev/null 2>&1 || gh release create dev-screen --prerelease --latest=false \
   --title "Екран: робоча збірка (тимчасово)" --notes "Проміжні збірки для перевірки; не для встановлення." >/dev/null
gh release upload dev-screen "$ROOT/build/nextion-out/dev-fw.bin" --clobber >/dev/null
URL="https://github.com/roman885-85/potuzhne-radio-next/releases/download/dev-screen/dev-fw.bin"
"$ROOT/build/venv/bin/python" - "$URL" <<'PY'
import serial, sys, time, re
url = sys.argv[1]
s = serial.Serial('/dev/cu.usbserial-14610', 115200, timeout=0.2); s.dtr = False; s.rts = False
s.reset_input_buffer()
for c in ('upd fw ' + url, 'upd go'):
    s.write((c + '\n').encode()); s.flush(); time.sleep(0.6)
out = b''; t = time.time(); seen = False
while time.time() - t < 240:
    d = s.read(4096)
    if not d: continue
    out += d
    for l in d.decode('utf-8', 'replace').splitlines():
        if '##UPD#' in l or 'Ready!' in l or 'v0.9' in l or 'rst:' in l: print(time.strftime('%H:%M:%S'), l.strip(), flush=True)
    if b'Ready!' in out and seen: break
    if b'##UPD#' in out: seen = True
s.close()
PY
