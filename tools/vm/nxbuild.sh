#!/bin/bash
# nxbuild.sh <тека в build/nx> — зібрати екран Nextion у ВМ і забрати .tft на Mac.
#
# Як це працює і чому саме так (перевірено дорогою ціною):
#   редактор запускається з текою проєкту й сам виконує main.LoadFrom — але компіляцію він
#   не починає, а .HMI встигає записати ще до того, як створить сторінки. Тому чекати появи
#   файлу не можна: треба питати сам редактор, скільки в ньому вже картинок (агент NeBuild),
#   і лише коли всі — просити скомпілювати. Редактор при цьому НЕ перезапускати.
set -e
N="$1"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; NB="$ROOT/tools/vm/nb.sh"; EX="$ROOT/tools/vm/exec.sh"
WIN='\\Mac\Home\Documents\radio_potughne_next'
[ -f "$ROOT/build/nx/$N/hmi.txt" ] || { echo "немає build/nx/$N/hmi.txt"; exit 1; }
WANT="$(ls "$ROOT/build/nx/$N/img" | wc -l | tr -d ' ')"

printf '%s\n' \
  "Get-Process NeBuild, NeLaunch, 'Nextion Editor' -ErrorAction SilentlyContinue | Stop-Process -Force; Start-Sleep 2" \
  "Remove-Item 'C:\\Tools\\work\\out\\$N.HMI','C:\\Tools\\work\\out\\$N.tft' -ErrorAction SilentlyContinue" \
  ". '$WIN\\tools\\vm\\ui.ps1'" \
  "\$arg = '$WIN\\build\\nx\\$N'.Replace(' ', '&;nspace&')" \
  "Start-Process \$Global:NeLaunch -ArgumentList \"\`\"\$arg\`\"\" -WorkingDirectory 'C:\\Tools\\NextionEditor'" \
  "'редактор запущено'" | "$EX" 2>&1 | tail -1

for i in $(seq 1 ${NX_WAIT:-80}); do
  sleep 20
  RC="$(NB_WAIT=60 "$NB" rescount 2>/dev/null | tail -1 | tr -d '\r')"
  echo "$(date +%H:%M:%S) $RC (треба pictures=$WANT)"
  echo "$RC" | grep -q "pictures=$WANT" && break
done
echo "$RC" | grep -q "pictures=$WANT" || { echo "проєкт не завантажився повністю"; exit 1; }
sleep 20

MSG="$(NB_WAIT=2400 "$NB" "tft C:\\Tools\\work\\out\\$N.tft" 2>&1)"
echo "$MSG" | sed -n '2,$p'
echo "$MSG" | grep -q "Compile Successful" || { echo "компіляція не вдалася"; exit 1; }

mkdir -p "$ROOT/build/nextion-out"
printf '%s\n' ". '$WIN\\tools\\vm\\ui.ps1'" \
  "Copy-Shared 'C:\\Tools\\work\\out\\$N.tft' '$WIN\\build\\nextion-out\\$N.tft'" | "$EX" >/dev/null
ls -l "$ROOT/build/nextion-out/$N.tft" | awk '{print "tft:", $5, "байт"}'
