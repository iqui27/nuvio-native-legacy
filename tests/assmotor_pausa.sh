#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Troca do motor ASS em pausa (PR 734 D, bloqueio 2 da revisao): placa visivel
# no motor novo, video parado, a chave desliga — a placa tem de continuar no
# MESMO pts pelo caminho antigo e os recursos do motor novo saem na hora.
# Ver tests/assmotor_pausa.c. Precisa do libass do Homebrew; sem ele, pula.
#
# Contra a base: -Dassrender_motor_invalidar=assrender_aplicar_invalidacao
# reproduz a chamada de b3a9a7108 (a funcao nova nao existia) e o teste falha.
#
#   bash tests/assmotor_pausa.sh
set -eu
cd "$(dirname "$0")/.."
if ! command -v pkg-config >/dev/null 2>&1 || ! pkg-config --exists libass; then
  echo "assmotor_pausa: libass ausente (teste ignorado)"; exit 0
fi
cc -DNV_ASS_LIBASS -DNV_ASS_NOVO -include tests/ass_pisca_gl.h -Isrc -Itests \
  tests/assmotor_pausa.c src/assperf.c src/assfios.c tests/ass_render_stub.c \
  -o ${TMPDIR:-/tmp}/nuvio-assmotor-pausa-test \
  $(pkg-config --cflags --libs libass) -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lGLESv2 -framework OpenGL -lpthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-assmotor-pausa-test 2>/dev/null
