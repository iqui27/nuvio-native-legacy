#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-addons-proveniencia.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-std=gnu11 -O1 -g -pthread -Isrc -ffunction-sections -fdata-sections
       -I/opt/homebrew/include -I/opt/homebrew/include/SDL2)
if [[ $(uname -s) == Darwin ]]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
if command -v pkg-config >/dev/null && pkg-config --exists sdl2; then
  read -r -a sdk_flags <<< "$(pkg-config --cflags sdl2)"
  flags+=("${sdk_flags[@]}")
fi
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" tests/addons_proveniencia.c src/js.c src/jsw.c src/watchstate.c src/addonstats.c src/fontefalha.c src/idcanal.c -o "$work/test"
"$work/test"
