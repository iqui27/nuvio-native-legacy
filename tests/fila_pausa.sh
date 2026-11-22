#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Fila antecipada na troca de motor em pausa (correcao F102, bloqueio 1 da
# revisao PR734CD-r2): quadro de >=3000 ms publicado pelo motor novo nao pode
# aparecer no primeiro desenho do motor antigo. Ver tests/fila_pausa.c.
# Precisa do libass do Homebrew; sem ele, pula.
#
#   bash tests/fila_pausa.sh
set -eu
cd "$(dirname "$0")/.."
if ! command -v pkg-config >/dev/null 2>&1 || ! pkg-config --exists libass; then
  echo "fila_pausa: libass ausente (teste ignorado)"; exit 0
fi
cc -DNV_ASS_LIBASS -DNV_ASS_NOVO -include tests/ass_pisca_gl.h -Isrc -Itests \
  tests/fila_pausa.c src/assperf.c src/assfios.c tests/ass_render_stub.c \
  -o ${TMPDIR:-/tmp}/nuvio-fila-pausa-test \
  $(pkg-config --cflags --libs libass) -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lGLESv2 -lfribidi -lpthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-fila-pausa-test 2>/dev/null
