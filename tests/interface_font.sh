#!/bin/bash
# Actual Settings and renderer, including runtime font changes. Requires GL.
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-interface-font.XXXXXX")
trap 'rm -rf "$work"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done
flags=(-O0 -g -std=gnu11 -Isrc -Wno-deprecated-declarations)
libs=(-lz -lm -lpthread)
if [ "$(uname -s)" = Darwin ]; then
  flags+=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2)
  libs+=(-L/opt/homebrew/lib -framework OpenGL)
else
  libs+=(-lGLESv2)
fi
if [ "${SANITIZE:-0}" = 1 ]; then
  flags+=(-O1 -fsanitize=address,undefined -fno-omit-frame-pointer)
  if [ "$(uname -s)" = Linux ]; then flags+=(-fno-pie -no-pie); fi
fi
# shellcheck disable=SC2046
"${CC:-cc}" "${flags[@]}" $(pkg-config --cflags sdl2 SDL2_ttf SDL2_image) \
  "${sources[@]}" tests/interface_font.c -o "$work/test" \
  $(pkg-config --libs sdl2 SDL2_ttf SDL2_image) "${libs[@]}"
mkdir "$work/ttf" "$work/otf"
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" NUVIO_DADOS="$work/ttf" "$work/test" "$work/ttf" deploy/app/fonts/JetBrainsMonoNL-Regular.ttf
python3 tests/font_fixture.py "$work/fixture.otf"
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" NUVIO_DADOS="$work/otf" "$work/test" "$work/otf" "$work/fixture.otf"
