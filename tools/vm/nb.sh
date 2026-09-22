#!/bin/bash
# nb.sh — надіслати команди агенту NeBuild у Nextion Editor (ВМ) і дочекатися відповіді.
#   echo "dump" | tools/vm/nb.sh          або   tools/vm/nb.sh "types"
# Агент читає build/nebuild/cmd.txt зі спільної теки й пише build/nebuild/out.txt.
set -e
D="$(cd "$(dirname "$0")/../.." && pwd)/build/nebuild"
VM="${POTUZHNE_VM:-Мой Boot Camp}"
mkdir -p "$D"; rm -f "$D/out.txt"
if [ $# -gt 0 ]; then printf '%s\n' "$@" > "$D/cmd.tmp"; else cat > "$D/cmd.tmp"; fi
mv "$D/cmd.tmp" "$D/cmd.txt"
[ "$(prlctl status "$VM" 2>/dev/null | awk '{print $NF}')" = "running" ] || prlctl resume "$VM" >/dev/null 2>&1 || true
for i in $(seq 1 ${NB_WAIT:-600}); do
  if [ -f "$D/out.txt" ]; then cat "$D/out.txt"; exit 0; fi
  sleep 0.5
  # ВМ сама стає на паузу без активності — будимо
  [ $((i % 20)) -eq 0 ] && { [ "$(prlctl status "$VM" 2>/dev/null | awk '{print $NF}')" = "running" ] || prlctl resume "$VM" >/dev/null 2>&1 || true; }
done
echo "НЕМАЄ ВІДПОВІДІ (агент не запущений?)"; exit 1
