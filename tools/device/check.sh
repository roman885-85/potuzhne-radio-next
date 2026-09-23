#!/bin/bash
# check.sh — повний прогін системи одним рухом, без людських очей.
#
# Сенс: не «подивись на екран», а перевірка числами, яку можна повторити будь-коли. Усе, що
# раніше доводилось питати у власника («скільки смужок видно», «чи їде ручка гучності»),
# перевіряється тут виміром із самого радіо.
#
# Говоримо по мережі (rc.py), а не через USB: порт один, і дві перевірки одночасно псували
# одна одну — звідси була половина хибних «зламано».
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; PY="$ROOT/build/venv/bin/python"
cd "$ROOT"
RC="$PY tools/device/rc.py"
IP="${RADIO_IP:-192.168.1.158}"
bad=0
say(){ printf '%-26s %s  %s\n' "$1" "$2" "$3"; [ "$2" = "ЗЛАМАНО" ] && bad=1; return 0; }

echo "=== 1. самоперевірка радіо ==="
OUT="$($RC 'nx test' 2>&1 | tr -d '\000' | grep -a '##NXT#' | sed 's/.*##NXT#[[:space:]]*//')"
[ -n "$OUT" ] || { echo "радіо не відповідає по мережі"; exit 2; }
echo "$OUT"
echo "$OUT" | grep -q "ЗЛАМАНО" && bad=1

echo
echo "=== 2. секунди йдуть? ==="
sec(){ $RC 'nx dump' 2>&1 | tr -d '\000' | grep -a 'sc\.txt' | sed 's/.*= *//' | tr -d ' \r'; }
A="$(sec)"; sleep 4; B="$(sec)"
[ -n "$A" ] && [ "$A" != "$B" ] && say "секунди" ГАРАЗД "$A → $B" || say "секунди" ЗЛАМАНО "$A → $B (стоять)"

echo
echo "=== 3. спектр: чи живі всі смужки ==="
#  Беремо кілька знімків висот і рахуємо, скільки смужок бодай раз ворухнулось. Саме тут
#  колись ховалось «бачу десять із чотирнадцяти»: решта стояла на нулі й була невидима.
$RC 'play' >/dev/null 2>&1; sleep 6
$PY - "$IP" <<'PY'
import subprocess, sys, time, re
def bars():
    o = subprocess.run(['build/venv/bin/python', 'tools/device/rc.py', 'nx bars'],
                       capture_output=True, text=True).stdout
    m = re.search(r'висоти:([0-9 ]+)', o)
    return [int(x) for x in m.group(1).split()] if m else []
snaps = []
for i in range(6):
    b = bars()
    if b: snaps.append(b)
    time.sleep(0.6)
if not snaps:
    print('смужки                     ЗЛАМАНО  радіо не відповіло на «nx bars»'); sys.exit(1)
n = len(snaps[0])
moved = sum(1 for i in range(n) if len({s[i] for s in snaps if i < len(s)}) > 1)
mx = max(max(s) for s in snaps)
print('%-26s %s  %d з %d ворухнулись, найвища %d%%' %
      ('смужки', 'ГАРАЗД' if moved >= n - 1 else 'ЗЛАМАНО', moved, n, mx))
sys.exit(0 if moved >= n - 1 else 1)
PY
[ $? -ne 0 ] && bad=1

echo
echo "=== 4. ручка гучності доїжджає? ==="
#  Рахує сама прошивка: читати `vol.val` ззовні не можна — кожне читання зупиняє драйвер
#  і коштує майже секунду, а весь шлях ручки — чверть секунди. Секундною лінійкою мить не
#  зміряти, і перша версія цієї перевірки саме тому й брехала «зламано».
S0="$($RC 'nx vol' 2>&1 | tr -d '\000' | grep -ao 'кроків [0-9]*' | awk '{print $2}')"
$RC 'vol 5' >/dev/null 2>&1; sleep 1
$RC 'vol 40' >/dev/null 2>&1; sleep 1
S1="$($RC 'nx vol' 2>&1 | tr -d '\000' | grep -ao 'кроків [0-9]*' | awk '{print $2}')"
D=$(( ${S1:-0} - ${S0:-0} ))
[ "$D" -ge 3 ] && say "ручка гучності" ГАРАЗД "$D кроків на шляху" || say "ручка гучності" ЗЛАМАНО "кроків $D (стрибає)"

echo "=== 5. радіо видно програмам? ==="
H="$(curl -s -m 5 http://$IP/api/hello)"
echo "$H" | grep -q '"potuzhne":1' && say "/api/hello" ГАРАЗД "$(echo "$H" | cut -c1-70)" || say "/api/hello" ЗЛАМАНО "${H:-немає відповіді}"

echo
[ $bad -eq 0 ] && echo "ПІДСУМОК: усе гаразд" || echo "ПІДСУМОК: є зламане"
exit $bad
