#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
read -r -a sdl <<< "$(pkg-config --cflags sdl2)"
cc -O1 -Isrc "${sdl[@]}" -ffunction-sections -fdata-sections \
  tests/rotahost_canal.c src/rotahost.c -pthread -Wl,--gc-sections -o "$work/test"
"$work/test"
