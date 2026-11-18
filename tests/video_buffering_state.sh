#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
read -r -a sdl_flags <<< "$(pkg-config --cflags --libs sdl2)"
if [ "$(uname)" = Darwin ]; then gc=-Wl,-dead_strip; else gc=-Wl,--gc-sections; fi
san=()
if [ "${SANITIZE:-0}" = 1 ]; then san=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc -std=c11 -D_GNU_SOURCE -DNV_NATIVE_SEEK_TEST -O1 -g -ffunction-sections -fdata-sections -Isrc \
  "${san[@]}" tests/video_buffering_state.c src/js.c src/linguas.c src/lsregistro.c src/audioinfo.c src/audioescolha.c src/audioext.c src/webosver.c src/cacheboost.c \
  "$gc" -o "$work/test" "${sdl_flags[@]}" -ldl -pthread -lm
"$work/test" "$@"
