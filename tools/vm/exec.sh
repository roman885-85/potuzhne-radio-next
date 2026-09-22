#!/bin/bash
# =============================================================================
#  tools/vm/exec.sh — виконати PowerShell у Windows (Parallels), де стоїть Nextion Editor
# =============================================================================
#  Той самий прийом, що в tools/nas-migrator-win/vm/exec.sh проєкту сайту:
#  сценарій зі stdin кладеться поруч із собою у тимчасовий файл із BOM і
#  запускається у віртуальній машині від імені користувача, що сидить за нею.
#
#  BOM обов'язковий: без нього PowerShell 5.1 читає українські рядки кашею.
#  «--current-user» теж: без нього команда йде в сеансі служб, де немає
#  робочого столу, і вікно програми не з'явиться.
#
#  Використання:
#      echo 'Get-Date' | vm/exec.sh
#      vm/exec.sh < сценарій.ps1
# =============================================================================
set -euo pipefail

cd "$(dirname "$0")"

VM="${POTUZHNE_VM:-Мой Boot Camp}"
export PATH="/usr/local/bin:$PATH"

command -v prlctl >/dev/null || { echo "prlctl не знайдено — Parallels не встановлено?"; exit 1; }

# ВМ сама стає на паузу без активності («paused»), буває й «suspended» — будимо, доки не запрацює.
for i in 1 2 3 4 5 6; do
    st="$(prlctl status "$VM" 2>/dev/null | awk '{print $NF}')"
    [ "$st" = "running" ] && break
    echo "Віртуальна машина «$VM»: $st — вмикаю…" >&2
    prlctl resume "$VM" >/dev/null 2>&1 || prlctl start "$VM" >/dev/null 2>&1 || true
    sleep 4
done

TMP="_run-$$.ps1"
{
    printf '\xEF\xBB\xBF'
    printf '$OutputEncoding = [Console]::OutputEncoding = [Text.Encoding]::UTF8\r\n'
    sed 's/$/\r/'
} < /dev/stdin > "$TMP"

# Шлях до сценарію з боку Windows: домашня тека Mac видна як \\Mac\Home.
GUEST="\\\\Mac\\Home\\Documents\\radio_potughne_next\\tools\\vm\\$TMP"

set +e
for attempt in 1 2 3; do
    OUT="$(prlctl exec "$VM" --current-user powershell -NoProfile -ExecutionPolicy Bypass -File "$GUEST" 2>&1)"
    CODE=$?
    # ВМ могла стати на паузу між перевіркою й запуском — будимо й повторюємо
    if printf '%s' "$OUT" | grep -q 'is in the "paused" state\|is in the "suspended" state'; then
        prlctl resume "$VM" >/dev/null 2>&1; sleep 4; continue
    fi
    break
done
printf '%s\n' "$OUT"
set -e

rm -f "$TMP"
exit $CODE
