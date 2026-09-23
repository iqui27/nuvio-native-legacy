#!/bin/bash
# Queda da C9 no libass (ass_start_frame assert) ao reentregar fontes. Ver
# tests/ass_fontes.c. Precisa do libass do Homebrew; sem ele, pula.
set -eu
cd "$(dirname "$0")/.."
if ! command -v pkg-config >/dev/null 2>&1 || ! pkg-config --exists libass; then
  echo "ass_fontes: libass ausente (teste ignorado)"; exit 0
fi
cc -DNV_ASS_LIBASS -Isrc tests/ass_fontes.c src/assrender.c -o /tmp/nuvio-ass-fontes-test \
  $(pkg-config --cflags --libs libass) -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -framework OpenGL -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-ass-fontes-test 2>/dev/null
