#!/bin/bash
# nxbuild.sh <тека в build/nx> [--shot "сторінка сторінка …"]
# Зібрати екран Nextion з теки (hmi.txt, star.txt, font/, img/ — робить tools/nextion/*.py) у ВМ:
#   редактор (з агентом NeBuild: кодування utf-8) виконує main.LoadFrom і компілює .tft →
#   build/nextion-out/<тека>.tft. З --shot — відкрити симулятор редактора (Debug), для кожної сторінки
#   виконати «page <ім'я>» і зняти екран: build/nx/shots/<тека>-<сторінка>.png (480×320, як на дисплеї).
set -e
N="$1"; shift || true
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; NB="$ROOT/tools/vm/nb.sh"; EX="$ROOT/tools/vm/exec.sh"
VM="${POTUZHNE_VM:-Мой Boot Camp}"
SHOT=""; [ "$1" = "--shot" ] && SHOT="${2:-}"
WIN='\\Mac\Home\Documents\radio_potughne_next'
[ -f "$ROOT/build/nx/$N/hmi.txt" ] || { echo "немає build/nx/$N/hmi.txt"; exit 1; }

# 1. автозбірка: редактор з аргументом-текою сам виконує main.LoadFrom, зберігає HMI, компілює .tft
#    і закривається (tools/vm/autobuild.ps1). Агент NeBuild тим часом ставить кодування нових
#    проєктів utf-8 (інакше редактор бере koi8-r з кирилічної Windows).
if [ "${NX_NOBUILD:-0}" = "1" ]; then OUT="TFT: вже зібрано"; else
OUT="$(printf '%s\n' "\$Folder = '$WIN\\build\\nx\\$N'" "\$Name = '$N'" "\$Timeout = ${NX_TIMEOUT:-1500}" ". '$WIN\\tools\\vm\\autobuild.ps1'" | "$EX" 2>&1 | tail -1)"
fi
echo "$OUT" | grep -q "^TFT: " || { echo "збірка: $OUT"; exit 1; }
if [ "${NX_NOBUILD:-0}" != "1" ]; then
mkdir -p "$ROOT/build/nextion-out"
printf '%s\n' ". '$WIN\\tools\\vm\\ui.ps1'" \
  "Copy-Shared 'C:\\Tools\\work\\out\\$N.tft' '$WIN\\build\\nextion-out\\$N.tft'" | "$EX" >/dev/null
ls -l "$ROOT/build/nextion-out/$N.tft" | awk '{print "tft:", $5, "байт"}'
fi

# 2. знімки з симулятора
if [ -n "$SHOT" ]; then
  mkdir -p "$ROOT/build/nx/shots"
  printf '%s\n' ". '$WIN\\tools\\vm\\ui.ps1'" "Start-NE; Dismiss-NeMessages | Out-Null" \
    "Open-NEProject 'C:\\Tools\\work\\out\\$N.HMI' | Out-Null; Start-Sleep 5; Dismiss-NeMessages | Out-Null" | "$EX" >/dev/null
  NB_WAIT=60 "$NB" "pget app.appdata.encodeid" | grep -q "Byte 24" || echo "УВАГА: кодування проєкту не utf-8"
  NB_WAIT=30 "$NB" "clickitem Debug" >/dev/null; sleep 5
  for pg in $SHOT; do
    if [ "$pg" != "-" ]; then NB_WAIT=30 "$NB" "sim page $pg" >/dev/null; sleep 1.5; fi
    # значення для сторінки: build/nx/<сторінка>.cmds — команди, які на радіо шле прошивка
    if [ -f "$ROOT/tools/nextion/${pg}sim.txt" ]; then
      NB_WAIT=90 "$NB" "simfile \\\\Mac\\Home\\Documents\\radio_potughne_next\\tools\\nextion\\${pg}sim.txt" >/dev/null; sleep 2
    fi
    RECT="$(NB_WAIT=30 "$NB" "ctlrect 480 320" | grep TFTRUN | head -1)"
    X=$(echo "$RECT" | awk '{print $4}'); Y=$(echo "$RECT" | awk '{print $5}')
    prlctl capture "$VM" --file "$ROOT/build/nx/shots/_vm.png" >/dev/null
    "$ROOT/build/venv/bin/python" - "$ROOT/build/nx/shots/_vm.png" "$X" "$Y" "$ROOT/build/nx/shots/$N-$pg.png" <<'PY'
import sys
from PIL import Image
im = Image.open(sys.argv[1]); x, y = int(sys.argv[2]), int(sys.argv[3])
k = round(im.width / 1787) if im.width > 2000 else 1      # екран ВМ 200 %: логічні координати ×2
c = im.crop((x * k, y * k, x * k + 480 * k, y * k + 320 * k)).resize((480, 320), Image.NEAREST)
c.save(sys.argv[4]); print('знімок', sys.argv[4])
PY
  done
fi
