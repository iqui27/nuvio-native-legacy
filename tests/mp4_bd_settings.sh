#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d);trap 'rm -rf "$dir"' EXIT
read -r -a flags <<< "$(pkg-config --cflags --libs sdl2)"
for mode in enabled disabled;do
 extra=();if [[ $mode == enabled ]];then extra+=(-DNV_MP4_BD_TEST);fi
 cc -std=gnu11 -D_GNU_SOURCE -DNV_DV7_CONVERSION "${extra[@]}" -O0 -g \
  -ffunction-sections -fdata-sections -Isrc tests/mp4_bd_settings.c \
  "${flags[@]}" -lSDL2_image -lSDL2_ttf -lGL -lz -ldl -pthread -lm -Wl,--gc-sections -o "$dir/$mode"
 "$dir/$mode"
done
