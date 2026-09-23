#!/bin/bash
# probe.sh <тека в build/nx> [команди агенту…] — завантажити маленький проєкт у редактор
# і спитати в нього, що саме завантажилось.  Потрібен, бо повна збірка йде пів години, а
# питання на кшталт «чи доїхав код події» вирішується за хвилину на проєкті з трьох об'єктів.
set -e
N="$1"; shift
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; NB="$ROOT/tools/vm/nb.sh"; EX="$ROOT/tools/vm/exec.sh"
WIN='\\Mac\Home\Documents\radio_potughne_next'
printf '%s\n' \
  "Get-Process NeBuild, NeLaunch, 'Nextion Editor' -ErrorAction SilentlyContinue | Stop-Process -Force; Start-Sleep 3" \
  "Remove-Item 'C:\\Tools\\work\\out\\$N.HMI','C:\\Tools\\work\\out\\$N.tft' -ErrorAction SilentlyContinue" \
  ". '$WIN\\tools\\vm\\ui.ps1'" \
  "\$arg = '$WIN\\build\\nx\\$N'.Replace(' ', '&;nspace&')" \
  "Start-Process \$Global:NeLaunch -ArgumentList \"\`\"\$arg\`\"\" -WorkingDirectory 'C:\\Tools\\NextionEditor'" \
  "'запущено'" | "$EX" >/dev/null 2>&1
for i in $(seq 1 24); do
  sleep 5
  ALIVE="$(printf '%s\n' "(Get-Process NeBuild -ErrorAction SilentlyContinue).Count" | "$EX" 2>/dev/null | tr -d ' \r\n')"
  if [ "$i" -gt 3 ]; then case "$ALIVE" in ''|0) echo "агент помер на $((i*5)) с — редактор упав на цьому проєкті"; exit 2;; esac; fi
  OUT="$(NB_WAIT=6 "$NB" 'list' 2>/dev/null || true)"
  echo "$OUT" | grep -q "type=" && { echo "$OUT"; break; }
done
for c in "$@"; do echo "--- $c"; NB_WAIT=60 "$NB" "$c" 2>&1 | sed -n '2,20p'; done
