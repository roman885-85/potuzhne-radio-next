#!/bin/bash
S=/Users/admin/Documents/radio_potughne_next/build/device-work
E=~/Library/Arduino15/packages/esp32/tools/esptool_py/5.1.0/esptool
$S/venv/bin/python $S/watch.py || exit 1
echo "=== reading whole flash at 115200 ($(date +%T))"
$E --port /dev/cu.usbserial-14610 --baud 115200 --before no-reset --after no-reset read-flash 0 ALL $S/flash_full.bin > $S/read_flash.log 2>&1
rc=$?
tr '\r' '\n' < $S/read_flash.log | grep -v '^\s*$' | tail -8
echo "esptool rc=$rc ($(date +%T))"
ls -la $S/flash_full.bin && shasum -a 256 $S/flash_full.bin
exit $rc
