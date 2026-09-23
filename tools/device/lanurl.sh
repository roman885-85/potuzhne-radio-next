#!/bin/bash
# lanurl.sh — підняти роздачу build/nextion-out з Mac і надрукувати адресу для радіо.
# Радіо качає звідси, а не з GitHub: без TLS вистачає пам'яті (під шифрування треба суцільний
# шматок ~40 КБ, якого у радіо не лишалось) і файл їде домашньою мережею, а не через інтернет.
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PORT=8765
lsof -nP -iTCP:$PORT -sTCP:LISTEN >/dev/null 2>&1 || {
  nohup "$ROOT/build/venv/bin/python" "$ROOT/tools/device/serve.py" "$ROOT/build/nextion-out" $PORT \
    > /tmp/nxserve.log 2>&1 &
  for i in $(seq 1 20); do sleep 0.3; lsof -nP -iTCP:$PORT -sTCP:LISTEN >/dev/null 2>&1 && break; done
}
IP=$(ipconfig getifaddr en0 2>/dev/null || ipconfig getifaddr en1)
[ -n "$IP" ] || { echo "немає адреси Mac у мережі" >&2; exit 1; }
echo "http://$IP:$PORT"
