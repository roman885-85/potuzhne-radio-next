#!/bin/bash
# ---------------------------------------------------------------------------
#  Збірка прошивки «ПОТУЖНЕ РАДІО (Nextion)»: ESP32 + VS1053 + Nextion NX4832F035.
#  Версія — у firmware/VERSION. Ядро arduino-esp32 2.0.17 — власна копія в
#  build/arduino15 (глобальне ядро 3.3.3 потрібне ПОТУЖНОМУ РАДІО, його не чіпаємо).
#
#  Чому 2.0.17, а не 3.x: на 3.3.3 з Bluetooth-колонкою не вміщається IRAM
#  (не вистачає 456 байт — код Wi-Fi і BT з готових бібліотек), а без BT
#  образ більший на 167 КБ (замір 22.09.2026, див. ЖУРНАЛ.md).
#
#  Розмітка — source/yoRadio/partitions.csv: factory (оновлювач) + app0 2,75 МБ.
# ---------------------------------------------------------------------------
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(dirname "$HERE")"
B="$ROOT/build"
SKETCH="$ROOT/source/yoRadio"
A15="$B/arduino15/data"

CLI="$HOME/bin/arduino-cli"
_p="$(command -v arduino-cli || true)"; [ -n "$_p" ] && CLI="$_p"
[ -x "$CLI" ] || { echo "arduino-cli не знайдено"; exit 1; }

if [ ! -f "$B/arduino-cli.yaml" ]; then
  mkdir -p "$B/sketchbook/libraries" "$B/arduino15/staging"
  cat > "$B/arduino-cli.yaml" <<YAML
board_manager:
  additional_urls:
    - https://espressif.github.io/arduino-esp32/package_esp32_index.json
directories:
  data: $A15
  downloads: $B/arduino15/staging
  user: $B/sketchbook
logging:
  level: warn
YAML
fi
#  Ядро 2.0.17 ставиться в build/arduino15 один раз (≈2,5 ГБ).
[ -d "$A15/packages/esp32/hardware/esp32/2.0.17" ] || \
  "$CLI" --config-file "$B/arduino-cli.yaml" core install esp32:esp32@2.0.17
#  Бібліотека Bluetooth-колонки — не з менеджера бібліотек (її там немає).
[ -d "$B/sketchbook/libraries/ESP32-A2DP" ] || \
  git clone -q --depth 1 https://github.com/pschatzmann/ESP32-A2DP.git "$B/sketchbook/libraries/ESP32-A2DP"

APP_MAX=$((0x2B0000))
FQBN="esp32:esp32:esp32:FlashMode=dio,FlashFreq=80,PSRAM=disabled,DebugLevel=none"

VER="$(tr -d ' \n\r' < "$HERE/VERSION" 2>/dev/null)"; [ -n "$VER" ] || VER="0.0.0"
BUILD="$(date '+%d.%m.%Y %H:%M')"
mkdir -p "$SKETCH/src/extras"
printf '/* Створює firmware/rebuild.sh під час кожної збірки — вручну не правити. */\n#define PR_VERSION "%s"\n#define PR_BUILD   "%s"\n' "$VER" "$BUILD" > "$SKETCH/src/extras/yoBuild.h"
echo ">>> версія $VER, збірка $BUILD"

echo ">>> компіляція основної прошивки"
"$CLI" --config-file "$B/arduino-cli.yaml" compile --fqbn "$FQBN" \
  --build-property "upload.maximum_size=$APP_MAX" \
  --build-path "$B/bp" --output-dir "$B/out" "$SKETCH"

echo ">>> компіляція оновлювача (розділ factory)"
"$CLI" --config-file "$B/arduino-cli.yaml" compile --fqbn "$FQBN" \
  --build-property "upload.maximum_size=$((0x110000))" \
  --build-path "$B/bp-upd" --output-dir "$B/out-upd" "$ROOT/source/updater"

echo ">>> образ файлової системи (сторінка, станції)"
MKSPIFFS=$(ls -d "$A15"/packages/esp32/tools/mkspiffs/*/mkspiffs | head -1)
"$MKSPIFFS" -c "$SKETCH/data" -b 4096 -p 256 -s 0x20000 "$B/out/yoRadio.spiffs.bin" > /dev/null

ESPTOOL=$(ls -d "$A15"/packages/esp32/tools/esptool_py/*/esptool* | sort -V | tail -1)
cp "$A15"/packages/esp32/hardware/esp32/2.0.17/tools/partitions/boot_app0.bin "$B/out/"
cd "$B/out"
N=PotuzhneRadio-Nextion
cp yoRadio.ino.bin            "$HERE/$N-update.bin"
cp yoRadio.ino.bootloader.bin "$HERE/$N-bootloader.bin"
cp yoRadio.ino.partitions.bin "$HERE/$N-partitions.bin"
cp yoRadio.spiffs.bin         "$HERE/$N-files.bin"
cp boot_app0.bin              "$HERE/boot_app0.bin"
cp "$B/out-upd/updater.ino.bin" "$HERE/$N-updater.bin"
#  Повний образ для першого встановлення по USB (без SPIFFS — станції й мережі на радіо лишаються):
"$ESPTOOL" --chip esp32 merge_bin -o "$HERE/$N-full.bin" --flash_mode dio --flash_freq 80m --flash_size 4MB \
  0x1000 yoRadio.ino.bootloader.bin 0x8000 yoRadio.ino.partitions.bin 0xe000 boot_app0.bin \
  0x10000 "$B/out-upd/updater.ino.bin" 0x120000 yoRadio.ino.bin > /dev/null
SZ=$(stat -f %z yoRadio.ino.bin); SU=$(stat -f %z "$B/out-upd/updater.ino.bin")
echo ">>> оновлювач $SU байт із $((0x110000)) ($((SU*100/0x110000))%)"
echo ">>> готово: версія $VER від $BUILD; прошивка $SZ байт із $APP_MAX ($((SZ*100/APP_MAX))%)"
