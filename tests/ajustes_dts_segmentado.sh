#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d);trap 'rm -rf "$tmp"' EXIT
read -r -a flags <<< "$(pkg-config --cflags --libs sdl2)"
sources=()
for s in src/*.c src/dts/*.c; do
 case "$s" in src/main.c|src/ajustes.c)continue;;esac
 sources+=("$s")
done
cc -std=gnu11 -D_GNU_SOURCE -O0 -g -ffunction-sections -fdata-sections -Isrc "${sources[@]}" \
 tests/ajustes_dts_segmentado.c "${flags[@]}" -lSDL2_image -lSDL2_ttf -lz -lGL \
 -ldl -pthread -lm -Wl,--gc-sections -o "$tmp/test"
"$tmp/test" | grep -v '^\['
