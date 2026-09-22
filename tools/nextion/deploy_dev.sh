#!/bin/bash
# deploy_dev.sh <ім'я .tft у C:\Tools\work\out без розширення>
# Забрати зібраний .tft із ВМ, покласти в тимчасовий випуск GitHub «dev-screen» і залити в екран
# з консолі радіо (nx upload). З Mac по http радіо файл не бере (HTTP -1), тож — через GitHub.
set -e
N="$1"; ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; OUT="$ROOT/build/nextion-out"; mkdir -p "$OUT"
printf '%s\n' ". '\\\\Mac\\Home\\Documents\\radio_potughne_next\\tools\\vm\\ui.ps1'" \
  "Copy-Shared 'C:\\Tools\\work\\out\\$N.tft' '\\\\Mac\\Home\\Documents\\radio_potughne_next\\build\\nextion-out\\dev-screen.tft'" \
  | "$ROOT/tools/vm/exec.sh" | tail -1
cd "$ROOT"
gh release view dev-screen >/dev/null 2>&1 || gh release create dev-screen --prerelease --latest=false \
   --title "Екран: робоча збірка (тимчасово)" --notes "Проміжна збірка екрана для перевірки; не для встановлення." >/dev/null
gh release upload dev-screen "$OUT/dev-screen.tft" --clobber >/dev/null
URL="https://github.com/roman885-85/potuzhne-radio-next/releases/download/dev-screen/dev-screen.tft"
"$ROOT/build/venv/bin/python" "$ROOT/tools/device/nxupload.py" "$URL"
