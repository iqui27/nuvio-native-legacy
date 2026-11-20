#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-subtitle-bold.XXXXXX")
trap 'rm -rf "$work"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/text.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/subtitle_fallback_bold.c -Isrc -o "$work/text" \
  -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
pkg-config --exists libass
cc -DNV_ASS_LIBASS -include tests/ass_pisca_gl.h -Isrc tests/ass_fallback_bold.c \
  src/plainass.c src/bidi.c -o "$work/ass" \
  $(pkg-config --cflags --libs libass fribidi) \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
  -lSDL2 -framework OpenGL -Wno-deprecated-declarations -Wno-macro-redefined
rc=0
NUVIO_SEM_RESERVA_DE_SISTEMA=1 NUVIO_DADOS="$work" "$work/text" || rc=1
"$work/ass" || rc=1
exit "$rc"
