#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
flags=(-std=gnu11 -Isrc -O1 -g -ffunction-sections -fdata-sections -Wno-unused-function)
if [ "$(uname -s)" = Darwin ]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
read -r -a sdk <<< "$(pkg-config --cflags sdl2)"
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
out=$(mktemp "${TMPDIR:-/tmp}/nuvio-addon-order-settings.XXXXXX")
trap 'rm -f "$out"' EXIT
cc "${flags[@]}" "${sdk[@]}" tests/addon_order_settings.c src/fontefalha.c src/fontefiltro.c \
  src/linguas.c src/limpa.c src/dobra.c -pthread -lm -o "$out"
"$out"
