#!/bin/bash
# usb_install.sh — дочекатися режиму завантажувача (BOOT + подати живлення) і записати по USB:
# завантажувач, розмітку, otadata, оновлювач (factory) і основну прошивку (app0).
# NVS (налаштування) і SPIFFS (станції, мережі) НЕ чіпає — тому не full.bin (merge_bin заливає пропуски 0xFF).
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; F="$ROOT/firmware"; N=PotuzhneRadio-Nextion
PORT=${PORT:-/dev/cu.usbserial-14610}
E=$(ls -d "$ROOT"/build/arduino15/data/packages/esp32/tools/esptool_py/*/esptool | sort -V | tail -1)
"$ROOT/build/venv/bin/python" "$ROOT/tools/device/watch.py" || { echo "не дочекався завантажувача"; exit 1; }
"$E" --chip esp32 --port "$PORT" --baud 115200 --before no_reset --after no_reset write_flash --flash_mode dio --flash_freq 80m --flash_size 4MB \
  0x1000 "$F/$N-bootloader.bin" 0x8000 "$F/$N-partitions.bin" 0xe000 "$F/boot_app0.bin" \
  0x10000 "$F/$N-updater.bin" 0x120000 "$F/$N-update.bin" 2>&1 | tr '\r' '\n' | grep -vE '^Writing at|^\s*$' | tail -12
echo "ЗАПИСАНО $(date +%T) — вимкніть і ввімкніть живлення (без BOOT)"
