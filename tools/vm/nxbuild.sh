#!/bin/bash
# nxbuild.sh <тека в build/nx> — зібрати екран Nextion у ВМ і забрати .tft на Mac.
#
# Як це працює (з'ясовано дорогою ціною). Редактор, запущений із текою проєкту, робить усе сам:
# читає hmi.txt, створює сторінки, компілює .tft — і **закриває себе**. Раніше скрипт після
# завантаження картинок просив агента скомпілювати, а агента вже не було, і це виглядало як
# падіння редактора: чотири «зриви» поспіль за вечір, хоча .tft щоразу лежав готовий.
#
# Тому тут нічим не керуємо: запускаємо, чекаємо, доки у ВМ з'явиться .tft і його розмір
# перестане рости, і забираємо. Якщо за відведений час не з'явився — тоді це справді зрив.
set -e
N="$1"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; EX="$ROOT/tools/vm/exec.sh"
WIN='\\Mac\Home\Documents\radio_potughne_next'
[ -f "$ROOT/build/nx/$N/hmi.txt" ] || { echo "немає build/nx/$N/hmi.txt"; exit 1; }
TFT="C:\\Tools\\work\\out\\$N.tft"

printf '%s\n' \
  "Get-Process NeBuild, NeLaunch, 'Nextion Editor' -ErrorAction SilentlyContinue | Stop-Process -Force; Start-Sleep 2" \
  "Remove-Item '$TFT','C:\\Tools\\work\\out\\$N.HMI' -ErrorAction SilentlyContinue" \
  ". '$WIN\\tools\\vm\\ui.ps1'" \
  "\$arg = '$WIN\\build\\nx\\$N'.Replace(' ', '&;nspace&')" \
  "Start-Process \$Global:NeLaunch -ArgumentList \"\`\"\$arg\`\"\" -WorkingDirectory 'C:\\Tools\\NextionEditor'" \
  "'редактор запущено'" | "$EX" 2>&1 | tail -1

prev=0; same=0; SZ=0
for i in $(seq 1 ${NX_WAIT:-90}); do
  sleep 20
  SZ="$(printf '%s\n' "\$f = Get-Item '$TFT' -ErrorAction SilentlyContinue; if (\$f) { \$f.Length } else { 0 }" | "$EX" 2>/dev/null | tail -1 | tr -d ' \r')"
  case "$SZ" in ''|*[!0-9]*) SZ=0;; esac
  echo "$(date +%H:%M:%S) .tft: $SZ байт"
  if [ "$SZ" -gt 0 ] && [ "$SZ" = "$prev" ]; then
    same=$((same + 1))
    [ $same -ge 2 ] && break        # розмір не мінявся два заміри поспіль — файл дописано
  else
    same=0
  fi
  prev="$SZ"
done
[ "${SZ:-0}" -gt 0 ] || { echo "редактор не зібрав .tft"; exit 1; }

mkdir -p "$ROOT/build/nextion-out"
printf '%s\n' ". '$WIN\\tools\\vm\\ui.ps1'" \
  "Copy-Shared '$TFT' '$WIN\\build\\nextion-out\\$N.tft'" | "$EX" >/dev/null
ls -l "$ROOT/build/nextion-out/$N.tft" | awk '{print "tft:", $5, "байт"}'
