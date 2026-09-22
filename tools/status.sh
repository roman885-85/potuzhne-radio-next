#!/bin/bash
# status.sh — що зараз відбувається на всіх ділянках: збірка прошивки, збірка екрана у ВМ,
# заливка .tft у дисплей, оновлення прошивки, зв'язок із радіо. Без очікування: знімок стану.
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
T="${CLAUDE_TASKS:-/private/tmp/claude-501/-Users-admin-Documents-cloude-work/f09beb39-11ac-43a4-a3f7-5ce9eba4f37b/tasks}"
echo "== $(date '+%H:%M:%S') =="

# 1. прошивка
if pgrep -f "[a]rduino-cli" >/dev/null; then
  N=$(find "$ROOT/build/bp" -name '*.o' -newermt '-60 seconds' 2>/dev/null | wc -l | tr -d ' ')
  echo "прошивка: КОМПІЛЮЄТЬСЯ (об'єктних файлів за хвилину: $N)"
else
  B="$ROOT/firmware/PotuzhneRadio-Nextion-update.bin"
  [ -f "$B" ] && echo "прошивка: готова $(stat -f '%z байт, %Sm' -t '%H:%M' "$B")" || echo "прошивка: немає"
fi

# 2. екран у ВМ
VM="${POTUZHNE_VM:-Мой Boot Camp}"
ST=$(prlctl status "$VM" 2>/dev/null | awk '{print $NF}')
if [ "$ST" != "running" ]; then
  echo "екран: ВМ $ST"
else
  R=$(printf '%s\n' \
    "\$p = Get-Process NeBuild -ErrorAction SilentlyContinue" \
    "\$e = if (\$p) { 'редактор працює, процесор ' + [int]\$p.TotalProcessorTime.TotalSeconds + ' с' } else { 'редактора немає' }" \
    "\$h = if (Test-Path 'C:\\Tools\\work\\out\\potuzhne.HMI') { 'HMI ' + [int]((Get-Item 'C:\\Tools\\work\\out\\potuzhne.HMI').Length/1MB) + ' МБ ' + (Get-Item 'C:\\Tools\\work\\out\\potuzhne.HMI').LastWriteTime.ToString('HH:mm') } else { 'HMI немає' }" \
    "\$t = if (Test-Path 'C:\\Tools\\work\\out\\potuzhne.tft') { 'TFT ' + (Get-Item 'C:\\Tools\\work\\out\\potuzhne.tft').Length + ' ' + (Get-Item 'C:\\Tools\\work\\out\\potuzhne.tft').LastWriteTime.ToString('HH:mm') } else { 'TFT немає' }" \
    "\"\$e | \$h | \$t\"" | "$ROOT/tools/vm/exec.sh" 2>/dev/null | tail -1)
  echo "екран: ${R:-ВМ не відповідає}"
fi

# 3. заливка в дисплей і оновлення прошивки: живі процеси й останній рядок їхнього журналу.
#    ВАЖЛИВО: не пускати вивід цих скриптів через «| tail» — він копиться до кінця, і ходу не видно.
for pat in nxupload.py ota_dev.sh nxdump.py; do
  pgrep -f "[${pat:0:1}]${pat:1}" >/dev/null 2>&1 && echo "радіо: $pat ПРАЦЮЄ"
done
for f in $(ls -t "$T"/b*.output 2>/dev/null | head -6); do
  [ -s "$f" ] || continue
  [ "$(stat -f %z "$f")" -gt 200000 ] && continue          # журнали підагентів — не наші
  L=$(grep -aE '##NX#|##UPD#' "$f" 2>/dev/null | tail -1)
  [ -n "$L" ] && echo "радіо: ${L:0:110}"
done | tail -2

# 4. HTTP-роздача і зв'язок
pgrep -f "[h]ttp.server 8765" >/dev/null && echo "роздача :8765 працює" || echo "роздача :8765 НЕ працює"
tail -1 "$ROOT/build/http-8765.log" 2>/dev/null | sed 's/^/останній запит: /'
[ -e /dev/cu.usbserial-14610 ] && echo "консоль радіо: порт на місці" || echo "консоль радіо: ПОРТУ НЕМАЄ"
