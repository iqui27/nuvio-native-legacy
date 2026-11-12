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
if [[ $(uname) == Darwin ]]; then
  # Host check on the Mac: same flags as tools/build-local.sh.
  cflags=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations)
  libs=(-L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL); plat=(); gc=(-Wl,-dead_strip)
else
  read -r -a cflags <<< "$(pkg-config --cflags sdl2 SDL2_image SDL2_ttf glesv2 egl zlib)"
  read -r -a libs <<< "$(pkg-config --libs sdl2 SDL2_image SDL2_ttf glesv2 egl zlib)"
  libs+=(-ldl); plat=(-DNV_LINUX_DESKTOP); gc=(-Wl,--gc-sections)
fi
for dv7 in off on; do
  extra=(); if [[ $dv7 == on ]]; then extra+=(-DNV_DV7_CONVERSION); fi
  cc -std=gnu11 "${plat[@]}" "${extra[@]}" -Isrc -O1 -g -ffunction-sections -fdata-sections \
    "${sources[@]}" tests/dv7_settings.c "${cflags[@]}" \
    "${gc[@]}" -o "$task_tmp/settings" "${libs[@]}" -pthread -lm
  "$task_tmp/settings"
done
