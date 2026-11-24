#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# The worker's per-frame shortcuts (22/11/2026): event time index, rigid
# signature cache, flatten of split frames. See tests/ass_indice.c. Needs
# libass (pkg-config); without it, skipped.
set -eu
cd "$(dirname "$0")/.."
if ! command -v pkg-config >/dev/null 2>&1 || ! pkg-config --exists libass; then
  echo "ass_indice: libass ausente (teste ignorado)"; exit 0
fi
SDLI="${NV_SDL_INC:-$(pkg-config --cflags-only-I sdl2 2>/dev/null | sed 's#/SDL2##') -I/opt/homebrew/include}"
cc -DNV_ASS_LIBASS -DNV_ASS_NOVO -DNV_ASSPERF_TESTE -include tests/ass_matriz_gl.h \
  -Isrc $SDLI tests/ass_indice.c src/asssinais.c src/assperf.c src/assfios.c tests/ass_render_stub.c \
  -o "$TMPDIR/nuvio-ass-indice-test" \
  $(pkg-config --cflags --libs libass) -lSDL2 -lpthread -lm -Wno-deprecated-declarations
"$TMPDIR/nuvio-ass-indice-test"
