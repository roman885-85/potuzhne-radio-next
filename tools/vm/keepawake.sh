#!/bin/bash
# keepawake.sh [секунд] — тримати ВМ бадьорою: Parallels ставить її на паузу за ~хвилину без звернень
# з Mac, і тоді генерація шрифтів / компіляція в редакторі просто завмирають.
VM="${POTUZHNE_VM:-Мой Boot Camp}"; END=$(( $(date +%s) + ${1:-1800} ))
while [ "$(date +%s)" -lt "$END" ]; do
  [ "$(prlctl status "$VM" 2>/dev/null | awk '{print $NF}')" = "running" ] || prlctl resume "$VM" >/dev/null 2>&1
  prlctl exec "$VM" cmd /c "ver" >/dev/null 2>&1
  sleep 20
done
