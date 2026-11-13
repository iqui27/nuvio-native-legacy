#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
read -r -a sdl_flags <<< "$(pkg-config --cflags --libs sdl2)"
for mode in enabled disabled; do
  flags=();if [[ $mode == enabled ]]; then flags+=(-DNV_DV_FIXED_AUDIO_TEST); fi
  cc -std=gnu11 -D_GNU_SOURCE -O2 -g -ffunction-sections -fdata-sections \
    -Isrc "${flags[@]}" tests/dv_fixed_audio_settings.c -o "$tmp/$mode" \
    "${sdl_flags[@]}" -lSDL2_image -lSDL2_ttf -lGL -lz -ldl -pthread -lm -Wl,--gc-sections
  "$tmp/$mode"
done
