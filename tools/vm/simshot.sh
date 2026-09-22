#!/bin/bash
# simshot.sh <проєкт у C:\Tools\work\out без .HMI> <знімок.png> [файл команд]
# Відкрити проєкт у редакторі (якщо ще не відкритий), запустити симулятор, надіслати команди
# (по одній на рядок, «#» — коментар, «wait N» — пауза в мс, «touch x y [мс]» — дотик) і зняти екран 480×320.
set -e
N="$1"; OUTP="$2"; CMDS="${3:-}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; NB="$ROOT/tools/vm/nb.sh"; EX="$ROOT/tools/vm/exec.sh"
VM="${POTUZHNE_VM:-Мой Boot Camp}"; WIN='\\Mac\Home\Documents\radio_potughne_next'
CUR="$(NB_WAIT=16 "$NB" 'pget app.appdata.AppFilePath' 2>/dev/null | tail -1 || true)"
if ! echo "$CUR" | grep -qi "\\\\$N.HMI"; then
  printf '%s\n' ". '$WIN\\tools\\vm\\ui.ps1'" "Start-NE; Dismiss-NeMessages | Out-Null" \
    "Open-NEProject 'C:\\Tools\\work\\out\\$N.HMI' | Out-Null; Start-Sleep 4; Dismiss-NeMessages | Out-Null" | "$EX" >/dev/null
fi
if ! NB_WAIT=10 "$NB" "ctlrect 480 320" | grep -q TFTRUN; then
  NB_WAIT=30 "$NB" "clickitem Debug" >/dev/null; sleep 5
fi
if [ -n "$CMDS" ]; then
  while IFS= read -r line || [ -n "$line" ]; do
    case "$line" in
      ''|'#'*) ;;
      wait\ *) sleep "$(echo "${line#wait }" | awk '{print $1/1000}')" ;;
      touch\ *) NB_WAIT=30 "$NB" "simtouch ${line#touch }" >/dev/null ;;
      file\ *) NB_WAIT=120 "$NB" "simfile ${line#file }" >/dev/null ;;
      *) NB_WAIT=30 "$NB" "sim $line" >/dev/null ;;
    esac
  done < "$CMDS"
  sleep 1
fi
RECT="$(NB_WAIT=30 "$NB" "ctlrect 480 320" | grep TFTRUN | head -1)"
X=$(echo "$RECT" | awk '{print $4}'); Y=$(echo "$RECT" | awk '{print $5}')
prlctl capture "$VM" --file "$ROOT/build/nx/_vm.png" >/dev/null
"$ROOT/build/venv/bin/python" - "$ROOT/build/nx/_vm.png" "$X" "$Y" "$OUTP" <<'PY'
import sys
from PIL import Image
im = Image.open(sys.argv[1]); x, y = int(sys.argv[2]), int(sys.argv[3])
k = round(im.width / 1787) if im.width > 2000 else 1
im.crop((x * k, y * k, x * k + 480 * k, y * k + 320 * k)).resize((480, 320), Image.NEAREST).save(sys.argv[4])
print('знімок', sys.argv[4])
PY
