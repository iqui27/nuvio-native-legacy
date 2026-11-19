#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-live-error.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-O1 -g -Isrc -ffunction-sections -fdata-sections)
if [ "$(uname -s)" = Darwin ]; then flags+=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wl,-dead_strip); else
  read -r -a sdl_flags <<< "$(pkg-config --cflags sdl2)"
  flags+=("${sdl_flags[@]}" -Wl,--gc-sections)
fi
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" tests/live_error_recovery.c -lm -lpthread -o "$work/test"
"$work/test"
