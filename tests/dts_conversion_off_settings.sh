#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
read -r -a sdl_flags <<< "$(pkg-config --cflags --libs sdl2)"
cc -std=gnu11 -D_GNU_SOURCE -O2 -g -ffunction-sections -fdata-sections \
  -Isrc tests/dts_conversion_off_settings.c -o "$tmp/dts-off" \
  "${sdl_flags[@]}" -lSDL2_image -lSDL2_ttf -lGL -lz -ldl -pthread -lm -Wl,--gc-sections
"$tmp/dts-off"
echo "PASS DTS conversion off: settings, buffer limits, Dolby Vision and MP4 settings"
