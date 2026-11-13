#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d);trap 'rm -rf "$tmp"' EXIT
read -r -a flags <<< "$(pkg-config --cflags --libs sdl2)"
extra=();if [[ ${NV_TEST_MP4:-0} == 1 ]];then extra+=(-DNV_MP4_BD_TEST -DNV_DV7_CONVERSION);fi
sources=()
for s in src/*.c src/dts/*.c; do
 case "$s" in src/main.c|src/ajustes.c)continue;;esac
 sources+=("$s")
done
cc -std=gnu11 -D_GNU_SOURCE -DNV_HTTP_BUFFER_DIAG -DNV_DV_FIXED_AUDIO_TEST \
 "${extra[@]}" -O0 -g -ffunction-sections -fdata-sections -Isrc "${sources[@]}" \
 tests/buffer_player_settings.c "${flags[@]}" -lSDL2_image -lSDL2_ttf -lz -lGL \
 -ldl -pthread -lm -Wl,--gc-sections -o "$tmp/test"
"$tmp/test"
