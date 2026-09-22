#!/bin/bash
# run.sh <сценарій…> — прогнати сценарії стенда, домалювати відсутні спрайти (два проходи) і зняти знімки
#   build/nx/shots/<сценарій>.png (програвач nxrender.py — ті самі картинки й шрифти, що в .tft).
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
export NX_KEYS="$ROOT/nextion/sprite-keys.txt"
mkdir -p build/nx/cmds build/nx/shots
need=0
for s in "$@"; do build/nxhost/nxhost "$s" "build/nx/cmds/$s.txt" 2>&1 | grep -q "бракує спрайтів: 0" || need=1; done
if [ $need = 1 ]; then
  build/venv/bin/python tools/nextion/nxassets.py | tail -1
  tools/nxhost/build.sh
  for s in "$@"; do build/nxhost/nxhost "$s" "build/nx/cmds/$s.txt" 2>&1 | tail -1; done
fi
for s in "$@"; do build/venv/bin/python tools/nextion/nxrender.py "build/nx/cmds/$s.txt" "build/nx/shots/$s.png" | grep -v "^знімок" || true; done
echo "знімки: build/nx/shots/{$(echo "$@" | tr ' ' ',')}.png"
