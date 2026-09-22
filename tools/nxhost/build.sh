#!/bin/bash
# Зібрати стенд меню на Mac: tools/nxhost/build.sh → build/nxhost/nxhost
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; M="$ROOT/source/port/m2"
mkdir -p "$ROOT/build/nxhost"
clang++ -std=c++17 -O1 -g -Wno-dangling-else -Wno-parentheses -I "$ROOT/tools/nxhost/shim" -I "$M" \
  "$M/m2gfx.cpp" "$M/m2icons.cpp" "$M/m2lang.cpp" "$M/m2ui.cpp" "$M/m2player.cpp" "$M/nxassets.cpp" \
  "$M/m2pages_main.cpp" "$M/m2pages_net.cpp" "$M/m2pages_sound.cpp" "$M/m2stations.cpp" "$M/m2update.cpp" \
  "$ROOT/tools/nxhost/mock_wifi.cpp" \
  "$M/../extras/yoExtras.cpp" "$ROOT/tools/nxhost/mock_radio.cpp" "$ROOT/tools/nxhost/host.cpp" \
  -o "$ROOT/build/nxhost/nxhost"
