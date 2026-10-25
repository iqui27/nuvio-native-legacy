#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# txt_largura == largura rasterizada; blocos so rasterizam as linhas finais.
#   bash tests/text_largura.sh
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do [ "$source" != src/main.c ] && sources+=("$source"); done
flags=(-O1 -g)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
"${CC:-cc}" "${flags[@]}" "${sources[@]}" tests/text_largura.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-text-largura \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-text-font.XXXXXX")
trap 'rm -rf "$work"' EXIT
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" NUVIO_DADOS="$work" ${TMPDIR:-/tmp}/nuvio-text-largura
python3 tests/font_fixture.py "$work/fixture.otf"
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" NV_TEST_FONT="$work/fixture.otf" NUVIO_DADOS="$work" ${TMPDIR:-/tmp}/nuvio-text-largura
