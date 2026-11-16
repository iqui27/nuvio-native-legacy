#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
read -r -a sdl_flags <<< "$(pkg-config --cflags --libs sdl2)"
if [ "$(uname)" = Darwin ]; then gc=-Wl,-dead_strip; else gc=-Wl,--gc-sections; fi
cc -std=c11 -D_GNU_SOURCE -DNV_HTTP_BUFFER_DIAG -DNV_BUFFER_SEEK_TEST -O1 -g -ffunction-sections -fdata-sections -Isrc \
  tests/video_buffer_test_osd.c src/js.c src/linguas.c src/lsregistro.c src/audioinfo.c src/audioescolha.c src/webosver.c src/cacheboost.c \
  "$gc" -o "$work/test" "${sdl_flags[@]}" -ldl -pthread -lm
"$work/test"
# Compile the real renderer and webOS backend with the test-only flag enabled.
cc -std=gnu11 -D_GNU_SOURCE -DNV_HTTP_BUFFER_DIAG -DNV_BUFFER_SEEK_TEST -Werror=implicit-function-declaration -Isrc \
  "${sdl_flags[@]}" -c src/player.c -o "$work/player.o"
cc -std=gnu11 -D_GNU_SOURCE -DNV_HTTP_BUFFER_DIAG -DNV_BUFFER_SEEK_TEST -Werror=implicit-function-declaration -Isrc \
  "${sdl_flags[@]}" -c src/video.c -o "$work/video.o"
