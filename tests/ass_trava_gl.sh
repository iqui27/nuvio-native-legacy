#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Log 14 (23/11/2026): per-frame GL-thread calls never wait for a libass
# render holding assTrava. See tests/ass_trava_gl.c. Needs
# libass (pkg-config); without it, skipped.
set -eu
cd "$(dirname "$0")/.."
if ! command -v pkg-config >/dev/null 2>&1 || ! pkg-config --exists libass; then
  echo "ass_trava_gl: libass ausente (teste ignorado)"; exit 0
fi
SDLI="${NV_SDL_INC:-$(pkg-config --cflags-only-I sdl2 2>/dev/null | sed 's#/SDL2##') -I/opt/homebrew/include}"
cc -DNV_ASS_LIBASS -DNV_ASS_NOVO -DNV_ASSPERF_TESTE -include tests/ass_matriz_gl.h \
  -Isrc $SDLI tests/ass_trava_gl.c src/asssinais.c src/assperf.c src/assfios.c tests/ass_render_stub.c \
  -o "$TMPDIR/nuvio-ass-trava-gl-test" \
  $(pkg-config --cflags --libs libass) -lSDL2 -lpthread -lm -Wno-deprecated-declarations
"$TMPDIR/nuvio-ass-trava-gl-test"
