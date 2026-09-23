#!/bin/bash
# deploy.sh — одна команда на весь цикл: прошивка → екран → перевірка.
#   deploy.sh          — залити прошивку й екран, потім перевірити
#   deploy.sh fw       — лише прошивку
#   deploy.sh tft      — лише екран
# Екран і прошивку радіо качає саме радіо — з роздачі на Mac (tools/device/lanurl.sh).
# Через GitHub не качаємо: HTTPS просить у радіо суцільний шматок пам'яті, якого немає
# («завантаження: HTTP -1»), та й 8,7 МБ ходили б через інтернет замість домашньої мережі.
# Потрібен GitHub (радіо не бачить Mac) — DEPLOY_SRC=gh.
# Команди радіо шлемо по мережі (rc.py): USB-порт один, і паралельні перевірки псували
# одна одну — звідси була половина хибних «зламано». USB лишається тільки для прошивки.
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
PY="$ROOT/build/venv/bin/python"
what="${1:-all}"

if [ "$what" = "all" ] || [ "$what" = "fw" ]; then
  echo "=== прошивка ==="
  ls /dev/cu.usbserial-* >/dev/null 2>&1 || { echo "адаптера радіо немає — під'єднайте кабель"; exit 1; }
  tools/device/ota_dev.sh 2>&1 | grep -E "прошивка:|Ready!|помилка" || true
  #  після перезавантаження радіо мережа піднімається не миттєво
  for i in $(seq 1 30); do sleep 2; $PY tools/device/rc.py 'nx status' >/dev/null 2>&1 && break; done
fi

if [ "$what" = "all" ] || [ "$what" = "tft" ]; then
  echo "=== екран ==="
  if [ "$DEPLOY_SRC" = "gh" ]; then
    gh release upload dev-screen build/nextion-out/potuzhne.tft --clobber >/dev/null 2>&1 || true
    URL=https://github.com/roman885-85/potuzhne-radio-next/releases/download/dev-screen/potuzhne.tft
  else
    URL="$(tools/device/lanurl.sh)/potuzhne.tft"
  fi
  #  Швидкість заливки — робоча швидкість екрана (921600), а не 115200: той самий файл
  #  ішов 12,5 хв, тепер близько 2,5 хв. Протокол старий (v1): новий зривався на 8 КБ.
  $PY - "$URL" <<'PY'
import sys
sys.path.insert(0, 'tools/device')
import rc
s = rc.connect()
print(' ', rc.cmd(s, 'nx upload %s 921600 v1' % sys.argv[1], 8).replace('\n> ', ' ').strip()[:200])
s.close()
PY
  echo "  (заливка йде в радіо; чекаю…)"
  #  Порожня відповідь — не кінець заливки, а радіо, що ще піднімається: колись цикл через
  #  це виходив одразу й лишав екран зі старим файлом.
  for i in $(seq 1 90); do
    sleep 20
    st=$($PY tools/device/rc.py 'nx status' 2>/dev/null | sed 's/.*##NX#[[:space:]]*//' | head -1)
    echo "  $st"
    [ -z "$st" ] && continue
    echo "$st" | grep -q "зайнято" || break
  done
fi

echo "=== перевірка ==="
sleep 20
tools/device/check.sh
