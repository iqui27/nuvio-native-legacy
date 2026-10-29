#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
task_tmp=$(mktemp -d)
trap 'rm -rf "$task_tmp"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done
read -r -a cflags <<< "$(pkg-config --cflags sdl2 SDL2_image SDL2_ttf glesv2 egl zlib)"
read -r -a libs <<< "$(pkg-config --libs sdl2 SDL2_image SDL2_ttf glesv2 egl zlib)"
for dv7 in off on; do
  extra=(); if [[ $dv7 == on ]]; then extra+=(-DNV_DV7_CONVERSION); fi
  cc -std=gnu11 -DNV_LINUX_DESKTOP "${extra[@]}" -Isrc -O1 -g -ffunction-sections -fdata-sections \
    "${sources[@]}" tests/dv7_settings.c "${cflags[@]}" \
    -Wl,--gc-sections -o "$task_tmp/settings" "${libs[@]}" -ldl -pthread -lm
  "$task_tmp/settings"
done
