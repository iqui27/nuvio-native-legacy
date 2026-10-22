#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
read -r -a sdl_flags <<< "$(pkg-config --cflags --libs sdl2)"
if [ "$(uname)" = Darwin ]; then gc=-Wl,-dead_strip; else gc=-Wl,--gc-sections; fi
cc -std=c11 -D_GNU_SOURCE -O1 -g -ffunction-sections -fdata-sections -Isrc \
  tests/video_acb_lg.c src/js.c src/linguas.c src/lsregistro.c src/audioinfo.c "$gc" -o "$tmp/test" "${sdl_flags[@]}" -ldl -pthread -lm
"$tmp/test"
